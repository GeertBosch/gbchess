#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include "engine/pgn/pgn.h"

namespace {

bool verbose = false;

std::string extractMoves(const std::string& movetext) {
    std::string result;
    pgn::PGN pgn;
    pgn.movetext = movetext;

    for (auto move : pgn)
        if (move) result += std::string(move) + " ";

    if (!result.empty()) result.pop_back();  // Remove trailing space

    return result;
}

int testFromStream(std::istream& in) {
    int gameCount = 0;

    while (auto game = pgn::readPGN(in)) {

        ++gameCount;
        auto moves = extractMoves(game.movetext);
        auto terminator = game.terminator();
        auto nextAfterTerminator = terminator;
        if (nextAfterTerminator != game.end()) ++nextAfterTerminator;
        bool valid = terminator != game.end() && *terminator && nextAfterTerminator == game.end();

        if (verbose || !valid) {
            std::cout << "\n=== Game " << gameCount << " ===\n";
            std::cout << "  Valid: " << (valid ? "Yes" : "No") << "\n";
            if (!valid && terminator != game.end())
                std::cout << "  Error at move: \"" << game.error(terminator) << "\"\n";
            if (!valid) std::cout << "  Tags: " << game.tags.size() << "\n";

            for (const auto& [tag, value] : game.tags)
                std::cout << "    " << tag << ": " << value << "\n";

            std::cout << "  Movetext length: " << game.movetext.size() << " chars\n";
            std::cout << "  Moves: " << moves << "\n";
        }

        if (!verbose && gameCount % 100 == 0) std::cerr << gameCount << "\r";
    }

    if (verbose) {
        if (gameCount)
            std::cout << "Processed " << gameCount << " game(s)\n";
        else
            std::cout << "No game read (empty movetext)\n";
    } else {
        std::cerr << "Processed " << gameCount << " game(s)\n";
    }
    return gameCount;
}

void extractMovesTests() {
    assert(extractMoves(" 0-0\n\n") == "");  // Really should be ""
    assert(extractMoves("") == "");
    assert(extractMoves("1. e4 e5 2. Nf3 Nc6") == "e4 e5 Nf3 Nc6");
    assert(extractMoves("1. e4! e5? 2. Nf3!! Nc6") == "e4 e5 Nf3 Nc6");
    assert(extractMoves("1. e4 {Best by test} e5 2. Nf3 Nc6") == "e4 e5 Nf3 Nc6");
    assert(extractMoves("1. e4 e5 2. Nf3 (2. d4 exd4) Nc6") == "e4 e5 Nf3 Nc6");
    assert(extractMoves("1. e4 e5 2. Nf3 Nxe4?! 3.Re1 b5 Bb3 d5") ==
           "e4 e5 Nf3 Nxe4 Re1 b5 Bb3 d5");
    std::cout << "All extractSANMoves tests passed!\n";
}

void basicPGNtests() {
    // Win for White in 31 moves
    constexpr std::string_view pgn_1 = R"PGN(
[Event "Parser Test"]
[Site "Local"]
[Date "2025.01.01"]
[Round "1"]
[White "WhitePlayer"]
[Black "BlackPlayer"]
[Result "1-0"]

1. e4! e5 2. Nf3 Nc6 3. Bb5 a6
(3... Nf6 4. O-O (4. d3?! Be7) Be7)
4. Ba4 Nf6 5. O-O Be7
6. Re1 b5 7. Bb3 d6
8. c3 O-O 9. h3 $1
10. d4 Nbd7 11. c4!? exd4
12. Nxd4 Bb7 13. Nc3 b4
14. Nd5 Nxe4 15. Rxe4 Bxd5
16. Bxd5 c6 17. Bxf7+ Rxf7
18. Nxc6 Qb6 19. Nxe7+ Rxe7
20. Qb3+ Kh8 21. Be3 Qc6
22. Bd5 Qc7 23. Rac1 Qb8
24. Rc6 a5 25. Rb6 Qc7
26. Qxb5 Rb8 27. Rxb8+ Qxb8
28. Qxa5 Qxb2 29. Qa8+ Nf8
30. Qc6 Qb4 31. Qe8 1-0)PGN";

