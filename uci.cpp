#include "options.h"
#include <algorithm>
#include <chrono>
#include <ostream>
#include <sstream>
#include <thread>

#include "common.h"
#include "fen.h"
#include "moves.h"
#include "search.h"
#include "uci.h"

namespace {
const char* const cmdName = "gbchess";
const char* const authorName = "Geert Bosch";

class UCIRunner {
public:
    UCIRunner(std::ostream& out, std::ostream& log) : out(out), log(log) {}
    ~UCIRunner() { stop(); }

    void execute(std::string line);

private:
    using UCIArguments = std::vector<std::string>;
    using UCICommandHandler = std::function<void(std::ostream&, UCIArguments args)>;

    Position position = fen::parsePosition(fen::initialPosition);

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
        auto search = [this, depth, pos = position] {
            auto position = pos;  // need to copy the position
            auto pv = search::computeBestMove(position, depth, [this](std::string info) -> bool {
                out << "info " << info << "\n";
                std::flush(out);
                if (&out != &log) {
                    log << "info " << info << "\n";
                    std::flush(log);
                }
                return false;
            });
            std::stringstream ss;
            ss << "bestmove " << pv.front();
            if (Move ponder = pv[1]) ss << " ponder " << ponder;
            ss << "\n";
            out << ss.str();
            std::flush(out);
        };
        if (wait) {
            search();
        } else {
            thread = std::thread(search);
        }
    }

    void stop() {
        if (thread.joinable()) {
            search::stop();
            std::flush(out);
            thread.join();
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
        UCIArguments args;
        UCIArguments moves;
        std::copy(std::istream_iterator<std::string>(in), {}, std::back_inserter(args));
        if (positionKind == "startpos") {
            position = fen::parsePosition(fen::initialPosition);
            moves = std::move(args);
        } else if (positionKind == "fen" && args.size() >= 6) {
            std::string fen = "";
            join(&args[0], &args[6], std::string(" "), std::back_inserter(fen));
            if (fen.size() >= 2 && fen[0] == '"' && fen[fen.size() - 1] == '"')
                fen = fen.substr(1, fen.size() - 2);

            position = fen::parsePosition(fen);
            skip(args.begin(), args.end(), 6, std::back_inserter(moves));

        } else {
            out << "Unknown position kind: " << positionKind << "\n";
            return;
        }
        if (!moves.size()) return;
        if (moves[0] != "moves") {
            out << "Expected moves, got: " << moves[0] << "\n";
            return;
        }
        moves.erase(moves.begin());
        for (auto move : moves) {
            position = applyMove(position, parseUCIMove(position, move));
        }
    } else if (command == "go") {
        UCIArguments args;
        std::copy(std::istream_iterator<std::string>(in), {}, std::back_inserter(args));
        go(args);
    } else if (command == "stop") {
        stop();
    } else if (command == "d") {
        out << fen::to_string(position) << "\n";
    } else if (command == "sleep") {
        int ms;
        in >> ms;
        std::cerr << "sleeping for " << ms << "ms\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    } else {
        out << "Unknown command: " << command << "\n";
    }
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