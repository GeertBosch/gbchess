#include <iostream>
#include <map>
#include <set>
#include <limits>

#include "eval.h"
#include "moves.h"

constexpr bool debug = 1;
#define D if (debug) std::cout

std::ostream& operator<<(std::ostream& os, const Move& sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, const Square& sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, const EvaluatedMove& sq) {
    return os << std::string(sq);
}

EvaluatedMove::operator std::string() const {
    if (!move)
        return "none";
    std::stringstream ss;
    const char* kind[2][2] = {{"", "="}, {"+", "#"}};  // {{check, mate}, {check, mate}}
    ss << move;
    if (depth <= 1)
        ss << kind[check][mate];
    else if (check && mate)
        ss << " M" << depth;
    ss << " " << evaluation << " @ " << depth;
    return ss.str();
}

float evaluateBoard(const ChessBoard& board) {
    float value = 0.0f;
    const std::map<char, float> pieceValues = {
        {'P', 1.0f}, {'N', 3.0f}, {'B', 3.0f}, {'R', 5.0f}, {'Q', 9.0f}, // White pieces
        {'p', -1.0f}, {'n', -3.0f}, {'b', -3.0f}, {'r', -5.0f}, {'q', -9.0f} // Black pieces
    };

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square square = {rank, file};
            char piece = board[square];
            if(pieceValues.count(piece)) value += pieceValues.at(piece);
        }
    }

    return value;
}

void printBoard(std::ostream& os, const ChessBoard& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            char piece = board[Square(rank, file)];
            if (piece == ' ') {
                os << " .";
            } else {
                os << ' ' << piece;
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
        if (debug) printBoard(std::cout, position.board);
        return {};
    }

    EvaluatedMove best; // Default to the worst possible move
    auto indent = std::string(std::max(0, 4 - depth) * 4, ' ');

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth == 0) {
        for (const auto& move : allMoves) {
            ChessPosition newPosition = move.second;
            EvaluatedMove ours{move.first, false, false, evaluateBoard(newPosition.board), 0};
            if (position.activeColor == 'b') ours.evaluation = -ours.evaluation;
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
        }
    }

    return best;
}
