#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <utility>

#include "core/core.h"
#include "core/hash/hash.h"
#include "core/options.h"
#include "core/square_set/occupancy.h"
#include "core/uint128_str.h"
#include "engine/fen/fen.h"
#include "eval/eval.h"
#include "eval/nnue/nnue.h"
#include "move/move.h"
#include "move/move_gen.h"
#include "pv.h"

#include "search.h"

namespace search {
EvalTable evalTable;

int maxSelDepth = 0;

// Diagnostic counters
uint64_t evalCount = 0;
uint64_t nodeCount = 0;
uint64_t quiescenceCount = 0;
uint64_t cacheCount = 0;

uint64_t betaCutoffs = 0;
uint64_t countermoveAttempts = 0;
uint64_t countermoveHits = 0;
uint64_t firstMoveCutoffs = 0;
uint64_t futilityPruned = 0;
uint64_t mainSeePruned = 0;
uint64_t lmrAttempts = 0;
uint64_t lmrResearches = 0;
uint64_t nullMoveAttempts = 0;
uint64_t nullMoveCutoffs = 0;
uint64_t nullMoveSkippedInCheck = 0;
uint64_t nullMoveSkippedMate = 0;
uint64_t nullMoveSkippedEndgame = 0;
uint64_t nullMoveSkippedPV = 0;
uint64_t nullMoveSkippedBehind = 0;
uint64_t pvsAttempts = 0;
uint64_t pvsResearches = 0;
uint64_t rootBetaCutoffs = 0;
uint64_t rootCutoffMove1 = 0;
uint64_t rootCutoffMoveLe3 = 0;
uint64_t rootCutoffMoveGt3 = 0;
uint64_t ply1BetaCutoffs = 0;
uint64_t ply1CutoffMove1 = 0;
uint64_t ply1CutoffMoveLe3 = 0;
uint64_t ply1CutoffMoveGt3 = 0;
uint64_t rootMoveVisits = 0;
uint64_t rootSearchCalls = 0;
uint64_t aspirationAttempts = 0;
uint64_t aspirationFailLow = 0;
uint64_t aspirationFailHigh = 0;
uint64_t aspirationFallbackFullWindow = 0;
uint64_t qsTTCutoffs = 0;
uint64_t qsTTRefinements = 0;
uint64_t qsTTRefineNoCut = 0;
uint64_t ttCutoffs = 0;
uint64_t ttRefinements = 0;
uint64_t ttRefineNoCut = 0;
uint64_t ttRefineNoCutShallow = 0;
uint64_t ttProbesMain = 0;
uint64_t ttHitsMain = 0;
uint64_t ttNoCutMain = 0;
uint64_t ttMissKeyMain = 0;
uint64_t ttMissDepthMain = 0;
uint64_t ttMissGenerationMain = 0;
uint64_t ttMissRepetitionMain = 0;
uint64_t ttProbesQs = 0;
uint64_t ttHitsQs = 0;
uint64_t ttNoCutQs = 0;
uint64_t ttMissKeyQs = 0;
uint64_t ttMissDepthQs = 0;
uint64_t ttMissGenerationQs = 0;
uint64_t ttMissRepetitionQs = 0;
uint64_t ttInsertAttempts = 0;
uint64_t ttInsertWrites = 0;
uint64_t ttInsertRejected = 0;
uint64_t ttInsertExact = 0;
uint64_t ttInsertLower = 0;
uint64_t ttInsertUpper = 0;
uint64_t ttRawProbesMain = 0;
uint64_t ttRawHitsMain = 0;
uint64_t ttRawProbesQs = 0;
uint64_t ttRawHitsQs = 0;
uint64_t shallowMainNodes = 0;
uint64_t shallowLeavesToQS = 0;

namespace {
using namespace std::chrono;
using clock = high_resolution_clock;
using timepoint = clock::time_point;

// Copy various counters at the start of the search, so they can be reported later
uint64_t searchEvalCount = 0;
uint64_t searchNodeCount = 0;
uint64_t searchQuiescenceCount = 0;
uint64_t searchCacheCount = 0;
timepoint searchStartTime = {};

std::optional<nnue::NNUE> network = nnue::loadNNUE("nn-82215d0fd0df.nnue", nnue::kQuiet);

std::string pct(uint64_t some, uint64_t all) {
    return all ? " " + std::to_string((some * 100) / all) + "%" : "";
}

}  // namespace

static Hash debugHash = {};
void debugPosition(Position position) {
    debugHash = Hash(position);
    // Convert debugHash to a hexadecimal string
    std::cerr << "Debugging position:\n"
              << fen::to_string(position) << ", hash 0x" << to_hex_string(debugHash()) << "\n";
}

namespace {

/**
 * The repetition table stores the hashes of all positions played so far in the game, so we can
 * detect repetitions and apply the rule that a third repetition is a draw.
 */
class Repetitions {
    std::vector<Hash> hashes;
    void push_back(Hash hash) { hashes.push_back(hash); }
    void pop_back() { hashes.pop_back(); }
    Hash back() const { return hashes.back(); }

    /**
     * Returns the number of occurrences in the game of a position with the same hash. Only
     * considers up to halfmove moves, as any capture or pawn move prevents repetition.
     */
    int count(Hash hash, int halfmove) const {
        // When starting with some FEN game positions, we may not have all move history.
        halfmove = std::min(halfmove, int(hashes.size()));

        int count = 0;
        for (auto it = hashes.begin() + hashes.size() - halfmove; it != hashes.end(); ++it) {
            count += (*it == hash);
        }
        return count;
    }

public:
    Repetitions() = default;
    void clear() { hashes.clear(); }

    /**
     * RAII type that will remove the hashes from the repetition table when it goes out of scope.
     */
    class State {
        friend class Repetitions;
        Repetitions& repetitions;
        size_t count;
        State(Repetitions& repetitions) : repetitions(repetitions), count(1) {};

