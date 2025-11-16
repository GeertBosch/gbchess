#include <chrono>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <numeric>
#include <ostream>
#include <sstream>
#include <string>

#include "core/core.h"
#include "core/options.h"
#include "elo.h"
#include "eval/eval.h"
#include "eval/nnue/nnue_stats.h"
#include "fen/fen.h"
#include "hash/hash.h"
#include "move/move.h"
#include "move/move_gen.h"
#include "pv.h"
#include "search.h"

namespace {
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves) {
        if (move) os << to_string(move) << ", ";
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << to_string(mv);
}
std::ostream& operator<<(std::ostream& os, Score score) {
    return os << std::string(score);
}
std::ostream& operator<<(std::ostream& os, const PrincipalVariation& pv) {
    return os << to_string(pv);
}

std::string cmdName = "search-test";

void usage(std::string cmdName, std::string errmsg) {
    std::cerr << "Error: " << errmsg << "\n\n";
    std::cerr << "Usage: " << cmdName << " <FEN-string> [move...] <search-depth>" << std::endl;
    std::cerr << "       " << cmdName << " <search-depth>" << std::endl;
    std::cerr << "       " << cmdName << " <FEN-string>" << std::endl;
    std::exit(1);
}

constexpr bool debugAnalysis = false;

PrincipalVariation analyzePosition(Position position, int maxdepth) {
    auto pv = search::computeBestMove(position, maxdepth);
    if (debugAnalysis)
        std::cout << "        analyzePosition \"" << fen::to_string(position) << "\" as "
                  << pv.score << ", move " << to_string(pv.front()) << "\n";
    return pv;
}

PrincipalVariation analyzeMoves(Position position, int maxdepth) {
    MoveVector moves;
    PrincipalVariation pv;

    if (maxdepth > 0) moves = moves::allLegalMovesAndCaptures(position.turn, position.board);

    if (debugAnalysis)
        std::cout << "Analyzing " << moves.size() << " moves from " << fen::to_string(position)
                  << "\n";

    if (moves.empty()) pv = analyzePosition(position, 0);

    for (auto move : moves) {
        if (debugAnalysis)
            std::cout << "    Considering " << move << " depth " << maxdepth - 1 << "\n";
        auto newPosition = moves::applyMove(position, move);
        auto newVariation = PrincipalVariation{move, -analyzePosition(newPosition, maxdepth - 1)};
        if (debugAnalysis)
            std::cout << "    Evaluated " << move << " as " << newVariation.score << "\n";

        if (newVariation > pv) {
            pv = newVariation;
            if (debugAnalysis) std::cout << "    New best move: " << to_string(pv) << "\n";
        }
    }

    return pv;
}

template <typename F>
void printEvalRate(const F& fun) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto startEvals = search::evalCount;
    auto startCache = search::cacheCount;
    auto startQuiescence = search::quiescenceCount;
    fun();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endEvals = search::evalCount;
    auto endCache = search::cacheCount;
    auto endQuiescence = search::quiescenceCount;

    auto quiesce = endQuiescence - startQuiescence;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto evals = endEvals - startEvals;
    auto cached = endCache - startCache;
    auto evalRate = evals / (duration.count() / 1000'000.0);  // evals per second

    std::cerr << evals << " evals, " << cached << " cached, " << quiesce << " quiesced in "
              << duration.count() / 1000 << " ms @ " << evalRate / 1000.0 << "K evals/sec"
              << std::endl;
}

void printBestMove(Position position, int maxdepth) {
    auto bestMove = search::computeBestMove(position, maxdepth);
    std::cout << "Best Move: " << bestMove << " for " << fen::to_string(position) << std::endl;
}