    // Win for Black by checkmate in 39 moves
    constexpr std::string_view pgn_2 = R"PGN(
[Event "Parser Test"]
[Site "Local"]
[Date "2025.01.02"]
[Round "2"]
[White "WhitePlayer"]
[Black "BlackPlayer"]
[Result "0-1"]

e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6
O-O Nxe4?! (Nxe4 d4 $2)
Re1 b5 Bb3 d5 d3 Nc5
Nxe5 Nxb3 axb3 dxe4
d4 exd3 Qxd3 Be6 Qb5+ axb5
Rxa8 Qxa8 Qxb5+ Qxb5
axb5 Nb4 Na3 Nd3 Rd1 Bxb3
Rxd3 Bc4 Nxc4 dxc4 Rd8+ Rxd8
Nxd8 Kxd8 b4 c3 b5 c2
bxa6 c1=Q+ Rxc1 bxc1=Q# 0-1

$4 {Promotion tactic}
)PGN";

    // Two concatenated games in one PGN file:
    //   - Drawn game in 47 moves
    //   - Ongoing game after 8 moves
    constexpr std::string_view pgn_3 = R"PGN(
[Event "Parser Test"]
[Site "Local"]
[Date "2025.01.03"]
[Round "3"]
[White "WhitePlayer"]
[Black "BlackPlayer"]
[Result "1/2-1/2"]

1. Nf3 d5 2. g3 c6 3. Bg2 Bf5 4. O-O Nd7 5. d3 h6 6. Nbd2 Ngf6 7. b3 e5 8. Bb2 Bd6 9. e4 dxe4 10. dxe4 Nxe4 11. Nh4 Bh7 (11... Bc5 12. Nxe4 Bxe4 (12... Qxh4?!)) 12. Nxe4 Bxe4 13. Bxe4 Qxh4 14. Qxd6 Qxe4 15. Rad1 O-O-O 16. Rfe1 Qxc2 17. Bxe5 Nxe5 18. Qxe5 Rhe8 19. Qf5+ Qxf5 20. gxf5 Rxd1 21. Rxd1 Nf3+ 22. Kg2 Ng5 23. h4 Ne4 24. Rd4 Nf6 25. Kf3 Re5 26. g4 c5 27. Rd6 Kc7 28. Rd2 c4 29. bxc4 bxc4 30. Rd4 Rc5 31. Ke3 Ra5 32. Rxc4+ Kd6 33. a4 Rc5 34. Rb4 Nd5+ 35. Kd4 Nxb4 36. cxb4 Rc2 37. b5 axb5 38. axb5 Rb2 39. b6 Rxb6 40. g5 hxg5 41. hxg5 Rb4+ 42. Ke3 Ke5 43. f6 gxf6 44. g6 fxg6 45. fxg6 Ke6 46. g7 Kf7 47. g8=Q+ Kxg8 1/2-1/2
{Theoretical draw}

[Event "Parser Test"]
[Site "Local"]
[Date "2025.01.04"]
[Round "4"]
[White "WhitePlayer"]
[Black "BlackPlayer"]
[Result "*"]

e4 e5 f4?! exf4 Nf3 g5 h4 g4 Ne5 h5 Bc4 Rh7 d4 Nf6 Bxf4 Nxe4 O-O g3 Bxf7+ Rxf7 Qxh5 Rxh5 Bg5 Rxg5 hxg5 Qxg5 Nc3 Qe3+ Kh2 Qg3+ Kg1 Qe3+ Kh2 Qg3+ Kg1 *

; Game abandoned during analysis
)PGN";

    // Test the 4 test cases above
    {
        std::istringstream in((std::string(pgn_1)));
        if (verbose) std::cout << "=== Running basic test 1 ===\n";
        assert(testFromStream(in) == 1);
    }
    {
        std::istringstream in((std::string(pgn_2)));
        if (verbose) std::cout << "=== Running basic test 2 ===\n";
        assert(testFromStream(in) == 1);
    }
    {
        std::istringstream in((std::string(pgn_3)));
        if (verbose) std::cout << "=== Running basic test 3 ===\n";
        assert(testFromStream(in) == 2);
    }
    std::cout << "All basic PGN tests passed!\n";
}

void error(int code, const std::string& message) {
    std::cerr << "Error (" << code << "): " << message << "\n";
    std::exit(code);
}

int testFromFile(const char* filename) {
    // If filename is a single dash, read from standard input
    if (std::string(filename) == "-") return testFromStream(std::cin);

    std::ifstream file(filename);
    if (!file.is_open()) error(2, "Could not open file: " + std::string(filename));

    if (verbose) std::cout << "=== Running test from file: " << filename << " ===\n";
    return testFromStream(file);
}

}  // namespace

int main(int argc, char* argv[]) {
    while (argc > 1 && (std::string(argv[1]) == "-v" || std::string(argv[1]) == "--verbose")) {
        verbose = true;
        --argc;
        ++argv;
    }

    // Alwyas run basic tests
    basicPGNtests();
    extractMovesTests();

    // Run tests from each provided file
    for (int i = 1; i < argc; ++i) testFromFile(argv[i]);

    return 0;
}
