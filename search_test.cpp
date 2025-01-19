#include <chrono>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>

#include "common.h"
#include "elo.h"
#include "eval.h"
#include "fen.h"
#include "moves.h"
#include "options.h"
#include "search.h"

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
    return os << eval.move << " " << eval.score;
}

std::string cmdName = "search-test";

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
    auto eval = search::computeBestMove(position, maxdepth);
    std::cout << "        analyzePosition \"" << fen::to_string(position) << "\" as " << eval.score
              << ", move " << std::string(eval.front()) << "\n";
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
        std::cout << "    Evaluated " << move << " as " << newEval.score << "\n";

        if (newEval.score > bestMove.score) {
            bestMove = {move, newEval.score};
            std::cout << "    New best move: " << bestMove.move << " " << bestMove.score << "\n";
        }
    }

    return bestMove;
}

template <typename F>
void printEvalRate(const F& fun) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto startEvals = search::evalCount;
    auto startCache = search::cacheCount;
    fun();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endEvals = search::evalCount;
    auto endCache = search::cacheCount;

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
    auto bestMove = search::computeBestMove(position, maxdepth);
    std::cout << "Best Move: " << Eval(bestMove) << " for " << fen::to_string(position)
              << std::endl;
    std::cout << "pv " << bestMove.moves << "\n";
}

