#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <numeric>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/text_util.h"
#include "engine/fen/fen.h"
#include "eval/eval.h"
#include "move/move.h"
#include "search/elo.h"

namespace {

bool verbose = false;
bool firstOnly = false;
int numJobs = -1;  // -1 = default to one engine per CPU
std::string cmdName = "puzzle-test";
std::string debugPath;  // empty = disabled
std::atomic<int> engineCounter{0};

int getNumCPUs() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
}

void usage() {
    std::cerr
        << "Usage: " << cmdName
        << " [-v|--verbose] [-j[N]] [-d] [--first-only] [--depth N] [--nodes N] [--movetime N]"
           " <engine-cmd> [engine-opts...] [puzzle-file.csv]\n"
        << "  -j               Use one engine per CPU (default)\n"
        << "  -jN              Use N parallel engines\n"
        << "  -d               Log UCI I/O to $TMPDIR/puzzle-test (or ./puzzle-test) per engine\n"
        << "  --first-only     Only check the engine's first move in each puzzle\n"
        << "  --depth N        Search depth for 'go depth N'\n"
        << "  --nodes N        Node limit for 'go nodes N'\n"
        << "  --movetime N     Time limit in ms for 'go movetime N'\n"
        << "  engine-cmd       UCI engine to test (e.g. ./build/engine or stockfish)\n"
        << "  engine-opts      Options starting with '-' forwarded to the engine command\n"
        << "  puzzle-file.csv  Lichess CSV format; read from stdin if omitted\n";
    std::exit(1);
}

void usage(const std::string& msg) {
    std::cerr << "Error: " << msg << "\n";
    usage();
}

void info(std::ostream& oss, const std::string& msg) {
    if (verbose) oss << msg << "\n";
}

// ─── Go parameters ───────────────────────────────────────────────────────────

struct GoParams {
    int depth = 0;  // signed, to avoid unsigned underflow when comparing depths
    unsigned nodes = 0;
    unsigned movetime = 0;

    std::string command() const {
        std::string cmd = "go";
        if (depth > 0) cmd += " depth " + std::to_string(depth);
        if (nodes > 0) cmd += " nodes " + std::to_string(nodes);
        if (movetime > 0) cmd += " movetime " + std::to_string(movetime);
        return cmd;
    }
};

// ─── UCI engine subprocess ────────────────────────────────────────────────────

/** Spawns a UCI engine process and provides send/readline communication. */
class UCIEngine {
    pid_t pid = -1;
    int toEngine = -1;    // write to engine stdin
    FILE* reader = nullptr;  // read from engine stdout
    std::string lastInfo;    // last info string from engine since list sendLine call
    GoParams lastGoParams;
    std::ofstream debugOut;

public:
    explicit UCIEngine(const std::string& command, const std::string& debugFilePath = {}) {
        if (!debugFilePath.empty()) debugOut.open(debugFilePath);
        int toChild[2], fromChild[2];
        if (pipe(toChild) || pipe(fromChild))
            throw std::runtime_error("pipe() failed");

        pid = fork();
        if (pid < 0) throw std::runtime_error("fork() failed");

        if (pid == 0) {
            // Child: wire up pipes, suppress engine stderr, exec
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);
            int devNull = open("/dev/null", O_WRONLY);
            if (devNull >= 0) {
                dup2(devNull, STDERR_FILENO);
                close(devNull);
            }
            close(toChild[0]);
            close(toChild[1]);
            close(fromChild[0]);
            close(fromChild[1]);
            execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
            _exit(1);
        }

