#include "book/book.h"
#include "book/pgn/pgn.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "engine/fen/fen.h"
#include "move/move.h"

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace book;

struct BookStats {
    uint64_t gamesProcessed = 0;
    uint64_t gamesAccepted = 0;
};

/** Check if a game meets quality criteria for inclusion in the book */
bool shouldIncludeGame(const pgn::PGN& game) {
    auto timeControl = game["TimeControl"];
    if (timeControl.find("90") == std::string::npos) return false;  // Only 90+ minutes games

    auto whiteElo = game["WhiteElo"];
    auto blackElo = game["BlackElo"];
    if (!whiteElo.empty() && std::stoi(whiteElo) < 2000) return false;  // Skip low-rated games
    if (!blackElo.empty() && std::stoi(blackElo) < 2000) return false;

    return true;
}

/** Add a game to the book entries */
void insertGame(const pgn::VerifiedGame& verifiedGame,
                std::unordered_map<uint64_t, BookEntry>& entries,
                std::unordered_map<uint64_t, std::string>& positions) {
    auto&& [moves, term] = verifiedGame;

    if (term != pgn::Termination::WHITE_WIN && term != pgn::Termination::BLACK_WIN &&
        term != pgn::Termination::DRAW)
        return;  // Only record decided games

    Position position = Position::initial();
    for (auto move : moves) {
        position = moves::applyMove(position, move);
        uint64_t key = Hash(position)();

        auto& entry = entries[key];
        if (term == pgn::Termination::WHITE_WIN) {
            if (entry.white < BookEntry::kMaxResultCount) ++entry.white;
        } else if (term == pgn::Termination::BLACK_WIN) {
            if (entry.black < BookEntry::kMaxResultCount) ++entry.black;
        } else if (term == pgn::Termination::DRAW) {
            if (entry.draw < BookEntry::kMaxResultCount) ++entry.draw;
        }

        if (entry.total() == kMinGames && !positions.count(key)) {
            positions[key] = fen::to_string(position);
        }
    }
}

/** Process a PGN file and add all qualifying games to the book */
BookStats processPGNFile(const std::string& pgnFile,
                         std::unordered_map<uint64_t, BookEntry>& entries,
                         std::unordered_map<uint64_t, std::string>& positions) {
    std::ifstream in(pgnFile);
    if (!in || !in.is_open()) {
        std::cerr << "Could not open PGN file: " << pgnFile << "\n";
        return {};
    }

    BookStats stats;
    while (in) {
        auto game = pgn::readPGN(in);
        ++stats.gamesProcessed;

        if (!shouldIncludeGame(game)) continue;

        auto verifiedGame = pgn::verify(game);
        insertGame(verifiedGame, entries, positions);
        ++stats.gamesAccepted;
    }

    return stats;
}

/** Write book entries to a CSV file */
size_t writeBookCSV(const std::string& csvFile,
                    const std::unordered_map<uint64_t, BookEntry>& entries,
                    const std::unordered_map<uint64_t, std::string>& positions) {
    std::ofstream out(csvFile);
    if (!out || !out.is_open()) {
        std::cerr << "Could not open output file: " << csvFile << "\n";
        return 0;
    }

    out << "fen,white,draw,black\n";
    size_t writtenCount = 0;
    for (const auto& [key, entry] : entries) {
        if (entry.total() >= kMinGames && positions.count(key)) {
            const std::string& fen = positions.at(key);
            out << fen << "," << entry.white << "," << entry.draw << "," << entry.black << "\n";
            ++writtenCount;
        }
    }

    return writtenCount;
}

/** Generate a book CSV file from one or more PGN files */
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input1.pgn> [input2.pgn ...] <output.csv>\n";
        return 1;
    }

    const char* csvFile = argv[argc - 1];
    std::unordered_map<uint64_t, BookEntry> entries;
    std::unordered_map<uint64_t, std::string> positions;

    uint64_t totalGamesProcessed = 0;
    uint64_t totalGamesAccepted = 0;

    // Process each PGN file
    for (int i = 1; i < argc - 1; ++i) {
        const char* pgnFile = argv[i];
        std::cout << "Processing " << pgnFile << "...\n";

        BookStats stats = processPGNFile(pgnFile, entries, positions);
        totalGamesProcessed += stats.gamesProcessed;
        totalGamesAccepted += stats.gamesAccepted;

        std::cout << "  " << stats.gamesAccepted << " games accepted (out of "
                  << stats.gamesProcessed << " total)\n";
    }

    std::cout << "\nTotal: " << totalGamesAccepted << " games (out of " << totalGamesProcessed
              << " total)\n";
    std::cout << "Found " << entries.size() << " unique positions\n";

    size_t writtenCount = writeBookCSV(csvFile, entries, positions);
    std::cout << "Wrote " << writtenCount << " positions with at least " << kMinGames
              << " games to " << csvFile << "\n";

    return writtenCount > 0 ? 0 : 1;
}