void printAnalysis(Position position, int maxdepth) {
    auto analyzed = analyzeMoves(position, maxdepth);
    std::cerr << "Analyzed: " << analyzed << std::endl;
    Eval bestMove = search::computeBestMove(position, maxdepth);
    if (bestMove.score != analyzed.score)
        std::cerr << "Mismatch: " << bestMove << " != " << analyzed << "\n";
    assert(analyzed == bestMove);
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

using ScoreWithReason = std::pair<Score, std::string>;
ScoreWithReason computeScore(Position position, MoveVector moves) {
    Color active = position.activeColor();
    std::string side = active == Color::WHITE ? "white" : "black";
    position = applyMoves(position, moves);
    auto score = search::quiesce(position, options::quiescenceDepth);
    assert(!isStalemate(position) || score == 0_cp);
    assert(!isCheckmate(position) || score == Score::min());
    if (isCheckmate(position)) score = -Score::mateIn((moves.size() + 1) / 2);
    if (position.activeColor() != active) score = -score;

    return {score, " (" + side + " side) \"" + fen::to_string(position) + "\""};
}

bool sameMate(Eval expected, Eval got) {
    return expected.score.mate() && got.score.mate() && expected.score == got.score;
}

enum PuzzleError { NO_ERROR, DEPTH_ERROR, EVAL_ERROR, SEARCH_ERROR, COUNT };

struct PuzzleStats {
    std::array<uint64_t, PuzzleError::COUNT> counts = {0};
    uint64_t& operator[](PuzzleError error) { return counts[error]; }
    // Return a string representation of the stats
    uint64_t total() const { return std::accumulate(counts.begin(), counts.end(), 0); }
    std::string operator()() {
        std::stringstream ss;
        ss << total() << " puzzles, "                 //
           << counts[NO_ERROR] << " correct "         //
           << counts[DEPTH_ERROR] << " too deep, "    //
           << counts[EVAL_ERROR] << " eval errors, "  //
           << counts[SEARCH_ERROR] << " search errors";
        return ss.str();
    }
};

/**
 * Analyze the solution to a puzzle. For mate puzzles, the solution is considered correct if the
 * mate is found in the same number of moves, regardless of the exact move sequence. For other
 * puzzles, the solution is considered correct if the best move found matches the expected move,
 * even if the principal variation diverges in subsequent moves.
 *
 * For incorrect solutions, displays the expected and actual move sequences, as well as the
 * resulting positions in FEN format, their evaluations according to our quiescence search
 * and whether the error is a evaluation error or a search error.
 */
PuzzleError analyzePuzzleSolution(std::string puzzle,
                                  Position position,
                                  MoveVector expected,
                                  MoveVector got) {
    if (expected.front() == got.front()) return NO_ERROR;

    std::cout << "\n" << puzzle << ": \"" << fen::to_string(position) << "\" " << expected << "\n";
    auto [gotScore, gotReason] = computeScore(position, got);
    auto [expectedScore, expectedReason] = computeScore(position, expected);
    if (expectedScore.mate() && gotScore.mate() && expectedScore == gotScore) {
        std::cout << "Note: Both " << expected.front() << " and " << got.front() << " lead to "
                  << gotScore << "\n";
        return NO_ERROR;
    }
    auto evalError = gotScore >= expectedScore;
    auto errorKind = evalError ? "Evaluation error" : "Search error";
    std::cout << errorKind << ": Got " << got << ", but expected " << expected << "\n";
    std::cout << "Got: " << gotScore << gotReason << "\n";
    std::cout << "Expected: " << expectedScore << expectedReason << "\n";
    return evalError ? EVAL_ERROR : SEARCH_ERROR;
}

PuzzleError doPuzzle(std::string puzzle, Position position, MoveVector moves, int maxdepth) {
    if (moves.size() > maxdepth) {
        std::cout << puzzle << " too deep:\"" << fen::to_string(position) << "\" " << moves
                  << " (skipped)\n";
        return DEPTH_ERROR;  // Skip puzzles that are too deep
    }
    auto best = search::computeBestMove(position, maxdepth);
    return analyzePuzzleSolution(puzzle, position, moves, best.moves);
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
    auto puzzleRating = ELO(kExpectedPuzzleRating);
    PuzzleStats stats;

    while (std::cin) {
        std::getline(std::cin, line);
        columns = split(line, ',');
        if (columns.size() < std::max(colMoves, colFEN)) continue;
        auto initialPosition = fen::parsePosition(columns[colFEN]);
        auto currentPosition = initialPosition;
        auto rating = ELO(std::stoi(columns[colRating]));

        MoveVector moves;
        for (auto move : split(columns[colMoves], ' ')) {
            moves.emplace_back(parseUCIMove(currentPosition, move));
            currentPosition = applyMove(currentPosition, moves.back());
            // In puzzles, the first move is just to establish the initial position
            if (moves.size() == 1) initialPosition = currentPosition;
        }
        // Drop the first move
        std::move(std::next(moves.begin()), moves.end(), moves.begin());
        moves.pop_back();

        auto puzzle = "Puzzle " + columns[colPuzzleId] + ", rating " + std::to_string(rating());
        auto result = doPuzzle(puzzle, initialPosition, moves, depth);
        ++stats[result];
        // Don't count skipped puzzles for our puzzle rating
        if (result != DEPTH_ERROR)
            puzzleRating.updateOne(rating, result == NO_ERROR ? ELO::WIN : ELO::LOSS);
    }
    std::cout << stats() << ", " << puzzleRating() << " rating\n";
    assert(puzzleRating() >= kExpectedPuzzleRating - ELO::K);
}

void testBasicPuzzles() {
    {
        auto puzfen = "4r3/1k6/pp3r2/1b2P2p/3R1p2/P1R2P2/1P4PP/6K1 w - - 0 35";
        auto puzpos = fen::parsePosition(puzfen);
        puzpos = applyUCIMove(puzpos, "e5f6");
        auto puzmoves = parseUCIMoves(puzpos, "e8e1 g1f2 e1f1");
        assert(doPuzzle("000Zo, ranking 1311", puzpos, puzmoves, 5) == NO_ERROR);
    }

    std::cout << "Basic puzzle test passed!\n";
}

// Returns true if and only if number consists exclusively of digits and  is not empty
bool isAllDigits(std::string number) {
    return !number.empty() && std::all_of(number.begin(), number.end(), ::isdigit);
}

bool maybeFEN(std::string fen) {
    std::string startChars = "rnbqkpRNBQKP12345678";
    return fen != "" && startChars.find(fen[0]) != std::string::npos &&
        std::count(fen.begin(), fen.end(), '/') == 7;
}

}  // namespace

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    testBasicPuzzles();

    if (argc < 2) return 0;

    // Need at least one argument
    if (!isAllDigits(argv[1]) && !maybeFEN(argv[1])) usage(argv[0], "invalid argument");

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

    std::string fen(argv[1]);

    // Parse the FEN string into a Position
    Position position = fen::parsePosition(fen);

    Score quiescenceEval = search::quiesce(position, 4);
    if (position.turn.activeColor == Color::BLACK) quiescenceEval = -quiescenceEval;
    std::cout << "Quiescence search: " << std::string(quiescenceEval) << std::endl;

    if (depth) {
        printEvalRate([&]() { printBestMove(position, depth); });
        if (debug) printEvalRate([&]() { printAnalysis(position, depth); });
    }
    return 0;
}
