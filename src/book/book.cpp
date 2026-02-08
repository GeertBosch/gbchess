#include "book/book.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "core/random.h"
#include "core/text_util.h"
#include "engine/fen/fen.h"
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
                const DirichletPrior& prior,
                double temperature) {
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

        // Apply temperature to the sampled score (the stochastic component)
        // Lower temperature = more exploitation (amplify differences from 0.5)
        // Higher temperature = more exploration (compress toward 0.5)
        sampledScore = 0.5 + (sampledScore - 0.5) / temperature;

        // Add game count bonus, scaled by temperature^2 to reduce its influence at high temp
        // At high temperature, we want mostly stochastic exploration
        double gameCountBonus =
            (kGameCountBonus * std::log(entry.total())) / (temperature * temperature);

        // Add uniform noise that scales with temperature for true exploration at high temperatures
        // This ensures that at very high temperature, selection approaches uniform random
        double noise = (temperature - 1.0) * uniformRandom() * 0.1;

        double finalScore = sampledScore + gameCountBonus + noise;

        if (finalScore > bestScore)
            std::tie(bestScore, bestMove) = std::make_pair(finalScore, ms.move);
    }

    return bestMove;
}
}  // namespace

uint64_t BookEntry::add(Term term) {
    return full()                 ? 0  // Avoid overflow
        : term == Term::WHITE_WIN ? ++white + black + draw
        : term == Term::BLACK_WIN ? ++black + white + draw
        : term == Term::DRAW      ? ++draw + white + black
                                  : 0;
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

    auto moveStats = collectMoveStats(position, entries, prior);
    return selectMove(position, entries, moveStats, prior, temperature);
}

Book loadBook(std::string csvfile) {
    std::ifstream in(csvfile);
    if (!in || !in.is_open()) {
        std::cerr << "Could not open book file: " << csvfile << "\n";
        return Book();
    }

    Book book;
    std::string line;

    // Read header line to fine column positions
    std::getline(in, line);
    auto header = split(line, ',');
    auto fenCol = find(header, "fen");
    auto whiteCol = find(header, "white");
    auto drawCol = find(header, "draw");
    auto blackCol = find(header, "black");

    uint64_t totalW = 0, totalD = 0, totalL = 0;

    while (std::getline(in, line)) {
        auto columns = split(line, ',');
        if (columns.size() < header.size()) continue;  // Skip malformed lines

        // Parse CSV fields
        auto pos = fen::parsePosition(columns[fenCol]);
        auto white = std::stoull(columns[whiteCol]);
        auto draw = std::stoull(columns[drawCol]);
        auto black = std::stoull(columns[blackCol]);

        uint64_t key = Hash(pos)();

        book.entries[key] = {white, draw, black};

        totalW += white;
        totalD += draw;
        totalL += black;
    }

    book.prior = DirichletPrior::fromGlobalStats(totalW, totalD, totalL);

    std::cout << "Loaded book " << csvfile << " with " << book.entries.size() << " positions\n";
    return book;
}
}  // namespace book