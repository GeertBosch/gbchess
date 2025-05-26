#include "common.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <vector>

#include "eval.h"
#include "hash.h"
#include "moves.h"
#include "options.h"
#include "pv.h"

#include "search.h"

namespace search {
EvalTable evalTable;
uint64_t evalCount = 0;
uint64_t cacheCount = 0;

namespace {
using namespace std::chrono;
using clock = high_resolution_clock;
using timepoint = clock::time_point;
uint64_t searchEvalCount = 0;  // copy of evalCount at the start of the search
timepoint searchStartTime = {};

std::string pct(uint64_t some, uint64_t all) {
    return " " + std::to_string((some * 100) / all) + "%";
}

/**
 * The transposition table is a hash table that stores the best move found for a position, so it can
 * be reused in subsequent searches. The table is indexed by the hash of the position, and stores
 * the best move found so far, along with the evaluation of the position after that move.
 * Evaluations are always from the perspective of the player to move. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 */
struct TranspositionTable {
    static constexpr size_t kNumEntries = options::transpositionTableEntries;

    enum EntryType : uint8_t { EXACT, LOWERBOUND, UPPERBOUND };
    struct Eval {
        Move move;
        Score score = Score::min();
        operator bool() const { return move; }
    };

    struct Entry {
        Hash hash;
        Eval eval;
        uint8_t depth = 0;
        EntryType type = EXACT;
        Entry() = default;
        Entry& operator=(const Entry& other) = default;

        Entry(Hash hash, Eval move, uint8_t depth, EntryType type)
            : hash(hash), eval(move), depth(depth), type(type) {}
    };

    std::vector<Entry> entries{kNumEntries};
    uint64_t numInserted = 0;
    uint64_t numOccupied = 0;  // Number of non-obsolete entries
    uint64_t numCollisions = 0;
    uint64_t numImproved = 0;
    uint64_t numHits = 0;
    uint64_t numStale = 0;  // Hits on entries from previous generations
    uint64_t numMisses = 0;

    ~TranspositionTable() {
        if (debug && options::transpositionTableDebug) printStats();
    }

    Eval find(Hash hash) {
        if constexpr (kNumEntries == 0) return {Move(), Score()};
        ++numHits;
        auto& entry = entries[hash() % kNumEntries];
        if (entry.hash() == hash()) return entry.eval;
        --numHits;
        ++numMisses;
        return {};  // no move found
    }

    Entry* lookup(Hash hash, int depth) {
        if constexpr (kNumEntries == 0) return nullptr;

        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numHits;
        if (entry.hash == hash && entry.depth >= depth) {
            return &entry;
        }
        --numHits;
        ++numMisses;
        return nullptr;
    }

    void refineAlphaBeta(Hash hash, int depth, Score& alpha, Score& beta) {
        if constexpr (kNumEntries == 0) return;
        auto entry = lookup(hash, depth);
        ++numMisses;
        if (!entry) return;

        --numMisses;
        ++numHits;
        ++cacheCount;
        switch (entry->type) {
        case EXACT: alpha = beta = entry->eval.score; break;
        case LOWERBOUND: alpha = std::max(alpha, entry->eval.score); break;
        case UPPERBOUND: beta = std::min(beta, entry->eval.score); break;
        }
    }


    void insert(Hash hash, Eval move, uint8_t depthleft, EntryType type) {
        if constexpr (kNumEntries == 0) return;
        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numInserted;

        if (entry.type == EXACT && entry.depth > depthleft && entry.eval.move &&
            (type != EXACT || entry.eval.score >= move.score))
            return;

        // 3 cases: improve existing entry, collision with unrelated entry, or occupy an empty slot
        if (entry.eval.move && entry.hash == hash)
            ++numImproved;
        else if (entry.eval.move)
            ++numCollisions;
        else
            ++numOccupied;

        entry = Entry{hash, move, depthleft, type};
    }

