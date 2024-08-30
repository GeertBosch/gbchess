#include <iostream>
#include <unordered_set>
#include <vector>

#include "hash.h"
#include "moves.h"
#include "options.h"
#include "search.h"

EvalTable evalTable;
uint64_t evalCount = 0;
uint64_t cacheCount = 0;

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

        Entry(Hash hash, Eval move, uint8_t generation, uint8_t depth, EntryType type)
            : hash(hash), move(move), generation(generation), depth(depth), type(type) {}
    };

    std::vector<Entry> entries{kNumEntries};
    // std::array<Entry, kNumEntries> entries{};
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
        ++numHits;
        auto& entry = entries[hash() % kNumEntries];
        if (entry.hash() == hash()) return entry.move;
        --numHits;
        ++numMisses;
        return {Move(), drawEval};  // no move found
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

    void insert(Hash hash, Eval move, uint8_t depthleft, EntryType type) {
        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numInserted;

        if (entry.generation != generation && entry.move) {
            --numObsoleted;
            entry.move = {};
        }

        if (entry.type == EXACT && entry.depth > depthleft && entry.move &&
            (type != EXACT || entry.move.evaluation >= move.evaluation))
            return;

        // 3 cases: improve existing entry, collision with unrelated entry, or occupy an empty slot
        if (entry.move && entry.hash == hash)
            ++numImproved;
        else if (entry.move)
            ++numCollisions;
        else
            ++numOccupied;

        entry.hash = hash;
        entry.move = move;
        entry.generation = generation;
        entry.depth = depthleft;
        entry.type = type;
    }

    void clear() {
        if (debug && numInserted) {
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

Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    Score stand_pat = evaluateBoard(position.board, evalTable);
    ++evalCount;
    if (depthleft == 0) return stand_pat;
    if (position.activeColor() == Color::BLACK) stand_pat = -stand_pat;
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    auto moveList = allLegalQuiescentMoves(position.turn, position.board, depthleft);
    Hash hash(position);
    sortMoves(position, hash, moveList.begin(), moveList.end());

    Eval best;
    for (auto move : moveList) {
        auto newPosition = applyMove(position, move);
        auto score = -quiesce(newPosition, -beta, -alpha, depthleft - 1).adjustDepth();

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

/**
 * The alpha-beta search algorithm with fail-soft negamax search.
 */
Score alphaBeta(Position& position, Score alpha, Score beta, int depthleft) {
    if (depthleft == 0) return quiesce(position, alpha, beta, options::quiescenceDepth);

    Hash hash(position);

    // Check the transposition table
    if (auto ttEntry = transpositionTable.lookup(hash, depthleft)) {
        auto move = ttEntry->move;
        ++cacheCount;
        switch (ttEntry->type) {
        case TranspositionTable::EXACT: alpha = beta = move.evaluation; break;
        case TranspositionTable::LOWERBOUND: alpha = std::max(alpha, move.evaluation); break;
        case TranspositionTable::UPPERBOUND: beta = std::min(beta, move.evaluation); break;
        }
        if (alpha >= beta) {
            return move.evaluation;
        }
    }

    auto moveList = allLegalMovesAndCaptures(position.turn, position.board);

    //  Forced moves don't count towards depth, but avoid infinite recursion
    if (moveList.size() == 1 && position.turn.halfmoveClock < 50) ++depthleft;

    sortMoves(position, hash, moveList.begin(), moveList.end());

    Eval best;
    TranspositionTable::EntryType type = TranspositionTable::EXACT;
    for (auto move : moveList) {
        auto newPosition = applyMove(position, move);
        auto score =
            -alphaBeta(newPosition, -beta, -std::max(alpha, best.evaluation), depthleft - 1)
                 .adjustDepth();
        if (score > best.evaluation) {
            best = {move, score};
        }
        if (best.evaluation >= beta) {
            type = TranspositionTable::LOWERBOUND;
            break;
        }
    }

    if (moveList.empty()) {
        auto kingPos =
            SquareSet::find(position.board, addColor(PieceType::KING, position.activeColor()));
        if (!isAttacked(position.board, kingPos, Occupancy(position.board, position.activeColor())))
            return drawEval;
    }

    if (best.evaluation <= alpha) {
        type = TranspositionTable::UPPERBOUND;
    }

    transpositionTable.insert(
        hash, best, depthleft, type);  // Cache the best move for this position
    return best.evaluation;
}

Eval computeBestMove(Position& position, int maxdepth) {
    evalTable = EvalTable{position.board, true};
    transpositionTable.clear();

    Eval best;  // Default to the worst possible move

    best.evaluation = quiesce(position, worstEval, bestEval, options::quiescenceDepth);

    for (auto depth = 1; depth <= maxdepth; ++depth) {
        transpositionTable.newGeneration();
        auto score = alphaBeta(position, worstEval, bestEval, depth);
        best = transpositionTable.find(Hash(position));
        assert(best.evaluation == score);

        if (best.evaluation.mate()) break;
    }

    return best;
}

MoveVector principalVariation(Position position, int depth) {
    MoveVector pv;
    std::unordered_multiset<Hash> seen;
    Hash hash = Hash(position);
    while (auto found = transpositionTable.find(hash)) {
        pv.push_back(found.move);
        seen.insert(hash);
        // Limit to 3 repetitions, as that's a draw. This prevents unbounded recursion.
        if (seen.count(hash) == 3) break;
        position = applyMove(position, found.move);
        hash = Hash(position);
    }
    return pv;
}
