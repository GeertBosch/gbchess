#include <chrono>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "eval.h"
#include "fen.h"
#include "moves.h"

namespace {
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves) {
        if (move) os << std::string(move) << ", ";
    }
    os << "]";
    return os;
}
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
}  // namespace

template <typename F>
void printEvalRate(const F& fun) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto startEvals = evalCount;
    auto startCache = cacheCount;
    fun();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endEvals = evalCount;
    auto endCache = cacheCount;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto evals = endEvals - startEvals;
    auto cached = endCache - startCache;
    auto evalRate = evals / (duration.count() / 1000'000.0);  // evals per second

    std::cerr << evals << " evals, " << cached << " cached in " << duration.count() / 1000
              << " ms @ " << evalRate / 1000.0 << "K evals/sec" << std::endl;
}

void printAvailableMoves(const Position& position) {
    MoveVector moves;
    addAvailableMoves(moves, position.board, position.activeColor());
    std::cout << "Moves: " << moves << std::endl;
}

void printAvailableCaptures(const Position& position) {
    MoveVector captures;
    addAvailableCaptures(captures, position.board, position.activeColor());
    std::cout << "Captures: " << captures << std::endl;
}

void printBestMove(Position position, int maxdepth) {
    auto bestMove = computeBestMove(position, maxdepth);
    std::cout << "Best Move: " << bestMove << std::endl;
}

/**
 * Prints the chess board to the specified output stream in grid notation.
 */
void printBoard(std::ostream& os, const Board& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            auto piece = board[Square(rank, file)];
            os << ' ' << to_char(piece);
        }
        os << std::endl;
    }
    os << "   ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << ' ' << file;
    }
    os << std::endl;
}


void testFromStdIn(int depth) {
    // While there is input on stdin, read a line, parse it as a FEN string and print the best move.
    while (std::cin) {
        std::string fen;
        std::getline(std::cin, fen);

        if (fen.empty()) continue;

        // Parse the FEN string into a Position
        std::cerr << fen << std::endl;
        Position position = fen::parsePosition(fen);
        auto startTime = std::chrono::high_resolution_clock::now();

        // Print the board in grid notation
        printBoard(std::cerr, position.board);

        // Compute the best move
        EvaluatedMove bestMove;
        printEvalRate([&]() {
            bestMove = computeBestMove(position, depth);
            auto newPosition = applyMove(position, bestMove.move);
            auto kingPos = SquareSet::find(newPosition.board,
                                           addColor(PieceType::KING, newPosition.turn.activeColor));
            const char* kind[2][2] = {{"", "="}, {"+", "#"}};  // {{check, mate}, {check, mate}}

            auto check = isAttacked(newPosition.board, kingPos, newPosition.turn.activeColor);
            auto mate = allLegalMoves(newPosition.turn, newPosition.board).empty();
            // Print the best move and its evaluation
            std::cout << bestMove.move << kind[check][mate] << " " << bestMove.evaluation << "\n";
            std::cerr << "Solution: " << bestMove << "\t";
        });
    }
}

void testScore() {
    Score zero;
    Score q = -9'00_cp;
    Score Q = 9'00_cp;
    assert(Q == -q);
    assert(Q + q == zero);
    assert(q < Q);
    assert(std::string(q) == "-9.00");
}

void testEvaluatedMove() {
    {
        EvaluatedMove none;
        EvaluatedMove mateIn3 = {Move("f6"_sq, "e5"_sq, Move::QUIET),
                                 bestEval.adjustDepth().adjustDepth()};
        EvaluatedMove mateIn1 = {Move("e7"_sq, "g7"_sq, Move::QUIET), bestEval};
        std::cout << "none: " << none << std::endl;
        std::cout << "mateIn3: " << mateIn3 << std::endl;
        std::cout << "mateIn1: " << mateIn1 << std::endl;
        assert(none < mateIn3);
        assert(mateIn3 < mateIn1);
        assert(none < mateIn1);
        assert(mateIn1.evaluation == bestEval);
        assert(bestEval == Score::max());
    }
    {
        EvaluatedMove stalemate = {Move("f6"_sq, "e5"_sq, Move::QUIET), 0_cp};
        EvaluatedMove upQueen = {Move("f7"_sq, "a2"_sq, Move::CAPTURE), 9'00_cp};
        assert(stalemate < upQueen);
        assert(stalemate.evaluation == drawEval);
    }
    std::cout << "EvaluatedMove tests passed" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        int depth = std::stoi(argv[1]);
        testFromStdIn(depth);
        std::exit(0);
    }
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [FEN-string] <search-depth>" << std::endl;
        std::exit(1);
    }

    testScore();
    testEvaluatedMove();

    std::string fen(argv[1]);
    int depth = std::stoi(argv[2]);

    // Parse the FEN string into a Position
    Position position = fen::parsePosition(fen);

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    std::cout << "Board Evaluation: " << std::string(evaluateBoard(position.board)) << std::endl;

    printAvailableCaptures(position);
    printAvailableMoves(position);
    printEvalRate([&]() { printBestMove(position, depth); });

    return 0;
}
