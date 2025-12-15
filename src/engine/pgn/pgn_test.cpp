#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "engine/pgn/pgn.h"

namespace {

bool verbose = false;

/**
 * A fixed-size array indexed by enum values.
 */
template <typename E, typename T, size_t N>
class EnumArray {
public:
    using value_type = T;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = typename std::array<value_type, N>::iterator;
    using const_iterator = typename std::array<value_type, N>::const_iterator;

    constexpr size_t size() const { return N; }

    reference operator[](E e) { return data[static_cast<size_t>(e)]; }
    const_reference operator[](E e) const { return data[static_cast<size_t>(e)]; }

    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

private:
    std::array<T, N> data = {};
};

std::string extractMoves(const std::string& movetext) {
    std::string result;
    pgn::PGN pgn;
    pgn.movetext = movetext;

    for (auto move : pgn)
        if (move) result += std::string(move) + " ";

    if (!result.empty()) result.pop_back();  // Remove trailing space

    return result;
}

int testFromStream(std::istream& in, bool progress) {
    uint64_t gameCount = 0;
    uint64_t moveCount = 0;
    using TerminationArray = EnumArray<pgn::Termination, uint64_t, size_t(pgn::Termination::COUNT)>;
    TerminationArray counts = {};
    using clock = std::chrono::high_resolution_clock;
    auto startTime = clock::now();

    while (auto game = pgn::readPGN(in)) {
        ++gameCount;
        auto [moves, termination] = verify(game);
        moveCount += moves.size();
        counts[termination]++;

        bool valid = termination >= pgn::Termination::WHITE_WIN;

        if (verbose || !valid) {
            std::cout << "\n=== Game " << gameCount << " ===\n";
            std::cout << "  Valid: " << (valid ? "Yes" : "No") << "\n";
            if (!valid)
                std::cout << "  Error at move: \""
                          << game.error(std::next(game.begin(), moves.size())) << "\"\n";

            for (const auto& [tag, value] : game.tags)
                std::cout << "    " << tag << ": " << value << "\n";

            std::cout << "  Movetext length: " << game.movetext.size() << " chars\n";
            std::cout << "  Moves: " << to_string(moves) << "\n";
        }

        if (progress && gameCount % 100 == 0) std::cerr << gameCount << "\r";
    }
    if (progress) std::cerr << gameCount << "\n";
    auto endTime = clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto gameRate = gameCount / (micros.count() / 1'000'000.0);
    auto moveRate = moveCount / (micros.count() / 1'000'000.0);

    if (!gameCount) return std::cout << "No games processed\n", 0;
    if (!progress) return gameCount;

    std::cerr << gameCount << " games (" << gameRate / 1000 << "K games/sec), " << moveCount
              << " moves (" << moveRate / 1000 << "K moves/sec)\n";
    std::cerr << "    " << counts[pgn::Termination::WHITE_WIN] << " white wins, "
              << counts[pgn::Termination::BLACK_WIN] << " black wins, "
              << counts[pgn::Termination::DRAW] << " draws, " << counts[pgn::Termination::UNKNOWN]
              << " unknowns\n";
    std::cerr << "    " << counts[pgn::Termination::NOTATION_ERROR] << " notation errors, "
              << counts[pgn::Termination::MOVE_ERROR] << " move errors, "
              << counts[pgn::Termination::INCOMPLETE_ERROR] << " incomplete errors\n";

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
    // Win for White in 49 moves
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
8. c3 O-O 9. h3 $1 Be6 ; Claude made up a null move here, fixed up myself from here
10. 10. Bxe6 fxe6 11. d4 Qd7 12. Nbd2 exd4 13. cxd4 a5 14. e5 dxe5 15. dxe5 Nd5
16. Ne4 a4 17. Qe2 h6 18. Bd2 a3 19. b3 Rad8 20. Rad1 Qc8 21. Qxb5 Ncb4
22. Bxb4 Bxb4 23. Re2 Nc3 24. Nxc3 Bxc3 25. Rxd8 Rxd8 26. Rc2 Bb2
 27. Kh2 Qd7 28. Qb7 Qf7 29. Qxc7 Qf8 30. Qb6 Qf5 31. Qxd8+ Kh7
 32. Qd1 Bxe5+ 33. Nxe5 Qxe5+ 34. Kg1 Qf5 35. Rc8 e5 36. Qc2 e4
 37. Re8 Kg6 38. Qxe4 Qxe4 39. Rxe4 h5 40. Re1 h4 41. b4 Kf5
 42. b5 g6 43. b6 g5 44. b7 g4 45. b8=Q g3 46. fxg3 hxg3 47.
 Qd6 Kg5 48. Re5+ Kf4 49. Qd4# 1-0
 )PGN";

