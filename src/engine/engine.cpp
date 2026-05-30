#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <utility>

#include "book/book.h"
#include "core/core.h"
#include "core/options.h"
#include "core/uint128_str.h"
#include "engine/fen/fen.h"
#include "engine/perft/perft_core.h"
#include "move/move.h"
#include "search/pv.h"
#include "search/search.h"
#include "time/time.h"

namespace {
std::string cmdName = "gbchess";
const char* const authorName = "Geert Bosch";

using UCIArguments = std::vector<std::string>;

std::string basename(std::string_view path) {
    auto slash = path.rfind('/');
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

inline std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << to_string(mv);
}

uint64_t seeds = 1;  // Deterministic seed generator for UCI. Used for book move selection.
MoveVector parseUCIMoves(Position position, const std::vector<std::string>& moves) {
    MoveVector vector;
    for (auto uci : moves)
        position =
            moves::applyMove(position, vector.emplace_back(fen::parseUCIMove(position.board, uci)));

    return vector;
}

Position applyMoves(Position position, const MoveVector& moves) {
    for (auto move : moves) position = moves::applyMove(position, move);
    return position;
}

template <class InputIt, class OutputIt, class Joiner>
OutputIt join(InputIt first, InputIt last, Joiner joiner, OutputIt d_first) {
    for (; first != last; ++first) {
        d_first = std::copy(first->begin(), first->end(), d_first);
        if (first != last) d_first = std::copy(joiner.begin(), joiner.end(), d_first);
    }
    return d_first;
}

template <class InputIt, class OutputIt>
OutputIt skip(InputIt first, InputIt last, size_t skip, OutputIt d_first) {
    for (; first != last; ++first) {
        if (skip)
            --skip;
        else
            *d_first++ = *first;
    }
    return d_first;
}

using namespace std::chrono;
using namespace std::chrono_literals;
using clock = steady_clock;

class UCIRunner {
public:
    UCIRunner(std::ostream& out, std::ostream& log) : out(out), log(log) {
        if (debug) timeControl.setFixedTimeMillis(36'000'000);  // 10 hours per side in debug mode
    }
    ~UCIRunner() { wait(); }

    void execute(std::string line);

private:
    using UCICommandHandler = std::function<void(std::ostream&, UCIArguments args)>;
    static constexpr auto kDefaultTimeControl = TimeControl::infinite();  // No limits by default

    book::Book book;
    uint64_t bookMoveCount = 0;
    bool useOwnBook = true;
    Position position = fen::parsePosition(fen::initialPosition);
    TimeControl timeControl = kDefaultTimeControl;
    int64_t nodestimeBudget = 0;  // Optional total nodes budget for reproducible time control
    int64_t maxNodes = 0;         // Max nodes as specified in the go command
    int64_t maxMillis = 0;        // Max time in milliseconds as determined by time control
    MoveVector moves;
    std::atomic_bool stopping = false;
    time_point<clock> startTime = {};
    uint64_t startNodes = 0;

    void respond(std::ostream& out, const std::string& response) {
        out << response << "\n";
        std::flush(out);
    }

    void respond(const std::string& response) {
        respond(out, response);
        if (&out == &log) return;  // don't log to log if it's the same as out
        respond(log, response);
    }

    /**
     * Checks if the search should stop due to time or node limits being exceeded. Accepts a limit
     * argument in the range 0 .. 100 indicating the percentage of the hard limit to check against.
     * Returns true if the search should stop, false otherwise.
     */
    bool timecheck(int limit) {
        limit = std::clamp(limit, 0, 100);
        int64_t nodes = search::nodeCount - startNodes;
        bool nodesExceeded = maxNodes && int64_t(nodes) > maxNodes;

        auto elapsedRealtime = duration_cast<milliseconds>(clock::now() - startTime).count();
        auto elapsedNodestime = options::nodestime ? nodes / options::nodestime : 0;
        auto elapsed = options::nodestime ? elapsedNodestime : elapsedRealtime;
        bool timeExceeded = maxMillis && elapsed * 100 > maxMillis * limit;

        if (!nodesExceeded && !timeExceeded) return stopping;  // Happy path, nothing exceeded
        if (stopping.exchange(true)) return true;              // Already stopping, just return

        // Exceeded something for the first time, so report it
        auto npsString =
            elapsedRealtime ? " " + std::to_string(nodes * 1000 / elapsedRealtime) + " nps" : "";

        auto exceededString = nodesExceeded
            ? "nodes " + std::to_string(nodes) + " exceeded " + std::to_string(maxNodes)
            : "time " + std::to_string(elapsed) + " exceeded " + std::to_string(maxMillis);

        return respond("info string " + exceededString + npsString), true;
    }