    public:
        ~State() {
            while (count--) repetitions.pop_back();
        }
    };

    /**
     * Returns true iff the state represents a game drawn by repetition or the fifty move rule.
     * Avoid any repetition if possible, so the transposition table remains accurate.
     */
    bool drawn(int halfmove) const {
        // Need at least 4 half moves since the last irreversible move, to get to draw by
        // repetition. Account for the move we're about to make, which is not yet in the table.
        ++halfmove;
        if (halfmove < 4) return false;
        if (halfmove >= 100) return true;  // Fifty-move rule

        // Check for repetition
        return count(hashes.back(), halfmove) >= 2;  // Avoid repeating
    }

    /**
     * Enters a new position in the repetition table. The hash is the hash of the position.
     * Returns a RAII State object that will remove the hash from the table when it goes out of
     * scope. The state object reports whether the position is a draw by repetition.
     */
    [[nodiscard]] State enter(Hash hash) {
        push_back(hash);
        return State(*this);
    }
    void enter(State& state, Hash hash) {
        push_back(hash);
        ++state.count;
    }
} repetitions;

/**
 * The transposition table is a hash table that stores the best move found for a position, so it can
 * be reused in subsequent searches. The table is indexed by the hash of the position, and stores
 * the best move found so far, along with the evaluation of the position after that move.
 * Evaluations are always from the perspective of the player to move. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 */
struct TranspositionTable {

    enum class EntryType : uint8_t { EXACT, LOWERBOUND, UPPERBOUND };
    std::string to_string(EntryType type) const {
        switch (type) {
        case EntryType::EXACT: return "EXACT";
        case EntryType::LOWERBOUND: return "LOWERBOUND";
        case EntryType::UPPERBOUND: return "UPPERBOUND";
        default: return "UNKNOWN";
        }
    }
    struct Eval {
        Move move;
        Score score = Score::min();
        operator bool() const { return move; }
    };

    struct Entry {
        uint64_t key = 0;  // High bits of the hash, used to check for collisions
        Eval eval;
        uint8_t depthleft = 0;
        EntryType type = EntryType::EXACT;
        uint8_t generation;
        uint8_t fullMoveNumber;  // for informational purposes only, not used in lookups
        Entry() = default;

        Entry(Hash hash,
              Eval move,
              uint8_t depth,
              EntryType type,
              uint8_t generation,
              uint8_t fullMoveNumber)
            : key(keyOf(hash)),
              eval(move),
              depthleft(depth),
              type(type),
              generation(generation),
              fullMoveNumber(fullMoveNumber) {}
    };

    struct Stats {
        uint64_t numInserted = 0;
        uint64_t numWorse = 0;
        uint64_t numOccupied = 0;
        uint64_t numCollisions = 0;
        uint64_t numImproved = 0;
        uint64_t numHits = 0;
        uint64_t numMisses = 0;
    };

    static constexpr size_t MB = 1024 * 1024;
    static constexpr size_t kNumEntries = options::transpositionTableMB * MB / sizeof(Entry);

    static uint64_t keyOf(Hash hash) { return hash() >> 64; }
    static uint64_t indexOf(Hash hash) { return hash() % kNumEntries; }

    std::vector<Entry> entries{kNumEntries};
    uint8_t numGenerations = 0;
    Stats stats;

    ~TranspositionTable() {
        if (transpositionTableDebug) printStats();
    }

    Eval find(Hash hash) {
        if constexpr (kNumEntries == 0) return {};

        auto& entry = entries[indexOf(hash)];
        ++stats.numMisses;
        if (keyOf(hash) != entry.key || entry.generation != numGenerations) return {};

        --stats.numMisses;
        ++stats.numHits;

        if (hash == debugHash)
            std::cout << "Found debug position with move " << ::to_string(entry.eval.move)
                      << " score " << std::string(entry.eval.score) << " depthleft "
                      << int(entry.depthleft) << " type " << to_string(entry.type) << "\n";
        return entry.eval;
    }

    Eval peek(Hash hash) const {
        if constexpr (kNumEntries == 0) return {};

        const auto& entry = entries[indexOf(hash)];
        if (keyOf(hash) != entry.key || entry.generation != numGenerations) return {};
        return entry.eval;
    }

    PrincipalVariation pv(Position pos, int depth) {
        if constexpr (kNumEntries == 0) return {};
        auto hash = Hash(pos);
        auto eval = find(hash);
        auto move = eval.move;
        if (!move) return {};
        PrincipalVariation pv(Move(), eval.score);
        do {
            pv.moves.push_back(move);
            pos = moves::applyMove(pos, move);
            hash = Hash(pos);
        } while ((move = find(hash).move) && --depth > 0);
        return pv;
    }

    enum class RefineResult : uint8_t {
        Hit,
        MissKey,
        MissDepth,
        MissGeneration,
        MissRepetition,
    };

    RefineResult refineAlphaBeta(Hash hash, Turn turn, int depthleft, Score& alpha, Score& beta) {
        if constexpr (kNumEntries == 0) return RefineResult::MissKey;
        auto idx = indexOf(hash);
        auto& entry = entries[idx];

        ++stats.numMisses;
        if (entry.key != keyOf(hash)) return RefineResult::MissKey;
        if (entry.depthleft < depthleft) return RefineResult::MissDepth;
        if (entry.generation != numGenerations) return RefineResult::MissGeneration;
        if (repetitions.drawn(turn.halfmove())) {
            // Don't refine if this position may be drawn by repetition, to avoid
            // polluting the table with inaccurate evaluations.
            return RefineResult::MissRepetition;
        }
        --stats.numMisses;
        ++stats.numHits;

        ++cacheCount;
        if (depthleft)
            ++ttRefinements;
        else
            ++qsTTRefinements;

        switch (entry.type) {
        case EntryType::EXACT: alpha = beta = entry.eval.score; break;
        case EntryType::LOWERBOUND: alpha = std::max(alpha, entry.eval.score); break;
        case EntryType::UPPERBOUND: beta = std::min(beta, entry.eval.score); break;
        }
        return RefineResult::Hit;
    }

