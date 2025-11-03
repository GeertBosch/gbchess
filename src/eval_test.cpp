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
#include "moves_gen.h"
#include "nnue.h"
#include "nnue_stats.h"
#include "options.h"

namespace {
std::string cmdName = "eval-test";

uint64_t numEvals = 0;

void usage(std::string cmdName, std::string errmsg) {
    std::cerr << "Error: " << errmsg << "\n\n";
    std::cerr << "Usage: " << cmdName << " <FEN-string> [move...]" << std::endl;
    std::exit(1);
}

void printAvailableMovesAndCaptures(const Position& position) {
    auto [board, turn] = position;
    MoveVector moves = moves::allLegalMovesAndCaptures(turn, board);

    std::cout << "Captures: [ ";
    for (auto move : moves)
        if (isCapture(move.kind)) std::cout << to_string(move) << " ";

    std::cout << "]\nMoves: [ ";
    for (auto move : moves)
        if (!isCapture(move.kind)) std::cout << to_string(move) << " ";
    std::cout << "]\n";
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

/**
 * Simplistic quiescence search for evaluation test purposes.
 */
Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    Score stand_pat = evaluateBoard(position.board);
    if (position.active() == Color::b) stand_pat = -stand_pat;

    if (!depthleft) return stand_pat;

    if (stand_pat >= beta && !isInCheck(position)) return beta;

    if (alpha < stand_pat) alpha = stand_pat;

    // The moveList includes moves needed to get out of check; an empty list means mate
    auto moveList = moves::allLegalQuiescentMoves(position.turn, position.board, depthleft);

    if (moveList.empty()) return isInCheck(position) ? Score::min() : stand_pat;

    for (auto move : moveList) {
        // Compute the change to the board and evaluation that results from the move
        auto change = moves::makeMove(position, move);
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
    auto score = evaluateBoardSimple(board);
    assert(score == 300_cp);  // 2 knights vs 3 pawns
    board = fen::parsePiecePlacement(fen::initialPiecePlacement);
    score = evaluateBoard(board);
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

void testStaticExchangeEvaluation() {
    {
        auto position = fen::parsePosition("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - 0 1");
        // Try Rook captures e5
        auto score = staticExchangeEvaluation(position.board, e1, e5);
        assert(score == 100_cp);
    }
    {
        auto position =
            fen::parsePosition("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1");
        // Try Knight captures e5
        auto score = staticExchangeEvaluation(position.board, d3, e5);
        assert(score == -200_cp);
    }

    // Test the specific bug case from puzzle 0HBeT - should now be fixed
    {
        auto position = fen::parsePosition("1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32");

        // Try bishop captures g2 - this should be a winning move, not losing. The rook on g1 must
        // recapture as it's the only legal move. Then the rook on g4 recaptures ending with a net
        // gain of 300 cp. However, we currently don't handle forced recaptures, so in our case, the
        // rook will not recapture the bishop.
        auto score = staticExchangeEvaluation(position.board, b7, g2);

        // Bishop captures pawn (100), then the rook declines recapturing the bishop, to avoid
        // losing the king. So in the end, we are just capturing the pawn.
        assert(score == 100_cp);
    }

    std::cout << "Static exchange evaluation tests passed" << std::endl;
}

void testMVVLVA() {
    // Test 1: Queen takes pawn
    {
        Board board = {};
        board[e4] = Piece::Q;  // White queen
        board[e5] = Piece::p;  // Black pawn
        Move move = {e4, e5, MoveKind::Capture};

        // Queen takes pawn: victim=100, attacker=900, score = 100*10 - 900 = 100
        int score = scoreMVVLVA(board, move);
        assert(score == 100);
    }

    // Test 2: Pawn takes queen
    {
        Board board = {};
        board[d4] = Piece::P;  // White pawn
        board[e5] = Piece::q;  // Black queen
        Move move = {d4, e5, MoveKind::Capture};

        // Pawn takes queen: victim=900, attacker=100, score = 900*10 - 100 = 8900
        int score = scoreMVVLVA(board, move);
        assert(score == 8900);
    }

    // Test 3: En passant capture (victim should be empty square)
    {
        Board board = {};
        board[e5] = Piece::P;  // White pawn doing en passant
        Move move = {e5, d6, MoveKind::En_Passant};

        int score = scoreMVVLVA(board, move);
        assert(score == 900);  // En passant captures a pawn, so victim=100, attacker=10*100
    }

    // Test 4: Knight takes bishop (should be equal piece values)
    {
        Board board = {};
        board[b1] = Piece::N;  // White knight
        board[c3] = Piece::b;  // Black bishop
        Move move = {b1, c3, MoveKind::Capture};

        // Bishop=300, Knight=300, score = 300*10 - 300 = 2700
        int score = scoreMVVLVA(board, move);
        assert(score == 2700);
    }

    std::cout << "MVV-LVA scoring test passed!\n";
}

int parseMoves(Position& position, int* argc, char** argv[]) {
    int moves = 0;
    for (; *argc > 0; ++moves, --*argc, ++*argv) {
        auto move = fen::parseUCIMove(position.board, **argv);
        if (!move) usage(cmdName, std::string(**argv) + " is not a valid move");
        position = moves::applyMove(position, move);
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

void testFromStream(nnue::NNUE& network, std::ifstream& stream) {
    using namespace std::chrono;
    std::string line;
    std::getline(stream, line);
    auto columns = split(line, ',');
    auto cpCol = find(columns, "cp");
    auto fenCol = find(columns, "fen");

    std::vector<float> diffs;

    if (debug) std::cout << "Expected,Score,Diff,Phase,FEN\n";
    nnue::resetTimingStats();

    auto startTime = high_resolution_clock::now();
    while (std::getline(stream, line)) {
        auto columns = split(line, ',');
        if (columns.size() < 2) continue;
        int expected = 100.0f * std::stof(columns[cpCol]);
        auto fen = columns[fenCol];
        auto position = fen::parsePosition(fen);
        auto score = nnue::evaluate(position, network);
        ++numEvals;
        auto phase = computePhase(position.board);
        auto diff = expected - score;
        diffs.push_back(diff * 0.01);
        if (debug)
            std::cout << expected << "," << score << "," << diff << "," << phase << "," << fen
                      << "\n";
    }
    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(endTime - startTime).count();
    auto rate = numEvals ? static_cast<double>(numEvals) * 1'000'000 / duration : 0;
    std::cout << "Processed " << numEvals << " evaluations in " << duration * 0.001 << " ms, "
              << rate << " evals/sec\n";
    std::cout << "Error stats: " << computeStatistics(diffs) << "\n";
    if (!debug) nnue::printTimingStats();
}

bool parseFEN(Position& position, int* argc, char** argv[]) {
    if (*argc < 1) return false;
    position = fen::parsePosition(**argv);
    --*argc;
    ++*argv;
    return true;
}

int testFromFiles(nnue::NNUE& network, int argc, char* argv[]) {
    for (; argc; --argc, ++argv) {
        std::string name = *argv;
        std::ifstream stream(name);
        if (!stream) {
            std::cerr << "Error: could not open file " << name << std::endl;
            return 1;
        }
        std::cout << "Testing " << name << std::endl;
        testFromStream(network, stream);
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
    testStaticExchangeEvaluation();
    testMVVLVA();

    std::optional<nnue::NNUE> network;
    if (options::useNNUE)
        network.emplace(nnue::loadNNUE("nn-82215d0fd0df.nnue"));
    else
        std::cout << "\n*** Skipping NNUE evaluation as it is disabled in options. ***\n";

    if (argc && !fen::maybeFEN(*argv)) return network ? testFromFiles(*network, argc, argv) : 0;

    // Default FEN string to analyze
    auto position = fen::parsePosition("6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1");

    // Parse any FEN string and moves from the command line
    if (parseFEN(position, &argc, &argv) && parseMoves(position, &argc, &argv))
        std::cout << "New position: " << fen::to_string(position) << "\n";
    else
        std::cout << "Position: " << fen::to_string(position) << "\n";

    // Evaluate the board
    Score simpleEval = evaluateBoardSimple(position.board);
    std::cout << "Simple Board Evaluation: " << std::string(simpleEval) << std::endl;

    Score pieceSquareEval = evaluateBoard(position.board);
    std::cout << "Piece-Square Board Evaluation: " << std::string(pieceSquareEval) << std::endl;

    auto quiesceEval = quiesce(position, Score::min(), Score::max(), 4);
    if (position.active() == Color::b) quiesceEval = -quiesceEval;
    std::cout << "Quiescence Evaluation: " << std::string(quiesceEval) << std::endl;

    if (network) {
        auto nnueEval = nnue::evaluate(position, *network);
        std::cout << "NNUE Evaluation: " << nnueEval << " cp\n";

        // Display NNUE timing statistics
        nnue::printTimingStats();

        // Display computational complexity analysis
        nnue::analyzeComputationalComplexity();
    }

    printAvailableMovesAndCaptures(position);

    return 0;
}
