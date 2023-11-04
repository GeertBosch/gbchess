#include <map>
#include <set>
#include <limits>

#include "eval.h"
#include "moves.h"

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

std::map<Move, float> computeBestMoves(const ChessPosition& position, int depth) {
    std::map<Move, float> bestMoves;

    // Base case: if depth is zero, return the static evaluation of the position
    if (depth == 0) {
        for (const auto& move : computeAllLegalMoves(position)) {
            ChessPosition newPosition = move.second;
            float evaluation = evaluateBoard(newPosition.board);
            bestMoves[move.first] = (position.activeColor == 'w') ? evaluation : -evaluation;
        }
        return bestMoves;
    }

    // Recursive case: compute all legal moves and evaluate them
    for (const auto& move : computeAllLegalMoves(position)) {
        ChessPosition newPosition = move.second;
        // Recursively compute the best moves for the opponent
        auto opponentBestMoves = computeBestMoves(newPosition, depth - 1);

        // Find the opponent's best move (which is the worst move for us)
        float bestScore = (position.activeColor == 'w') ? std::numeric_limits<float>::lowest() : std::numeric_limits<float>::max();
        for (const auto& opponentMove : opponentBestMoves) {
            if (position.activeColor == 'w') {
                // White is maximizing
                bestScore = std::max(bestScore, opponentMove.second);
            } else {
                // Black is minimizing
                bestScore = std::min(bestScore, opponentMove.second);
            }
        }

        // Store the move and its evaluation
        bestMoves[move.first] = bestScore;
    }

    // For white, we want to maximize the score, and for black, we want to minimize the score.
    if (position.activeColor == 'w') {
        // Choose the move with the maximum score
        float maxEvaluation = std::numeric_limits<float>::lowest();
        for (const auto& moveEval : bestMoves) {
            maxEvaluation = std::max(maxEvaluation, moveEval.second);
        }
        for (auto& moveEval : bestMoves) {
            moveEval.second = maxEvaluation - moveEval.second;
        }
    } else {
        // Choose the move with the minimum score
        float minEvaluation = std::numeric_limits<float>::max();
        for (const auto& moveEval : bestMoves) {
            minEvaluation = std::min(minEvaluation, moveEval.second);
        }
        for (auto& moveEval : bestMoves) {
            moveEval.second = moveEval.second - minEvaluation;
        }
    }

    return bestMoves;
}