    /**
     * Decide whether a TT entry should be replaced.
     */
    [[nodiscard]] bool shouldReplace(const Entry& oldEntry, const Entry& newEntry) {
        // Empty slot or past generation, always replace
        if (!oldEntry.eval.move || oldEntry.generation != newEntry.generation) return true;

        // Collision: prefer replacing shallower entries
        if (oldEntry.key != newEntry.key) return newEntry.depthleft > oldEntry.depthleft;

        // Same key from here on. Prefer deeper entries
        if (newEntry.depthleft != oldEntry.depthleft)
            return newEntry.depthleft > oldEntry.depthleft;

        // Equal depth from here on. Prefer EXACT over bounds.
        if (oldEntry.type == EntryType::EXACT) return false;
        if (newEntry.type == EntryType::EXACT) return true;

        // Prefer LOWERBOUND as it can cause beta cutoffs, or tighten lower bounds
        if (newEntry.type == EntryType::LOWERBOUND)
            return newEntry.eval.score > oldEntry.eval.score ||
                oldEntry.type == EntryType::UPPERBOUND;

        // New entry is upper bound. Keep it if it's tighter.
        return newEntry.eval.score <
            oldEntry.eval.score;  // tighter upper bound or keep lower bound
    }

    void insert(Hash hash, int16_t moveNumber, Eval move, uint8_t depthleft, EntryType type) {
        if constexpr (kNumEntries == 0) return;
        if (!move || depthleft < 1) return;

        ++ttInsertAttempts;

        const Entry newEntry = {hash, move, depthleft, type, numGenerations, uint8_t(moveNumber)};
        auto& oldEntry = entries[indexOf(hash)];
        if (!shouldReplace(oldEntry, newEntry)) {
            ++stats.numWorse;
            ++ttInsertRejected;
            return;
        }
        ++stats.numInserted;
        ++ttInsertWrites;
        if (type == EntryType::EXACT)
            ++ttInsertExact;
        else if (type == EntryType::LOWERBOUND)
            ++ttInsertLower;
        else
            ++ttInsertUpper;

        // 3 cases: improve existing entry, collision with unrelated entry, or occupy an empty slot
        if (oldEntry.key == keyOf(hash))
            ++stats.numImproved;
        else if (oldEntry.eval.move)
            ++stats.numCollisions;
        else
            ++stats.numOccupied;  // nothing in this slot yet

        if (hash == debugHash)
            std::cout << "Inserting debug position with move " << ::to_string(move.move)
                      << " score " << std::string(move.score) << " depthleft " << int(depthleft)
                      << " type " << to_string(type) << "\n";

        oldEntry = newEntry;
    }

    void insert(
        Hash hash, int16_t fullMoveNumber, Eval move, uint8_t depthleft, Score alpha, Score beta) {
        if (move.score > alpha && move.score < beta)
            insert(hash, fullMoveNumber, move, depthleft, TranspositionTable::EntryType::EXACT);
        else if (move.score <= alpha)
            insert(
                hash, fullMoveNumber, move, depthleft, TranspositionTable::EntryType::UPPERBOUND);
        else if (move.score >= beta)
            insert(
                hash, fullMoveNumber, move, depthleft, TranspositionTable::EntryType::LOWERBOUND);
    }

    void clear() {
        if (debug && transpositionTableDebug && stats.numInserted) {
            printStats();
            std::cout << "Cleared transposition table\n";
        }
        stats = {};
        if (!++numGenerations) std::fill(entries.begin(), entries.end(), Entry{});
    };

