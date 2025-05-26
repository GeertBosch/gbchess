#include <algorithm>
#include <atomic>
#include <chrono>
#include <ostream>
#include <sstream>
#include <thread>

#include "common.h"
#include "fen.h"
#include "moves.h"
#include "options.h"
#include "search.h"
#include "uci.h"

namespace {
const char* const cmdName = "gbchess";
const char* const authorName = "Geert Bosch";

using UCIArguments = std::vector<std::string>;

namespace {
MoveVector parseUCIMoves(Position position, const std::vector<std::string>& moves) {
    MoveVector vector;
    for (auto uci : moves)
        position = applyMove(position, vector.emplace_back(parseUCIMove(position, uci)));

    return vector;
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


}  // namespace

using namespace std::chrono;
using namespace std::chrono_literals;
using clock = steady_clock;

class UCIRunner {
public:
    UCIRunner(std::ostream& out, std::ostream& log) : out(out), log(log) {}
    ~UCIRunner() { stop(); }

    void execute(std::string line);

private:
    using UCICommandHandler = std::function<void(std::ostream&, UCIArguments args)>;

    Position position = fen::parsePosition(fen::initialPosition);
    MoveVector moves;
    std::atomic_bool stopping = false;
    time_point<clock> startTime;

    void respond(std::ostream& out, const std::string& response) {
        out << response << "\n";
        std::flush(out);
    }

    void respond(const std::string& response) {
        respond(out, response);
        if (&out == &log) return;  // don't log to log if it's the same as out
        respond(log, response);
    }

    UCIArguments getUCIArguments(std::istream& in) {
        UCIArguments args;
        std::copy(std::istream_iterator<std::string>(in), {}, std::back_inserter(args));
        return args;
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

    void go(UCIArguments arguments) {
        auto depth = options::defaultDepth;
        auto movetime = options::defaultMoveTime;
        auto wait = false;
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (arguments[i] == "depth" && ++i < arguments.size()) {
                depth = std::stoi(arguments[i]);
                continue;
            }
            if (arguments[i] == "wait") {
                wait = true;
                continue;
            }
            if (arguments[i] == "movetime" && ++i < arguments.size()) {
                movetime = std::stoi(arguments[i]);
                continue;
            }
        }
        stop();
        auto search =
            [this, depth, pos = position, moves = moves, movetime, &startTime = startTime]() {
                auto position = pos;  // need to copy the position
                auto pv = search::computeBestMove(
                    position, depth, moves, [this, movetime, &startTime](std::string info) -> bool {
                        auto elapsed =
                            duration_cast<milliseconds>(clock::now() - startTime).count();
                        respond("info " + info);
                        if (elapsed > movetime) {
                            stopping.store(true);
                            respond("info string time exceeded " + std::to_string(elapsed) + "ms");
                        }
                        return stopping;
                    });
                std::stringstream ss;
                ss << "bestmove " << pv.front();
                if (Move ponder = pv[1]) ss << " ponder " << ponder;
                respond(ss.str());
            };
        startTime = clock::now();
        if (wait) {
            search();
        } else {
            thread = std::thread(search);
        }
    }

    void stop() {
        if (thread.joinable()) {
            stopping = true;
            std::flush(out);
            thread.join();
            stopping = false;
        }
    }

    std::ostream& out;
    std::ostream& log;
    std::thread thread;
};

void UCIRunner::execute(std::string line) {
    auto in = std::stringstream(line);
    std::string command;
    in >> command;

    if (command == "uci") {
        out << "id name " << cmdName << "\n";
        out << "id author " << authorName << "\n";
        out << "uciok\n";
    } else if (command == "isready") {
        out << "readyok\n";
    } else if (command == "quit") {
        std::exit(0);
    } else if (command == "ucinewgame") {
        search::newGame();
    } else if (command == "position") {
        std::string positionKind;
        in >> positionKind >> std::ws;
        parsePosition(positionKind, getUCIArguments(in));
    } else if (command == "go") {
        go(getUCIArguments(in));
    } else if (command == "stop") {
        stop();
    } else if (command == "d") {
        out << fen::to_string(position) << "\n";
    } else if (command == "sleep") {
        // Test command to sleep for a number of milliseconds
        int ms;
        in >> ms;
        std::cerr << "sleeping for " << ms << "ms\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        std::cerr << "waking from sleep after " << ms << "ms\n";
    } else if (command == "#" || command == "expect") {
        // This is to allow comments and expected output in UCI test scripts
        std::cerr << line << "\n";
    } else {
        out << "Unknown command: " << command << "\n";
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