        // Parent
        close(toChild[0]);
        close(fromChild[1]);
        toEngine = toChild[1];
        reader = fdopen(fromChild[0], "r");
        if (!reader) throw std::runtime_error("fdopen() failed");
    }

    ~UCIEngine() {
        if (toEngine >= 0) {
            sendLine("quit");
            close(toEngine);
            toEngine = -1;
        }
        if (reader) {
            fclose(reader);
            reader = nullptr;
        }
        if (pid > 0) {
            waitpid(pid, nullptr, 0);
            pid = -1;
        }
    }

    UCIEngine(const UCIEngine&) = delete;

    void sendLine(const std::string& line) {
        if (debugOut.is_open()) debugOut << "> " << line << "\n" << std::flush;
        lastInfo = {};
        std::string msg = line + "\n";
        write(toEngine, msg.c_str(), msg.size());
    }

    std::string readline() {
        char buf[4096];
        if (!fgets(buf, sizeof(buf), reader)) return {};
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (debugOut.is_open()) debugOut << "< " << line << "\n" << std::flush;
        return line;
    }

    void debugLine(const std::string& line) {
        if (debugOut.is_open()) debugOut << "! " << line << "\n" << std::flush;
    }

    /** Send uci / isready handshake. Must be called once before searching. */
    void initialize() {
        sendLine("uci");
        for (std::string line; (line = readline()) != "uciok" && !line.empty();)
            ;
        waitReady();
    }

    /** Send ucinewgame + isready to reset engine state between puzzles. */
    void newGame() {
        sendLine("ucinewgame");
        waitReady();
    }

    /**
     * Set position from FEN + list of UCI moves, then search with the given params.
     * Returns the bestmove string (e.g. "e2e4"), or empty string on failure.
     */
    std::string go(const std::string& fen, const StringVector& moves, const GoParams& params) {
        lastGoParams = params;
        std::string pos = "position fen " + fen;
        if (!moves.empty()) {
            pos += " moves";
            for (const auto& m : moves) pos += " " + m;
        }
        sendLine(pos);
        sendLine(params.command());

        for (std::string line; !(line = readline()).empty();) {
            if (startsWith(line, "bestmove ")) {
                auto parts = split(line, ' ');
                return parts.size() >= 2 ? parts[1] : std::string{};
            }
            if (startsWith(line, "info ")) lastInfo = line.substr(5);
        }
        return {};
    }

    std::string info() const { return lastInfo; }

    int depth() const { return lastGoParams.depth; }

private:
    void waitReady() {
        sendLine("isready");
        for (std::string line; (line = readline()) != "readyok" && !line.empty();)
            ;
    }
};

// ─── Puzzle types ──────────────────────────────────────────────────────-────

struct Puzzle {
    std::string fen;
    std::string label;
    StringVector moves;
    int depth() const { return moves.size() - 1; }
};

enum PuzzleError { NO_ERROR, DEPTH_ERROR, MOVE_ERROR, EVAL_ERROR, SEARCH_ERROR, MATE_ERROR, COUNT };

std::string to_string(PuzzleError e) {
    switch (e) {
    case NO_ERROR: return "no error";
    case DEPTH_ERROR: return "depth error";
    case MOVE_ERROR: return "move error";
    case EVAL_ERROR: return "evaluation error";
    case SEARCH_ERROR: return "search error";
    case MATE_ERROR: return "mate error";
    default: return "unknown error";
    }
}

struct PuzzleStats {
    std::array<uint64_t, PuzzleError::COUNT> counts = {};
    uint64_t& operator[](PuzzleError e) { return counts[e]; }
    uint64_t total() const { return std::accumulate(counts.begin(), counts.end(), 0ULL); }
    std::string str() const {
        std::ostringstream ss;
        ss << total() << " puzzles, " << counts[NO_ERROR] << " correct, " << counts[DEPTH_ERROR]
           << " depth errors, " << counts[MOVE_ERROR] << " move errors, " << counts[MATE_ERROR]
           << " mate errors, " << counts[SEARCH_ERROR] << " search errors, " << counts[EVAL_ERROR]
           << " evaluation errors";
        return ss.str();
    }
};

// ─── Helpers ────────────────────────────────────────────────────────────────

/**
 * Apply a UCI move string to a Position, returning the new Position.
 */
Position applyUCIMove(const Position& pos, const std::string& uciMove) {
    Move move = fen::parseUCIMove(pos.board, uciMove);
    return moves::applyMove(pos, move);
}

std::string parseScore(const std::string& info) {
    auto parts = split(info, ' ');
    for (size_t i = 0; i + 2 < parts.size(); ++i)
        if (parts[i] == "score" && (parts[i + 1] == "cp" || parts[i + 1] == "mate"))
            return parts[i + 1] + " " + parts[i + 2];
    return {};
}

