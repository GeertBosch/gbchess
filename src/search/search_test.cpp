#include <chrono>
#include <cstdlib>  // For std::exit
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <ostream>
#include <sstream>
#include <string>

#include "book/pgn/pgn.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "core/options.h"
#include "core/text_util.h"
#include "elo.h"
#include "engine/fen/fen.h"
#include "eval/eval.h"
#include "eval/nnue/nnue_stats.h"
#include "move/move.h"
#include "move/move_gen.h"
#include "pv.h"
#include "search.h"

namespace {
bool verbose = false;
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves)
        if (move) os << to_string(move) << ", ";

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

void usage() {
    std::cerr << "Usage: " << cmdName << " <FEN-string> moves [move...] <search-depth>\n";
    std::cerr << "       " << cmdName << " <FEN-string>\n";
    std::cerr << "       " << cmdName << " <search-depth> [puzzle-file.csv]\n";
    std::cerr << "The first form searches from the given position after applying the given moves\n";
    std::cerr << "The second form runs a quiescence search from the given position\n";
    std::cerr << "The third accept puzzles in lichess CSV format and solves them\n";
    std::exit(1);
}

void usage(std::string message) {
    std::cerr << "Error: " << message << "\n";
    usage();
}

constexpr bool debugAnalysis = false;

PrincipalVariation analyzePosition(Position position, int maxdepth) {
    auto pv = search::computeBestMove(position, maxdepth);
    if (debugAnalysis)
        std::cout << "        analyzePosition \"" << fen::to_string(position) << "\" as "
                  << pv.score << ", move " << to_string(pv.front()) << "\n";
    return pv;
}

Position applyMoves(Position position, MoveVector const& moves) {
    for (auto move : moves) position = moves::applyMove(position, move);

    return position;
}