    void performSearch(int depth, Position position, MoveVector moves) try {
        auto color = position.turn.activeColor();
        if (moves.size() % 2 == 1) color = !color;
        maxMillis =
            timeControl.computeMillisForMove(color, position.turn.fullmove() + moves.size() / 2);

        // For maximum reproducibility we support using a node count as a clock, where we assume
        // a constant nodes-per-second rate, in practice this can be reasonably as evaluation is
        // the most expensive part of the search, and tends to have a predictable per-node cost.
        // In this mode, we determine a total "node budget" once at the start of a new game, and
        // maintain our remaining time by tracking the nodes we've searched so far.
        if (options::nodestime)
            respond("info string nodestime " + std::to_string(options::nodestime) + " time " +
                    std::to_string(maxMillis) + " nodes " + std::to_string(nodestimeBudget));

        auto pv = search::computeBestMove(position, depth, moves, [this](std::string info) -> bool {
            // Called every node - hard bounds check
            if (info.empty()) return timecheck(100);
            // We've processed a root node. If most of the available time was spent, don't try
            // to search more root nodes, as it is unlikely that search will finish in time.

            respond("info " + info);
            return timecheck(options::softTimecheckPct);
        });

        // When using node count as clock for reproducibility, at least allow some nodes to be
        // searched in case the opponent doesn't flag us on time. This also allows us to use
        // the nodes budget being non-zero as a signal tht we're in this mode.
        int64_t kMinNodesBudget = options::nodestime;  // Allow at least 1ms worth
        if (nodestimeBudget)
            nodestimeBudget = std::max(kMinNodesBudget,
                                       nodestimeBudget - int64_t(search::nodeCount - startNodes));
        std::stringstream ss;
        ss << "bestmove " << pv.front();
        if (Move ponder = pv[1]) ss << " ponder " << ponder;
        respond(ss.str());
    } catch (const std::exception& e) {
        respond("info string error in search: " + std::string(e.what()));
        throw;
    }

    UCIArguments getUCIArguments(std::istream& in) {
        UCIArguments args;
        std::copy(std::istream_iterator<std::string>(in), {}, std::back_inserter(args));
        return args;
    }

    void setOption(UCIArguments args) {
        if (args.size() < 4 || args[0] != "name" || args[2] != "value") return;

        const std::string& name = args[1];
        const std::string& value = args[3];

        if (name == "OwnBook") {
            useOwnBook = (value == "true");
            respond("info string OwnBook set to " + value);
            return;
        }
        for (auto& info : options::UCIOptionInfo::registry()) {
            if (name != info.name) continue;
            info.set(value);
            respond("info string " + name + " set to " + value);
            return;
        }
        respond("info string unknown option: " + name);
    }

    static std::string to_string(Position pos) {
        return pos == Position::initial() ? "startpos" : "fen \"" + fen::to_string(pos) + '"';
    }

    /** Append the current UCI position and non-default options to the output stream. */
    bool saveUCIState(std::ostream& stream) const {
        // First save the position and moves, for easy retrieval using head -1 when debugging
        stream << "position " << to_string(position);
        if (moves.size()) stream << " moves " << ::to_string(moves);
        if (!useOwnBook) stream << "\nsetoption name OwnBook value false";
        for (const auto& info : options::UCIOptionInfo::registry()) {
            if (!info.isDefault()) {
                stream << "\nsetoption name " << info.name << " value ";
                if (info.isBool)
                    stream << (info.opt->value ? "true" : "false");
                else
                    stream << info.opt->value;
            }
        }
        stream << "\n\n";  // Empty line indicates end of UCI state
        return stream.good();
    }

