#include <algorithm>
#include <array>
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
int numJobs = -1;  // -1 = default to one engine per CPU
std::string cmdName = "puzzle-test";

int getNumCPUs() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
}

void usage() {
    std::cerr << "Usage: " << cmdName
              << " [-v|--verbose] [-j[N]] <engine-cmd> <depth> [puzzle-file.csv]\n"
              << "  -j       Use one engine per CPU (default)\n"
              << "  -jN      Use N parallel engines\n"
              << "  engine-cmd       UCI engine to test (e.g. ./build/engine or stockfish)\n"
              << "  depth            Search depth passed to 'go depth'\n"
              << "  puzzle-file.csv  Lichess CSV format; read from stdin if omitted\n";
    std::exit(1);
}

void usage(const std::string& msg) {
    std::cerr << "Error: " << msg << "\n";
    usage();
}

// ─── UCI engine subprocess ────────────────────────────────────────────────────

/** Spawns a UCI engine process and provides send/readline communication. */
class UCIEngine {
    pid_t pid = -1;
    int toEngine = -1;    // write to engine stdin
    FILE* reader = nullptr;  // read from engine stdout

public:
    explicit UCIEngine(const std::string& command) {
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
        std::string msg = line + "\n";
        write(toEngine, msg.c_str(), msg.size());
    }

    std::string readline() {
        char buf[4096];
        if (!fgets(buf, sizeof(buf), reader)) return {};
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        return line;
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
     * Set position from FEN + list of UCI moves, then search at the given depth.
     * Returns the bestmove string (e.g. "e2e4"), or empty string on failure.
     */
    std::string go(const std::string& fen,
                   const std::vector<std::string>& moves,
                   int depth) {
        std::string pos = "position fen " + fen;
        if (!moves.empty()) {
            pos += " moves";
            for (const auto& m : moves) pos += " " + m;
        }
        sendLine(pos);
        sendLine("go depth " + std::to_string(depth));

        for (std::string line; !(line = readline()).empty();) {
            if (line.substr(0, 9) == "bestmove ") {
                auto parts = split(line, ' ');
                return parts.size() >= 2 ? parts[1] : std::string{};
            }
        }
        return {};
    }

private:
    void waitReady() {
        sendLine("isready");
        for (std::string line; (line = readline()) != "readyok" && !line.empty();)
            ;
    }
};

// ─── Puzzle result types ──────────────────────────────────────────────────────

enum PuzzleError { NO_ERROR, DEPTH_ERROR, WRONG_MOVE, MATE_ERROR, COUNT };

struct PuzzleStats {
    std::array<uint64_t, PuzzleError::COUNT> counts = {};
    uint64_t& operator[](PuzzleError e) { return counts[e]; }
    uint64_t total() const { return std::accumulate(counts.begin(), counts.end(), 0ULL); }
    std::string str() const {
        std::ostringstream ss;
        ss << total() << " puzzles, "
           << counts[NO_ERROR] << " correct, "
           << counts[DEPTH_ERROR] << " skipped, "
           << counts[WRONG_MOVE] << " wrong moves, "
           << counts[MATE_ERROR] << " mate errors";
        return ss.str();
    }
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

/**
 * Apply a UCI move string to a Position, returning the new Position.
 */
Position applyUCIMove(const Position& pos, const std::string& uciMove) {
    Move move = fen::parseUCIMove(pos.board, uciMove);
    return moves::applyMove(pos, move);
}

// ─── Puzzle runner ────────────────────────────────────────────────────────────

/**
 * Run a single puzzle against the engine.
 *
 * puzzleMoves[0] is the opponent's last move that sets up the puzzle position;
 * subsequent moves alternate engine / opponent in the expected solution.
 *
 * The engine must play the expected move at each step, unless its move achieves
 * checkmate. Output (error messages and verbose info) is written to `out`.
 */
PuzzleError runPuzzle(UCIEngine& engine,
                      const std::string& label,
                      const std::string& fen,
                      const std::vector<std::string>& puzzleMoves,
                      int depth,
                      std::ostream& out) {
    if (puzzleMoves.size() < 2) {
        if (verbose) out << label << ": too few moves, skipping\n";
        return DEPTH_ERROR;
    }
    // The solution is puzzleMoves[1..]; skip puzzles whose solution exceeds depth.
    size_t solutionLen = puzzleMoves.size() - 1;
    if (solutionLen > size_t(depth)) {
        if (verbose)
            out << label << ": solution length " << solutionLen
                << " exceeds depth " << depth << " (skipped)\n";
        return DEPTH_ERROR;
    }

    engine.newGame();

    Position pos = fen::parsePosition(fen);

    // Each loop iteration applies one puzzle move and checks the engine's response.
    for (size_t i = 0; i + 1 < puzzleMoves.size(); i += 2) {
        pos = applyUCIMove(pos, puzzleMoves[i]);

        auto engineMove = engine.go(fen::to_string(pos), {}, depth);

        auto enginePos = applyUCIMove(pos, engineMove);
        pos = applyUCIMove(pos, puzzleMoves[i + 1]);

        if (engineMove == puzzleMoves[i + 1] || isCheckmate(enginePos)) continue;

        // print error message
        out << label << ": step " << (i / 2) + 1 << ", expected " << puzzleMoves[i + 1]
            << ", got " << engineMove << "\n";
        return isCheckmate(pos) ? MATE_ERROR : WRONG_MOVE;
    }
    return NO_ERROR;
}

// ─── CSV driver ──────────────────────────────────────────────────────────────

void testFromStream(std::istream& input, const std::string& engineCmd, int depth, int numEngines) {
    constexpr int kExpectedPuzzleRating = 2300;

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
        std::string label, fen;
        std::vector<std::string> puzzleMoves;
        std::promise<std::pair<PuzzleError, std::string>> promise;
    };

