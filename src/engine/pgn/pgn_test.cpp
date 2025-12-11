#include <fstream>
#include <iostream>

#include "engine/pgn/pgn.h"

namespace {
void testFromStream(std::istream& in) {
    constexpr int maxGames = 100;
    int gameCount = 0;

    while (gameCount < maxGames) {
        auto game = pgn::readPGN(in);

        if (!game) {
            if (gameCount == 0) {
                std::cout << "No game read (empty movetext)\n";
            }
            break;
        }

        ++gameCount;
        std::cout << "\n=== Game " << gameCount << " ===\n";
        std::cout << "  Tags: " << game.tags.size() << "\n";
        for (const auto& [tag, value] : game.tags) {
            std::cout << "    " << tag << ": " << value << "\n";
        }
        std::cout << "  Movetext length: " << game.movetext.size() << " chars\n";

        if (!game.movetext.empty()) {
            auto sanMoves = pgn::extractSANMoves(game.movetext);
            std::cout << "  SAN moves: " << sanMoves << "\n";
        }
    }

    std::cout << "\nProcessed " << gameCount << " game(s)\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << argv[1] << "\n";
            return 1;
        }
        testFromStream(file);
    } else {
        testFromStream(std::cin);
    }
    return 0;
}
