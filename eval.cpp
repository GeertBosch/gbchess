#include <iostream>
#include <map>
#include <set>

#include "eval.h"
#include "moves.h"

constexpr bool debug = 0;
#define D if (debug) std::cerr

std::ostream& operator<<(std::ostream& os, const Move& mv) {
    return os << std::string(mv);
}
std::ostream& operator<<(std::ostream& os, const Square& sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, const EvaluatedMove& sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, Color color) {
    return os << (color == Color::BLACK ? 'b' : 'w')    ;
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

float evaluateBoard(const ChessBoard& board) {
    float value = 0.0f;
    std::array<float, kNumPieces> pieceValues = {
        0.0f,
        1.0f, 3.0f, 3.0f, 5.0f, 9.0f, 0.0f,      // White pieces, not counting the king
        -1.0f, -3.0f, -3.0f, -5.0f, -9.0f, 0.0f, // Black pieces
    };

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square square = {rank, file};
            auto piece = board[square];
            value += pieceValues[static_cast<uint8_t>(piece)];
        }
    }

    return value;
}

void printBoard(std::ostream& os, const ChessBoard& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            auto piece = board[Square(rank, file)];
            if (piece == Piece::INVALID) {
                os << " .";
            } else {
                os << ' ' << to_char(piece);
            }
        }
        os << std::endl;
    }
    os << "   ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << ' ' << file;
    }
    os << std::endl;
}


EvaluatedMove computeBestMove(const ChessPosition& position, int depth) {
    auto allMoves = computeAllLegalMoves(position);

    if (allMoves.empty()) {
        D << "No moves available for " << position.activeColor << "!\n";
        if (debug) printBoard(std::cerr, position.board);
        return {};
    }

    EvaluatedMove best; // Default to the worst possible move
    auto indent = std::string(std::max(0, 4 - depth) * 4, ' ');

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth == 0) {
        for (const auto& move : allMoves) {
            ChessPosition newPosition = move.second;
            EvaluatedMove ours{move.first, false, false, evaluateBoard(newPosition.board), 0};
            if (position.activeColor == Color::BLACK) ours.evaluation = -ours.evaluation;
            if (best < ours) {
                best = ours;
            }
        }
        D << indent << position.activeColor << " " << best << std::endl;
        return best;
    }

    // Recursive case: compute all legal moves and evaluate them
    for (const auto& move : allMoves) {
        const ChessPosition& newPosition = move.second;
        D << indent << position.activeColor << " " << move.first << std::endl;

        // Recursively compute the best moves for the opponent, worst for us.
        auto opponentMove = -computeBestMove(newPosition, depth - 1);

        bool mate = !opponentMove.move; // Either checkmate or stalemate
        bool check = isInCheck(newPosition.board, newPosition.activeColor);
        float evaluation = mate ? (check ? bestEval : drawEval) : opponentMove.evaluation;
        EvaluatedMove ourMove(move.first, check, mate, evaluation, opponentMove.depth + 1);
        D << indent << best << " <  " << ourMove << " ? " << (best < ourMove) << std::endl;

        // Update the best move if the opponent's move is better than our current best.
        if (best < ourMove) {
            D << indent << best << " => " << ourMove << std::endl;
            best = ourMove;
            if (best.check && best.mate) {
                D << indent << "Checkmate!\n";
                break;
            }
        }
    }

    return best;
}
