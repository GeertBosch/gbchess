#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>


#include "book/book.h"
#include "core/core.h"
#include "core/options.h"
#include "core/uint128_str.h"
#include "engine/fen/fen.h"
#include "engine/perft/perft_core.h"
#include "move/move.h"
#include "search/search.h"
#include "time/time.h"

namespace {
const char* const cmdName = "gbchess";
const char* const authorName = "Geert Bosch";

using UCIArguments = std::vector<std::string>;

inline std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << to_string(mv);
}

namespace {
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
void printStatistics(std::ostream& os) {
    using namespace search;
    // === gbchess Search Statistics ===
    // Total nodes: 47
    // Null move attempts: 0 (0% of nodes)
    // Null move cutoffs: 0 (0% of attempts)
    // Beta cutoffs: 14 (29% of nodes)
    // First-move cutoffs: 14 (100% of beta cutoffs)
    // ===================================
    os << "\n=== " + std::string(cmdName) + " Search Statistics ===\n";
    os << "Total nodes: " << search::nodeCount << "\n";
    os << "Null move attempts: " << search::nullMoveAttempts << " ("
       << (search::nodeCount ? (search::nullMoveAttempts * 100 / search::nodeCount) : 0)
       << "% of nodes)\n";
    os << "Null move cutoffs: " << search::nullMoveCutoffs << " ("
       << (search::nullMoveAttempts ? (search::nullMoveCutoffs * 100 / search::nullMoveAttempts)
                                    : 0)
       << "% of attempts)\n";
    os << "Beta cutoffs: " << search::betaCutoffs << " ("
       << (search::nodeCount ? (search::betaCutoffs * 100 / search::nodeCount) : 0)
       << "% of nodes)\n";
    os << "First-move cutoffs: " << search::firstMoveCutoffs << " ("
       << (search::betaCutoffs ? (search::firstMoveCutoffs * 100 / search::betaCutoffs) : 0)
       << "% of beta cutoffs)\n";
    os << "===================================\n";
}

}  // namespace

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

    book::Book book;
    uint64_t bookMoveCount = 0;
    bool useOwnBook = true;
    Position position = fen::parsePosition(fen::initialPosition);
    TimeControl timeControl = TimeControl::infinite();  // No time limits by default
    uint64_t maxNodes = options::fixedNodesSearch;
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

    void performSearch(int depth, Position position, MoveVector moves) {
        auto color = position.turn.activeColor();
        if (moves.size() % 2 == 1) color = !color;
        auto timeMillis =
            timeControl.computeMillisForMove(color, position.turn.fullmove() + moves.size() / 2);

        auto pv = search::computeBestMove(
            position, depth, moves, [this, timeMillis](std::string info) -> bool {
                auto elapsed = duration_cast<milliseconds>(clock::now() - startTime).count();
                auto nodes = search::nodeCount - startNodes;
                bool nodesExceeded = maxNodes && nodes > maxNodes;
                bool timeExceeded = timeMillis && elapsed > timeMillis;

                respond("info " + info);
                if (nodesExceeded) {
                    stopping.store(true);
                    respond("info string nodes exceeded " + std::to_string(nodes) + " nodes");

                } else if (timeExceeded && !maxNodes) {
                    stopping.store(true);
                    respond("info string time exceeded " + std::to_string(elapsed) + "ms");
                }
                return stopping;
            });
        std::stringstream ss;
        ss << "bestmove " << pv.front();
        if (Move ponder = pv[1]) ss << " ponder " << ponder;
        respond(ss.str());
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
        } else {
            respond("info string unknown option: " + name);
        }
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
        auto depth = options::defaultDepth;
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
                maxNodes = std::stoull(arguments[i]);
            else if (arguments[i] == "perft" && ++i < arguments.size())
                return perft(std::stoi(arguments[i]));
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

    std::ostream& out;
    std::ostream& log;
    std::thread thread;
};

void UCIRunner::execute(std::string line) {
    auto in = std::stringstream(line);
    std::string command;
    in >> command;

    if (command != "stop" && thread.joinable())
        thread.join();  // Wait for previous search to finish

    if (command == "uci") {
        out << "id name " << cmdName << "\n";
        out << "id author " << authorName << "\n";
        out << "option name OwnBook type check default true\n";
        out << "uciok\n";
    } else if (command == "isready") {
        out << "readyok\n";
    } else if (command == "quit") {
        printStatistics(out);
        std::exit(0);
    } else if (command == "ucinewgame") {
        search::newGame();
        if (!book) book = book::loadBook("book.csv");
        // Reseed the opening book random generator
        uint64_t random = ++seeds;  // Non-zero random seed
        book.reseed(random);        // Use seed 0 for deterministic book moves in new game
        respond("info string book reseeded with " + ::to_string(random));
        timeControl = TimeControl{180'000};  // Default to 3 minutes per side
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

void fromStdIn() {
    std::ofstream log("engine.log");
    log << "Entering UCI\n";
    enterUCI(std::cin, std::cout, log);
}

void fromFile(const char* filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        enterUCI(file, std::cout, std::cout);
    } else {
        std::cerr << "Failed to open file: " << filename << std::endl;
        std::exit(2);
    }
}

int main(int argc, char** argv) {
    if (argc == 1)
        fromStdIn();
    else
        for (int i = 1; i < argc; i++) fromFile(argv[i]);

    return 0;
}