std::string parsePV(const std::string& info) {
    std::string pv;
    auto parts = split(info, ' ');
    for (size_t i = 0; i + 1 < parts.size(); ++i)
        if (parts[i] == "pv")
            while (++i < parts.size()) pv += parts[i] + " ";
    if (!pv.empty()) pv.pop_back();
    return pv;
}

// ─── Puzzle runner ──────────────────────────────────────────────────────────

struct ErrorContext {
    std::string fen;      // FEN at the position where engine moved
    std::string expected; // expected move sequence (and mate annotation if applicable)
    bool mate = false;    // whether the solution ends in checkmate
};

/** Build position context at the error step: FEN, expected moves, and mate info. */
ErrorContext buildErrorContext(const Puzzle& puzzle, size_t i) {
    Position pos = fen::parsePosition(puzzle.fen);
    for (size_t j = 0; j <= i; ++j) pos = applyUCIMove(pos, puzzle.moves[j]);
    auto errorFen = fen::to_string(pos);

    for (size_t j = i + 1; j < puzzle.moves.size(); ++j) pos = applyUCIMove(pos, puzzle.moves[j]);
    bool mate = isCheckmate(pos);
    int mateInN = mate ? puzzle.moves.size() / 2 - i / 2 : 0;

    StringVector expectedMoves(puzzle.moves.begin() + i + 1, puzzle.moves.end());
    std::string expected = join(expectedMoves, ' ');
    if (mate) expected += " mate " + std::to_string(mateInN);

    return {errorFen, expected, mate};
}

/**
 * Refine a MOVE_ERROR by re-searching the correct move in a fresh context. Compares the engine's
 * score on the wrong move vs. the correct move to distinguish SEARCH_ERROR (engine agrees solution
 * is better but missed it) from EVAL_ERROR (engine disagrees). Updates `expected` and `got` with
 * score annotations.
 */
PuzzleError classifyMoveError(UCIEngine& engine,
                               const Puzzle& puzzle,
                               size_t i,
                               std::string& expected,
                               std::string& got,
                               const std::string& gotScore) {
    // Re-search from the correct move at depth-1 to avoid TT pollution from the wrong search.
    // The resulting eval is from the opponent's perspective, so we negate it.
    auto correctMoves = StringVector(puzzle.moves.begin(), puzzle.moves.begin() + i + 2);
    auto depth = engine.depth() - 1;
    engine.newGame();
    engine.go(puzzle.fen, correctMoves, GoParams{.depth = depth});

    auto solvedScore = parseScore(engine.info());
    auto solvedCP = -parseInt(solvedScore.substr(3));
    auto gotCP = parseInt(gotScore.substr(3));
    if (startsWith(solvedScore, "cp ")) expected += " score " + std::to_string(solvedCP);
    got += " score " + gotScore;

    if (startsWith(solvedScore, "mate "))
        return MATE_ERROR;   // Engine thinks the solution (it didn't find!) is mate
    if (solvedCP > gotCP)
        return SEARCH_ERROR; // Engine agrees solution is better, but didn't find it
    return EVAL_ERROR;       // Engine disagrees that the solution is better
}

/** Report an error for a puzzle step. */
PuzzleError reportPuzzleError(
    UCIEngine& engine, Puzzle puzzle, size_t i, std::string got, std::ostream& out) {
    assert(i + 1 < puzzle.moves.size() && i % 2 == 0);
    engine.debugLine("expected " + puzzle.moves[i + 1]);

    auto ctx = buildErrorContext(puzzle, i);

    auto gotScore = parseScore(engine.info());
    auto gotPV = parsePV(engine.info());
    if (!startsWith(gotPV, got + " "))
        gotPV = {};
    else
        got = gotPV;

    auto error = ctx.mate ? MATE_ERROR : MOVE_ERROR;

    if (engine.depth() && engine.depth() < puzzle.depth() - (int)i)
        error = DEPTH_ERROR;

    // Refine MOVE_ERROR into SEARCH_ERROR or EVAL_ERROR by comparing scores.
    if (error == MOVE_ERROR && engine.depth() && startsWith(gotScore, "cp "))
        error = classifyMoveError(engine, puzzle, i, ctx.expected, got, gotScore);

    out << puzzle.label << ": " + to_string(error) + " in step " << (i / 2) + 1 << ", expected "
        << ctx.expected << ", got " << got << ", fen " << ctx.fen << "\n";
    return error;
}