    void insert(Hash hash, Eval move, uint8_t depthleft, Score alpha, Score beta) {
        if (move.score > alpha && move.score < beta)
            insert(hash, move, depthleft, TranspositionTable::EXACT);
        else if (move.score <= alpha)
            insert(hash, move, depthleft, TranspositionTable::UPPERBOUND);
        else if (move.score >= beta)
            insert(hash, move, depthleft, TranspositionTable::LOWERBOUND);
    }

    void clear() {
        if (debug && options::transpositionTableDebug && numInserted) {
            printStats();
            std::cout << "Cleared transposition table\n";
        }
        *this = {};
    };


    void printStats() {
        std::cout << "Transposition table stats:\n";
        std::cout << "  Inserted: " << numInserted << "\n";
        std::cout << "  Collisions: " << numCollisions << "\n";
        std::cout << "  Occupied: " << numOccupied << "\n";
        std::cout << "  Improved: " << numImproved << "\n";
        std::cout << "  Hits: " << numHits << "\n";
        std::cout << "  Stale: " << numStale << "\n";
        std::cout << "  Misses: " << numMisses << "\n";
    }

} transpositionTable;

size_t history[2][kNumSquares][kNumSquares] = {0};

struct QuiescenceCache {
    using Counter = uint64_t;
    using Hash = uint64_t;
    constexpr static size_t kQsCacheSize = options::quiescenceCacheSize;
    using Entry = std::pair<Hash, Score>;
    std::vector<Entry> entries{kQsCacheSize};
    Counter count = 0;
    Counter collisions = 0;
    mutable Counter calls = 0;
    mutable Counter hits = 0;
    mutable Counter misses = 0;

    QuiescenceCache() {
        for (auto& entry : entries) entry = {0, Score::min()};
    }
    static constexpr size_t size() { return kQsCacheSize; }

    void reset() { *this = {}; }

    void insert(Hash hash, Score score) {
        if constexpr (size()) {
            ++count;
            auto& entry = entries[hash % kQsCacheSize];
            if (entry.first == hash) {
                ++hits;
                return;
            }
            ++collisions;
            entry = {hash, score};
        }
    }

    OptionalScore operator[](Hash hash) {
        if constexpr (size() == 0) return {};

        ++calls;
        ++hits;
        auto& entry = entries[hash % size()];
        if (entry.first == hash) return entry.second;

        --hits;
        ++misses;
        return {};
    }

    void printStats() {
        if constexpr (size() == 0) return;
        std::cout << "\nQuiescence cache stats:\n";

        uint64_t used = 0;
        for (auto entry : entries)
            if (entry.first) ++used;

        assert(calls == hits + misses);

        if (options::quiescenceCacheDebug) {
            std::cout << "  entries: " << count << "\n";
            std::cout << "  used: " << used << "\n";
            std::cout << "  collisions: " << collisions << "\n";
            std::cout << "  calls: " << calls << "\n";
            std::cout << "  hits: " << hits << "\n";
            std::cout << "  misses: " << misses << "\n";
        }
    }
} quiescenceCache;

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
    int count(Hash hash, int halfmove) {
        // When starting with some FEN game positions, we may not have all move history.
        halfmove = std::min(halfmove, int(hashes.size()));

        int count = 0;
        for (auto it = hashes.begin() + halfmove; it != hashes.end(); ++it) count += (*it == hash);
        return count;
    }

public:
    Repetitions() = default;
    void clear() { hashes.clear(); }

    class State {
        friend class Repetitions;
        Repetitions& repetitions;
        size_t count;
        State(Repetitions& repetitions) : repetitions(repetitions), count(1) {};

    public:
        ~State() {
            while (count--) repetitions.pop_back();
        }

        /**
         * Returns true iff the state represents a game drawn by repetition or the fifty move rule
         */
        bool drawn(int halfmove) const {
            // Need at least 4 half moves since the last irreversible move, to get to draw by
            // repetition.
            if (halfmove < 4) return false;
            if (halfmove >= 100) return true;  // Fifty-move rule

            // Check for three-fold repetition
            return repetitions.count(repetitions.back(), halfmove) >= 3;
        }
    };