    std::queue<std::unique_ptr<Task>> workQueue;
    std::mutex workMutex;
    std::condition_variable workCV;
    bool noMoreWork = false;

    auto workerFn = [&]() {
        UCIEngine engine(engineCmd);
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
            auto err = runPuzzle(engine, task->label, task->fen, task->puzzleMoves, depth, oss);
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
        task->label = label;
        task->fen = cols[colFEN];
        task->puzzleMoves = std::move(puzzleMoves);

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

// ─── Self-tests (run when invoked with no arguments) ─────────────────────────

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
}

}  // namespace

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    // Self-test mode when run without arguments (as a unit test)
    if (argc == 1) {
        testBasics();
        return 0;
    }

    int i = 1;
    while (i < argc) {
        std::string arg(argv[i]);
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-j") {
            numJobs = getNumCPUs();
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == 'j') {
            std::string numStr = arg.substr(2);
            bool allDigits =
                !numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit);
            if (!allDigits) usage("-j requires a positive integer or no argument");
            numJobs = std::stoi(numStr);
            if (numJobs <= 0) usage("-j value must be positive");
        } else {
            break;
        }
        ++i;
    }

    if (numJobs < 0) numJobs = getNumCPUs();  // default: one engine per CPU

    if (argc - i < 2) usage();

    std::string engineCmd = argv[i++];
    std::string depthStr = argv[i++];

    bool allDigits = !depthStr.empty() &&
                     std::all_of(depthStr.begin(), depthStr.end(), ::isdigit);
    if (!allDigits) usage("depth must be a positive integer");
    int depth = std::stoi(depthStr);
    if (depth <= 0) usage("depth must be positive");

    if (i < argc) {
        std::ifstream file(argv[i]);
        if (!file.is_open()) usage("Could not open file: " + std::string(argv[i]));
        testFromStream(file, engineCmd, depth, numJobs);
    } else {
        testFromStream(std::cin, engineCmd, depth, numJobs);
    }

    return 0;
}
