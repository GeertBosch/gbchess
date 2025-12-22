#include "book/book.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "core/random.h"
#include "move/move.h"
#include "move/move_gen.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sys/types.h>

namespace book {
using Term = pgn::Termination;

namespace {
static constexpr double 𝜋 = M_PI;
static xorshift rng;  // Random number generator for move selection

/** Generate uniform random in (0, 1), avoiding exact 0 and 1 */
double uniformRandom() {
    // Use standard library to properly convert random bits to [0,1)
    double u = std::generate_canonical<double, std::numeric_limits<double>::digits>(rng);
    // Ensure strictly positive for logarithms in gamma sampling
    if (u == 0.0) u = std::nextafter(0.0, 1.0);
    return u;
}

/** Sample from Gamma(shape, scale = 1) distribution using Marsaglia & Tsang's method */
double sampleGamma(double shape) {
    if (shape < 1.0) {
        // For shape < 1, use Gamma(shape + 1) * U^(1 / shape)
        double result = sampleGamma(shape + 1.0);
        return result * std::pow(uniformRandom(), 1.0 / shape);
    }

    // Marsaglia & Tsang's method for shape >= 1
    double d = shape - 1.0 / 3.0;
    double c = 1.0 / std::sqrt(9.0 * d);

    // Squeeze constant from Marsaglia & Tsang (2000) for fast acceptance Derived as 0.07 - 1/300 to
    // approximate the acceptance region. See https://dl.acm.org/doi/pdf/10.1145/358407.358414
    constexpr double kSqueezeConstant = 0.0331;

    while (true) {
        double x, v;
        do {
            // Box-Muller for standard normal (using safe uniform random)
            double u1 = uniformRandom();
            double u2 = uniformRandom();
            x = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * 𝜋 * u2);
            v = 1.0 + c * x;
        } while (v <= 0.0);

        v = v * v * v;
        double u = uniformRandom();
        double x2 = x * x;
        if (u < 1.0 - kSqueezeConstant * x2 * x2) return d * v;
        if (std::log(u) < 0.5 * x2 + d * (1.0 - v + std::log(v))) return d * v;
    }
}

/** Sample score from Dirichlet posterior via Gamma sampling */
double samplePosteriorScore(uint64_t w, uint64_t d, uint64_t l, const DirichletPrior& prior) {
    auto γW = sampleGamma(w + prior.ɑW);
    auto γD = sampleGamma(d + prior.ɑD);
    auto γL = sampleGamma(l + prior.ɑL);
    auto Σ = γW + γD + γL;
    return (γW + 0.5 * γD) / Σ;
}

struct MoveStat {
    Move move;
    double posteriorMean;
    uint64_t games;
};

std::vector<MoveStat> collectMoveStats(const Position& position,
                                       const std::unordered_map<uint64_t, BookEntry>& entries,
                                       const DirichletPrior& prior) {
    std::vector<MoveStat> moveStats;
    MoveVector moves = moves::allLegalMoves(position.turn, position.board);

    for (auto move : moves) {
        Position newPos = moves::applyMove(position, move);
        uint64_t newKey = Hash(newPos)();
        auto it = entries.find(newKey);
        if (it == entries.end()) continue;

        const BookEntry& entry = it->second;

        if (entry.total() < kMinGames) continue;  // Require minimum sample size

        // Use Bayesian posterior mean instead of raw winrate
        moveStats.push_back({move, entry.posteriorMean(position.active(), prior), entry.total()});
    }

    // Sort by posterior mean descending (for debugging/display)
    std::sort(moveStats.begin(), moveStats.end(), [](const MoveStat& a, const MoveStat& b) {
        return a.posteriorMean > b.posteriorMean;
    });

    return moveStats;
}

Move selectMove(const Position& position,
                const std::unordered_map<uint64_t, BookEntry>& entries,
                const std::vector<MoveStat>& moveStats,
                const DirichletPrior& prior) {
    if (moveStats.empty()) return Move();

    // Use Thompson sampling: sample from each move's Dirichlet posterior and pick max
    Move bestMove;
    double bestScore = -1.0;

    for (const auto& ms : moveStats) {
        Position newPos = moves::applyMove(position, ms.move);
        uint64_t newKey = Hash(newPos)();
        const BookEntry& entry = entries.at(newKey);

        // Get W/D/L counts for active player
        uint64_t w = position.active() == Color::w ? entry.white : entry.black;
        uint64_t d = entry.draw;
        uint64_t l = position.active() == Color::w ? entry.black : entry.white;

        double sampledScore = samplePosteriorScore(w, d, l, prior);
        if (sampledScore > bestScore)
            std::tie(bestScore, bestMove) = std::make_pair(sampledScore, ms.move);
    }

    return bestMove;
}
}  // namespace

