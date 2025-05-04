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

std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    for (const auto& move : moves) os << std::string(move) << " ";

    return os;
}
std::ostream& operator<<(std::ostream& os, const UCIArguments& args) {
    for (const auto& arg : args) os << arg << " ";

    return os;
}

namespace {
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

MoveVector parseUCIMoves(Position position, const std::vector<std::string>& moves) {
    MoveVector vector;
    for (auto uci : moves)
        position = applyMove(position, vector.emplace_back(parseUCIMove(position, uci)));

    return vector;
}

MoveVector parseUCIMoves(Position position, const std::string& moves) {
    return parseUCIMoves(position, split(moves, ' '));
}

}  // namespace

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

    void go(UCIArguments arguments) {
        auto depth = options::defaultDepth;
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
        }
        stop();
        auto search = [this, depth, pos = position, moves = moves] {
            auto position = pos;  // need to copy the position
            auto pv =
                search::computeBestMove(position, depth, moves, [this](std::string info) -> bool {
                    out << "info " << info << "\n";
                    std::flush(out);
                    if (&out != &log) {
                        log << "info " << info << "\n";
                        std::flush(log);
                    }
                    return stopping;
                });
            std::stringstream ss;
            if (!pv.front()) {
                ss << "resign\n";
            } else {
                ss << "bestmove " << pv.front();
                if (Move ponder = pv[1]) ss << " ponder " << ponder;
            }
            ss << "\n";
            out << ss.str();
            std::flush(out);
            if (&out != &log) {
                log << ss.str();
                std::flush(log);
            }
        };
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
    } else if (command == "position") {
        std::string positionKind;
        in >> positionKind >> std::ws;
        UCIArguments posArgs;
        UCIArguments moveArgs;
        Position position;
        std::copy(std::istream_iterator<std::string>(in), {}, std::back_inserter(posArgs));
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
    } else if (command == "go") {
        UCIArguments args;
        std::copy(std::istream_iterator<std::string>(in), {}, std::back_inserter(args));
        go(args);
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
    for (std::string line; std::getline(in, line);) {
        log << "UCI: " << line << "\n";
        std::flush(log);
        runner.execute(line);
    }
}