    void printStats() {
        auto numLookups = stats.numHits + stats.numMisses;
        auto numInserts = stats.numInserted + stats.numWorse;
        std::cout << "Transposition table stats:\n";
        std::cout << "  occupied: " << stats.numOccupied << pct(stats.numOccupied, kNumEntries)
                  << "\n";
        std::cout << "  inserts: " << stats.numInserted << "\n";
        std::cout << "  worse: " << stats.numWorse << pct(stats.numWorse, numInserts) << "\n";
        std::cout << "  collisions: " << stats.numCollisions << pct(stats.numCollisions, numInserts)
                  << "\n";
        std::cout << "  improved: " << stats.numImproved << pct(stats.numImproved, numInserts)
                  << "\n";
        std::cout << "  lookup hits: " << stats.numHits << pct(stats.numHits, numLookups) << "\n";
        std::cout << "  lookup misses: " << stats.numMisses << pct(stats.numMisses, numLookups)
                  << "\n";
    }
} transpositionTable;

/**
 * History table for move ordering.
 */
size_t history[2][kNumSquares][kNumSquares] = {{{0}}};

/**
 * Countermove heuristic: stores the best move to play in response to opponent's last move.
 * Indexed by [color][to_square] of the opponent's move.
 */
Move countermoves[2][kNumSquares] = {{}};

/**
 * Killer move heuristic: stores the best quiet moves that caused beta cutoffs at each depth.
 */
Move killerMoves[options::maxKillerDepth][options::maxKillerMoves] = {{{}}};

void storeKillerMove(Move move, int depth) {
    if (depth >= options::maxKillerDepth || depth < 0 || !options::maxKillerMoves) return;

    // Only store quiet moves (not captures, promotions, or castling)
    if (move.kind != MoveKind::Quiet_Move && move.kind != MoveKind::Double_Push) return;

    // Don't store if it's already the first killer move
    if (killerMoves[depth][0] == move) return;

    // Shift moves: second becomes first, new move becomes second, etc.
    for (int i = options::maxKillerMoves - 1; i > 0; --i)
        killerMoves[depth][i] = killerMoves[depth][i - 1];

    killerMoves[depth][0] = move;
}

using MoveIt = MoveVector::iterator;

[[maybe_unused]] MoveIt sortCountermove(Move countermove, MoveIt begin, MoveIt end) {
    if (!countermove) return begin;

    ++countermoveAttempts;
    auto it = std::find(begin, end, countermove);
    if (it != end) {
        ++countermoveHits;
        std::swap(*begin++, *it);
    }
    return begin;
}

[[maybe_unused]] MoveIt sortKillerMoves(int depth, MoveIt begin, MoveIt end) {
    if (depth >= options::maxKillerDepth) return begin;

    MoveIt current = begin;

    // Try to find and move killer moves to the front
    for (int i = 0; i < options::maxKillerMoves && current < end; ++i) {
        Move killer = killerMoves[depth][i];
        if (!killer) continue;

        auto it = std::find(current, end, killer);
        if (it != end) std::swap(*current++, *it);
    }

    return current;
}
}  // namespace

/**
 * Sorts the moves (including captures) in the range [begin, end) in place, so that the move or
 * capture from the transposition table, if any, comes first. Returns an iterator to the next move.
 */
MoveIt sortTransposition(Move ttMove, Hash hash, MoveIt begin, MoveIt end) {
    // Prefer the probed TT move when available, otherwise do a lightweight table peek.
    auto cachedMove = ttMove ? ttMove : transpositionTable.peek(hash).move;
    if (!cachedMove) return begin;

    // If the move is not part of the current legal moves, nothing to do here.
    auto it = std::find(begin, end, cachedMove);

    // If the move was part of a pv for this position, move it to the beginning of the list
    if (it != end) std::swap(*begin++, *it);

    // begin now points past the transposition move, or the beginning of the list
    return begin;
}

/**
 * Sorts the moves in the range [begin, end) in place, so that captures come before
 * non-captures, and captures are sorted by victim value, attacker value, and move kind.
 * Countermove and killer moves are placed among the quiet moves.
 */
MoveIt sortMoves(const Position& position, MoveIt begin, MoveIt end, Move lastMove, int depth = 0) {
    if (begin == end) return begin;

    // First, partition captures from quiet moves using MoveKind
    auto quietBegin = std::partition(begin, end, [](const Move& move) {
        return move.kind != MoveKind::Quiet_Move && move.kind != MoveKind::Double_Push;
    });

    // Sort captures/promotions by their score (highest first)
    std::stable_sort(begin, quietBegin, [&](const Move& a, const Move& b) {
        return scoreMove(position.board, a) > scoreMove(position.board, b);
    });

    // For quiet moves, first try countermove, then killer moves
    if constexpr (options::useCountermove) {
        if (lastMove) {
            auto piece = position.board[lastMove.to];
            Move countermove = countermoves[int(color(piece))][lastMove.to];
            quietBegin = sortCountermove(countermove, quietBegin, end);
        }
    }
    if constexpr (options::maxKillerMoves > 0 && options::maxKillerDepth > 0)
        quietBegin = sortKillerMoves(depth, quietBegin, end);

    // Then sort remaining quiet moves by history heuristic
    std::stable_sort(quietBegin, end, [&](const Move& a, const Move& b) {
        return history[int(position.active())][a.from][a.to] >
            history[int(position.active())][b.from][b.to];
    });

    return begin;
}

void sortMoves(const Position& position,
               Move ttMove,
               Hash hash,
               MoveIt begin,
               MoveIt end,
               Move lastMove,
               int depth = 0) {
    begin = sortTransposition(ttMove, hash, begin, end);
    begin = sortMoves(position, begin, end, lastMove, depth);
}

void sortMoves(
    const Position& position, Hash hash, MoveIt begin, MoveIt end, Move lastMove, int depth = 0) {
    sortMoves(position, Move(), hash, begin, end, lastMove, depth);
}

std::pair<moves::UndoPosition, Score> makeMoveWithEval(Position& position, Move move, Score eval) {
    using namespace moves;
    BoardChange change = prepareMove(position.board, move);
    UndoPosition undo;
    if constexpr (options::useNNUE) {
        // Use NNUE evaluation, which is more accurate than the piece-square evaluation
        undo = makeMove(position, change, move);
        eval = Score::fromCP(nnue::evaluate(position, *network));
        // Note that we evaluated the board after the move, so our evaluation is the inverse
        if (position.active() == Color::w) eval = -eval;
    } else if constexpr (options::incrementalEvaluation) {
        // Incremental evaluation of the piece-square evaluation
        eval += evaluateMove(position.board, position.active(), change, evalTable);
        undo = makeMove(position, change, move);
        dassert(-eval == evaluateBoard(position.board, position.active(), evalTable));
    } else {
        // Use non-incremental piece-square evaluation, the simplest approach
        undo = makeMove(position, change, move);
        eval = evaluateBoard(position.board, !position.active(), evalTable);
    }
    return {undo, eval};
}

bool isQuiet(Position& position, int depthleft) {
    if (isInCheck(position)) return false;
    if (depthleft <= options::promotionMinDepthLeft) return true;
    if (moves::mayHavePromoMove(
            !position.active(), position.board, Occupancy(position.board, !position.active())))
        return false;
    return true;
}

Score quiesce(Position& position, Score alpha, Score beta, int depthleft, Score standPat) {
    ++nodeCount;

    if constexpr (options::useQsTT) {
        Hash hash(position);
        ++ttRawProbesQs;
        if (transpositionTable.peek(hash).move) ++ttRawHitsQs;
        ++ttProbesQs;
        auto refine = transpositionTable.refineAlphaBeta(hash, position.turn, 0, alpha, beta);
        if (refine == TranspositionTable::RefineResult::Hit)
            ++ttHitsQs;
        else if (refine == TranspositionTable::RefineResult::MissKey)
            ++ttMissKeyQs;
        else if (refine == TranspositionTable::RefineResult::MissDepth)
            ++ttMissDepthQs;
        else if (refine == TranspositionTable::RefineResult::MissGeneration)
            ++ttMissGenerationQs;
        else if (refine == TranspositionTable::RefineResult::MissRepetition)
            ++ttMissRepetitionQs;

        if (alpha >= beta) {
            ++qsTTCutoffs;
            return beta;
        }
        ++qsTTRefineNoCut;
        if (refine == TranspositionTable::RefineResult::Hit) ++ttNoCutQs;
    }

    if (!depthleft) return standPat;

    if (standPat >= beta && isQuiet(position, depthleft)) return beta;

    if (standPat > alpha && isQuiet(position, depthleft)) alpha = standPat;

    ++evalCount;

    // The moveList includes moves needed to get out of check; an empty list means mate
    auto moveList = moves::allLegalQuiescentMoves(position.turn, position.board, depthleft);
    if (moveList.empty() && isInCheck(position)) return Score::min();
    sortMoves(position, moveList.begin(), moveList.end(), Move());  // No killer moves in quiescence
    for (auto move : moveList) {
        if (options::staticExchangeEvaluation && move.kind == MoveKind::Capture &&
            staticExchangeEvaluation(position.board, move.from, move.to) < 0_cp &&
            !isInCheck(position))
            continue;  // Don't consider simple captures that lose material unless in check

        // Compute the change to the board and evaluation that results from the move
        auto [undo, newEval] = makeMoveWithEval(position, move, standPat);
        auto score = -quiesce(position, -beta, -alpha, depthleft - 1, -newEval);
        unmakeMove(position, undo);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    ++quiescenceCount;  // Increment quiescence count
    Score stand_pat;
    if (options::useNNUE)
        // Use NNUE evaluation, which is more accurate than the piece-square evaluation
        stand_pat = Score::fromCP(nnue::evaluate(position, *network));
    else
        // Use simple piece-square evaluation
        stand_pat = evaluateBoard(position.board);
    if (position.active() == Color::b) stand_pat = -stand_pat;
    return quiesce(position, alpha, beta, depthleft, stand_pat);
}

struct Depth {
    int current;
    int left;
};

Score quiesce(Position& position, int depthleft) {
    return quiesce(position, Score::min(), Score::max(), depthleft);
}

bool betaCutoff(Score score,
                Score beta,
                Move move,
                int moveCount,
                Color side,
                int ply,
                const Board& board,
                Move lastMove) {
    if (score < beta) return false;  // No beta cutoff
    ++betaCutoffs;
    if (moveCount == 1) ++firstMoveCutoffs;

    // Cutoff histograms for depth-3 efficiency analysis.
    auto bucketCutoff = [&](uint64_t& move1, uint64_t& moveLe3, uint64_t& moveGt3) {
        if (moveCount == 1)
            ++move1;
        else if (moveCount <= 3)
            ++moveLe3;
        else
            ++moveGt3;
    };
    if (ply == 0) {
        ++rootBetaCutoffs;
        bucketCutoff(rootCutoffMove1, rootCutoffMoveLe3, rootCutoffMoveGt3);
    } else if (ply == 1) {
        ++ply1BetaCutoffs;
        bucketCutoff(ply1CutoffMove1, ply1CutoffMoveLe3, ply1CutoffMoveGt3);
    }

    // Only apply heuristics to quiet moves
    if (move.kind == MoveKind::Quiet_Move || move.kind == MoveKind::Double_Push) {
        if (options::historyHeuristic) history[int(side)][move.from][move.to] += ply * ply;
        storeKillerMove(move, ply);

        // Store countermove: this move is a good response to the opponent's last move
        if (options::useCountermove && lastMove) {
            auto piece = board[lastMove.to];
            countermoves[int(color(piece))][lastMove.to] = move;
        }
    }
    return true;
}

// Forward declaration for null move pruning
PrincipalVariation alphaBeta(
    Position& position, Hash hash, Score alpha, Score beta, Depth depth, Move lastMove = Move());

/**
 * Attempts null move pruning. Returns true if we can prune (cutoff occurred).
 * Only effective in non-PV nodes with sufficient margin.
 */
bool tryNullMovePruning(Position& position, Hash hash, Score alpha, Score beta, Depth depth) {
    if (!options::nullMovePruning) return false;
    if (depth.left < options::nullMoveMinDepth) return false;

    if (beta.mate()) {
        ++nullMoveSkippedMate;
        return false;
    }

    // Skip null-move in PV nodes (full-window search). NMP is for non-PV (cut/all) nodes only.
    if (beta.cp() - alpha.cp() > 1) {
        ++nullMoveSkippedPV;
        return false;
    }

    if (isInCheck(position)) {
        ++nullMoveSkippedInCheck;
        return false;
    }
    if (!hasNonPawnMaterial(position)) {
        ++nullMoveSkippedEndgame;
        return false;
    }

    // Get quick evaluation for pruning decisions
    Score staticEval = Score::fromCP(nnue::evaluate(position, *network));
    if (position.active() == Color::b) staticEval = -staticEval;
    ++evalCount;

    if (staticEval < beta) {
        ++nullMoveSkippedBehind;
        return false;
    }

    ++nullMoveAttempts;

    // Make null move
    Turn savedTurn = position.turn;
    Hash nullHash = hash.makeNullMove(position.turn);
    position.turn.makeNullMove();
    dassert(nullHash == Hash(position));

    // Search with reduced depth
    auto nullDepth =
        Depth{depth.current + 1, std::max(0, depth.left - 1 - options::nullMoveReduction)};
    auto nullResult = -alphaBeta(position, nullHash, -beta, -beta + 1_cp, nullDepth, Move());

    position.turn = savedTurn;

    // If null move search fails high, we can prune
    bool canPrune = nullResult.score >= beta;
    nullMoveCutoffs += canPrune;
    return canPrune;
}

/**
 * Checks the transposition table and returns a cutoff PV if the TT bounds exclude the current
 * window. Validates that the TT best move doesn't walk into a draw-by-repetition that wasn't
 * present when the entry was stored (stale entry guard). Returns empty optional on miss.
 */
PrincipalVariation tryTTCutoff(
    const Position& position, Hash hash, int depthleft, Score& alpha, Score& beta, Move& ttMove) {
    auto origAlpha = alpha, origBeta = beta;
    ++ttRawProbesMain;
    if (transpositionTable.peek(hash).move) ++ttRawHitsMain;
    ++ttProbesMain;
    auto refine = transpositionTable.refineAlphaBeta(hash, position.turn, depthleft, alpha, beta);
    ttMove = transpositionTable.peek(hash).move;
    if (refine == TranspositionTable::RefineResult::Hit)
        ++ttHitsMain;
    else if (refine == TranspositionTable::RefineResult::MissKey)
        ++ttMissKeyMain;
    else if (refine == TranspositionTable::RefineResult::MissDepth)
        ++ttMissDepthMain;
    else if (refine == TranspositionTable::RefineResult::MissGeneration)
        ++ttMissGenerationMain;
    else if (refine == TranspositionTable::RefineResult::MissRepetition)
        ++ttMissRepetitionMain;

    if (alpha < beta) {
        ++ttRefineNoCut;
        if (depthleft <= 3) ++ttRefineNoCutShallow;
        if (refine == TranspositionTable::RefineResult::Hit) ++ttNoCutMain;
        return {};
    }

    if (refine != TranspositionTable::RefineResult::Hit) return {};

    auto pv = transpositionTable.pv(position, depthleft);
    if (!pv) {
        ++ttNoCutMain;
        return {};
    }

    // Guard against stale TT entries from previous turns: the TT best move may now step
    // into a position that is drawn by repetition given the current game history, even
    // though it wasn't when the entry was stored. Check one ply ahead.
    auto nextPos = moves::applyMove(position, pv.front());
    auto nextState = repetitions.enter(Hash(nextPos));
    if (!repetitions.drawn(nextPos.turn.halfmove())) return ++ttCutoffs, pv;

    // Stale entry: restore the original window and fall through to a fresh search.
    ++ttNoCutMain;
    alpha = origAlpha;
    beta = origBeta;
    return {};
}

/**
 * The alpha-beta search algorithm with fail-soft negamax search.
 * The alpha represents the best score that the maximizing player can guarantee at the current
 * level, and beta the best score that the minimizing player can guarantee. The function returns
 * the best score that the current player can guarantee, assuming the opponent plays optimally.
 * lastMove is the opponent's last move, used for countermove heuristic.
 */
PrincipalVariation alphaBeta(
    Position& position, Hash hash, Score alpha, Score beta, Depth depth, Move lastMove) {
    ++nodeCount;

    // Step-3 diagnostics: classify shallow main-search nodes and shallow leaves that enter QS.
    if (depth.current <= 3) {
        if (depth.left <= 0)
            ++shallowLeavesToQS;
        else
            ++shallowMainNodes;
    }

    // Track maximum selective depth reached in main search (excludes quiescence)
    if (depth.current > maxSelDepth) maxSelDepth = depth.current;

    // Check for draws due to repetition or the fifty-move rule
    auto drawState = repetitions.enter(hash);
    if (repetitions.drawn(position.turn.halfmove())) return {{}, Score()};

    if (depth.left <= 0) return {{}, quiesce(position, alpha, beta, options::quiescenceDepth)};

    const bool inCheck = isInCheck(position);

    // Check the transposition table, which may tighten one or both search bounds
    Move ttMove;
    if (auto pv = tryTTCutoff(position, hash, depth.left, alpha, beta, ttMove)) return pv;

    // Try null move pruning (only in non-PV nodes)
    if (tryNullMovePruning(position, hash, alpha, beta, depth)) return {{}, beta};

    // Futility pruning: if static eval is way above beta at shallow depth, return early
    // This is also called "reverse futility pruning" or "static null move pruning"
    const bool isPVNode = beta.cp() - alpha.cp() > 1;

    if (options::reverseFutilityPruning && !isPVNode &&
        depth.left <= options::reverseFutilityMaxDepth && !inCheck && !beta.mate() &&
        hasNonPawnMaterial(position)) {
        // Get static evaluation
        Score staticEval = Score::fromCP(nnue::evaluate(position, *network));
        if (position.active() == Color::b) staticEval = -staticEval;
        ++evalCount;

        // Futility margin increases with depth (more conservative at higher depths)
        Score futilityMargin = Score::fromCP(100 * depth.left + 100);

        if (staticEval - futilityMargin >= beta) {
            ++futilityPruned;
            return {{}, staticEval};
        }
    }

    auto moveList = moves::allLegalMovesAndCaptures(position.turn, position.board);

    // Forced moves don't count towards depth
    if (moveList.size() == 1) ++depth.left;

    sortMoves(position, ttMove, hash, moveList.begin(), moveList.end(), lastMove, depth.current);

    PrincipalVariation pv;
    int moveCount = 0;

    for (auto move : moveList) {
        ++moveCount;

        if (options::staticExchangeEvaluation && !isPVNode && depth.left <= 3 && !inCheck &&
            moveCount > 1 && move.kind == MoveKind::Capture &&
            staticExchangeEvaluation(position.board, move.from, move.to) < -100_cp) {
            ++mainSeePruned;
            continue;
        }

        auto newHash = hash;

        auto piece = position.board[move.from];
        auto target = position.board[move.kind == MoveKind::En_Passant
                                         ? makeSquare(file(move.to), rank(move.from))
                                         : move.to];
        auto mwp = MoveWithPieces{move, piece, target};
        auto mask = moves::castlingMask(move.from, move.to);
        newHash.applyMove(position.turn, mwp, mask);

        auto undo = moves::makeMove(position, move);
        dassert(newHash == Hash(position));

        // Apply Late Move Reduction (LMR)
        bool applyLMR = options::lateMoveReductions && depth.left > 2 && moveCount > 2 &&
            isQuiet(position, depth.left);
        if (applyLMR) ++lmrAttempts;

        // Current alpha is the best score found so far at this node
        const auto curAlpha = std::max(alpha, pv.score);
        const auto fullDepth = depth.left - 1;
        const auto reducedDepth = depth.left - 1 - applyLMR;

        PrincipalVariation newVar;
        if (moveCount == 1 || !options::principleVariationSearch) {

            // First move: full window (PV candidate)
            newVar = -alphaBeta(
                position, newHash, -beta, -curAlpha, {depth.current + 1, reducedDepth}, move);
            if (newVar.score > curAlpha && applyLMR && ++lmrResearches)
                // If it beats alpha, re-search at full depth
                newVar = -alphaBeta(
                    position, newHash, -beta, -curAlpha, {depth.current + 1, fullDepth}, move);

        } else {
            // Later moves: Principal Variation Search (PVS) null-window probe first
            const auto probeBeta = curAlpha + 1_cp;

            ++pvsAttempts;

            // Try the null-window probe first at reduced depth if LMR applies
            newVar = -alphaBeta(
                position, newHash, -probeBeta, -curAlpha, {depth.current + 1, reducedDepth}, move);
            if (newVar.score > curAlpha && applyLMR && ++lmrResearches)
                // If it fails high, reprobe at full depth as that's cheaper than full window
                newVar = -alphaBeta(
                    position, newHash, -probeBeta, -curAlpha, {depth.current + 1, fullDepth}, move);

            // If it still beats alpha, confirm with full window
            if (newVar.score > curAlpha && newVar.score < beta && ++pvsResearches)
                newVar = -alphaBeta(
                    position, newHash, -beta, -curAlpha, {depth.current + 1, fullDepth}, move);
        }

        unmakeMove(position, undo);

        if (newVar.score > pv.score || pv.moves.empty()) pv = {move, newVar};
        if (betaCutoff(pv.score,
                       beta,
                       move,
                       moveCount,
                       position.active(),
                       depth.current,
                       position.board,
                       lastMove))
            break;
    }

    if (moveList.empty() && !isInCheck(position)) pv.score = Score();  // Stalemate

    transpositionTable.insert(
        hash, position.turn.fullmove(), {pv.front(), pv.score}, depth.left, alpha, beta);

    return pv;
}

bool currmoveInfo(InfoFn info, int depthleft, Move currmove, int currmovenumber) {
    if (!info || depthleft < options::currmoveMinDepthLeft) return false;
    std::stringstream ss;
    ss << "depth " << std::to_string(depthleft)     //
       << " nodes " << nodeCount - searchNodeCount  //
       << " currmove " << to_string(currmove)       //
       << " currmovenumber " + std::to_string(currmovenumber);
    auto stop = info(ss.str());
    return stop;
}

bool pvInfo(InfoFn info, int depthleft, Score score, MoveVector pv) {
    if (!info) return false;

    std::string pvString = "depth " + std::to_string(depthleft);
    if (maxSelDepth) pvString += " seldepth " + std::to_string(maxSelDepth);
    if (score.mate())
        pvString += " score mate " + std::to_string(score.mate());
    else
        pvString += " score cp " + std::to_string(score.cp());

    auto nodes = nodeCount - searchNodeCount;
    pvString += " nodes " + std::to_string(nodes);

    auto millis = duration_cast<milliseconds>(clock::now() - searchStartTime).count();
    pvString += " time " + std::to_string(millis);

    if (millis) pvString += " nps " + std::to_string(nodes * 1000 / millis);

    if (pv.size()) pvString += " pv " + to_string(pv);

    auto stop = info(pvString);
    return stop;
}

PrincipalVariation toplevelAlphaBeta(
    Position& position, Score alpha, Score beta, int depthleft, InfoFn info) {
    assert(depthleft > 0);
    ++rootSearchCalls;
    Depth depth = {0, depthleft};

    Hash hash(position);

    auto moveList = moves::allLegalMovesAndCaptures(position.turn, position.board);

    // Forced moves don't count towards depth
    if (moveList.size() == 1) ++depth.left;

    // No moves means either checkmate or stalemate
    if (moveList.empty() && !isInCheck(position)) return {Move(), Score()};

    // computeBestMove already entered the current position in the repetition table
    sortMoves(position, hash, moveList.begin(), moveList.end(), Move(), depthleft);
    PrincipalVariation pv;

    int currmovenumber = 0;
    for (auto move : moveList) {
        ++rootMoveVisits;
        if (currmoveInfo(info, depthleft, move, ++currmovenumber)) return pv;

        auto newPosition = moves::applyMove(position, move);
        Hash newHash(newPosition);

        auto newVar = -alphaBeta(newPosition,
                                 newHash,
                                 -beta,
                                 -std::max(alpha, pv.score),
                                 {depth.current + 1, depth.left - 1},
                                 move);
        if (newVar.score > pv.score || !pv.front()) pv = {move, newVar};
        if (betaCutoff(pv.score,
                       beta,
                       move,
                       currmovenumber,
                       position.active(),
                       depth.current,
                       position.board,
                       Move()))
            return pv;
    }
    transpositionTable.insert(
        hash, position.turn.fullmove(), {pv.front(), pv.score}, depthleft, alpha, beta);

    return pv;
}

PrincipalVariation toplevelAlphaBeta(Position& position, int maxdepth, InfoFn info) {
    return toplevelAlphaBeta(position, Score::min(), Score::max(), maxdepth, info);
}

PrincipalVariation aspirationWindows(Position position,
                                     PrincipalVariation pv,
                                     int maxdepth,
                                     InfoFn info) {
    auto windows = options::aspirationWindows;
    auto maxWindow = windows.empty() ? 0_cp : Score::fromCP(windows.back());
    auto expected = std::clamp(pv.score, Score::min() + maxWindow, Score::max() - maxWindow);
    auto alphaIt = windows.begin();
    auto betaIt = windows.begin();

    PrincipalVariation newpv;
    while (maxdepth >= options::aspirationWindowMinDepth && alphaIt != windows.end() &&
           betaIt != windows.end()) {
        auto alpha = expected - Score::fromCP(*alphaIt);
        auto beta = expected + Score::fromCP(*betaIt);

        ++aspirationAttempts;
        newpv = toplevelAlphaBeta(position, alpha, beta, maxdepth, info);

        if (newpv.score <= alpha) {
            ++aspirationFailLow;
            ++alphaIt;
        } else if (newpv.score >= beta) {
            ++aspirationFailHigh;
            ++betaIt;
        } else {
            break;
        }
        newpv = {};
    }
    if (!newpv) {
        ++aspirationFallbackFullWindow;
        newpv = toplevelAlphaBeta(position, maxdepth, info);
    }

    return newpv ? newpv : pv;
}

PrincipalVariation iterativeDeepening(Position& position, int maxdepth, InfoFn info) {
    auto pv = toplevelAlphaBeta(position, 1, info);
    if (pvInfo(info, 1, pv.score, pv.moves)) return pv;
    for (auto depth = 2; depth <= maxdepth; ++depth) {
        pv = aspirationWindows(position, pv, depth, info);
        if (pvInfo(info, depth, pv.score, pv.moves)) break;
        if (pv.score.mate() && pv && isCheckmate(moves::applyMoves(position, pv.moves))) break;
    }
    return pv;
}

void newGame() {
    clearState();
}

bool saveState(std::ostream& out) {
    if (!out) return false;

    // Save transposition table
    size_t numEntries = transpositionTable.entries.size();
    out.write(reinterpret_cast<const char*>(&numEntries), sizeof(numEntries));
    out.write(reinterpret_cast<const char*>(&transpositionTable.numGenerations),
              sizeof(transpositionTable.numGenerations));
    out.write(reinterpret_cast<const char*>(transpositionTable.entries.data()),
              numEntries * sizeof(TranspositionTable::Entry));

    // Save history table
    out.write(reinterpret_cast<const char*>(&history[0][0][0]), sizeof(history));

    // Save killer moves
    out.write(reinterpret_cast<const char*>(&killerMoves[0][0]), sizeof(killerMoves));

    // Save countermoves
    out.write(reinterpret_cast<const char*>(&countermoves[0][0]), sizeof(countermoves));

    return out.good();
}

void clearState() {
    transpositionTable.clear();
    maxSelDepth = 0;
    repetitions.clear();
    std::fill(&history[0][0][0], &history[0][0][0] + sizeof(history) / sizeof(history[0][0][0]), 0);
    std::fill(&killerMoves[0][0],
              &killerMoves[0][0] + sizeof(killerMoves) / sizeof(killerMoves[0][0]),
              Move());
    std::fill(&countermoves[0][0],
              &countermoves[0][0] + sizeof(countermoves) / sizeof(countermoves[0][0]),
              Move());
}

bool restoreState(std::istream& in) {
    if (!in) return false;
    clearState();

    // Restore transposition table
    size_t numEntries;
    in.read(reinterpret_cast<char*>(&numEntries), sizeof(numEntries));
    if (numEntries != transpositionTable.entries.size()) return false;

    in.read(reinterpret_cast<char*>(&transpositionTable.numGenerations),
            sizeof(transpositionTable.numGenerations));
    in.read(reinterpret_cast<char*>(transpositionTable.entries.data()),
                numEntries * sizeof(TranspositionTable::Entry));

    // Restore history table
    in.read(reinterpret_cast<char*>(&history[0][0][0]), sizeof(history));

    // Restore killer moves
    in.read(reinterpret_cast<char*>(&killerMoves[0][0]), sizeof(killerMoves));

    // Restore countermoves
    in.read(reinterpret_cast<char*>(&countermoves[0][0]), sizeof(countermoves));

    return in.good();
}

PrincipalVariation computeBestMove(Position position, int maxdepth, MoveVector moves, InfoFn info) {
    evalTable = EvalTable{position.board, true};
    searchNodeCount = nodeCount;
    searchEvalCount = evalCount;
    searchQuiescenceCount = quiescenceCount;
    searchCacheCount = cacheCount;

    searchStartTime = clock::now();

    auto drawState = repetitions.enter(Hash(position));
    for (auto move : moves) {
        position = moves::applyMove(position, move);
        repetitions.enter(drawState, Hash(position));
    }

    auto pv = options::iterativeDeepening ? iterativeDeepening(position, maxdepth, info)
                                          : toplevelAlphaBeta(position, maxdepth, info);

    // Sanity check for repetitions: if the PV includes a repetition, it should be a draw
    MoveVector checkedMoves;
    for (auto move : pv.moves) {
        position = moves::applyMove(position, move);
        repetitions.enter(drawState, Hash(position));
        checkedMoves.push_back(move);
        if (repetitions.drawn(position.turn.halfmove()) &&
            checkedMoves.size() < pv.moves.size()) {
            auto scoreStr = std::string(pv.score);
            std::stringstream ss;
            pv.moves.resize(checkedMoves.size());  // truncate after last non-repeating move
            ss << "string pv continues after repetition with move " << to_string(pv) << " score "
               << scoreStr;
            info(ss.str());
            break;
        }
    }

    // Adjust mate scores to reflect the number of moves to mate
    return pv;
}

}  // namespace search