/**
 * Run a single puzzle against the engine.
 *
 * puzzleMoves[0] is the opponent's last move that sets up the puzzle position;
 * subsequent moves alternate engine / opponent in the expected solution.
 *
 * The engine must play the expected move at each step, unless its move achieves
 * checkmate. Output (error messages and verbose info) is written to `out`.
 */
PuzzleError runPuzzle(UCIEngine& engine, Puzzle puzzle, const GoParams& params, std::ostream& out) {
    if (params.depth && puzzle.depth() > (int)params.depth && false)
        return info(out, "skipping " + puzzle.label + ", depth " + std::to_string(puzzle.depth())),
               DEPTH_ERROR;

    engine.newGame();

    Position pos = fen::parsePosition(puzzle.fen);
    std::string uciPos = "position fen \"" + puzzle.fen + "\"";

    // Each loop iteration applies one puzzle move and checks the engine's response.
    size_t limit = firstOnly ? 2 : puzzle.moves.size();
    for (size_t i = 0; i + 1 < limit; i += 2) {
        pos = applyUCIMove(pos, puzzle.moves[i]);

        // Pass puzzleMoves[0..i] as the move history to the engine, so it can use them for move
        // ordering and TT probes.
        auto moves = std::vector<std::string>(puzzle.moves.begin(), puzzle.moves.begin() + i + 1);
        auto engineMove = engine.go(puzzle.fen, moves, params);

        if (engineMove != puzzle.moves[i + 1] && !isCheckmate(applyUCIMove(pos, engineMove)))
            return reportPuzzleError(engine, puzzle, i, engineMove, out);

        pos = applyUCIMove(pos, puzzle.moves[i + 1]);
    }
    return NO_ERROR;
}

// ─── CSV driver ──────────────────────────────────────────────────────────────