PrincipalVariation analyzeMoves(Position position, int maxdepth, MoveVector moves) {
    applyMoves(position, moves);

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
    auto startNullMoveAttempts = search::nullMoveAttempts;
    auto startNullMove = search::nullMoveCutoffs;
    auto startLMR = search::lmrReductions;
    auto startLMRResearch = search::lmrResearches;
    auto startBetaCutoffs = search::betaCutoffs;
    auto startFirstMoveCutoffs = search::firstMoveCutoffs;
    auto startTTRefinements = search::ttRefinements;
    auto startTTCutoffs = search::ttCutoffs;
    auto startQsTTRefinements = search::qsTTRefinements;
    auto startQsTTCutoffs = search::qsTTCutoffs;
    auto startFutilityPruned = search::futilityPruned;
    auto startCountermoveAttempts = search::countermoveAttempts;
    auto startCountermoveHits = search::countermoveHits;
    search::maxSelDepth = 0;
    fun();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endEvals = search::evalCount;
    auto endCache = search::cacheCount;
    auto endQuiescence = search::quiescenceCount;
    auto endNullMoveAttempts = search::nullMoveAttempts;
    auto endNullMove = search::nullMoveCutoffs;
    auto endLMR = search::lmrReductions;
    auto endLMRResearch = search::lmrResearches;
    auto endBetaCutoffs = search::betaCutoffs;
    auto endFirstMoveCutoffs = search::firstMoveCutoffs;
    auto endTTRefinements = search::ttRefinements;
    auto endTTCutoffs = search::ttCutoffs;
    auto endQsTTRefinements = search::qsTTRefinements;
    auto endQsTTCutoffs = search::qsTTCutoffs;
    auto endFutilityPruned = search::futilityPruned;
    auto endCountermoveAttempts = search::countermoveAttempts;
    auto endCountermoveHits = search::countermoveHits;

    auto quiesce = endQuiescence - startQuiescence;
    auto nullMoveAttempts = endNullMoveAttempts - startNullMoveAttempts;
    auto nullMove = endNullMove - startNullMove;
    auto lmr = endLMR - startLMR;
    auto lmrResearch = endLMRResearch - startLMRResearch;
    auto betaCuts = endBetaCutoffs - startBetaCutoffs;
    auto firstMoveCuts = endFirstMoveCutoffs - startFirstMoveCutoffs;
    auto ttRefinements = endTTRefinements - startTTRefinements;
    auto ttCutoffs = endTTCutoffs - startTTCutoffs;
    auto qsTTRefinements = endQsTTRefinements - startQsTTRefinements;
    auto qsTTCutoffs = endQsTTCutoffs - startQsTTCutoffs;
    auto futilityPruned = endFutilityPruned - startFutilityPruned;
    auto countermoveAttempts = endCountermoveAttempts - startCountermoveAttempts;
    auto countermoveHits = endCountermoveHits - startCountermoveHits;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto evals = endEvals - startEvals;
    auto cached = endCache - startCache;
    auto evalRate = evals / (duration.count() / 1000'000.0);  // evals per second

    if (!verbose) return;

    std::cerr << evals << " evals, " << cached << " cached, " << quiesce << " quiesced";
    if (search::maxSelDepth) std::cerr << " (seldepth " << search::maxSelDepth << ")";
    std::cerr << "\n";
    if (nullMoveAttempts) {
        std::cerr << "  " << nullMoveAttempts << " null-move attempts, " << nullMove << " cutoffs";
        if (nullMoveAttempts) std::cerr << " (" << (nullMove * 100 / nullMoveAttempts) << "%)";
        std::cerr << "\n";
    }
    if (lmr) std::cerr << "  " << lmr << " LMR reductions";
    if (lmrResearch) std::cerr << " (" << lmrResearch << " researches)";
    if (lmr) std::cerr << "\n";
    if (betaCuts) {
        std::cerr << "  " << betaCuts << " beta cutoffs";
        if (firstMoveCuts)
            std::cerr << " (" << firstMoveCuts
                      << " first-move = " << (firstMoveCuts * 100 / betaCuts) << "%)";
        std::cerr << "\n";
    }
    if (ttRefinements || ttCutoffs)
        std::cerr << "  " << ttRefinements << " TT refinements, " << ttCutoffs << " TT cutoffs\n";
    if (qsTTRefinements || qsTTCutoffs)
        std::cerr << "  " << qsTTRefinements << " QS TT refinements, " << qsTTCutoffs
                  << " QS TT cutoffs\n";

    if (futilityPruned) std::cerr << "  " << futilityPruned << " futility pruned\n";

    if (countermoveAttempts)
        std::cerr << "  " << countermoveAttempts << " countermove attempts (" << countermoveHits
                  << " hits = " << (countermoveHits * 100 / countermoveAttempts) << "%)\n";

    std::cerr << "  " << duration.count() / 1000 << " ms @ " << evalRate / 1000.0 << "K evals/sec"
              << std::endl;
}

void printBestMove(Position position, int maxdepth, MoveVector moves) {
    auto bestMove = search::computeBestMove(position, maxdepth, moves);
    std::cout << "Best Move: " << bestMove << " for " << fen::to_string(applyMoves(position, moves))
              << std::endl;
}

void printAnalysis(Position position, int maxdepth, MoveVector moves) {
    auto analyzed = analyzeMoves(position, maxdepth, moves);
    std::cout << "Analyzed: " << analyzed << std::endl;
    auto bestMove = search::computeBestMove(position, maxdepth, moves);
    std::cout << "Computed: " << bestMove << std::endl;

    // In the case of identical scores, there may be multiple best moves.
    if (bestMove.score != analyzed.score)
        std::cerr << "Mismatch: " << bestMove.score << " != " << analyzed.score << "\n";
    else if (bestMove.front() != analyzed.front())
        std::cerr << "Mismatch: " << bestMove.front() << " != " << analyzed.front() << "\n";
    assert(analyzed.adjustScore() == bestMove.adjustScore());
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

enum PuzzleError { NO_ERROR, DEPTH_ERROR, EVAL_ERROR, SEARCH_ERROR, MATE_ERROR, COUNT };

struct PuzzleStats {
    std::array<uint64_t, PuzzleError::COUNT> counts = {0};
    uint64_t& operator[](PuzzleError error) { return counts[error]; }
    // Return a string representation of the stats
    uint64_t total() const { return std::accumulate(counts.begin(), counts.end(), 0); }
    std::string operator()() {
        std::stringstream ss;
        ss << total() << " puzzles, "                     //
           << counts[NO_ERROR] << " correct "             //
           << counts[DEPTH_ERROR] << " too deep, "        //
           << counts[EVAL_ERROR] << " eval errors, "      //
           << counts[SEARCH_ERROR] << " search errors, "  //
           << counts[MATE_ERROR] << " mate errors";
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
    auto mateError = expectedScore.mate() && gotScore != expectedScore;
    auto evalError = gotScore >= expectedScore;
    auto errorKind = mateError ? "Mate error" : (evalError ? "Evaluation error" : "Search error");
    std::cout << errorKind << ": Got " << got << ", but expected " << expected << "\n";
    std::cout << "Got: " << gotScore << gotReason << "\n";
    std::cout << "Expected: " << expectedScore << expectedReason << "\n";
    return mateError ? MATE_ERROR : (evalError ? EVAL_ERROR : SEARCH_ERROR);
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

void testFromStream(std::istream& input, int depth) {
    // Assumes CSV input as from the lichess puzzle database:
    // PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags
    const int kExpectedPuzzleRating = 2300;
    std::string line;
    std::getline(input, line);
    auto columns = split(line, ',');
    auto colFEN = find(columns, "FEN");
    auto colMoves = find(columns, "Moves");
    auto colPuzzleId = find(columns, "PuzzleId");
    auto colRating = find(columns, "Rating");
    auto puzzleRating = ELO(kExpectedPuzzleRating);
    PuzzleStats stats;
    nnue::resetTimingStats();

    while (input) {
        std::getline(input, line);
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
    assert(stats[MATE_ERROR] == 0);
    assert(puzzleRating() >= kExpectedPuzzleRating - ELO::K);

    if (verbose) nnue::printTimingStats();
}

void testFromFile(const std::string& filename, int depth) {
    std::ifstream file(filename);
    if (!file.is_open()) usage("Could not open file: " + filename);
    testFromStream(file, depth);
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

void testMateInTwo() {
    // Test a mate-in-2: black to move, b3h3+ g2h3 f2h2#
    // Regression test for search extension handling
    std::string fen = "6k1/2P3pp/1P6/4b3/3p4/Br5P/4prP1/R5RK b - - 0 30";
    auto position = fen::parsePosition(fen);
    auto pv = search::computeBestMove(position, 6);
    // Should find M2, not incorrectly claim M1
    assert(pv.adjustScore().mate() == 2);
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
    testMateInTwo();
    testNullMoveHash();
    if (debug) std::cout << "Basic search test passed!\n";
}

// Returns true if and only if number consists exclusively of digits and  is not empty
bool isAllDigits(std::string number) {
    return !number.empty() && std::all_of(number.begin(), number.end(), ::isdigit);
}

MoveVector parseMovetext(Position position, int argc, char* argv[]) {

    MoveVector moves;
    for (int i = 2; i < argc; ++i) {
        pgn::PGN pgn = {{}, argv[i]};
        for (auto san : pgn) {
            if (!san) usage("Invalid SAN syntax: " + std::string(san));
            Move move = pgn::move(position, san);
            if (!move) usage("Invalid SAN move: " + std::string(san));
            moves.push_back(move);
            position = moves::applyMove(position, move);
        }
    }
    return moves;
}
MoveVector parseMoves(Position position, int argc, char* argv[]) {
    // Parse moves from the command line in either UCI format, like:
    //   moves e2e4 e7e5 g1f3 ...
    // or SAN format as used in PGN files, like:
    //   movetext Nf3 Nc6 Bb5 a6 ...
    if (argc >= 2 && !strcmp(argv[1], "movetext")) return parseMovetext(position, argc, argv);
    if (argc < 2 || strcmp(argv[1], "moves")) return {};

    MoveVector moves;
    for (int i = 2; i < argc; ++i) {
        Move move = fen::parseUCIMove(position.board, argv[i]);
        moves.push_back(move);
        position = moves::applyMove(position, move);
    }
    return moves;
}

bool isOption(char* arg, std::string shortOpt, std::string longOpt) {
    return std::string(arg) == shortOpt || std::string(arg) == longOpt;
}

/** Handle puzzle test mode: <depth> [puzzle-file] */
void handlePuzzleTestMode(int argc, char* argv[]) {
    if (argc > 3) usage("too many arguments for puzzle test mode");
    auto puzzleFile = argc == 3 ? argv[2] : nullptr;
    int depth = std::stoi(argv[1]);

    printEvalRate([&]() {
        return puzzleFile ? testFromFile(puzzleFile, depth)   // <depth> puzzle-file.csv
                          : testFromStream(std::cin, depth);  // <depth> < puzzle-file.csv (stdin)
    });
}
}  // namespace

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    if (argc > 1 && isOption(argv[1], "-v", "--verbose")) {
        verbose = true;
        --argc;
        ++argv;
    }
    if (argc > 1 && isOption(argv[1], "-h", "--help")) usage();

    testBasicSearch();
    testBasicPuzzles();

    if (argc < 2) return 0;

    // Check for puzzle test mode: <depth> [puzzle-file]
    if (isAllDigits(argv[1])) return handlePuzzleTestMode(argc, argv), 0;

    // FEN mode: parse FEN and optional moves/depth
    if (!fen::maybeFEN(argv[1])) usage("invalid argument: " + std::string(argv[1]));

    // If the last argument is a number, it's the search depth
    int depth = isAllDigits(argv[argc - 1]) ? std::stoi(argv[--argc]) : 0;

    // Any remaining argument is the FEN string, use initial position if none given
    std::string fen(--argc < 1 ? fen::initialPosition : *++argv);

    // Parse the FEN string into a Position
    Position position = fen::parsePosition(fen);

    // Apply any moves specified on the command line
    auto moves = parseMoves(position, argc, argv);

    {
        auto newPos = applyMoves(position, moves);
        Score quiescenceEval = search::quiesce(newPos, options::quiescenceDepth);
        if (newPos.active() == Color::b) quiescenceEval = -quiescenceEval;
        std::cout << "Quiescence search: " << std::string(quiescenceEval) << " (white side)\n";
    }

    if (depth) {
        nnue::resetTimingStats();
        printEvalRate([&]() { printBestMove(position, depth, moves); });
        if (verbose && nnue::g_totalEvaluations) {
            nnue::printTimingStats();
            nnue::analyzeComputationalComplexity();
        }
        if (debug) printEvalRate([&]() { printAnalysis(position, depth, moves); });
    }
    return 0;
}
