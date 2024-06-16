#include "common.h"
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
std::ostream& operator<<(std::ostream& os, const Eval& eval) {
    return os << eval.move << " " << eval.evaluation;
}

std::string cmdName = "eval-test";

void usage(std::string cmdName, std::string errmsg) {
    std::cerr << "Error: " << errmsg << "\n\n";
    std::cerr << "Usage: " << cmdName << " <FEN-string> [move...] <search-depth>" << std::endl;
    std::cerr << "       " << cmdName << " <search-depth>" << std::endl;
    std::exit(1);
}


Eval analyzePosition(Position position, int maxdepth) {
    auto eval = computeBestMove(position, maxdepth);
    std::cout << "        analyzePosition " << fen::to_string(position) << " as " << eval.evaluation
              << ", move " << eval.move << "\n";
    return eval;
}
Eval analyzeMoves(Position position, int maxdepth) {
    MoveVector moves;
    Eval bestMove;

    if (maxdepth > 0) moves = allLegalMovesAndCaptures(position.turn, position.board);

    std::cout << "Analyzing " << moves.size() << " moves from " << fen::to_string(position) << "\n";
    if (moves.empty()) bestMove = analyzePosition(position, 0);

    for (auto move : moves) {
        std::cout << "    Considering " << move << "\n";
        auto newPosition = applyMove(position, move);
        auto newEval = -analyzePosition(newPosition, maxdepth - 1);
        std::cout << "    Evaluated " << move << " as " << newEval.evaluation << "\n";

        if (newEval.evaluation > bestMove.evaluation) {
            bestMove = {move, newEval.evaluation};
            std::cout << "    New best move: " << bestMove.move << " " << bestMove.evaluation
                      << "\n";
        }
    }

    return bestMove;
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
    std::cout << "Best Move: " << bestMove << " for " << fen::to_string(position) << std::endl;
    std::cout << "pv " << principalVariation(position) << "\n";
}