void testFromStream(std::istream& input,
                    const std::string& engineCmd,
                    const GoParams& params,
                    int numEngines) {
    constexpr int kExpectedPuzzleRating = 2000;

    std::string line;
    std::getline(input, line);
    auto header = split(line, ',');
    auto colFEN = find(header, "FEN");
    auto colMoves = find(header, "Moves");
    auto colPuzzleId = find(header, "PuzzleId");
    auto colRating = find(header, "Rating");
    auto colThemes = find(header, "Themes");
    size_t minCols = std::max({colFEN, colMoves, colPuzzleId, colRating, colThemes}) + 1;

    // ─── Thread pool ──────────────────────────────────────────────────────────

    struct Task {
        Puzzle puzzle;
        std::promise<std::pair<PuzzleError, std::string>> promise;
    };

    std::queue<std::unique_ptr<Task>> workQueue;
    std::mutex workMutex;
    std::condition_variable workCV;
    bool noMoreWork = false;

    auto workerFn = [&]() {
        std::string dbgFile;
        if (!debugPath.empty()) {
            int id = engineCounter++;
            dbgFile = (numJobs == 1) ? debugPath : debugPath + "." + std::to_string(id);
        }
        UCIEngine engine(engineCmd, dbgFile);
        engine.initialize();
        while (true) {
            std::unique_ptr<Task> task;
            {
                std::unique_lock lk(workMutex);
                workCV.wait(lk, [&] { return !workQueue.empty() || noMoreWork; });
                if (workQueue.empty()) break;
                task = std::move(workQueue.front());
                workQueue.pop();
            }
            std::ostringstream oss;
            auto err = runPuzzle(engine, task->puzzle, params, oss);
            task->promise.set_value({err, oss.str()});
        }
    };

    std::vector<std::thread> workers;
    for (int i = 0; i < numEngines; ++i)
        workers.emplace_back(workerFn);

    // ─── Pipeline: ordered futures for in-order output ────────────────────────

    struct PipelineEntry {
        std::future<std::pair<PuzzleError, std::string>> future;
        int rating;
    };

    std::deque<PipelineEntry> pipeline;
    // Keep enough tasks ahead to saturate all workers, but bounded to avoid
    // reading the entire input into memory.
    const size_t kMaxPipeline = numEngines * 4;

    ELO puzzleRating(kExpectedPuzzleRating);
    PuzzleStats stats;

    auto outputEntry = [&](PipelineEntry& entry) {
        auto [err, output] = entry.future.get();  // blocks until result ready
        std::cout << output;
        ++stats[err];
        if (err != DEPTH_ERROR)
            puzzleRating.updateOne(ELO(entry.rating), err == NO_ERROR ? ELO::WIN : ELO::LOSS);
    };

    // Drain any pipeline entries that are already done (non-blocking).
    auto drainReady = [&]() {
        while (!pipeline.empty() &&
               pipeline.front().future.wait_for(std::chrono::seconds(0)) ==
                   std::future_status::ready) {
            outputEntry(pipeline.front());
            pipeline.pop_front();
        }
    };

    while (std::getline(input, line)) {
        if (line.empty()) continue;
        auto cols = split(line, ',');
        if (cols.size() < minCols) continue;

        auto puzzleMoves = split(cols[colMoves], ' ');
        if (puzzleMoves.empty()) continue;

        int ratingVal = std::stoi(cols[colRating]);
        auto label = "Puzzle " + cols[colPuzzleId] + ", rating " + std::to_string(ratingVal);

        // When the pipeline is full, wait for the oldest (in-order) result first.
        while (pipeline.size() >= kMaxPipeline) {
            outputEntry(pipeline.front());
            pipeline.pop_front();
        }

        auto task = std::make_unique<Task>();
        task->puzzle = {cols[colFEN], label, std::move(puzzleMoves)};

        PipelineEntry entry;
        entry.future = task->promise.get_future();
        entry.rating = ratingVal;

        {
            std::lock_guard lk(workMutex);
            workQueue.push(std::move(task));
        }
        workCV.notify_one();

        pipeline.push_back(std::move(entry));
        drainReady();
    }

    // Signal workers that no more tasks are coming, then join.
    {
        std::lock_guard lk(workMutex);
        noMoreWork = true;
    }
    workCV.notify_all();
    for (auto& t : workers) t.join();

    // Drain remaining results in input order.
    while (!pipeline.empty()) {
        outputEntry(pipeline.front());
        pipeline.pop_front();
    }

    std::cout << stats.str() << ", " << puzzleRating() << " rating\n";
}

