#include "common.h"
#include "square_set.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "fen.h"
#include "hash.h"
#include "moves.h"
#include "uint128_t.h"

bool quiet = false;

constexpr size_t MB = 1ull << 20;
constexpr size_t hashTableMemory = 1024 * MB;
using NodeCount = uint128_t;

struct HashTable {
    using Key = HashValue;
    using Value = NodeCount;
    static Key makeKey(HashValue hash, int level) {
        constexpr HashValue mixer = 0xd989bcacc137dcd5ull;
        return static_cast<Key>(hash ^ (mixer * level));
    }
    class Entry {
    public:
        Entry() = default;
        constexpr Entry(Key key, Value value) : _key(key), _value(value) {}
        HashValue key() const { return _key; }
        HashValue value() const { return _value; }

    private:
        Key _key = 0;
        Value _value = 0;
    };
    constexpr static size_t kHashTableSize =
        options::cachePerft ? hashTableMemory / sizeof(HashValue) : 1;

    HashTable() {
        table.clear();
        table.resize(kHashTableSize, Entry{});
    }
    const Entry* lookup(HashValue hash, int level) const {
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);
        auto& entry = table[idx];
        return entry.key() == key ? &entry : nullptr;
    }
    void enter(HashValue hash, int level, Value value) {
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);
        auto& entry = table[idx];
        dassert(entry.key() != key || entry.value() == value);
        entry = {key, value};
    };
    std::vector<Entry> table;
};

HashTable hashTable;
NodeCount cached = 0;

std::string pct(double some, double all) {
    double percentage = all > 0 ? some * 100.0 / all : 0.0;
    return all ? " " + std::to_string(percentage) + "%" : "";
}

void error(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}


/**
 *  a debugging function to walk the move generation tree of strictly legal moves to count all the
 *  leaf nodes of a certain depth, which can be compared to predetermined values and used to isolate
 *  bugs. (See https://www.chessprogramming.org/Perft)
 */
NodeCount perft(Board& board, Hash hash, SearchState& state, int depth) {
    if (depth <= 1) return depth == 1 ? countLegalMovesAndCaptures(board, state) : 1;

    dassert(!options::cachePerft || hash == Hash(Position{board, state.turn}));

    // Unlike normal Zobrist hashing, we need to include the level.

    if constexpr (options::cachePerft)
        if (auto entry = hashTable.lookup(hash(), depth))
            return cached += entry->value(), entry->value();

    NodeCount nodes = 0;
    auto newState = state;
    forAllLegalMovesAndCaptures(board, state, [&](Board& board, MoveWithPieces mwp) {
        auto delta = occupancyDelta(mwp.move);
        auto newHash = options::cachePerft ? applyMove(hash, state.turn, mwp) : Hash();
        newState.occupancy = !(state.occupancy ^ delta);
        newState.pawns = find(board, addColor(PieceType::PAWN, !state.active()));
        newState.turn = applyMove(state.turn, mwp);
        newState.kingSquare = *find(board, addColor(PieceType::KING, !state.active())).begin();
        newState.inCheck = isAttacked(board, newState.kingSquare, newState.occupancy);
        newState.pinned = pinnedPieces(board, newState.occupancy, newState.kingSquare);

        auto newNodes = nodes + perft(board, newHash, newState, depth - 1);
        if (newNodes < nodes) error("Node count overflow");
        nodes = newNodes;
    });
    if (options::cachePerft && nodes > 100) hashTable.enter(hash(), depth, nodes);
    return nodes;
}

NodeCount perft(Position position, int depth) {
    auto state = SearchState(position.board, position.turn);
    return perft(position.board, Hash{position}, state, depth);
}