void printAnalysis(Position position, int maxdepth) {
    auto analyzed = analyzeMoves(position, maxdepth);
    std::cerr << "Analyzed: " << analyzed << std::endl;
    if (debug) {
        auto bestMove = computeBestMove(position, maxdepth);
        assert(analyzed == bestMove);
    }
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

void testFromStdInOld(int depth) {
    // While there is input on stdin, read a line, parse it as a FEN string and print the best move.
    while (std::cin) {
        std::string fen;
        std::getline(std::cin, fen);

        if (fen.empty()) continue;

        // Parse the FEN string into a Position
        std::cerr << fen << std::endl;
        Position position = fen::parsePosition(fen);

        // Print the board in grid notation
        printBoard(std::cerr, position.board);

        // Compute the best move
        Eval bestMove;
        printEvalRate([&]() {
            bestMove = computeBestMove(position, depth);
            auto newPosition = applyMove(position, bestMove.move);
            auto kingPos = SquareSet::find(newPosition.board,
                                           addColor(PieceType::KING, newPosition.turn.activeColor));
            const char* kind[2][2] = {{"", "="}, {"+", "#"}};  // {{check, mate}, {check, mate}}

            auto check = isAttacked(newPosition.board, kingPos, newPosition.turn.activeColor);
            auto mate = allLegalMovesAndCaptures(newPosition.turn, newPosition.board).empty();
            // Print the best move and its evaluation
            std::cout << bestMove.move << kind[check][mate] << " " << bestMove.evaluation << "\n";
            std::cerr << "Solution: " << bestMove << "\t";
        });
    }
}
std::vector<std::string> split(std::string line, char delim) {
    std::vector<std::string> res;
    std::string word;
    for (auto c : line) {
        if (c == delim) {
            res.emplace_back(std::move(word));
            word = "";
        } else {
            word.push_back(c);
        }
    }
    if (word.size()) res.emplace_back(std::move(word));
    return res;
}
template <typename T>
int find(T t, std::string what) {
    auto it = std::find(t.begin(), t.end(), what);
    if (it == t.end()) usage(cmdName, "Missing field \"" + what + "\"");
    return it - t.begin();
}

bool testPuzzle(std::string puzzleId, Position position, MoveVector moves, int maxdepth) {
    auto best = computeBestMove(position, maxdepth);
    bool correct = best.move == moves.front();
    if (!correct) {
        std::cout << "Puzzle " << puzzleId << ": \"" << fen::to_string(position) << "\" " << moves
                  << "\n";
        // As a special case, if multiple mates are possible, they're considered equivalent.
        auto expected = applyMove(position, moves.front());
        auto actual = applyMove(position, best.move);
        correct = isMate(expected) && isMate(actual) && isInCheck(expected) == isInCheck(actual);
        if (correct)
            std::cout << "Note: Both " << moves.front() << " and " << best.move
                      << " lead to mate\n";
        else
            std::cout << "Error: Got " << best << ", but expected " << moves.front() << "\n";
    }
    return correct;
}

void testFromStdIn(int depth) {
    // Assumes CSV input as from the lichess puzzle database:
    // PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags
    std::string line;
    std::getline(std::cin, line);
    auto columns = split(line, ',');
    auto colFEN = find(columns, "FEN");
    auto colMoves = find(columns, "Moves");
    auto colPuzzleId = find(columns, "PuzzleId");
    uint64_t numPuzzles = 0;
    uint64_t numCorrect = 0;

    while (std::cin) {
        std::getline(std::cin, line);
        columns = split(line, ',');
        if (columns.size() < std::max(colMoves, colFEN)) continue;
        auto initialPosition = fen::parsePosition(columns[colFEN]);
        auto currentPosition = initialPosition;

        MoveVector moves;
        for (auto move : split(columns[colMoves], ' ')) {
            moves.emplace_back(parseMoveUCI(currentPosition, move));
            currentPosition = applyMove(currentPosition, moves.back());
            // In puzzles, the first move is just to establish the initial position
            if (moves.size() == 1) initialPosition = currentPosition;
        }
        // Drop the first move
        std::move(std::next(moves.begin()), moves.end(), moves.begin());
        auto puzzleId = columns[colPuzzleId];
        numCorrect += testPuzzle(puzzleId, initialPosition, moves, depth);
        ++numPuzzles;
    }
    std::cout << numPuzzles << " puzzles, " << numCorrect << " correct" << "\n";
}

void testScore() {
    Score zero;
    Score q = -9'00_cp;
    Score Q = 9'00_cp;
    assert(Q == -q);
    assert(Q + q == zero);
    assert(q < Q);
    assert(std::string(q) == "-9.00");
    std::cout << "Score tests passed" << std::endl;
}

void testEvaluateBoard() {
    std::string piecePlacement = "8/8/8/8/4p3/5pNN/4p3/2K1k3";
    auto board = fen::parsePiecePlacement(piecePlacement);
    auto score = evaluateBoard(board, false);
    std::cout << "Board Evaluation for "
                 ""
              << piecePlacement
              << ""
                 ": "
              << std::string(score) << std::endl;
    assert(score == 300_cp);  // 2 knights vs 3 pawns
    board = fen::parsePiecePlacement(fen::initialPiecePlacement);
    score = evaluateBoard(board, true);
    std::cout << "Board Evaluation for initial position: " << std::string(score) << std::endl;
    assert(score == 0_cp);
}

void testEval() {
    {
        Eval none;
        Eval mateIn3 = {Move("f6"_sq, "e5"_sq, Move::QUIET), bestEval.adjustDepth().adjustDepth()};
        Eval mateIn1 = {Move("e7"_sq, "g7"_sq, Move::QUIET), bestEval};
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
        Eval stalemate = {Move("f6"_sq, "e5"_sq, Move::QUIET), 0_cp};
        Eval upQueen = {Move("f7"_sq, "a2"_sq, Move::CAPTURE), 9'00_cp};
        assert(stalemate < upQueen);
        assert(stalemate.evaluation == drawEval);
    }
    std::cout << "Eval tests passed" << std::endl;
}


int main(int argc, char* argv[]) {
    cmdName = argv[0];
    if (argc == 2) {
        int depth = std::stoi(argv[1]);
        printEvalRate([depth]() { testFromStdIn(depth); });
        std::exit(0);
    }
    if (argc < 3) usage(argv[0], "missing search depth argument");

    testScore();
    testEvaluateBoard();
    testEval();

    std::string fen(argv[1]);

    // Parse the FEN string into a Position
    Position position = fen::parsePosition(fen);

    MoveVector moves;
    for (int j = 2; j < argc - 1; ++j) {
        moves.push_back(parseMoveUCI(position, argv[j]));
        if (!moves.back()) usage(argv[0], std::string(argv[j]) + " is not a valid move");
        position = applyMove(position, moves.back());
    }

    if (moves.size()) std::cout << "New position: " << fen::to_string(position) << "\n";

    int depth = std::stoi(argv[argc - 1]);

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    Score simpleEval = evaluateBoard(position.board, false);
    std::cout << "Simple Board Evaluation: " << std::string(simpleEval) << std::endl;

    Score pieceSquareEval = evaluateBoard(position.board, true);
    std::cout << "Piece-Square Board Evaluation: " << std::string(pieceSquareEval) << std::endl;

    printAvailableCaptures(position);
    printAvailableMoves(position);
    printEvalRate([&]() { printBestMove(position, depth); });
    printEvalRate([&]() { printAnalysis(position, depth); });
    return 0;
}
