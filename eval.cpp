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

std::ostream& operator<<(std::ostream& os, const ComputedMoveVector& moves) {
    os << "[";
    for (const auto& [move, position] : moves) {
        os << std::string(move) << ", ";
    }
    os << "]";
    return os;
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


EvaluatedMove computeBestMove(ComputedMoveVector& moves, int maxdepth) {
    auto position = moves.back().second;
    auto allMoves = computeAllLegalMoves(position);
    EvaluatedMove best;  // Default to the worst possible move
    int depth = moves.size();
    auto indent = debug ? std::string(depth * 4 - 4, ' ') : "";

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth > maxdepth) {
        auto currentEval = evaluateBoard(position.board);
        for (auto& [move, newPosition] : allMoves) {
            EvaluatedMove ourMove{move, false, false, evaluateBoard(newPosition.board), depth};
            if (position.activeColor == Color::BLACK) ourMove.evaluation = -ourMove.evaluation;
            D << indent << best << " <  " << ourMove << " @ " << ourMove.depth << " ? "
              << (best < ourMove) << std::endl;
            if (best < ourMove) {
                D << indent << best << " => " << ourMove << std::endl;

                best = ourMove;
            }
        }
        D << indent << position.activeColor << " " << best << std::endl;
        return best;
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
        D << indent << best << " <  " << ourMove << kind[check][mate] << " ? " << (best < ourMove)
          << std::endl;

        // Update the best move if the opponent's move is better than our current best.
        if (best < ourMove) {
            D << indent << best << " => " << ourMove << std::endl;
            best = ourMove;
            if (best.check && best.mate) {
                break;
            }
        }
    }
    return best;
}
