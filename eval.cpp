#include <algorithm>
#include <iostream>
#include <string>

#include "eval.h"
#include "moves.h"

constexpr bool debug = 0;
#define D      \
    if (debug) \
    std::cerr

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

EvaluatedMove::operator std::string() const {
    if (!move)
        return "none";
    std::stringstream ss;
    const char* kind[2][2] = {{"", "="}, {"+", "#"}};  // {{check, mate}, {check, mate}}
    ss << move;
    ss << kind[check][mate];
    ss << " " << evaluation << " @ " << depth;
    return ss.str();
}

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

uint64_t evalCount = 0;
float evaluateBoard(const Board& board) {
    int32_t value = 0;

    ++evalCount;
    for (auto piece : board.squares())
        value += pieceValues[static_cast<uint8_t>(piece)];

    return value / 100.0f;
}


EvaluatedMove computeBestMove(const Position& position, int depth) {
    auto allMoves = computeAllLegalMoves(position);
    EvaluatedMove best;  // Default to the worst possible move
    auto indent = debug ? std::string(std::max(0, 4 - depth) * 4, ' ') : "";

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth == 0) {
        for (auto& [move, newPosition] : allMoves) {
            EvaluatedMove ours{move, false, false, evaluateBoard(newPosition.board), 0};
            if (position.activeColor == Color::BLACK)
                ours.evaluation = -ours.evaluation;
            if (best < ours) {
                best = ours;
            }
        }
        D << indent << position.activeColor << " " << best << std::endl;
        return best;
    }

    // Recursive case: compute all legal moves and evaluate them
    auto opponentKing =
        SquareSet::find(position.board, addColor(PieceType::KING, !position.activeColor));
    for (auto [move, newPosition] : allMoves) {
        D << indent << position.activeColor << " " << move << std::endl;

        // Recursively compute the best moves for the opponent, worst for us.
        auto opponentMove = -computeBestMove(newPosition, depth - 1);

        bool mate = !opponentMove.move;  // Either checkmate or stalemate
        bool check = isAttacked(newPosition.board, opponentKing);

        float evaluation = mate ? (check ? bestEval : drawEval) : opponentMove.evaluation;
        EvaluatedMove ourMove(move, check, mate, evaluation, opponentMove.depth + 1);
        D << indent << best << " <  " << ourMove << " ? " << (best < ourMove) << std::endl;

        // Update the best move if the opponent's move is better than our current best.
        if (best < ourMove) {
            D << indent << best << " => " << ourMove << std::endl;
            best = ourMove;
            if (best.check && best.mate)
                break;
        }
    }
    return best;
}
