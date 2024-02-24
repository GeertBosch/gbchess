#include <iostream>
#include <random>
#include <string>

#include "eval.h"
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
std::ostream& operator<<(std::ostream& os, const EvaluatedMove& eval) {
    return os << eval.move << " " << eval.evaluation;
}

// Implement a hashing method for chess positions using Zobrist hashing
// https://en.wikipedia.org/wiki/Zobrist_hashing This relies just on the number of locations
// ("squares") and number of pieces, where we assume piece 0 to be "no piece". The hash allows for
// efficient incremental updating of the hash value when a move is made.

// 1 for black to move, 1 for each castling right, 8 for en passant file
static constexpr int kNumExtraVectors = 24;
static constexpr int kNumBoardVectors = kNumPieces * kNumSquares;
static constexpr int kNumHashVectors = kNumBoardVectors + kNumExtraVectors;

// A random 64-bit integer for each piece on each square, as well as the extra vectors. The first
// piece is None, but it is not omitted here, as it allows removing a hard-to-predict branch in the
// hash function.
static std::array<uint64_t, kNumHashVectors> hashVectors = []() {
    std::array<uint64_t, kNumHashVectors> vectors;
    std::ranlux48 gen(0xbad5eed5'bad5eed5);
    for (auto& v : vectors) v = gen();
    return vectors;
}();

// A Hash is a 64-bit integer that represents a position. It is the XOR of the hash vectors for
// each piece on each square, as well as the applicable extra vectors.
class Hash {
    uint64_t hash = 0;

public:
    enum ExtraVectors {
        BLACK_TO_MOVE = kNumBoardVectors + 0,
        CASTLING_1 = kNumBoardVectors + 1,
        CASTLING_15 = kNumBoardVectors + 15,
        EN_PASSANT_A = kNumBoardVectors + 16,
        EN_PASSANT_H = kNumBoardVectors + 23,
    };

    Hash() = default;
    Hash(Position position) {
        int location = 0;
        for (auto square : SquareSet::occupancy(position.board))
            toggle(position.board[square], square.index());
        if (position.activeColor() == Color::BLACK) toggle(BLACK_TO_MOVE);
        if (position.turn.castlingAvailability != CastlingMask::NONE)
            toggle(ExtraVectors(CASTLING_1 - 1 + uint8_t(position.turn.castlingAvailability)));
        if (position.turn.enPassantTarget.index())
            toggle(ExtraVectors(position.turn.enPassantTarget.file() + EN_PASSANT_A));
    }

    uint64_t operator()() { return hash; }

    void move(Piece piece, int from, int to) {
        toggle(piece, from);
        toggle(piece, to);
    }
    void capture(Piece piece, Piece target, int from, int to) {
        toggle(piece, from);
        toggle(target, to);
        toggle(piece, to);
    }

    // Does not cancel out castling rights or en passant targets.
    // Assumes that passed in board is the same as the board used to construct this hash.
    void applyMove(const Board& board, Move mv) {
        auto piece = board[mv.from()];
        auto target = board[mv.to()];
        move(piece, mv.from().index(), mv.to().index());
        switch (mv.kind()) {
        case MoveKind::QUIET_MOVE: break;
        case MoveKind::DOUBLE_PAWN_PUSH: toggle(ExtraVectors(mv.to().file() + EN_PASSANT_A)); break;
        case MoveKind::KING_CASTLE:  // Assume the move has the king move, so adjust the rook here.
            move(addColor(PieceType::ROOK, color(piece)),
                 (color(piece) == Color::WHITE ? Position::whiteKingSideRook
                                               : Position::blackKingSideRook)
                     .index(),
                 (color(piece) == Color::WHITE ? Position::whiteRookCastledKingSide
                                               : Position::blackRookCastledKingSide)
                     .index());
            break;
        case MoveKind::QUEEN_CASTLE:  // Assume the move has the king move, so adjust the rook here.
            move(addColor(PieceType::ROOK, color(piece)),
                 (color(piece) == Color::WHITE ? Position::whiteQueenSideRook
                                               : Position::blackQueenSideRook)
                     .index(),
                 (color(piece) == Color::WHITE ? Position::whiteRookCastledQueenSide
                                               : Position::blackRookCastledQueenSide)
                     .index());
            break;

        case MoveKind::CAPTURE: toggle(target, mv.to().index()); break;
        case MoveKind::EN_PASSANT:
            // Depending of the color of our piece, the captured pawn is either above or below the
            // destination square.
            toggle(target,
                   mv.to().index() + (color(piece) == Color::WHITE ? -kNumFiles : kNumFiles));
            break;
        case MoveKind::KNIGHT_PROMOTION:
        case MoveKind::BISHOP_PROMOTION:
        case MoveKind::ROOK_PROMOTION:
        case MoveKind::QUEEN_PROMOTION:
            toggle(piece, mv.to().index());  // Remove the pawn, add the promoted piece
            toggle(addColor(promotionType(mv.kind()), color(piece)), mv.to().index());
            break;
        case MoveKind::KNIGHT_PROMOTION_CAPTURE:
        case MoveKind::BISHOP_PROMOTION_CAPTURE:
        case MoveKind::ROOK_PROMOTION_CAPTURE:
        case MoveKind::QUEEN_PROMOTION_CAPTURE:
            toggle(target, mv.to().index());  // Remove the captured piece
            toggle(piece, mv.to().index());   // Remove the pawn, add the promoted piece
            toggle(addColor(promotionType(mv.kind()), color(piece)), mv.to().index());
            break;
        }
    }

    // Use toggle to add/remove a piece or non piece/location vector.
    void toggle(Piece piece, int location) { toggle(index(piece) * kNumSquares + location); }
    void toggle(int vector) { hash ^= hashVectors[vector]; }
    void toggle(ExtraVectors extra) { toggle(kNumBoardVectors + int(extra)); }
};

struct HashTable {
    static constexpr int kNumEntries = 1 << 20;
    static constexpr int kNumBits = 20;
    static constexpr int kNumMask = kNumEntries - 1;

    struct Entry {
        Hash hash;
        EvaluatedMove move;
    };

    std::array<Entry, kNumEntries> entries;

    EvaluatedMove* find(Hash hash) {
        auto& entry = entries[hash() & kNumMask];
        if (entry.hash() == hash()) return &entry.move;
        return nullptr;
    }

    void insert(Hash hash, EvaluatedMove move) {
        auto& entry = entries[hash() & kNumMask];
        entry.hash = hash;
        entry.move = move;
    }
} hashTable;

// Values of pieces, in centipawns
static std::array<Score, kNumPieces> pieceValues = {
    0_cp,     // No piece
    100_cp,   // White pawn
    300_cp,   // White knight
    300_cp,   // White bishop
    500_cp,   // White rook
    900_cp,   // White queen
    0_cp,     // Not counting the white king
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
    300_cp,  //  8 Knight promotion
    300_cp,  //  9 Bishop promotion
    500_cp,  // 10 Rook promotion
    900_cp,  // 11 Queen promotion
    300_cp,  // 12 Knight promotion capture
    300_cp,  // 13 Bishop promotion capture
    500_cp,  // 14 Rook promotion capture
    900_cp,  // 15 Queen promotion capture
};

uint64_t evalCount = 0;
uint64_t cacheCount = 0;
Score evaluateBoard(const Board& board) {
    Score value = 0_cp;

    for (auto piece : board.squares()) value += pieceValues[index(piece)];

    return value;
}

void improveMove(EvaluatedMove& best, const EvaluatedMove& ourMove) {
    std::string indent = "    ";
    bool improved = best < ourMove;
    D << indent << best << " <  " << ourMove << " ? " << improved << "\n";

    if (!improved) return;

    D << indent << best << " => " << ourMove << std::endl;
    best = ourMove;
}

EvaluatedMove staticEval(Position& position) {
    EvaluatedMove best;  // Default to the worst possible move
    auto currentEval = evaluateBoard(position.board);
    forAllLegalMoves(position.turn, position.board, [&](Board& board, MoveWithPieces mwp) {
        auto [move, piece, captured] = mwp;
        ++evalCount;
        auto newEval = currentEval;
        if (move.kind() >= MoveKind::CAPTURE) newEval -= pieceValues[index(captured)];
        if (position.activeColor() == Color::BLACK) newEval = -newEval;
        newEval += moveValues[index(move.kind())];
        EvaluatedMove ourMove{move, newEval};
        improveMove(best, ourMove);
    });
    return best;
}

EvaluatedMove computeBestMove(Position& position, int depth, int maxdepth) {
    EvaluatedMove best;  // Default to the worst possible move
    ++depth;
    auto indent = debug ? std::string(depth * 4 - 4, ' ') : "";

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth > maxdepth) return staticEval(position);

    Hash hash(position);
    auto cachedMove = hashTable.find(hash);
    if (cachedMove) {
        ++cacheCount;
        D << indent << "cached " << *cachedMove << std::endl;
        return *cachedMove;
    }

    auto allMoves = allLegalMoves(position.turn, position.board);

    // TODO: Sort moves by Most Valuable Victim (MVV) / Least Valuable Attacker (LVA)

    // Recursive case: compute all legal moves and evaluate them
    auto opponentKing =
        SquareSet::find(position.board, addColor(PieceType::KING, !position.activeColor()));
    for (auto move : allMoves) {
        // Recursively compute the best moves for the opponent, worst for us.
        auto newPosition = applyMove(position, move);
        auto opponentMove = -computeBestMove(newPosition, depth, maxdepth);

        bool mate = !opponentMove.move;  // Either checkmate or stalemate
        bool check = isAttacked(newPosition.board,
                                opponentKing,
                                Occupancy(newPosition.board, newPosition.activeColor()));


        Score evaluation =
            mate ? (check ? bestEval : drawEval) : opponentMove.evaluation.adjustDepth();
        EvaluatedMove ourMove(move, evaluation);
        improveMove(best, ourMove);
        if (best.evaluation.mate()) break;
    }
    // Cache the best move for this position
    hashTable.insert(hash, best);
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
