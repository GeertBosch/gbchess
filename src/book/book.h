#pragma once

#include <cstdint>
#include <sys/types.h>
#include <unordered_map>

#include "book/pgn/pgn.h"

namespace book {
// Opening book using Bayesian statistics with Thompson sampling to balance exploration and
// exploitation. Uses Dirichlet priors to shrink small-sample estimates toward reasonable
// chess expectations, and Thompson sampling for move selection (automatically balances
// strength, uncertainty, and variety without tuning constants).

// Minimum number of games required to consider a move from the book
static constexpr uint32_t kMinGames = 10;

// Prior strength for Bayesian shrinkage (virtual games)
static constexpr double kPriorStrength = 50.0;

struct DirichletPrior {
    double ɑW;  // Prior pseudo-count for wins
    double ɑD;  // Prior pseudo-count for draws
    double ɑL;  // Prior pseudo-count for losses

    /** Compute prior from global W/D/L frequencies, scaled by strength k */
    static DirichletPrior fromGlobalStats(uint64_t totalW,
                                          uint64_t totalD,
                                          uint64_t totalL,
                                          double k = kPriorStrength);
};

struct BookEntry {
    static constexpr size_t kCountBits = 20;
    static constexpr uint64_t kMaxResultCount = (1ull << kCountBits) - 1;
    uint64_t white : kCountBits;  // White wins
    uint64_t draw : kCountBits;   // Draws
    uint64_t black : kCountBits;  // Black wins

    bool add(pgn::Termination term);  // Accepts: WHITE_WIN, DRAW, BLACK_WIN
    bool full() const { return std::max({white, draw, black}) == kMaxResultCount; }
    uint64_t total() const { return white + draw + black; }  // May be up to 3 * kMaxResultCount

    /** Compute posterior mean score using Bayesian shrinkage with given prior */
    double posteriorMean(Color active, const DirichletPrior& prior) const;
};

struct Book {
    std::unordered_map<uint64_t, BookEntry> entries;

    operator bool() const { return !entries.empty(); }

    /** Reset the random number generator with the given seed. Zero disables random selection. */
    static void reseed(uint64_t seed);

    /** Insert a verified game into the book.  */
    void insert(pgn::VerifiedGame game);

    /**
     * Choose a book move for the given position after applying the given moves, or return a null
     * move if none is available.
     */
    Move choose(Position position, const MoveVector& moves);

};

/** Loads a book based on a file with PGN games. Returns an empty book if file cannot be opened. */
Book loadBook(std::string pgnfile);
}  // namespace book