#include "eval.h"

#include "pv.h"
#include <iostream>
#include <sstream>
#include <vector>

#include "hash.h"
#include "moves.h"
#include "options.h"
#include "search.h"
#include "single_runner.h"

namespace search {
EvalTable evalTable;
uint64_t evalCount = 0;
uint64_t cacheCount = 0;
uint64_t searchEvalCount = 0;  // copy of evalCount at the start of the search

/**
 * The transposition table is a hash table that stores the best move found for a position, so it can
 * be reused in subsequent searches. The table is indexed by the hash of the position, and stores
 * the best move found so far, along with the evaluation of the position after that move.
 * Evaluations are always from the perspective of the player to move. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 *
 * Generations are used to be able to tell stale entries from fresh ones. When a new search is
 * started, the generation is incremented, and all new entries are marked with the new generation.
 * This allows us to still use the old entries for things like move ordering, but prefer new ones.
 */
struct TranspositionTable {
    static constexpr size_t kNumEntries = options::transpositionTableEntries;

    enum EntryType { EXACT, LOWERBOUND, UPPERBOUND };

    struct Entry {
        Hash hash;
        Eval move;
        uint8_t generation = 0;
        uint8_t depth = 0;
        EntryType type = EXACT;
        Entry() = default;
        Entry& operator=(const Entry& other) = default;

        Entry(Hash hash, Eval move, uint8_t generation, uint8_t depth, EntryType type)
            : hash(hash), move(move), generation(generation), depth(depth), type(type) {}
    };

    std::vector<Entry> entries{kNumEntries};
    uint64_t numInserted = 0;
    uint64_t numOccupied = 0;   // Number of non-obsolete entries
    uint64_t numObsoleted = 0;  // Number of entries in the table from previous generations
    uint64_t numCollisions = 0;
    uint64_t numImproved = 0;
    uint64_t numHits = 0;
    uint64_t numStale = 0;  // Hits on entries from previous generations
    uint64_t numMisses = 0;

    uint8_t generation = 0;

    ~TranspositionTable() {
        if (debug && numInserted) printStats();
    }

    Eval find(Hash hash) {
        if constexpr (kNumEntries == 0) return {Move(), Score()};
        ++numHits;
        auto& entry = entries[hash() % kNumEntries];
        if (entry.hash() == hash() && entry.generation == generation) return entry.move;
        --numHits;
        ++numMisses;
        return {};  // no move found
    }

    Entry* lookup(Hash hash, int depth) {
        if constexpr (kNumEntries == 0) return nullptr;

        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numHits;
        if (entry.hash == hash && entry.generation == generation && entry.depth >= depth) {
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
        case EXACT: alpha = beta = entry->move.score; break;
        case LOWERBOUND: alpha = std::max(alpha, entry->move.score); break;
        case UPPERBOUND: beta = std::min(beta, entry->move.score); break;
        }
    }

    void insert(Hash hash, Eval move, uint8_t depthleft, EntryType type) {
        if constexpr (kNumEntries == 0) return;
        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numInserted;

        if (entry.generation != generation && entry.move) {
            --numObsoleted;
            entry.move = {};
        }

        if (entry.type == EXACT && entry.depth > depthleft && entry.move &&
            (type != EXACT || entry.move.score >= move.score))
            return;

        // 3 cases: improve existing entry, collision with unrelated entry, or occupy an empty slot
        if (entry.move && entry.hash == hash)
            ++numImproved;
        else if (entry.move)
            ++numCollisions;
        else
            ++numOccupied;

        entry = Entry{hash, move, generation, depthleft, type};
    }

    void clear() {
        if (debug && options::transpositionTableDebug && numInserted) {
            printStats();
            std::cout << "Cleared transposition table\n";
        }
        *this = {};
    };

    void newGeneration() {
        ++generation;
        numObsoleted += numOccupied;
        numOccupied = 0;
    }