bool BookEntry::add(Term term) {
    if (full()) return false;  // Avoid overflow without bias

    switch (term) {
    case Term::WHITE_WIN: return ++white;
    case Term::BLACK_WIN: return ++black;
    case Term::DRAW: return ++draw;
    default: return false;
    }
}

double BookEntry::posteriorMean(Color active, const DirichletPrior& prior) const {
    uint64_t w = active == Color::w ? white : black;
    uint64_t d = draw;
    uint64_t l = active == Color::w ? black : white;
    double totalPosterior = w + prior.ɑW + d + prior.ɑD + l + prior.ɑL;
    return ((w + prior.ɑW) + 0.5 * (d + prior.ɑD)) / totalPosterior;
}

DirichletPrior DirichletPrior::fromGlobalStats(uint64_t totalW,
                                               uint64_t totalD,
                                               uint64_t totalL,
                                               double k) {
    uint64_t total = totalW + totalD + totalL;
    if (total == 0) return {k / 3.0, k / 3.0, k / 3.0};  // Uniform prior if no data
    return {k * totalW / total, k * totalD / total, k * totalL / total};
}

void Book::reseed(uint64_t seed) {
    rng = xorshift(seed);
}

void Book::insert(pgn::VerifiedGame game) {
    using Term = pgn::Termination;
    auto&& [moves, term] = game;

    if (term != Term::WHITE_WIN && term != Term::BLACK_WIN && term != Term::DRAW)
        return;  // Only record decided games

    Position position = Position::initial();
    for (auto move : moves) {
        position = moves::applyMove(position, move);
        uint64_t key = Hash(position)();
        entries[key].add(term);
    }
}

Move Book::choose(Position position, const MoveVector& moves) {
    // Apply any moves to reach the current position
    for (auto move : moves) position = moves::applyMove(position, move);

    // Compute prior from global statistics
    uint64_t totalW = 0, totalD = 0, totalL = 0;
    for (const auto& [key, entry] : entries) {
        totalW += entry.white;
        totalD += entry.draw;
        totalL += entry.black;
    }
    DirichletPrior prior = DirichletPrior::fromGlobalStats(totalW, totalD, totalL);

    auto moveStats = collectMoveStats(position, entries, prior);
    return selectMove(position, entries, moveStats, prior);
}
Book loadBook(std::string pgnfile) {
    std::ifstream in(pgnfile);
    if (!in || !in.is_open()) std::cerr << "Could not open book file: " << pgnfile << "\n";

    Book book;
    uint64_t gameCount = 0;
    while (in) {
        auto game = pgn::readPGN(in);
        auto whiteElo = game["WhiteElo"];
        auto blackElo = game["BlackElo"];
        if (!whiteElo.empty() && std::stoi(whiteElo) < 2400) continue;  // Skip low-rated games
        if (!blackElo.empty() && std::stoi(blackElo) < 2400) continue;  //
        auto verifiedGame = pgn::verify(game);
        ++gameCount;
        book.insert(verifiedGame);
    }
    std::cout << "Loaded book " << pgnfile << " with " << book.entries.size() << " positions from "
              << gameCount << " games\n";
    return book;
}
}  // namespace book