// ─── Self-tests ─────────────────────────────────────────────────────────────
void testBasics() {
    // ELO - winning against an equal opponent increases our rating
    ELO a(1500), b(1500);
    a.updateOne(b, ELO::WIN);
    assert(a() > 1500);

    // ELO - losing against an equal opponent decreases our rating
    ELO c(1500), d(1500);
    c.updateOne(d, ELO::LOSS);
    assert(c() < 1500);

    // split
    auto parts = split("a b c", ' ');
    assert(parts.size() == 3 && parts[0] == "a" && parts[2] == "c");

    // parseScore - centipawn and mate scores extracted from typical UCI info lines
    assert(parseScore("info depth 10 score cp 42 nodes 1234 pv e2e4") == "cp 42");
    assert(parseScore("info depth 5 score mate 3 nodes 100 pv d1h5") == "mate 3");
    assert(parseScore("info depth 5 score mate -1 nodes 100 pv d1h5") == "mate -1");
    assert(parseScore("info depth 8 nodes 500") == "");  // no score token

    // parsePV - PV extracted from typical UCI info lines
    assert(parsePV("info depth 10 score cp 42 nodes 1234 pv e2e4 e7e5 g1f3") == "e2e4 e7e5 g1f3");
    assert(parsePV("info depth 5 score mate 1 nodes 100 pv d1h5") == "d1h5");
    assert(parsePV("info depth 8 nodes 500 score cp 0") == "");  // no pv token

    // applyUCIMove - e2e4 from start position advances the e-pawn
    auto startPos = fen::parsePosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto afterE4 = applyUCIMove(startPos, "e2e4");
    auto afterE4Fen = fen::to_string(afterE4);
    assert(afterE4Fen.find("4P3") != std::string::npos);       // e4 pawn is now on rank 4
    assert(afterE4Fen.find("PPPP1PPP") != std::string::npos);  // e2 is vacated

    // buildErrorContext - non-mate puzzle: step 0 wrong, solution is e7e5
    // Puzzle: start pos, setup move e2e4 (index 0), solution e7e5 (index 1)
    Puzzle nonMatePuzzle{
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "Test",
        {"e2e4", "e7e5"},
    };
    auto ctx0 = buildErrorContext(nonMatePuzzle, 0);
    assert(!ctx0.mate);
    assert(ctx0.expected == "e7e5");
    assert(ctx0.fen.find("PPPP1PPP") != std::string::npos);  // e2 vacated after e2e4

    // buildErrorContext - mate-in-1 puzzle: Scholar's mate setup, solution h5f7 is checkmate
    // FEN after 1.e4 e5 2.Qh5 Nc6 3.Bc4 — it's black's turn to blunder, but as a puzzle the
    // setup move (index 0) is black's last move b8c6 from the prior position, and the engine
    // must reply h5f7#.
    // Prior FEN: after 1.e4 e5 2.Qh5 (black to move, before Nc6)
    Puzzle matePuzzle{
        "rnbqkbnr/pppp1ppp/8/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3",
        "Test",
        {"b8c6", "h5f7"},  // setup: black plays Nc6; solution: white plays Qxf7#
    };
    auto ctx1 = buildErrorContext(matePuzzle, 0);
    assert(ctx1.mate);
    assert(ctx1.expected == "h5f7 mate 1");
}

}  // namespace

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    // Self-test mode when run without arguments (as a unit test)
    testBasics();

    GoParams goParams;
    int i = 1;
    while (i < argc) {
        std::string arg(argv[i]);
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-d") {
            const char* tmpdir = std::getenv("TMPDIR");
            std::string dir = tmpdir ? tmpdir : ".";
            if (dir.size() > 1 && dir.back() == '/') dir.pop_back();
            debugPath = dir + "/puzzle-test";
        } else if (arg == "-j") {
            numJobs = getNumCPUs();
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'j') {
            std::string numStr = arg.substr(2);
            bool allDigits =
                !numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit);
            if (!allDigits) usage("-j requires a positive integer or no argument");
            numJobs = std::stoi(numStr);
            if (numJobs <= 0) usage("-j value must be positive");
        } else if (arg == "--first-only") {
            firstOnly = true;
        } else if (arg == "--depth") {
            if (++i >= argc) usage("--depth requires an argument");
            goParams.depth = parsePositiveInt(std::string(argv[i]));
            if (goParams.depth <= 0 || goParams.depth >= 1000)
                usage("--depth must be positive and less than 1000");
        } else if (arg == "--nodes") {
            if (++i >= argc) usage("--nodes requires an argument");
            goParams.nodes = parsePositiveInt(std::string(argv[i]));
            if (goParams.nodes <= 0) usage("--nodes must be positive and less than 1,000,000,000");
        } else if (arg == "--movetime") {
            if (++i >= argc) usage("--movetime requires an argument");
            goParams.movetime = parsePositiveInt(std::string(argv[i]));
            if (goParams.movetime <= 0)
                usage("--movetime must be positive and less than 1,000,000,000 ms");
        } else {
            break;
        }
        ++i;
    }

    if (numJobs < 0) numJobs = getNumCPUs();  // default: one engine per CPU

    // Exit after self-tests if no arguments are provided. This runs the binary as a unit test.
    if (argc - i < 1) return 0;

    std::string engineCmd = argv[i++];

    // Append any engine options (args starting with '-') to the engine command string.
    while (i < argc && argv[i][0] == '-') engineCmd += std::string(" ") + argv[i++];

    if (i >= argc) return testFromStream(std::cin, engineCmd, goParams, numJobs), 0;

    std::ifstream file(argv[i]);
    if (!file.is_open()) usage("Could not open file: " + std::string(argv[i]));

    return testFromStream(file, engineCmd, goParams, numJobs), 0;
}