    void printStats() {
        std::cout << "Transposition table stats:\n";
        std::cout << "  Inserted: " << numInserted << "\n";
        std::cout << "  Collisions: " << numCollisions << "\n";
        std::cout << "  Occupied: " << numOccupied << "\n";
        std::cout << "  Obsoleted: " << numObsoleted << "\n";
        std::cout << "  Improved: " << numImproved << "\n";
        std::cout << "  Hits: " << numHits << "\n";
        std::cout << "  Stale: " << numStale << "\n";
        std::cout << "  Misses: " << numMisses << "\n";
    }

} transpositionTable;
using MoveIt = MoveVector::iterator;

/**
 * Sorts the moves (including captures) in the range [begin, end) in place, so that the move or
 * capture from the transposition table, if any, comes first. Returns an iterator to the next move.
 */
MoveIt sortTransposition(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    // See if the current position is a transposition of a previously seen one
    auto cachedMove = transpositionTable.find(hash);
    if (!cachedMove) return begin;

    // If the move is not part of the current legal moves, nothing to do here.
    auto it = std::find(begin, end, cachedMove.move);
    if (it == end) return begin;

    // If the transposition is not at the beginning, shift preceeding moves to the right
    if (it != begin) {
        std::swap(*begin, *it);
    }

    // The transposition is now in the beginning, so return an iterator to the next move
    return std::next(begin);
}

/**
 * Sorts the moves in the range [begin, end) in place, so that the capture come before
 * non-captures, and captures are sorted by victim value, attacker value, and move kind.
 */
MoveIt sortCaptures(const Position& position, MoveIt begin, MoveIt end) {
    std::sort(begin, end, [&board = position.board](Move a, Move b) {
        if (!isCapture(a.kind())) return false;

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
    });

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

Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    ++evalCount;
    Score stand_pat = evaluateBoard(position.board, position.activeColor(), evalTable);

    if (!depthleft) return stand_pat;

    if (stand_pat >= beta && !isInCheck(position)) return beta;

    if (alpha < stand_pat) alpha = stand_pat;

    // The moveList includes moves needed to get out of check; an empty list means mate
    auto moveList = allLegalQuiescentMoves(position.turn, position.board, depthleft);
    if (moveList.empty() && isInCheck(position)) return Score::min();

    sortMoves(position, moveList.begin(), moveList.end());

    for (auto move : moveList) {
        // Apply the move to the board
        auto undo = makeMove(position, move);
        auto score = -quiesce(position, -beta, -alpha, depthleft - 1);
        unmakeMove(position, undo);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

Score quiesce(Position& position, int depthleft) {
    SingleRunner quiesce;

    return search::quiesce(position, Score::min(), Score::max(), depthleft);
}

/**
 * The alpha-beta search algorithm with fail-soft negamax search.
 * The alpha represents the best score that the maximizing player can guarantee at the current
 * level, and beta the best score that the minimizing player can guarantee. The function returns
 * the best score that the current player can guarantee, assuming the opponent plays optimally.
 */
PrincipalVariation alphaBeta(Position& position, Score alpha, Score beta, int depthleft) {
    if (depthleft == 0) return {{}, quiesce(position, alpha, beta, options::quiescenceDepth)};

    Hash hash(position);

    // Check the transposition table, which may tighten one or both search bounds
    transpositionTable.refineAlphaBeta(hash, depthleft, alpha, beta);
    if (alpha >= beta) return {{}, alpha};

    // Check for interrupts
    SingleRunner::checkStop();

    auto moveList = allLegalMovesAndCaptures(position.turn, position.board);

    //  Forced moves don't count towards depth, but avoid infinite recursion
    if (moveList.size() == 1 && position.turn.halfmoveClock < 50) ++depthleft;

    sortMoves(position, hash, moveList.begin(), moveList.end());

    PrincipalVariation pv;
    for (auto move : moveList) {
        auto newPosition = applyMove(position, move);
        auto newVar = -alphaBeta(newPosition, -beta, -std::max(alpha, pv.score), depthleft - 1);

        if (newVar.score > pv.score || pv.moves.empty()) pv = {move, newVar};
        if (pv.score >= beta) break;
    }

    if (moveList.empty() && !isInCheck(position)) pv.score = Score();  // Stalemate

    TranspositionTable::EntryType type = TranspositionTable::EXACT;
    if (pv.score <= alpha) type = TranspositionTable::UPPERBOUND;
    if (pv.score >= beta) type = TranspositionTable::LOWERBOUND;
    transpositionTable.insert(hash, pv, depthleft, type);

    return pv;
}

bool currmoveInfo(InfoFn info, int depthleft, Move currmove, int currmovenumber) {
    if (!info) return false;
    std::stringstream ss;
    ss << "depth " << std::to_string(depthleft)     //
       << " nodes " << evalCount - searchEvalCount  //
       << " currmove " << std::string(currmove)     //
       << " currmovenumber " + std::to_string(currmovenumber);
    auto stop = info(ss.str());
    if (stop) SingleRunner::stop();
    return stop;
}

bool pvInfo(InfoFn info, int depthleft, Score score, MoveVector pv) {
    if (!info) return false;

    std::string pvString = "depth " + std::to_string(depthleft);
    if (score.mate())
        pvString += " score mate " + std::to_string(score.mate());
    else
        pvString += " score cp " + std::to_string(score.cp());

    pvString += " pv";
    for (Move move : pv) {
        pvString += " " + std::string(move);
    }
    auto stop = info(pvString);
    if (stop) SingleRunner::stop();
    return stop;
}

PrincipalVariation toplevelAlphaBeta(
    Position& position, Score alpha, Score beta, int maxdepth, InfoFn info) {
    if (maxdepth <= 0)
        return {{}, quiesce(position, Score::min(), Score::max(), options::quiescenceDepth)};

    // No need to check the transposition table here, as we're at the top level

    auto moveList = allLegalMovesAndCaptures(position.turn, position.board);

    Hash hash(position);
    sortMoves(position, hash, moveList.begin(), moveList.end());
    PrincipalVariation pv;

    int currmovenumber = 0;
    for (auto currmove : moveList) {
        if (currmoveInfo(info, maxdepth, currmove, ++currmovenumber)) break;
        auto newPosition = applyMove(position, currmove);
        auto newVar = -alphaBeta(newPosition, -beta, -std::max(alpha, pv.score), maxdepth - 1);
        if (newVar.score > pv.score) pv = {currmove, newVar};
        if (pv.score >= beta) break;
    }
    if (moveList.empty() && !isInCheck(position)) pv = {Move(), Score()};

    transpositionTable.insert(hash, pv, maxdepth, TranspositionTable::EXACT);

    return pv;
}

PrincipalVariation toplevelAlphaBeta(Position& position, int maxdepth, InfoFn info) {
    return toplevelAlphaBeta(position, Score::min(), Score::max(), maxdepth, info);
}

PrincipalVariation aspirationWindows(Position position, Score expected, int maxdepth, InfoFn info) {
    auto windows = options::aspirationWindows;
    auto maxWindow = Score::fromCP(windows.back());
    expected = std::clamp(expected, Score::min() + maxWindow, Score::max() - maxWindow);
    auto alphaIt = windows.begin();
    auto betaIt = windows.begin();

    while (maxdepth > 3 && alphaIt != windows.end() && betaIt != windows.end()) {
        auto alpha = expected - Score::fromCP(*alphaIt);
        auto beta = expected + Score::fromCP(*betaIt);

        auto pv = toplevelAlphaBeta(position, alpha, beta, maxdepth, info);

        if (pv.score <= alpha)
            ++alphaIt;
        else if (pv.score >= beta)
            ++betaIt;
        else
            return pv;
        transpositionTable.newGeneration();
    }
    return toplevelAlphaBeta(position, maxdepth, info);
}

PrincipalVariation iterativeDeepening(Position& position, int maxdepth, InfoFn info) {
    PrincipalVariation pv = toplevelAlphaBeta(position, 0, info);
    for (auto depth = 1; depth <= maxdepth; ++depth) {
        transpositionTable.newGeneration();
        pv = aspirationWindows(position, pv.score, depth, info);
        if (pvInfo(info, depth, pv.score, pv.moves)) break;
        if (pv.score.mate() || SingleRunner::stopping()) break;
    }
    return pv;
}

PrincipalVariation computeBestMove(Position& position, int maxdepth, InfoFn info) try {
    SingleRunner search;
    evalTable = EvalTable{position.board, true};
    transpositionTable.clear();
    searchEvalCount = evalCount;

    return options::iterativeDeepening ? iterativeDeepening(position, maxdepth, info)
                                       : toplevelAlphaBeta(position, maxdepth, info);
} catch (SingleRunner::Stop&) {
    return transpositionTable.find(Hash(position));
}

void stop() {
    SingleRunner::stop();
}
}  // namespace search
