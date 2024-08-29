#include <chrono>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "common.h"
#include "elo.h"
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

using GridDrawing = std::array<std::string, 11>;
GridDrawing gridDrawing = {
    " ┌─",
    "─",
    "─┬─",
    "─┐ ",
    " │ ",
    " ├─",
    "─┼─",
    "─┤ ",
    " └─",
    "─┴─",
    "─┘ ",
};

std::string operator*(std::string str, int n) {
    if (!n) return "";
    return n == 1 ? str : str * (n / 2) + str * (n - n / 2);
}

std::string gridTop(int width) {
    std::string res;
    res += gridDrawing[0];
    res += (gridDrawing[1] + gridDrawing[2]) * (width - 1);
    res += gridDrawing[1] + gridDrawing[3];
    return res;
}
std::string gridRow(std::string row) {
    std::string res;
    res += gridDrawing[4];
    for (int i = 0; i < row.size(); ++i) {
        res += row[i];
        if (i < row.size() - 1) res += gridDrawing[4];
    }
    res += gridDrawing[4];
    return res;
}
std::string gridMiddle(int width) {
    std::string res;
    res += gridDrawing[5];
    res += (gridDrawing[1] + gridDrawing[6]) * (width - 1);
    res += gridDrawing[1] + gridDrawing[7];
    return res;
}
std::string gridBottom(int width) {
    std::string res;
    res += gridDrawing[8];
    res += (gridDrawing[1] + gridDrawing[9]) * (width - 1);
    res += gridDrawing[1] + gridDrawing[10];
    return res;
}

void usage(std::string cmdName, std::string errmsg) {
    std::cerr << "Error: " << errmsg << "\n\n";
    std::cerr << "Usage: " << cmdName << " <FEN-string> [move...] <search-depth>" << std::endl;
    std::cerr << "       " << cmdName << " <search-depth>" << std::endl;
    std::cerr << "       " << cmdName << " <FEN-string>" << std::endl;
    std::exit(1);
}