    /** Restore the current UCI position and options from the remaining stream data. */
    bool restoreUCIState(std::istream& stream) {
        for (std::string line; std::getline(stream, line);) {
            if (line.empty()) break;  // End of UCI state
            auto in = std::stringstream(line);
            std::string command;
            in >> command;
            if (command == "setoption")
                setOption(getUCIArguments(in));
            else if (command == "position") {
                std::string positionKind;
                in >> positionKind >> std::ws;
                parsePosition(positionKind, getUCIArguments(in));
            }
        }
        return stream.good();
    }

    /** Save search state and UCI state to the given file path. */
    bool saveStateToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        if (!saveUCIState(file)) return false;
        return search::saveState(file);
    }

    /** Restore search state and UCI state from the given file path. */
    bool restoreStateFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        if (!restoreUCIState(file)) return false;
        return search::restoreState(file);
    }

    void parsePosition(std::string positionKind, UCIArguments posArgs) {
        UCIArguments moveArgs;
        Position position;
        if (positionKind == "startpos") {
            position = fen::parsePosition(fen::initialPosition);
            moveArgs = std::move(posArgs);
        } else if (positionKind == "fen" && posArgs.size() >= 6) {
            std::string fen = "";
            join(&posArgs[0], &posArgs[6], std::string(" "), std::back_inserter(fen));
            while (fen.size() && fen[fen.size() - 1] == ' ') fen.pop_back();
            if (fen.size() >= 2 && fen[0] == '"' && fen[fen.size() - 1] == '"')
                fen = fen.substr(1, fen.size() - 2);

            position = fen::parsePosition(fen);
            skip(posArgs.begin(), posArgs.end(), 6, std::back_inserter(moveArgs));

        } else {
            out << "Unknown position kind: " << positionKind << "\n";
            return;
        }
        if (!moveArgs.size()) {
            this->moves = {};
            this->position = position;
            return;
        }
        if (moveArgs[0] == "moves") moveArgs.erase(moveArgs.begin());

        this->moves = parseUCIMoves(position, moveArgs);
        this->position = position;
    }

    void perft(int depth) {
        stop();
        auto position = this->position;  // need to copy the position
        auto moves = this->moves;
        auto totalNodes = NodeCount(0);
        totalNodes = ::perft(position, depth);
        respond("Nodes searched: " + ::to_string(totalNodes));
    }

    void go(UCIArguments arguments) {
        int depth = options::defaultDepth;
        auto wait = false;
        int64_t wtime = 0, btime = 0;
        uint32_t winc = 0, binc = 0;
        uint16_t movestogo = 0;

        for (size_t i = 0; i < arguments.size(); ++i) {
            if (arguments[i] == "depth" && ++i < arguments.size())
                depth = std::stoi(arguments[i]);
            else if (arguments[i] == "movetime" && ++i < arguments.size())
                timeControl.setFixedTimeMillis(std::stoi(arguments[i]));
            else if (arguments[i] == "wtime" && ++i < arguments.size())
                wtime = std::stoll(arguments[i]);
            else if (arguments[i] == "btime" && ++i < arguments.size())
                btime = std::stoll(arguments[i]);
            else if (arguments[i] == "winc" && ++i < arguments.size())
                winc = std::stoul(arguments[i]);
            else if (arguments[i] == "binc" && ++i < arguments.size())
                binc = std::stoul(arguments[i]);
            else if (arguments[i] == "movestogo" && ++i < arguments.size())
                movestogo = std::stoul(arguments[i]);
            else if (arguments[i] == "wait")
                wait = true;
            else if (arguments[i] == "nodes" && ++i < arguments.size())
                maxNodes = std::stoll(arguments[i]);
            else if (arguments[i] == "perft" && ++i < arguments.size())
                return perft(std::stoi(arguments[i]));
        }

        if (options::nodestime) {
            // Convert clock to a node budget (once per game), then instead of relying on the
            // passed wtime / btime each move, we track our own remaining time based on the
            // remaining nodes budget. This allows for more reproducible time controls, while
            // staying roughy aligned with real time, assuming a predictable nodes/s rate.
            bool white = position.active() == Color::w;
            if (!nodestimeBudget) nodestimeBudget = (white ? wtime : btime) * options::nodestime;
            int64_t time = nodestimeBudget / options::nodestime;
            if (white)
                wtime = time;
            else
                btime = time;
        }

        // Update time control with parsed values if any time arguments were provided
        if (wtime || btime) {
            timeControl.setTimeMillis(Color::w, wtime);
            timeControl.setTimeMillis(Color::b, btime);
            timeControl.setIncrementMillis(Color::w, winc);
            timeControl.setIncrementMillis(Color::b, binc);
            timeControl.setMovesToGo(movestogo);
        }

        stop();

        // Check opening book first - if we have a book move, use it without searching
        if (auto bookMove = useOwnBook ? book.choose(position, moves) : Move{})
            return ++bookMoveCount, respond("bestmove " + ::to_string(bookMove));

        startTime = clock::now();
        startNodes = search::nodeCount;
        assert(!stopping);
        if (wait)
            performSearch(depth, position, moves);
        else
            thread = std::thread(&UCIRunner::performSearch, this, depth, position, moves);
    }

    void wait() {
        if (thread.joinable()) thread.join();
        std::flush(out);
    }

    void stop() {
        stopping = true;
        wait();
        stopping = false;
    }

    void dispatch(const std::string& command, std::istream& in, const std::string& line);

    std::ostream& out;
    std::ostream& log;
    std::thread thread;
};

