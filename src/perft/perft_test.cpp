#include "core/core.h"
#include "core/options.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#include "fen/fen.h"
#include "perft_core.h"

using NodeCount = uint128_t;

void error(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}

// Test configuration - different depths for debug vs optimized builds
#ifdef DEBUG
constexpr bool kDebug = true;
#else
constexpr bool kDebug = false;
#endif

int startingPositionDepth = kDebug ? 4 : 6;  // 197K vs 119M nodes

using DepthAndCount = std::pair<int, NodeCount>;

struct Known {
    std::string_view fen;
    int depth;
    NodeCount count;
    constexpr Known(int depth, NodeCount count)
        : fen(fen::initialPosition), depth(depth), count(count) {}
    constexpr Known(std::string_view fen, int depth, NodeCount count)
        : fen(fen), depth(depth), count(count) {}
    constexpr Known(std::string_view fen, DepthAndCount depthAndCount)
        : fen(fen), depth(depthAndCount.first), count(depthAndCount.second) {}
};

/**
 * The following are well known perft positions, see https://www.chessprogramming.org/Perft_Results
 */
void testKnownPositions() {
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

    // The following position has the maximum initial number of moves (218) possible in chess.
    Known maxMoves = {"R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - - 0 1",
                      kDebug ? DepthAndCount{3, 19'073} : DepthAndCount{5, 13'853'661}};

    // A few positions with very few pieces to test caching of deep perft results and very large
    // total move counts.
    Known twoKings = {"8/8/3k4/8/8/8/8/2K5 w - - 4 3",
                      kDebug ? DepthAndCount{8, 4'656'573} : DepthAndCount{12, 10'109'930'485}};
    Known twoKingsRook = {"4k3/8/8/8/8/8/8/4Kr2 w - - 0 1",
                          kDebug ? DepthAndCount{5, 21'570} : DepthAndCount{7, 2'362'837}};
    Known twoKingsOnly = {"4k3/8/8/8/8/8/8/4K3 w - - 0 1",
                          kDebug ? DepthAndCount{7, 375'049} : DepthAndCount{10, 122'753'895}};

    // All but one moves are check mate, so depth 2 has only one node while depth 1 has 14.
    Known dontMate1 = {"B5KR/1r5B/6R1/2b1p1p1/2P1k1P1/1p2P2p/1P2P2P/3N1N2 w - - 0 1", 1, 14};
    Known dontMate2 = {dontMate1.fen, 2, 1};  // Depth 2 has less nodes due to mates
    Known dontMate3 = {dontMate1.fen, 3, 17};
    Known dontMate6 = {dontMate1.fen, 6, 5'405};

    // There is no freedom of choice, only one legal move at each ply, with only a single choice of
    // promotion at depth 10, though the they all lead to the same forced mate at depth 11.
    Known forcedPlay1 = {"K1k5/P1Pp4/1p1P4/8/p7/P2P4/8/8 w - - 0 1", 9, 1};
    Known forcedPlay9 = {forcedPlay1.fen, 9, 1};    // 1 legal move only
    Known forcedPlay10 = {forcedPlay1.fen, 10, 4};  // 4 promotions
    Known forcedPlay11 = {forcedPlay1.fen, 11, 4};
    Known forcedPlay12 = {forcedPlay1.fen, 12, 0};  // checkmate at 11

    std::vector<Known> knownPositions = {
        kiwipete,
        position3,
        position4,
        position4m,
        position5,
        position6,
        maxMoves,
        dontMate1,
        dontMate2,
        dontMate3,
        dontMate6,
        forcedPlay1,
        forcedPlay9,
        forcedPlay10,
        forcedPlay11,
        forcedPlay12,
    };

    if (options::cachePerft) {
        knownPositions.emplace_back(twoKings);
        knownPositions.emplace_back(twoKingsRook);
        knownPositions.emplace_back(twoKingsOnly);
    }

    for (const auto& known : knownPositions) {
        Position position = fen::parsePosition(std::string(known.fen));
        NodeCount count = perft(position, known.depth);
        if (count != known.count)
            error("Error: Expected " + to_string(known.count) + " nodes for position '" +
                  std::string(known.fen) + "' at depth " + std::to_string(known.depth) +
                  ", but got " + to_string(count));

        std::cout << "Passed: " << known.fen << " at depth " << known.depth
                  << ", count: " << to_string(count) << std::endl;
    }
    std::cout << "All known positions passed!\n";
}

/**
 *   Test starting position perft counts, see https://www.chessprogramming.org/Perft_Results.
 */
void testStartingPosition(int depth) {
    assert(depth > 0 && depth <= 15);
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
        knownPositions.emplace_back(14, NodeCount("61'885'021'521'585'529'237"_u128));
        knownPositions.emplace_back(15, NodeCount("2'015'099'950'053'364'471'960"_u128));
    }
    NodeCount count = perft(fen::parsePosition(fen::initialPosition), depth);
    if (count != knownPositions[depth - 1].count)
        error("Expected " + to_string(knownPositions[depth - 1].count) +
              " nodes for starting position at depth " + std::to_string(depth) + ", but got " +
              to_string(count));

    std::cout << "Passed starting position at depth " << depth << ", count: " << to_string(count)
              << "\n";
}

int main(int argc, char** argv) {
    if (argc == 2) startingPositionDepth = std::stoi(argv[1]);  // optional depth argument

    if (startingPositionDepth < 1 || startingPositionDepth > 15)
        error("Depth must be between 1 and 15");

    testStartingPosition(startingPositionDepth);
    testKnownPositions();
    std::cout << "All perft tests passed!\n";
    return 0;
}