    // Win for Black by checkmate in 47 moves
    constexpr std::string_view pgn_2 = R"PGN(
[Event "Parser Test"]
[Site "Local"]
[Date "2025.01.02"]
[Round "2"]
[White "WhitePlayer"]
[Black "BlackPlayer"]
[Result "0-1"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Nxe4 (5... Nxe4 6. d4) 6. Re1 b5 7. Bb3 d5 8. d3 Nc5 9. Nxe5 Nxb3 10. axb3 Nxe5 11. c4 Be6 12. Bf4 Ng6 13. Qf3 Nxf4 14. Qxf4 Bd6 15. Qd4 O-O 16. c5 Be7 17. Nc3 c6 18. b4 Bf6 19. Qf4 Bg5 20. Qe5 Qf6 21. g3 Qxe5 22. Rxe5 Bf6 23. Re2 Rfd8 24. Rae1 Bxc3 25. bxc3 d4 26. cxd4 Rxd4 27. Re4 Rxd3 28. R1e3 Rxe3 29. Rxe3 a5 30. Ra3 a4 31. f4 h5 32. h4 Bf5 33. Kf2 Kh7 34. Kf3 Re8 35. Re3 Rxe3+ 36. Kxe3 a3 37. g4 hxg4 38. h5 a2 39. h6 gxh6 40. Kd2 a1=R 41. Ke2 Ra2+ 42. Ke3 g3 43. Kd4 Re2 44. Kc3 g2 45. Kd4 g1=R 46. Kc3 Rg3+ 47. Kd4 Re4#
$4 0-1 {Promotion tactic}
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

1. Nf3 d5 2. g3 c6 3. Bg2 Bf5 4. O-O Nd7 5. d3 h6 6. Nbd2 Ngf6 7. b3 e5 8. Bb2 Bd6 9. e4 dxe4 10. dxe4 Nxe4 11. Nh4 Bh7 (11... Bc5 12. Nxe4 Bxe4 (12... Qxh4)) 12. Nxe4 Bxe4 13. Bxe4 Qxh4 14. Qxd6 Qxe4 15. Rad1 O-O-O 16. Rfe1 Qxc2 17. Bxe5 Nxe5 18. Qxe5 Rhe8 19. Qf5+ Qxf5 20. Rxe8 Qe5 21. Rdxd8+ (21. Re1) (21. Rd7 b5 22. Rxe5 Kxd7 23. a4 bxa4 24. bxa4 c5 25. Rxc5 Kd6 26. Ra5 Rd7 27. Ra6+ Kd5 28. Ra5+ Kd6 29. Ra6+ Kd5 30. Ra5+) 1/2-1/2
{Theoretical draw}

[Event "Parser Test"]
[Site "Local"]
[Date "2025.01.04"]
[Round "4"]
[White "WhitePlayer"]
[Black "BlackPlayer"]
[Result "*"]

e4 e5 f4?! exf4 Nf3 g5 h4 g4 Ne5 h5 Bc4 Rh7 d4 Nf6 Bxf4 Nxe4 O-O g3 Bxf7+ Rxf7 Qxh5 *

; Game abandoned during analysis
)PGN";

    // Test the 4 test cases above
    {
        std::istringstream in((std::string(pgn_1)));
        if (verbose) std::cout << "=== Running basic test 1 ===\n";
        assert(testFromStream(in, false) == 1);
    }
    {
        std::istringstream in((std::string(pgn_2)));
        if (verbose) std::cout << "=== Running basic test 2 ===\n";
        assert(testFromStream(in, false) == 1);
    }
    {
        std::istringstream in((std::string(pgn_3)));
        if (verbose) std::cout << "=== Running basic test 3 ===\n";
        assert(testFromStream(in, false) == 2);
    }
    std::cout << "All basic PGN tests passed!\n";
}

void disambiguationTests() {
    // Below should have 6. Nbd2 or 6. Nfd2, leaving disambiguation out is an error
    std::string_view movetext = "1. Nf3 d5 2. g3 c6 3. Bg2 Bf5 4. O-O Nd7 5. d3 h6 6. Nd2 *";
    pgn::PGN game;
    game.movetext = movetext;
    auto [moves, termination] = verify(game);
    assert(termination == pgn::Termination::MOVE_ERROR);
    std::string errorMsg = game.error(std::next(game.begin(), moves.size()));
    assert(errorMsg.find("Invalid move Nd2") != std::string::npos);
}

void error(int code, const std::string& message) {
    std::cerr << "Error (" << code << "): " << message << "\n";
    std::exit(code);
}

int testFromFile(const char* filename) {
    // If filename is a single dash, read from standard input
    if (std::string(filename) == "-") return testFromStream(std::cin, false);

    std::ifstream file(filename);
    if (!file.is_open()) error(2, "Could not open file: " + std::string(filename));

    if (verbose) std::cout << "=== Running test from file: " << filename << " ===\n";
    return testFromStream(file, !verbose);  // If showing verbose output, no progress needed
}

}  // namespace

int main(int argc, char* argv[]) {
    while (argc > 1 && (std::string(argv[1]) == "-v" || std::string(argv[1]) == "--verbose")) {
        verbose = true;
        --argc;
        ++argv;
    }

    // Always run basic unit tests
    extractMovesTests();
    basicPGNtests();
    disambiguationTests();

    // Run tests from each provided file
    for (int i = 1; i < argc; ++i) testFromFile(argv[i]);

    return 0;
}