void UCIRunner::dispatch(const std::string& command,
                         std::istream& in,
                         const std::string& line) try {
    if (command == "uci") {
        out << "id name " << cmdName << "\n";
        out << "id author " << authorName << "\n";
        out << "option name OwnBook type check default true\n";
        for (const auto& info : options::UCIOptionInfo::registry()) {
            if (info.isBool)
                out << "option name " << info.name << " type check default "
                    << (info.defaultVal ? "true" : "false") << "\n";
            else
                out << "option name " << info.name << " type spin default " << info.defaultVal
                    << " min " << info.minVal << " max " << info.maxVal << "\n";
        }
        out << "uciok\n";
    } else if (command == "isready") {
        out << "readyok\n";
    } else if (command == "quit") {
        search::printStats();
        std::exit(0);
    } else if (command == "ucinewgame") {
        search::newGame();
        if (!book) book = book::loadBook("book.csv");
        // Reseed the opening book random generator
        uint64_t random = ++seeds;  // Non-zero random seed
        book.reseed(random);        // Use seed 0 for deterministic book moves in new game
        respond("info string book reseeded with " + ::to_string(random));
        timeControl = kDefaultTimeControl;
        nodestimeBudget = 0;
        moves.clear();
        position = fen::parsePosition(fen::initialPosition);
    } else if (command == "position") {
        std::string positionKind;
        in >> positionKind >> std::ws;
        parsePosition(positionKind, getUCIArguments(in));
    } else if (command == "go") {
        go(getUCIArguments(in));
    } else if (command == "stop") {
        stop();
    } else if (command == "setoption") {
        setOption(getUCIArguments(in));
    } else if (command == "d") {
        out << fen::to_string(applyMoves(position, moves)) << "\n";
    } else if (command == "save") {
        std::string filename;
        in >> filename;
        if (filename.empty()) filename = "save-state.bin";
        if (saveStateToFile(filename)) respond("info string saved engine state to " + filename);
    } else if (command == "restore") {
        std::string filename;
        in >> filename;
        if (filename.empty()) filename = "save-state.bin";
        if (restoreStateFromFile(filename))
            respond("info string restored engine state from " + filename);
    } else if (command == "debug") {
        auto debugPosition = applyMoves(position, moves);
        in >> std::ws;
        auto debugMoves = parseUCIMoves(debugPosition, getUCIArguments(in));
        search::debugPosition(applyMoves(debugPosition, debugMoves));
    } else if (command == "lookup") {
        auto debugPosition = applyMoves(position, moves);
        in >> std::ws;
        auto args = getUCIArguments(in);
        auto debugMoves = parseUCIMoves(debugPosition, args);
        auto ttEntry = search::lookupPosition(applyMoves(debugPosition, debugMoves));
        std::cerr << "TT Entry: " << (ttEntry.empty() ? "(none)" : ttEntry) << "\n";
    } else if (command == "invalidate") {
        auto debugPosition = applyMoves(position, moves);
        in >> std::ws;
        auto debugMoves = parseUCIMoves(debugPosition, getUCIArguments(in));
        if (!search::invalidatePosition(applyMoves(debugPosition, debugMoves)))
            std::cerr << "No TT entry found for this position to invalidate\n";

    } else if (command == "sleep") {
        // Test command to sleep for a number of milliseconds
        int ms;
        in >> ms;
        std::cerr << "sleeping for " << ms << "ms\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        std::cerr << "waking from sleep after " << ms << "ms\n";
    } else if (command == "#" || command == "expect" || command == "expect-count") {
        // This is to allow comments and expected output in UCI test scripts
        std::cerr << line << "\n";
    } else if (command != "") {
        out << "Unknown command: '" << command << "'\n";
    }
} catch (const std::exception& e) {
    auto message = "error processing command '" + line + "': " + e.what();
    std::cerr << message << "\n";
    log << message << "\n";
    std::flush(log);
    respond("info string " + message);
}