void perftWithDivide(Position position, int depth, NodeCount expectedCount) {
    struct Division {
        Move move;
        NodeCount count;
    };
    std::vector<Division> divisions;

    auto startTime = std::chrono::high_resolution_clock::now();
    NodeCount count = 0;
    for (auto move : allLegalMovesAndCaptures(position.turn, position.board)) {
        auto newCount = perft(applyMove(position, move), depth - 1);
        if (!quiet) std::cout << to_string(move) << ": " << to_string(newCount) << "\n";
        divisions.push_back({move, newCount});
        if (count + newCount < count) error("Node count overflow");
        count += newCount;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto rate = count / (duration.count() / 1000'000.0);  // evals per second
    auto megarate = std::round(rate / 100'000.0) / 10.0;  // mega evals per second

    std::cout << "Nodes searched: " << to_string(count) << "\n";
    if (expectedCount && count != expectedCount)
        error("Expected " + to_string(expectedCount) + " nodes, got " + to_string(count) + ".\n");

    if (quiet) return;

    std::cout << duration.count() / 1000 << " ms @ " << std::fixed << std::setprecision(1)
              << megarate << "M nodes/sec";
    if (options::cachePerft) std::cout << ", " << pct(cached, count) << " cached";
    std::cout << "\n";
}

/**
 * The following are well known perft positions, see https://www.chessprogramming.org/Perft_Results
 */
void testKnownPositions() {
    struct Known {
        std::string fen;
        int depth;
        NodeCount count;
    };
    Known kiwipete = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603};
    Known position3 = {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624};
    Known position4 = {
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333};
    Known position4m = {
        "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4, 422333};
    Known position5 = {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487};
    Known position6 = {
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594};

    std::vector<Known> knownPositions = {
        kiwipete, position3, position4, position4m, position5, position6};

    if (options::cachePerft) {
        Known twoKingsRook = {"4k3/8/8/8/8/8/8/4Kr2 w - - 0 1", 7, 2362837};
        Known twoKings = {"4k3/8/8/8/8/8/8/4K3 w - - 0 1", 10, 122753895};
        knownPositions.emplace_back(twoKingsRook);  // Many repeated positions
        knownPositions.emplace_back(twoKings);      // Too slow without caching
    }

    for (const auto& known : knownPositions) {
        Position position = fen::parsePosition(known.fen);
        NodeCount count = perft(position, known.depth);
        if (count != known.count)
            error("Error: Expected " + to_string(known.count) + " nodes for position '" +
                  known.fen + "' at depth " + std::to_string(known.depth) + ", but got " +
                  to_string(count));

        std::cout << "Passed: " << known.fen << " at depth " << known.depth
                  << ", count: " << to_string(count) << std::endl;
    }
    std::cout << "All known positions passed!\n";
}

/**
 *   Test starting position perft counts, see https://www.chessprogramming.org/Perft_Results.
 */
void testStartingPosition(int depth) {
    struct Known {
        int depth;
        NodeCount count;
    };
    assert(depth > 0);
    std::vector<Known> knownPositions = {{1, "20"_u128},
                                         {2, "400"_u128},
                                         {3, "8'902"_u128},
                                         {4, "197'281"_u128},
                                         {5, "4'865'609"_u128},
                                         {6, "119'060'324"_u128},
                                         {7, "3'195'901'860"_u128},
                                         {8, "84'998'978'956"_u128},
                                         {9, "2'439'530'234'167"_u128},
                                         {10, "69'352'859'712'417"_u128},
                                         {11, "2'097'651'003'696'806"_u128},
                                         {12, "62'854'969'236'701'747"_u128},
                                         {13, "1'981'066'775'000'396'239"_u128}};
    if constexpr (sizeof(NodeCount) == sizeof(uint128_t)) {
        knownPositions[13] = {13, NodeCount("61'885'021'521'585'529'237"_u128)};
        knownPositions[14] = {14, NodeCount("2'015'099'950'053'364'471'960"_u128)};
    }
    NodeCount count = perft(fen::parsePosition(fen::initialPosition), depth);
    if (count != knownPositions[depth - 1].count)
        error("Expected " + to_string(knownPositions[depth - 1].count) +
              " nodes for starting position at depth " + std::to_string(depth) + ", but got " +
              to_string(count));

    if (!quiet)
        std::cout << "Passed starting position at depth " << depth
                  << ", count: " << to_string(count) << "\n";
}

void testTwoKings() {
    if (!options::cachePerft) return;  // This test is too slow without caching
    std::string fen = "8/8/3k4/8/8/8/8/2K5 w - - 4 3";
    Position position = fen::parsePosition(fen);
    NodeCount count = perft(position, 12);
    NodeCount expected = 10'109'930'485;
    if (count != expected)
        error("Expected " + to_string(expected) + " nodes for position '" + fen +
              "' at depth 12, but got " + to_string(count));
}

void testMaxMoves() {
    std::string fen = "R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB w - - 0 1";
    Position position = fen::parsePosition(fen);
    NodeCount moves = perft(position, 1);
    assert(moves == 218);  // Position with maximum number of legal moves
    NodeCount count = perft(position, 5);
    NodeCount expected = 13'853'661;
    if (count != expected)
        error("Expected " + to_string(expected) + " nodes for position '" + fen +
              "' at depth 5, but got " + to_string(count));
}

int selfTests() {
    testStartingPosition(6);
    testKnownPositions();
    testTwoKings();
    testMaxMoves();
    return 0;
}

bool maybeMove(const std::string& str) {
    return !fen::maybeFEN(str) && str[0] >= 'a' && str[0] <= 'h' && str[1] >= '1' &&
        str[1] <= '8' && str[2] >= 'a' && str[2] <= 'h' && str[3] >= '1' && str[3] <= '8';
}

void usage(std::string message) {
    std::cerr << "Error: " << message << "\n";
    std::cerr << "Usage: perft [-q] <depth> [expected-count]" << "\n";
    std::cerr << "Usage: perft [-q] [fen] <depth> [expected-count]" << "\n";
    std::exit(1);
}

void option(const std::string& arg) {
    if (arg == "-q" || arg == "--quiet")
        quiet = true;
    else
        error("Unknown option: " + arg);
}

int main(int argc, char** argv) {
    std::vector<Position> positions;

    auto shift = [&argc, &argv]() -> char* { return --argc, *++argv; };
    auto arg = [&argc, &argv]() -> std::string { return argc < 2 ? "" : std::string(argv[1]); };

    while (arg()[0] == '-') option(shift());

    if (argc < 2) return selfTests();

    while (fen::maybeFEN(arg())) {
        positions.push_back(fen::parsePosition(shift()));
        if (arg() == "moves") shift();

        while (maybeMove(arg())) {
            Move move = fen::parseUCIMove(positions.back().board, shift());
            auto pos = applyMove(positions.back(), move);
            std::cout << "applied move " << to_string(move) << "\n";
            positions.back() = pos;
        }
    }

    if (positions.empty()) positions.push_back(fen::parsePosition(fen::initialPosition));

    if (argc < 2) usage("missing depth argument");

    int depth = std::atoi(shift());
    NodeCount expectedCount = argc > 1 ? strtou128(shift()) : 0;

    for (auto& position : positions) perftWithDivide(position, depth, expectedCount);
    return 0;
}
