#include <iostream>
#include <random>
#include <string>

#include "eval.h"
#include "fen.h"
#include "hash.h"
#include "moves.h"

#ifdef DEBUG
#include "debug.h"
#endif
#define D \
    if constexpr (debug) std::cerr

std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << std::string(mv);
}
std::ostream& operator<<(std::ostream& os, Square sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, Color color) {
    return os << (color == Color::BLACK ? 'b' : 'w');
}
std::ostream& operator<<(std::ostream& os, Score score) {
    return os << std::string(score);
}
std::ostream& operator<<(std::ostream& os, const Eval& eval) {
    return os << eval.move << " " << eval.evaluation;
}

struct TranspositionTable {
    static constexpr int kNumBits = 20;
    static constexpr int kNumEntries = 1 << kNumBits;
    static constexpr int kNumMask = kNumEntries - 1;

    struct Entry {
        Hash hash;
        Eval move;
    };

    std::array<Entry, kNumEntries> entries;

    Eval* find(Hash hash) {
        auto& entry = entries[hash() & kNumMask];
        if (entry.hash() == hash()) return &entry.move;
        return nullptr;
    }

    void insert(Hash hash, Eval move) {
        auto& entry = entries[hash() & kNumMask];
        entry.hash = hash;
        entry.move = move;
    }
} transpositionTable;

// Values of pieces, in centipawns
static std::array<Score, kNumPieces> pieceValues = {
    100_cp,   // White pawn
    300_cp,   // White knight
    300_cp,   // White bishop
    500_cp,   // White rook
    900_cp,   // White queen
    0_cp,     // Not counting the white king
    0_cp,     // No piece
    -100_cp,  // Black pawn
    -300_cp,  // Black knight
    -300_cp,  // Black bishop
    -500_cp,  // Black rook
    -900_cp,  // Black queen
    0_cp,     // Not counting the black king
};
// Values of moves, in addition to the value of the piece captured, in centipawns
static std::array<Score, kNumMoveKinds> moveValues = {
    0_cp,    //  0 Quiet move
    0_cp,    //  1 Double pawn push
    0_cp,    //  2 King castle
    0_cp,    //  3 Queen castle
    0_cp,    //  4 Capture
    0_cp,    //  5 En passant
    0_cp,    //  6 (unused)
    0_cp,    //  7 (unused)
    200_cp,  //  8 Knight promotion
    200_cp,  //  9 Bishop promotion
    400_cp,  // 10 Rook promotion
    800_cp,  // 11 Queen promotion
    200_cp,  // 12 Knight promotion capture
    200_cp,  // 13 Bishop promotion capture
    400_cp,  // 14 Rook promotion capture
    800_cp,  // 15 Queen promotion capture
};

uint64_t evalCount = 0;
uint64_t cacheCount = 0;

Score evaluateBoard(const Board& board) {
    Score value = 0_cp;

    for (auto piece : board.squares()) value += pieceValues[index(piece)];

    return value;
}

void improveMove(Eval& best, const Eval& ourMove) {
    std::string indent = "    ";
    bool improved = best < ourMove;
    D << indent << best << " <  " << ourMove << " ? " << improved << "\n";

    if (!improved) return;

    D << indent << best << " => " << ourMove << std::endl;
    best = ourMove;
}

Eval staticEval(Position& position) {
    Eval best;  // Default to the worst possible move
    auto currentEval = evaluateBoard(position.board);
    forAllLegalMoves(position.turn, position.board, [&](Board& board, MoveWithPieces mwp) {
        auto [move, piece, captured] = mwp;
        ++evalCount;
        auto newEval = currentEval;
        if (move.kind() >= MoveKind::CAPTURE) newEval -= pieceValues[index(captured)];
        if (position.activeColor() == Color::BLACK) newEval = -newEval;
        newEval += moveValues[index(move.kind())];
        Eval ourMove{move, newEval};
        improveMove(best, ourMove);
    });
    return best;
}

using MoveIt = MoveVector::iterator;