Eval analyzePosition(Position position, int maxdepth) {
    auto eval = computeBestMove(position, maxdepth);
    std::cout << "        analyzePosition \"" << fen::to_string(position) << "\" as "
              << eval.evaluation << ", move " << eval.move << "\n";
    return eval;
}
Eval analyzeMoves(Position position, int maxdepth) {
    MoveVector moves;
    Eval bestMove;

    if (maxdepth > 0) moves = allLegalMovesAndCaptures(position.turn, position.board);

    std::cout << "Analyzing " << moves.size() << " moves from " << fen::to_string(position) << "\n";
    if (moves.empty()) bestMove = analyzePosition(position, 0);

    for (auto move : moves) {
        std::cout << "    Considering " << move << " depth " << maxdepth - 1 << "\n";
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
    std::cout << "pv " << principalVariation(position, maxdepth) << "\n";
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
    os << " " << gridTop(kNumFiles) << std::endl;
    for (int rank = kNumRanks - 1; rank >= 0; --rank) {
        os << rank + 1;
        std::string row;
        for (int file = 0; file < kNumFiles; ++file) {
            auto piece = board[Square(rank, file)];
            row.push_back(to_char(piece));
        }
        os << gridRow(row) << std::endl;
        if (rank > 0) os << " " << gridMiddle(kNumFiles) << std::endl;
    }
    os << " " << gridBottom(kNumFiles) << std::endl;
    os << " ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << "   " << file;
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

void reportFailedPuzzle(Position position, MoveVector moves, MoveVector pv) {
    std::cout << "Error: Got " << pv << ", but expected " << moves << "\n";
    auto gotPosition = applyMoves(position, pv);
    auto gotEval = evaluateBoard(gotPosition.board, false);
    auto expectedPosition = applyMoves(position, moves);
    auto expectedEval = evaluateBoard(expectedPosition.board, false);
    std::cout << "Got " << gotEval << " \"" << fen::to_string(gotPosition) << "\"\n";
    std::cout << "Expected " << expectedEval << " \"" << fen::to_string(expectedPosition) << "\"\n";
}

bool testPuzzle(
    std::string puzzleId, int rating, Position position, MoveVector moves, int maxdepth) {
    auto best = computeBestMove(position, maxdepth);
    bool correct = best.move == moves.front();
    if (!correct) {
        std::cout << "\nPuzzle " << puzzleId << ", rating " << rating << ": \""
                  << fen::to_string(position) << "\" " << moves << "\n";
        // As a special case, if multiple mates are possible, they're considered equivalent.
        auto expected = applyMove(position, moves.front());
        auto actual = applyMove(position, best.move);
        correct = isMate(expected) && isMate(actual) && isInCheck(expected) == isInCheck(actual);
        if (correct)
            std::cout << "Note: Both " << moves.front() << " and " << best.move
                      << " lead to mate\n";
        else
            reportFailedPuzzle(position, moves, principalVariation(position, maxdepth));
    }
    return correct;
}

void testFromStdIn(int depth) {
    // Assumes CSV input as from the lichess puzzle database:
    // PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags
    const int kExpectedPuzzleRating = 2000;
    std::string line;
    std::getline(std::cin, line);
    auto columns = split(line, ',');
    auto colFEN = find(columns, "FEN");
    auto colMoves = find(columns, "Moves");
    auto colPuzzleId = find(columns, "PuzzleId");
    auto colRating = find(columns, "Rating");
    uint64_t numPuzzles = 0;
    uint64_t numCorrect = 0;
    auto puzzleRating = ELO(kExpectedPuzzleRating);

    while (std::cin) {
        std::getline(std::cin, line);
        columns = split(line, ',');
        if (columns.size() < std::max(colMoves, colFEN)) continue;
        auto initialPosition = fen::parsePosition(columns[colFEN]);
        auto currentPosition = initialPosition;
        auto rating = ELO(std::stoi(columns[colRating]));

        MoveVector moves;
        for (auto move : split(columns[colMoves], ' ')) {
            moves.emplace_back(parseMoveUCI(currentPosition, move));
            currentPosition = applyMove(currentPosition, moves.back());
            // In puzzles, the first move is just to establish the initial position
            if (moves.size() == 1) initialPosition = currentPosition;
        }
        // Drop the first move
        std::move(std::next(moves.begin()), moves.end(), moves.begin());
        moves.pop_back();

        auto puzzleId = columns[colPuzzleId];
        auto correct = testPuzzle(puzzleId, rating(), initialPosition, moves, depth);
        puzzleRating.updateOne(rating, correct ? ELO::WIN : ELO::LOSS);
        numCorrect += correct;
        ++numPuzzles;
    }
    std::cout << numPuzzles << " puzzles, " << numCorrect << " correct"
              << ", " << puzzleRating() << " rating\n";
    assert(puzzleRating() >= kExpectedPuzzleRating - ELO::K);
}

void testScore() {
    Score zero;
    Score q = -9'00_cp;
    Score Q = 9'00_cp;
    assert(Q == -q);
    assert(Q + q == zero);
    assert(q < Q);
    assert(std::string(q) == "-9.00");
    assert(std::string(Q) == "9.00");
    Score f = -1'23_cp;
    Score F = 1'23_cp;
    assert(F == -f);
    assert(F + f == zero);
    assert(std::string(f) == "-1.23");
    assert(std::string(F) == "1.23");
    std::cout << "Score tests passed" << std::endl;
}

void testEvaluateBoard() {
    std::string piecePlacement = "8/8/8/8/4p3/5pNN/4p3/2K1k3";
    auto board = fen::parsePiecePlacement(piecePlacement);
    auto score = evaluateBoard(board, false);
    assert(score == 300_cp);  // 2 knights vs 3 pawns
    board = fen::parsePiecePlacement(fen::initialPiecePlacement);
    score = evaluateBoard(board, true);
    assert(score == 0_cp);
    std::cout << "evaluateBoard tests passed" << std::endl;
}

void testEval() {
    {
        Eval none;
        Eval mateIn3 = {Move("f6"_sq, "e5"_sq, Move::QUIET), bestEval.adjustDepth().adjustDepth()};
        Eval mateIn1 = {Move("e7"_sq, "g7"_sq, Move::QUIET), bestEval};
        assert(std::string(none) == "0000@-M1");
        assert(std::string(mateIn3) == "f6e5@M3");
        assert(std::string(mateIn1) == "e7g7@M1");
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

// Returns true if and only if number consists exclusively of digits and  is not empty
bool isAllDigits(std::string number) {
    return !number.empty() && std::all_of(number.begin(), number.end(), ::isdigit);
}

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    // Need at least one argument
    if (argc < 2) usage(argv[0], "missing argument");

    // If the last argument is a number, it's the search depth
    int depth = 0;
    if (isAllDigits(argv[argc - 1])) {
        depth = std::stoi(argv[argc - 1]);
        --argc;
    }

    // Special puzzle test mode reading from stdin
    if (argc == 1) {
        printEvalRate([depth]() { testFromStdIn(depth); });
        std::exit(0);
    }

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

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    Score simpleEval = evaluateBoard(position.board, false);
    std::cout << "Simple Board Evaluation: " << std::string(simpleEval) << std::endl;

    Score pieceSquareEval = evaluateBoard(position.board, true);
    std::cout << "Piece-Square Board Evaluation: " << std::string(pieceSquareEval) << std::endl;

    Score quiescenceEval = quiesce(position, worstEval, bestEval, 4);
    if (position.turn.activeColor == Color::BLACK) quiescenceEval = -quiescenceEval;
    std::cout << "Quiescence Evaluation: " << std::string(quiescenceEval) << std::endl;

    printAvailableCaptures(position);
    printAvailableMoves(position);

    if (depth) {
        printEvalRate([&]() { printBestMove(position, depth); });
        if (debug) printEvalRate([&]() { printAnalysis(position, depth); });
    }
    return 0;
}