    /**
     * Enters a new position in the repetition table. The hash is the hash of the position. Returns
     * a RAII State object that will remove the hash from the table when it goes out of scope. The
     * state object reports whether the position is a draw by repetition.
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

using MoveIt = MoveVector::iterator;

// Scores for sorting moves are not meant to be related to pawn values, but just as relative
// precedence for maximum chance of a beta cut-off.
const int kCapturePromoScore = 2'100'000'000;  // a bit below INT_MAX
const int kEnPassantScore = 2'099'999'999;     // a bit below kCapturePromoScore
const int kCaptureScore = 2'000'000'000;       // a bit below kEnPassantScore
const int kPromoScore = 1'950'000'000;         // a bit below kCaptureScore
const int K = 1'000;                           // a thousand
constexpr int kind[16] = {
    0,                       // QUIET
    -3,                      // DOUBLE_PAWN_PUSH
    -1,                      // KING_CASTLE
    -2,                      // QUEEN_CASTLE
    kCaptureScore,           // CAPTURE
    kEnPassantScore,         // EN_PASSANT, close to promo
    0,                       // unused (6)
    0,                       // unused (7)
    kPromoScore + 7,         // KNIGHT_PROMO, second most common after queen
    kPromoScore + 3,         // BISHOP_PROMO
    kPromoScore + 5,         // ROOK_PROMO
    kPromoScore + 9,         // QUEEN_PROMO
    kCapturePromoScore + 7,  // KNIGHT_PROMO_CAPTURE
    kCapturePromoScore + 3,  // BISHOP_PROMO_CAPTURE
    kCapturePromoScore + 5,  // ROOK_PROMO_CAPTURE
    kCapturePromoScore + 9,  // QUEEN_PROMO_CAPTURE
};
// Capture scores for piece capturing piece to achieve MVV/LVA
int xx[6][6] = {
    // P, N, B, R, Q, K
    {0, 10, 20, 30, 40, 0},  // pawn captures
    {-3, 7, 17, 27, 37, 0},  // knight captures
    {-3, 7, 17, 27, 37, 0},  // bishop captures
    {-5, 5, 15, 25, 35, 0},  // rook captures
    {-9, 1, 11, 21, 31, 0},  // queen captures
    {0, 10, 20, 30, 40, 0},  // king captures
};

int score(const Board& board, Move move) {
    int base = kind[int(move.kind())];
    auto piece = board[move.from()];
    auto side = color(piece);

    return base +
        (isCapture(move.kind()) ? xx[index(type(piece))][index(type(board[move.to()]))]
                                : history[int(side)][move.from().index()][move.to().index()]);
}
uint64_t totalMovesEvaluated = 0;
uint64_t betaCutoffMoves = 0;
}  // namespace

/**
 * Sorts the moves (including captures) in the range [begin, end) in place, so that the move or
 * capture from the transposition table, if any, comes first. Returns an iterator to the next move.
 */
MoveIt sortTransposition(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    // See if the current position is a transposition of a previously seen one
    auto cachedMove = transpositionTable.find(hash);
    if (!cachedMove.move) return begin;

    // If the move is not part of the current legal moves, nothing to do here.
    auto it = cachedMove.move ? std::find(begin, end, cachedMove.move) : end;

    // If the move was part of a pv for this position, move it to the beginning of the list
    if (it != end) std::swap(*begin++, *it);

    // begin now points past the transposition move, or the beginning of the list
    return begin;
}

bool less(const Board& board, Move a, Move b) {
    // For non-captures, sort by history score
    if (!isCapture(a.kind()))
        return options::historyStore ? score(board, a) > score(board, b) : false;

    // Most valuable victims first
    auto ato = board[a.to()];
    auto bto = board[b.to()];
    if (bto < ato) return true;
    if (ato < bto) return false;

    // Least valuable attackers next
    auto afrom = board[a.from()];
    auto bfrom = board[b.from()];
    if (afrom < bfrom) return true;
    if (bfrom < afrom) return false;

    // Otherwise, sort by move kind, highest first
    return b.kind() < a.kind();
}

/**
 * Sorts the moves in the range [begin, end) in place, so that the capture come before
 * non-captures, and captures are sorted by victim value, attacker value, and move kind.
 */
MoveIt sortCaptures(const Position& position, MoveIt begin, MoveIt end) {
    std::sort(begin, end, [&board = position.board](Move a, Move b) { return less(board, a, b); });

    return begin;
}

void sortMoves(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    begin = sortTransposition(position, hash, begin, end);
    begin = sortCaptures(position, begin, end);
}

void sortMoves(const Position& position, MoveIt begin, MoveIt end) {
    Hash hash(position);
    sortMoves(position, hash, begin, end);
}

std::pair<UndoPosition, Score> makeMoveWithEval(Position& position, Move move, Score eval) {
    auto change = prepareMove(position.board, move);
    UndoPosition undo;
    if (options::incrementalEvaluation) {
        eval += evaluateMove(position.board, position.active(), change, evalTable);
        undo = makeMove(position, change, move);
        assert(!debug || -eval == evaluateBoard(position.board, position.active(), evalTable));
    } else {
        undo = makeMove(position, change, move);
        eval = evaluateBoard(position.board, !position.active(), evalTable);
    }
    return {undo, eval};
}

bool isQuiet(Position& position, int depthleft) {
    if (isInCheck(position)) return false;
    if (depthleft <= options::promotionMinDepthLeft) return true;
    if (mayHavePromoMove(
            !position.active(), position.board, Occupancy(position.board, !position.active())))
        return false;
    return true;
}

Score quiesce(Position& position, Score eval, Score alpha, Score beta, int depthleft) {
    Score stand_pat = eval;

    if (!depthleft) return stand_pat;

    if (stand_pat >= beta && isQuiet(position, depthleft)) return beta;

    if (alpha < stand_pat && isQuiet(position, depthleft)) alpha = stand_pat;

    // The moveList includes moves needed to get out of check; an empty list means mate
    auto moveList = allLegalQuiescentMoves(position.turn, position.board, depthleft);
    if (moveList.empty() && isInCheck(position)) return Score::min();
    sortCaptures(position, moveList.begin(), moveList.end());
    for (auto move : moveList) {
        // Compute the change to the board and evaluation that results from the move
        auto [undo, newEval] = makeMoveWithEval(position, move, eval);
        auto score = -quiesce(position, -newEval, -beta, -alpha, depthleft - 1);
        unmakeMove(position, undo);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    ++evalCount;
    auto hash = Hash(position)();
    if (auto score = quiescenceCache[hash]) return *score;

    auto eval = evaluateBoard(position.board, position.active(), evalTable);
    eval = quiesce(position, eval, alpha, beta, depthleft);
    if (eval.mate() && !isCheckmate(position)) eval = std::clamp(eval, -1000_cp, 1000_cp);

    quiescenceCache.insert(hash, eval);
    return eval;
}

Score quiesce(Position& position, int depthleft) {
    return search::quiesce(position, Score::min(), Score::max(), depthleft);
}

struct Depth {
    int current;
    int left;
    Depth operator+(int i) const { return {current + i, left - i}; }
};

/**
 * The alpha-beta search algorithm with fail-soft negamax search.
 * The alpha represents the best score that the maximizing player can guarantee at the current
 * level, and beta the best score that the minimizing player can guarantee. The function returns
 * the best score that the current player can guarantee, assuming the opponent plays optimally.
 */
PrincipalVariation alphaBeta(Position& position, Hash hash, Score alpha, Score beta, Depth depth) {
    if (depth.left <= 0) return {{}, quiesce(position, alpha, beta, options::quiescenceDepth)};

    // Check the transposition table, which may tighten one or both search bounds
    transpositionTable.refineAlphaBeta(hash, depth.left, alpha, beta);

    // TODO: Figure out if we can return more of a PV here
    if (alpha >= beta)
        if (auto eval = transpositionTable.find(hash)) return {{eval.move}, alpha};

    auto moveList = allLegalMovesAndCaptures(position.turn, position.board);

    // Forced moves don't count towards depth
    if (moveList.size() == 1) ++depth.left;

    // Check for draws due to repetition or the fifty-move rule
    auto drawState = repetitions.enter(hash);
    if (drawState.drawn(position.turn.halfmove())) return {{}, Score()};

    sortMoves(position, hash, moveList.begin(), moveList.end());

    PrincipalVariation pv;
    int moveCount = 0;

    for (auto move : moveList) {
        ++totalMovesEvaluated;  // Increment total moves evaluated
        ++moveCount;
        auto newHash = hash;
        newHash.applyMove(position, move);
        auto newPosition = applyMove(position, move);
        dassert(newHash == Hash(newPosition));
        auto newAlpha = std::max(alpha, pv.score);

        // Apply Late Move Reduction (LMR)
        bool applyLMR = options::lateMoveReductions && depth.current > 1 && depth.left > 2 &&
            moveCount > 2 && isQuiet(newPosition, depth.left);

        // Perform a reduced-depth search
        auto newVar = -alphaBeta(
            newPosition, newHash, -beta, -newAlpha, {depth.current + 1, depth.left - 1 - applyLMR});

        // Re-search at full depth if the reduced search fails high
        if (applyLMR && newVar.score > alpha)
            newVar = -alphaBeta(
                newPosition, newHash, -beta, -newAlpha, {depth.current + 1, depth.left - 1});

        if (newVar.score > pv.score || pv.moves.empty()) pv = {move, newVar};
        if (pv.score >= beta) {
            ++betaCutoffMoves;  // Increment beta cutoff moves
            int side = int(position.active());
            if (!isCapture(move.kind()))
                history[side][move.from().index()][move.to().index()] += depth.left * depth.left;
            break;
        }
    }

    if (moveList.empty() && !isInCheck(position)) pv.score = Score();  // Stalemate

    transpositionTable.insert(hash, {pv.front(), pv.score}, depth.left, alpha, beta);

    return pv;
}

bool currmoveInfo(InfoFn info, int depthleft, Move currmove, int currmovenumber) {
    if (!info || depthleft < options::currmoveMinDepthLeft) return false;
    std::stringstream ss;
    ss << "depth " << std::to_string(depthleft)     //
       << " nodes " << evalCount - searchEvalCount  //
       << " currmove " << std::string(currmove)     //
       << " currmovenumber " + std::to_string(currmovenumber);
    auto stop = info(ss.str());
    return stop;
}

bool pvInfo(InfoFn info, int depthleft, Score score, MoveVector pv) {
    if (!info) return false;

    std::string pvString = "depth " + std::to_string(depthleft);
    if (score.mate())
        pvString += " score mate " + std::to_string(score.mate());
    else
        pvString += " score cp " + std::to_string(score.cp());

    auto nodes = evalCount - searchEvalCount;
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
    if (depthleft <= 0) return {{}, quiesce(position, alpha, beta, options::quiescenceDepth)};
    Depth depth = {0, depthleft};

    Hash hash(position);

    transpositionTable.refineAlphaBeta(hash, depth.left, alpha, beta);

    auto moveList = allLegalMovesAndCaptures(position.turn, position.board);

    // Forced moves don't count towards depth
    if (moveList.size() == 1) ++depth.left;

    // Register position for later checks of draws due to repetition or the fifty-move rule
    auto drawState = repetitions.enter(hash);
    if (drawState.drawn(position.turn.halfmove())) return {{}, Score()};

    sortMoves(position, hash, moveList.begin(), moveList.end());
    PrincipalVariation pv;

    int currmovenumber = 0;
    for (auto move : moveList) {
        if (currmoveInfo(info, depthleft, move, ++currmovenumber)) break;
        auto newHash = hash;
        newHash.applyMove(position, move);
        auto newPosition = applyMove(position, move);
        dassert(newHash == Hash(newPosition));
        auto newVar =
            -alphaBeta(newPosition, newHash, -beta, -std::max(alpha, pv.score), depth + 1);
        if (newVar.score > pv.score || !pv.front()) pv = {move, newVar};
        if (pv.score >= beta) {
            int side = int(position.active());
            history[side][move.from().index()][move.to().index()] += depthleft * depthleft;
            break;
        }
    }
    if (moveList.empty() && !isInCheck(position)) pv = {Move(), Score()};
    transpositionTable.insert(hash, {pv.front(), pv.score}, depthleft, alpha, beta);

    return pv;
}

PrincipalVariation toplevelAlphaBeta(Position& position, int maxdepth, InfoFn info) {
    return toplevelAlphaBeta(position, Score::min(), Score::max(), maxdepth, info);
}

PrincipalVariation aspirationWindows(Position position, Score expected, int maxdepth, InfoFn info) {
    auto windows = options::aspirationWindows;
    auto maxWindow = windows.empty() ? 0_cp : Score::fromCP(windows.back());
    expected = std::clamp(expected, Score::min() + maxWindow, Score::max() - maxWindow);
    auto alphaIt = windows.begin();
    auto betaIt = windows.begin();

    PrincipalVariation pv;
    while (maxdepth >= options::aspirationWindowMinDepth && alphaIt != windows.end() &&
           betaIt != windows.end()) {
        auto alpha = expected - Score::fromCP(*alphaIt);
        auto beta = expected + Score::fromCP(*betaIt);

        pv = toplevelAlphaBeta(position, alpha, beta, maxdepth, info);

        if (pv.score <= alpha)
            ++alphaIt;
        else if (pv.score >= beta)
            ++betaIt;
        else
            break;
        pv = {};
    }
    if (!pv) pv = toplevelAlphaBeta(position, maxdepth, info);

    return pv;
}

PrincipalVariation iterativeDeepening(Position& position, int maxdepth, InfoFn info) {
    PrincipalVariation pv = {{}, quiesce(position, options::quiescenceDepth)};
    for (auto depth = 1; depth <= maxdepth; ++depth) {
        pv = aspirationWindows(position, pv.score, depth, info);
        if (pvInfo(info, depth, pv.score, pv.moves)) break;
        if (pv.score.mate()) break;
    }
    return pv;
}

void newGame() {
    transpositionTable.clear();
    repetitions.clear();
    quiescenceCache.reset();
    std::fill(&history[0][0][0], &history[0][0][0] + sizeof(history) / sizeof(history[0][0][0]), 0);
}

PrincipalVariation computeBestMove(Position position, int maxdepth, MoveVector moves, InfoFn info) {
    evalTable = EvalTable{position.board, true};
    searchEvalCount = evalCount;
    searchStartTime = clock::now();

    auto drawState = repetitions.enter(Hash(position));
    for (auto move : moves) {
        position = applyMove(position, move);
        repetitions.enter(drawState, Hash(position));
    }

    auto pv = options::iterativeDeepening ? iterativeDeepening(position, maxdepth, info)
                                          : toplevelAlphaBeta(position, maxdepth, info);

    if (options::quiescenceCacheDebug) quiescenceCache.printStats();

    // Print beta cutoff statistics
    if (options::alphaBetaDebug && totalMovesEvaluated > 0)
        std::cout << "Beta cutoff moves: " << betaCutoffMoves << " / " << totalMovesEvaluated
                  << pct(betaCutoffMoves, totalMovesEvaluated) << std::endl;

    return pv;
}

}  // namespace search