void UCIRunner::execute(std::string line) {
    auto in = std::stringstream(line);
    std::string command;
    in >> command;

    if (command != "stop" && thread.joinable())
        thread.join();  // Wait for previous search to finish

    dispatch(command, in, line);
    std::flush(log);
}

}  // namespace

void enterUCI(std::istream& in, std::ostream& out, std::ostream& log) {
    UCIRunner runner(out, log);
    std::flush(log);
    search::newGame();
    for (std::string line; std::getline(in, line);) {
        log << "UCI: " << line << "\n";
        std::flush(log);
        runner.execute(line);
    }
}

void usage() {
    // engine [uci_tests_script ...]
    // engine --cmd [command1 command2 ...]
    std::cerr
        << "usage: " << cmdName << " [uci_tests_script ...]\n       " << cmdName
        << " --cmd [command1 command2 ...]\n\n"
        << "If no arguments are given, the engine will read UCI commands from standard input.\n"
        << "If one or more file paths are given as arguments, the engine will read UCI commands "
           "from each file in order. This is useful for running UCI test scripts.\n"
        << "If the first argument is '--cmd', the engine will execute each of the remaining "
           "arguments as "
           "a UCI command. This is useful for debugging  without needing "
           "to create a full test script.\n";
    std::exit(1);
}

void fromStream(std::istream& stream) {
    // Get the /tmp directory from the environment, or use the current directory if not set
    const char* tmpDir = std::getenv("TMPDIR");
    if (!tmpDir) tmpDir = ".";

    // Include a PID in the log file name to avoid conflicts between engines
    std::ofstream log(std::string(tmpDir) + "/engine-" + std::to_string(getpid()) + ".log");
    log << "Entering UCI for " << cmdName << " with PID " << getpid() << "\n";

    enterUCI(stream, std::cout, log);
    log << "Exiting UCI for " << cmdName << " with PID " << getpid() << "\n";
    std::flush(log);
}

void fromFile(const char* filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        fromStream(file);
    } else {
        std::cerr << "Failed to open file: " << filename << std::endl;
        std::exit(2);
    }
}

void fromArgs(int argc, char** argv) {
    std::stringstream ss;
    if (argc < 3) usage();
    for (int i = 2; i < argc; ++i) ss << argv[i] << "\n";
    std::istringstream iss(ss.str());
    enterUCI(iss, std::cout, std::cout);
}

int main(int argc, char** argv) {
    if (argc >= 1) cmdName = basename(argv[0]);

    if (argc == 1)
        fromStream(std::cin);
    else if (std::string(argv[1]) == "--cmd")
        fromArgs(argc, argv);
    else if (argv[1][0] == '-')
        usage();

    else
        for (int i = 1; i < argc; i++) fromFile(argv[i]);

    return 0;
}