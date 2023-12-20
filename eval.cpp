#include <algorithm>
#include <iostream>
#include <random>
#include <string>

#include "eval.h"
#include "moves.h"

constexpr bool debug = 0;
#define D \
    if (debug) std::cerr

std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << std::string(mv);
}
std::ostream& operator<<(std::ostream& os, Square sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, const EvaluatedMove& sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, Color color) {
    return os << (color == Color::BLACK ? 'b' : 'w');
}

std::ostream& operator<<(std::ostream& os, const ComputedMoveVector& moves) {
    os << "[";
    for (const auto& [move, position] : moves) {
        os << std::string(move) << ", ";
    }
    os << "]";
    return os;
}

EvaluatedMove::operator std::string() const {
    if (!move) return "none";
    std::stringstream ss;
    const char* kind[2][2] = {{"", "="}, {"+", "#"}};  // {{check, mate}, {check, mate}}
    ss << move;
    ss << kind[check][mate];
    ss << " " << evaluation << " @ " << depth;
    return ss.str();
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
        if (position.activeColor == Color::BLACK) toggle(BLACK_TO_MOVE);
        if (position.castlingAvailability != CastlingMask::NONE)
            toggle(ExtraVectors(CASTLING_1 - 1 + uint8_t(position.castlingAvailability)));
        if (position.enPassantTarget.index())
            toggle(ExtraVectors(position.enPassantTarget.file() + EN_PASSANT_A));
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
static std::array<int16_t, kNumPieces> pieceValues = {
    0,     // None
    100,   // White pawn
    300,   // White knight
    300,   // White bishop
    500,   // White rook
    900,   // White queen
    0,     // Not counting the white king
    -100,  // Black pawn
    -300,  // Black knight
    -300,  // Black bishop
    -500,  // Black rook
    -900,  // Black queen
    0,     // Not counting the black king
};
// Values of moves, in addition to the value of the piece captured, in centipawns
static std::array<int16_t, kNumMoveKinds> moveValues = {
    0,    //  0 Quiet move
    0,    //  1 Double pawn push
    0,    //  2 King castle
    0,    //  3 Queen castle
    0,    //  4 Capture
    0,    //  5 En passant
    0,    //  6 (unused)
    0,    //  7 (unused)
    300,  //  8 Knight promotion
    300,  //  9 Bishop promotion
    500,  // 10 Rook promotion
    900,  // 11 Queen promotion
    300,  // 12 Knight promotion capture
    300,  // 13 Bishop promotion capture
    500,  // 14 Rook promotion capture
    900,  // 15 Queen promotion capture
};

uint64_t evalCount = 0;
uint64_t cacheCount = 0;
float evaluateBoard(const Board& board) {
    int32_t value = 0;

    for (auto piece : board.squares()) value += pieceValues[index(piece)];

    return value / 100.0f;
}

bool improveMove(EvaluatedMove& best, const EvaluatedMove& ourMove) {
    auto indent = debug ? std::string(ourMove.depth * 4 - 4, ' ') : "";
    bool improved = best < ourMove;
    D << indent << best << " <  " << ourMove << " @ " << ourMove.depth << " ? " << improved << "\n";

    if (!improved) return false;

    D << indent << best << " => " << ourMove << std::endl;
    best = ourMove;
    return ourMove.mate && ourMove.check;
}

EvaluatedMove computeBestMove(ComputedMoveVector& moves, int maxdepth) {
    auto position = moves.back().second;
    auto allMoves = allLegalMoves(position);
    EvaluatedMove best;  // Default to the worst possible move
    int depth = moves.size();
    auto indent = debug ? std::string(depth * 4 - 4, ' ') : "";

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth > maxdepth) {
        auto currentEval = evaluateBoard(position.board);
        for (auto& [move, newPosition] : allMoves) {
            ++evalCount;
            auto newEval = currentEval;
            if (move.kind >= MoveKind::CAPTURE)
                newEval -= pieceValues[index(newPosition.board[move.to])];
            if (position.activeColor == Color::BLACK) newEval = -newEval;
            newEval += moveValues[index(move.kind)];
            EvaluatedMove ourMove{move, false, false, newEval, depth};
            improveMove(best, ourMove);
        }
        return best;
    }

    Hash hash(position);
    auto cachedMove = hashTable.find(hash);
    if (cachedMove) {
        ++cacheCount;
        D << indent << "cached " << *cachedMove << std::endl;
        return *cachedMove;
    }

    // TODO: Sort moves by Most Valuable Victim (MVV) / Least Valuable Attacker (LVA)

    // Recursive case: compute all legal moves and evaluate them
    auto opponentKing =
        SquareSet::find(position.board, addColor(PieceType::KING, !position.activeColor));
    for (auto& computedMove : allMoves) {
        // Recursively compute the best moves for the opponent, worst for us.
        auto move = computedMove.first;
        auto& newPosition = computedMove.second;
        moves.push_back(computedMove);
        auto opponentMove = -computeBestMove(moves, maxdepth);
        moves.pop_back();

        bool mate = !opponentMove.move;  // Either checkmate or stalemate
        bool check = isAttacked(newPosition.board, opponentKing);

        char kind[2][2] = {{' ', '='}, {'+', '#'}};  // {{check, mate}, {check, mate}}

        float evaluation = mate ? (check ? bestEval : drawEval) : opponentMove.evaluation;
        EvaluatedMove ourMove(
            move, check, mate, evaluation, mate ? moves.size() : opponentMove.depth);
        if (improveMove(best, ourMove)) break;
    }
    // Cache the best move for this position
    hashTable.insert(hash, best);
    return best;
}

uint64_t perft(Position position, int depth) {
    if (depth <= 0) return 1;
    uint64_t nodes = 0;
    auto moves = allLegalMoves(position);
    for (auto& [move, newPosition] : moves) {
        nodes += perft(newPosition, depth - 1);
    }
    return nodes;
}