/**
 * Sorts the moves (including captures) in the range [begin, end) in place, so that the move or
 * capture from the transposition table, if any, comes first. Returns an iterator to the next move.
 */
MoveIt sortTransposition(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    // See if the current position is a transposition of a previously seen one
    auto cachedMove = transpositionTable.find(hash);
    if (!cachedMove) return begin;

    ++cacheCount;

    // If the move is not part of the current legal moves, nothing to do here.
    auto it = std::find(begin, end, cachedMove->move);
    if (it == end) return begin;

    // If the transposition is not at the beginning, shift preceeding moves to the right
    if (it != begin) {
        auto itMove = *it;
        std::move_backward(begin, std::prev(it), it);
        *begin = itMove;
    }

    // The transposition is now in the beginning, so return an iterator to the next move
    return std::next(begin);
}

/**
 * Sorts the moves in the range [begin, end) in place, so that the capture come before
 */
MoveIt sortCaptures(const Position& position, MoveIt begin, MoveIt end) {

    return begin;
}

void sortMoves(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    begin = sortTransposition(position, hash, begin, end);
    begin = sortCaptures(position, begin, end);
}

/**
 * The alpha-beta search algorithm with fail-soft negamax search.
 */
Eval alphaBeta(Position& position, Eval alpha, Eval beta, int depthleft) {
    auto indent = debug ? std::string(depthleft * 4, ' ') : "";
    D << indent << "alphaBeta(" << fen::to_string(position) << ", " << alpha.evaluation << ", "
      << beta.evaluation << ", " << depthleft << ")\n";
    Eval bestscore = {Move(), worstEval};
    if (depthleft == 0) {
        bestscore = staticEval(position);
        D << indent << "staticEval: " << bestscore.evaluation << "\n";
        return bestscore;
    }

    auto allMoves = allLegalMoves(position.turn, position.board);

    Hash hash(position);
    sortTransposition(position, hash, allMoves.begin(), allMoves.end());

    for (auto move : allMoves) {
        auto newPosition = applyMove(position, move);
        D << indent << "considering " << move << "\n";
        auto score = Eval{
            move, -alphaBeta(newPosition, -beta, -alpha, depthleft - 1).evaluation.adjustDepth()};
        if (beta.evaluation < score.evaluation) {
            D << indent << "fail-soft beta cutoff: " << score.evaluation << "\n";
            bestscore = score;
            break;  // fail-soft beta-cutoff
        }
        if (bestscore.evaluation < score.evaluation) {
            D << indent << "new best score: " << score.evaluation << "\n";
            bestscore = score;
            if (alpha.evaluation < score.evaluation) {
                D << indent << "new alpha: " << score.evaluation << "\n";
                alpha = score;
            }
        }
    }
    if (allMoves.empty()) {
        auto kingPos =
            SquareSet::find(position.board, addColor(PieceType::KING, position.activeColor()));
        if (!isAttacked(position.board, kingPos, Occupancy(position.board, position.activeColor())))
            return {Move(), drawEval};
    }
    transpositionTable.insert(hash, bestscore);  // Cache the best move for this position
    return bestscore;
}

Eval computeBestMove(Position& position, int maxdepth) {
    Eval best;  // Default to the worst possible move
    for (auto depth = 1; depth <= maxdepth; ++depth) {
        best = alphaBeta(position, {Move(), worstEval}, {Move(), bestEval}, depth);
        std::cerr << "depth " << depth << ": " << best.move << " " << best.evaluation << std::endl;
        if (best.evaluation.mate()) break;
    }

    return best;
}

uint64_t perft(Turn turn, Board& board, int depth) {
    if (depth <= 0) return 1;
    uint64_t nodes = 0;
    forAllLegalMoves(turn, board, [&](Board& board, MoveWithPieces mwp) {
        auto newTurn = applyMove(turn, mwp.piece, mwp.move);
        nodes += perft(newTurn, board, depth - 1);
    });
    return nodes;
}