void printAnalysis(Position position, int maxdepth) {
    auto analyzed = analyzeMoves(position, maxdepth);
    std::cerr << "Analyzed: " << analyzed << std::endl;
    auto bestMove = search::computeBestMove(position, maxdepth);
    std::cerr << "Computed: " << bestMove << std::endl;

    // In the case of identical scores, there may be multiple best moves.
    if (bestMove.score != analyzed.score)
        std::cerr << "Mismatch: " << bestMove.score << " != " << analyzed.score << "\n";
    else if (bestMove.front() != analyzed.front())
        std::cerr << "Mismatch: " << bestMove.front() << " != " << analyzed.front() << "\n";
    assert(analyzed.score == bestMove.score);
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

Position applyMoves(Position position, MoveVector const& moves) {
    for (auto move : moves) position = moves::applyMove(position, move);

    return position;
}

using ScoreWithReason = std::pair<Score, std::string>;
ScoreWithReason computeScore(Position position, MoveVector moves) {
    Color active = position.active();
    std::string side = active == Color::w ? "white" : "black";
    position = applyMoves(position, moves);
    auto score = search::quiesce(position, options::quiescenceDepth);
    assert(!isStalemate(position) || score == 0_cp);
    assert(!isCheckmate(position) || score == Score::min());
    if (isCheckmate(position)) score = -Score::mateIn((moves.size() + 1) / 2);
    if (position.active() != active) score = -score;

    return {score, " (" + side + " side) \"" + fen::to_string(position) + "\""};
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

    auto [gotScore, gotReason] = computeScore(position, got);
    auto [expectedScore, expectedReason] = computeScore(position, expected);
    bool acceptableMate = expectedScore.mate() && gotScore.mate() && expectedScore == gotScore;
    // If we have a real error, or if we are debugging, print the puzzle information
    if (debugAnalysis || !acceptableMate)
        std::cout << puzzle << ": \"" << fen::to_string(position) << "\" " << expected << "\n";
    if (acceptableMate) {
        if (debugAnalysis)
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

PuzzleError doPuzzle(std::string puzzle, Position position, MoveVector moves, size_t maxdepth) {
    if (moves.size() > maxdepth) {
        std::cout << puzzle << " too deep:\"" << fen::to_string(position) << "\" " << moves
                  << " (skipped)\n";
        return DEPTH_ERROR;  // Skip puzzles that are too deep
    }
    search::newGame();
    PrincipalVariation pv = search::computeBestMove(position, maxdepth);
    return analyzePuzzleSolution(puzzle, position, moves, pv.moves);
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
    nnue::resetTimingStats();

    while (std::cin) {
        std::getline(std::cin, line);
        columns = split(line, ',');
        if (columns.size() < size_t(std::max(colMoves, colFEN))) continue;
        auto initialPosition = fen::parsePosition(columns[colFEN]);
        auto currentPosition = initialPosition;
        auto rating = ELO(std::stoi(columns[colRating]));

        MoveVector moves;
        for (auto move : split(columns[colMoves], ' ')) {
            moves.emplace_back(fen::parseUCIMove(currentPosition.board, move));
            currentPosition = moves::applyMove(currentPosition, moves.back());
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
    nnue::printTimingStats();
}

void testBasicPuzzles() {
    auto puzfen = "4r3/1k6/pp3P2/1b5p/3R1p2/P1R2P2/1P4PP/6K1 b - - 0 35";
    auto puzpos = fen::parsePosition(puzfen);
    auto puzmoves = MoveVector{{e8, e1, MoveKind::Quiet_Move},
                               {g1, f2, MoveKind::Quiet_Move},
                               {e1, f1, MoveKind::Quiet_Move}};
    assert(doPuzzle("000Zo, ranking 1311", puzpos, puzmoves, 5) == NO_ERROR);

    if (debug) std::cout << "Basic puzzle test passed!\n";
}

void testMissingPV() {
    // Test that the move for a missing PV is 0000, which indicates resignation
    PrincipalVariation pv;
    std::stringstream ss;
    ss << pv.front();
    assert(ss.str() == "0000");
}

void testCheckMated() {
    // Test a position where black already is check mate
    auto position = fen::parsePosition("1k6/1Q6/1K6/8/8/8/8/8 b - - 0 1");
    auto pv = search::computeBestMove(position, 1);
    {
        // Test the correct pv when we lost
        std::stringstream ss;
        ss << pv;
        assert(ss.str() == "mate -1");
    }
    {
        // Make sure we resign if we're check mated
        std::stringstream ss;
        ss << pv.front();
        assert(ss.str() == "0000");
    }
}

void testMateInOne() {
    // Test a mate-in-1
    std::string fen = "N6r/1p1k1ppp/2np4/b3p3/4P1b1/N1Q5/P4PPP/R3KB1R b KQ - 0 18";
    auto position = fen::parsePosition(fen);
    auto pv = search::computeBestMove(position, 1);
    std::stringstream ss;
    ss << pv;
    assert(ss.str() == "mate 1 pv a5c3");
}

void testNullMoveHash() {
    // Test position with en passant possibility (from previous analysis)
    auto position =
        fen::parsePosition("rnbqkbnr/pppp1ppp/8/4p3/2P1P3/8/PP1P1PPP/RNBQKBNR b KQkq e3 0 2");

    // Get the original hash
    Hash originalHash(position);

    // Make null move on position and hash
    Turn savedTurn = position.turn;
    Hash nullHash = originalHash.makeNullMove(position.turn);
    position.turn.makeNullMove();

    // Hash should match position after null move
    Hash recomputedHash(position);
    assert(nullHash == recomputedHash);

    // Restore position
    position.turn = savedTurn;

    // Test another position without en passant
    position = fen::parsePosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Hash startHash(position);

    savedTurn = position.turn;
    Hash nullHashNoEP = startHash.makeNullMove(position.turn);
    position.turn.makeNullMove();

    Hash recomputedHashNoEP(position);
    assert(nullHashNoEP == recomputedHashNoEP);

    // Restore position
    position.turn = savedTurn;

    if (debug) std::cout << "Null move hash test passed!\n";
}

void testBasicSearch() {
    testMissingPV();
    testCheckMated();
    testMateInOne();
    testNullMoveHash();
    if (debug) std::cout << "Basic search test passed!\n";
}

// Returns true if and only if number consists exclusively of digits and  is not empty
bool isAllDigits(std::string number) {
    return !number.empty() && std::all_of(number.begin(), number.end(), ::isdigit);
}

}  // namespace

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    testBasicSearch();
    testBasicPuzzles();

    if (argc < 2) return 0;

    // Need at least one argument
    if (!isAllDigits(argv[1]) && !fen::maybeFEN(argv[1])) usage(argv[0], "invalid argument");

    // If the last argument is a number, it's the search depth
    int depth = isAllDigits(argv[argc - 1]) ? std::stoi(argv[--argc]) : 0;

    // Special puzzle test mode reading from stdin
    if (argc == 1) return printEvalRate([depth]() { testFromStdIn(depth); }), 0;

    std::string fen(argv[1]);

    // Parse the FEN string into a Position
    Position position = fen::parsePosition(fen);

    Score quiescenceEval = search::quiesce(position, options::quiescenceDepth);
    if (position.active() == Color::b) quiescenceEval = -quiescenceEval;
    std::cout << "Quiescence search: " << std::string(quiescenceEval) << " (white side)\n";

    if (depth) {
        nnue::resetTimingStats();
        printEvalRate([&]() { printBestMove(position, depth); });
        if (nnue::g_totalEvaluations) {
            nnue::printTimingStats();
            nnue::analyzeComputationalComplexity();
        }
        if (debug) printEvalRate([&]() { printAnalysis(position, depth); });
    }
    return 0;
}
