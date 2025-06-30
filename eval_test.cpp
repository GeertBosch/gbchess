#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>  // For std::exit
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "common.h"
#include "eval.h"
#include "fen.h"
#include "moves.h"
#include "nnue.h"

namespace {
std::string cmdName = "eval-test";

std::optional<nnue::NNUE> network;
uint64_t numEvals = 0;

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
    for (size_t i = 0; i < row.size(); ++i) {
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
    std::cerr << "Usage: " << cmdName << " <FEN-string> [move...]" << std::endl;
    std::exit(1);
}

void printAvailableMovesAndCaptures(const Position& position) {
    auto [board, turn] = position;
    MoveVector moves = allLegalMovesAndCaptures(turn, board);

    std::cout << "Captures: [ ";
    for (auto move : moves)
        if (isCapture(move.kind)) std::cout << to_string(move) << " ";

    std::cout << "]\nMoves: [ ";
    for (auto move : moves)
        if (!isCapture(move.kind)) std::cout << to_string(move) << " ";
    std::cout << "]\n";
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
            auto piece = board[makeSquare(file, rank)];
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

Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    Score stand_pat = evaluateBoard(position.board, true);
    if (position.active() == Color::b) stand_pat = -stand_pat;

    if (!depthleft) return stand_pat;

    if (stand_pat >= beta && !isInCheck(position)) return beta;

    if (alpha < stand_pat) alpha = stand_pat;

    // The moveList includes moves needed to get out of check; an empty list means mate
    auto moveList = allLegalQuiescentMoves(position.turn, position.board, depthleft);
    if (moveList.empty() && isInCheck(position)) return Score::min();


    for (auto move : moveList) {
        // Compute the change to the board and evaluation that results from the move
        auto change = makeMove(position, move);
        auto score = -quiesce(position, -beta, -alpha, depthleft - 1);
        unmakeMove(position, change);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
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

void testMateScore() {
    Score M1 = Score::max();
    assert(std::string(M1) == "M1");
    assert(M1.mate() == 1);

    Score m1 = -M1;
    assert(m1.mate() == -1);
    assert(m1 == Score::min());
    assert(std::string(m1) == "-M1");

    std::cout << "Mate score tests passed" << std::endl;
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

void testCheckAndMate() {
    auto checkmate =
        fen::parsePosition("rn1qr3/pbppk1Q1/1p2p3/3nP1N1/1b1P4/2N5/PPP2PPP/R1B1K2R b KQ - 0 15");
    assert(isCheckmate(checkmate));
    assert(!isStalemate(checkmate));
    assert(isMate(checkmate));
    assert(isInCheck(checkmate));

    auto stalemate = fen::parsePosition("4k3/8/8/4b1r1/8/8/8/7K w - - 0 1");
    assert(isStalemate(stalemate));
    assert(isMate(stalemate));
    assert(!isCheckmate(stalemate));
    assert(!isInCheck(stalemate));
}

int parseMoves(Position& position, int* argc, char** argv[]) {
    int moves = 0;
    for (; *argc > 0; ++moves, --*argc, ++*argv) {
        auto move = fen::parseUCIMove(position.board, **argv);
        if (!move) usage(cmdName, std::string(**argv) + " is not a valid move");
        position = applyMove(position, move);
    }
    return moves;
}

template <typename T>
int find(T t, std::string what) {
    auto it = std::find(t.begin(), t.end(), what);
    if (it == t.end()) usage(cmdName, "Missing field \"" + what + "\"");
    return it - t.begin();
}

std::string computeStatistics(const std::vector<float>& diffs) {
    if (diffs.empty()) return "No data";
    double sum = 0;
    double sum2 = 0;
    for (auto diff : diffs) {
        sum += diff;
        sum2 += diff * diff;
    }
    double mean = sum / diffs.size();
    double variance = sum2 / diffs.size() - mean * mean;
    double stddev = std::sqrt(variance);
    assert(stddev < 0.1 && "Standard deviation is too high, check your data");
    assert(abs(mean) < 0.1 && "Mean is too far off, check your data");
    return "Mean: " + std::to_string(mean) + ", Standard Deviation: " + std::to_string(stddev);
}

void testFromStream(std::ifstream& stream) {
    using namespace std::chrono;
    std::string line;
    std::getline(stream, line);
    auto columns = split(line, ',');
    auto cpCol = find(columns, "cp");
    auto fenCol = find(columns, "fen");

    std::vector<float> diffs;

    if (debug) std::cout << "Expected,Score,Diff,Phase,FEN\n";

    auto startTime = high_resolution_clock::now();
    while (std::getline(stream, line)) {
        auto columns = split(line, ',');
        if (columns.size() < 2) continue;
        int expected = 100.0f * std::stof(columns[cpCol]);
        auto fen = columns[fenCol];
        auto position = fen::parsePosition(fen);
        auto score = nnue::evaluate(position, *network);
        ++numEvals;
        auto phase = computePhase(position.board);
        auto diff = expected - score;
        diffs.push_back(diff * 0.01);
        if (debug)
            std::cout << expected << "," << score << "," << diff << "," << phase << "," << fen
                      << "\n";
    }
    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(endTime - startTime).count();
    auto rate = numEvals ? static_cast<double>(numEvals) * 1000 / duration : 0;
    std::cout << "Processed " << numEvals << " evaluations in " << duration << " ms, " << rate
              << " evals/sec\n";
    std::cout << "Error stats: " << computeStatistics(diffs) << "\n";
}

bool parseFEN(Position& position, int* argc, char** argv[]) {
    if (*argc < 1) return false;
    position = fen::parsePosition(**argv);
    --*argc;
    ++*argv;
    return true;
}

int testFromFiles(int argc, char* argv[]) {
    for (; argc; --argc, ++argv) {
        std::string name = *argv;
        std::ifstream stream(name);
        if (!stream) {
            std::cerr << "Error: could not open file " << name << std::endl;
            return 1;
        }
        std::cout << "Testing " << name << std::endl;
        testFromStream(stream);
    }
    return 0;
}
}  // namespace

int main(int argc, char* argv[]) {
    cmdName = *argv++;
    --argc;

    testScore();
    testCheckAndMate();
    testMateScore();
    testEvaluateBoard();

    network.emplace(nnue::loadNNUE("nn-82215d0fd0df.nnue"));

    if (argc && !fen::maybeFEN(*argv)) return testFromFiles(argc, argv);

    // Default FEN string to analyze
    auto position = fen::parsePosition("6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1");

    // Parse any FEN string and moves from the command line
    if (parseFEN(position, &argc, &argv) && parseMoves(position, &argc, &argv))
        std::cout << "New position: " << fen::to_string(position) << "\n";
    else
        std::cout << "Position: " << fen::to_string(position) << "\n";

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    Score simpleEval = evaluateBoard(position.board, false);
    std::cout << "Simple Board Evaluation: " << std::string(simpleEval) << std::endl;

    Score pieceSquareEval = evaluateBoard(position.board, true);
    std::cout << "Piece-Square Board Evaluation: " << std::string(pieceSquareEval) << std::endl;

    auto quiesceEval = quiesce(position, Score::min(), Score::max(), 4);
    if (position.active() == Color::b) quiesceEval = -quiesceEval;
    std::cout << "Quiescence Evaluation: " << std::string(quiesceEval) << std::endl;

    if (network) {
        auto nnueEval = nnue::evaluate(position, *network);
        std::cout << "NNUE Evaluation: " << nnueEval << " cp\n";
    }

    printAvailableMovesAndCaptures(position);
    
    // Display NNUE timing statistics
    nnue::printTimingStats();
    
    // Display computational complexity analysis
    nnue::analyzeComputationalComplexity();
    
    return 0;
}
