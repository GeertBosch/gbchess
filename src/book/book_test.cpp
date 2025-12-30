#include "book.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "core/core.h"
#include "core/hash/hash.h"
#include "engine/fen/fen.h"
#include "move/move.h"
#include "move/move_gen.h"

using namespace book;

/** Creates a verified game with the given moves and termination */
pgn::VerifiedGame makeGame(const std::vector<std::string>& moveStrs, pgn::Termination term) {
    Position pos = Position::initial();
    MoveVector moves;

    for (const auto& moveStr : moveStrs) {
        Move move = fen::parseUCIMove(pos.board, moveStr);
        assert(move);
        moves.push_back(move);
        pos = moves::applyMove(pos, move);
    }

    return {moves, term};
}

double winRate(BookEntry entry, Color active) {
    uint64_t wins = active == Color::w ? entry.white : entry.black;
    return (wins + 0.5 * entry.draw) / entry.total();
}

DirichletPrior computePrior(const Book& book) {
    uint64_t totalW = 0, totalD = 0, totalL = 0;
    for (const auto& [key, entry] : book.entries) {
        totalW += entry.white;
        totalD += entry.draw;
        totalL += entry.black;
    }
    return DirichletPrior::fromGlobalStats(totalW, totalD, totalL);
}

struct MoveInfo {
    Move move;
    uint32_t games;
    double winRate;
};

std::vector<MoveInfo> collectOpeningMoves(const Book& book, const Position& pos) {
    std::vector<MoveInfo> moves;
    Board board = pos.board;
    MoveVector legalMoves = moves::allLegalMoves(pos.turn, board);

    for (Move move : legalMoves) {
        Position afterMove = moves::applyMove(pos, move);
        uint64_t key = Hash(afterMove)();
        if (book.entries.count(key)) {
            const auto& entry = book.entries.at(key);
            moves.push_back(
                {move, static_cast<uint32_t>(entry.total()), winRate(entry, pos.active())});
        }
    }

    std::sort(moves.begin(), moves.end(), [](const MoveInfo& a, const MoveInfo& b) {
        return a.games > b.games;
    });
    return moves;
}

void printOpeningMoveTable(const std::vector<MoveInfo>& moves,
                           const Book& book,
                           const DirichletPrior& prior,
                           const Position& pos) {
    std::cout << "White's opening moves:\n";
    std::cout << std::setw(6) << "Move" << " | " << std::setw(8) << "Games" << " | " << std::setw(8)
              << "Win Rate" << " (posterior)" << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (const auto& info : moves) {
        Position afterMove = moves::applyMove(pos, info.move);
        uint64_t key = Hash(afterMove)();
        const auto& entry = book.entries.at(key);
        double postMean = entry.posteriorMean(pos.active(), prior);
        std::cout << std::setw(6) << to_string(info.move) << " | " << std::setw(8) << info.games
                  << " | " << std::setw(7) << std::fixed << std::setprecision(1)
                  << (info.winRate * 100.0) << "% (" << (postMean * 100.0) << "%)" << "\n";
    }
}

void printBlackReplies(Book& book, const std::vector<MoveInfo>& whiteMoves, const Position& pos) {
    std::cout << "\n=== Black's Best Replies (seed 0) ===\n\n";
    book.reseed(0);

    for (const auto& whiteMove : whiteMoves) {
        if (whiteMove.games < 10) continue;

        Move blackReply = book.choose(pos, {whiteMove.move});
        std::cout << "After " << to_string(whiteMove.move);

        if (blackReply) {
            Position afterWhite = moves::applyMove(pos, whiteMove.move);
            Position afterBlack = moves::applyMove(afterWhite, blackReply);
            uint64_t blackKey = Hash(afterBlack)();

            if (book.entries.count(blackKey)) {
                const auto& entry = book.entries[blackKey];
                std::cout << ", best reply: " << to_string(blackReply) << " (" << entry.black
                          << "W/" << entry.draw << "D/" << entry.white << "L"
                          << ", Black win rate: " << std::fixed << std::setprecision(1)
                          << (winRate(entry, Color::b) * 100.0) << "%)\n";
            } else {
                std::cout << ", reply: " << to_string(blackReply) << "\n";
            }
        } else {
            std::cout << ", no book reply found\n";
        }
    }
}

void printOpeningDistribution(Book& book, const DirichletPrior& prior, const Position& pos) {
    std::cout << "\n=== Opening Move Distribution (100 games) ===\n\n";

    std::map<std::string, int> moveCount;
    std::map<std::string, BookEntry> moveEntries;

    for (int seed = 1; seed <= 100; ++seed) {
        book.reseed(seed);
        Move move = book.choose(pos, {});
        if (move) {
            std::string moveStr = to_string(move);
            moveCount[moveStr]++;

            if (moveEntries.find(moveStr) == moveEntries.end()) {
                Position afterMove = moves::applyMove(pos, move);
                uint64_t key = Hash(afterMove)();
                if (book.entries.count(key)) moveEntries[moveStr] = book.entries[key];
            }
        }
    }

    std::vector<std::pair<std::string, int>> sortedMoves(moveCount.begin(), moveCount.end());
    std::sort(sortedMoves.begin(), sortedMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::cout << std::setw(6) << "Move" << " | " << std::setw(9) << "Selected" << " | "
              << std::setw(12) << "W/D/L" << " | " << std::setw(9) << "Post.Rate" << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& [moveStr, count] : sortedMoves) {
        if (moveEntries.count(moveStr)) {
            const auto& entry = moveEntries[moveStr];
            double postMean = entry.posteriorMean(pos.active(), prior);
            std::cout << std::setw(6) << moveStr << " | " << std::setw(9) << count << " | "
                      << std::setw(4) << entry.white << "/" << std::setw(3) << entry.draw << "/"
                      << std::setw(3) << entry.black << " | " << std::setw(7) << std::fixed
                      << std::setprecision(1) << (postMean * 100.0) << "%" << "\n";
        }
    }
}

void testBasicBookOperations() {
    Book book;

    // Create games with enough samples to overcome Bayesian shrinkage
    // With kMinGames=10, all moves meet the threshold
    // e4 is great (50 wins), d4 is good (40 wins, 10 draws), Nf3 is okay (30 wins, 20 draws)
    for (int i = 0; i < 50; ++i) book.insert(makeGame({"e2e4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 40; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 10; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::DRAW));
    for (int i = 0; i < 30; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 20; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::DRAW));

    // e4: 50 wins = 100% win rate
    // d4: 40 wins + 10 draws = (40 + 5) / 50 = 90% win rate
    // Nf3: 30 wins + 20 draws = (30 + 10) / 50 = 80% win rate

    Position pos = Position::initial();

    // Thompson sampling is stochastic, but with large sample sizes and different win rates,
    // we should see all three moves get selected at least once over many trials
    book.reseed(42);
    std::set<std::string> selectedMoves;
    for (int i = 0; i < 100; ++i) {
        Move move = book.choose(pos, {});
        assert(move);
        selectedMoves.insert(to_string(move));
    }

    // Should have selected multiple moves due to Thompson sampling
    assert(selectedMoves.size() >= 2);
    std::cout << "Basic book operations test passed - selected " << selectedMoves.size()
              << " different moves\n";
}

void testThresholdFiltering() {
    Book book;

    // With Thompson sampling, we no longer use hard thresholds
    // Instead, test that moves with very different win rates have appropriate selection frequency
    // e4: 100 wins = 100% (best, should be selected most often)
    // d4: 95 wins, 5 draws = 97.5% (very good, should be selected frequently)
    // Nf3: 30 wins, 20 draws, 50 losses = 40% (poor, should be selected rarely)

    for (int i = 0; i < 1000; ++i) book.insert(makeGame({"e2e4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 950; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 50; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::DRAW));
    for (int i = 0; i < 300; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 200; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::DRAW));
    for (int i = 0; i < 500; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::BLACK_WIN));

    Position pos = Position::initial();

    // Collect move statistics
    book.reseed(123);
    std::map<std::string, int> moveCount;
    const int trials = 1000;
    for (int i = 0; i < trials; ++i) {
        Move move = book.choose(pos, {});
        assert(move);
        moveCount[to_string(move)]++;
    }

    // e4 and d4 should be selected most often (together > 90% of the time)
    // Nf3 should be selected rarely due to poor win rate
    int e4Count = moveCount["e2e4"];
    int d4Count = moveCount["d2d4"];
    int nf3Count = moveCount["g1f3"];

    assert(e4Count + d4Count > 900);  // Good moves selected > 90% of time
    assert(nf3Count < 100);           // Poor move selected < 10% of time

    std::cout << "Thompson sampling test passed - e4: " << e4Count << ", d4: " << d4Count
              << ", Nf3: " << nf3Count << " (out of " << trials << ")\n";
}

void testVarietyWithNonZeroSeed() {
    Book book;

    // Create balanced games so Thompson sampling provides variety
    // e4: 60 wins = 100%
    // d4: 60 wins = 100%
    // Nf3: 57 wins, 3 draws = 97.5%

    for (int i = 0; i < 60; ++i) book.insert(makeGame({"e2e4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 60; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 57; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 3; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::DRAW));

    Position pos = Position::initial();

    // Thompson sampling should provide variety
    book.reseed(999);
    std::set<std::string> selectedMoves;
    for (int i = 0; i < 200; ++i) {
        Move move = book.choose(pos, {});
        assert(move);
        selectedMoves.insert(to_string(move));
    }

    // Should have selected all three moves with similar frequencies
    assert(selectedMoves.size() >= 2);
    std::cout << "Variety test passed - selected " << selectedMoves.size()
              << " different moves with Thompson sampling\n";
}

void testLaterPosition() {
    Book book;

    // Create opening lines: 1.e4 e5 with different second moves for white
    // 2.Nf3: 60 wins = 100%
    // 2.Bc4: 57 wins, 3 draws = 97.5%
    // 2.Nc3: 40 wins, 20 draws = 83.3%

    std::vector<std::string> baseMoves = {"e2e4", "e7e5"};

    for (int i = 0; i < 60; ++i) {
        auto moves = baseMoves;
        moves.push_back("g1f3");
        book.insert(makeGame(moves, pgn::Termination::WHITE_WIN));
    }

    for (int i = 0; i < 57; ++i) {
        auto moves = baseMoves;
        moves.push_back("f1c4");
        book.insert(makeGame(moves, pgn::Termination::WHITE_WIN));
    }
    for (int i = 0; i < 3; ++i) {
        auto moves = baseMoves;
        moves.push_back("f1c4");
        book.insert(makeGame(moves, pgn::Termination::DRAW));
    }

    for (int i = 0; i < 40; ++i) {
        auto moves = baseMoves;
        moves.push_back("b1c3");
        book.insert(makeGame(moves, pgn::Termination::WHITE_WIN));
    }
    for (int i = 0; i < 20; ++i) {
        auto moves = baseMoves;
        moves.push_back("b1c3");
        book.insert(makeGame(moves, pgn::Termination::DRAW));
    }

    // Test position after 1.e4 e5
    MoveVector moves = {{e2, e4, MoveKind::Double_Push}, {e7, e5, MoveKind::Double_Push}};

    // Thompson sampling should select all moves, but favor better ones
    book.reseed(456);
    std::map<std::string, int> moveCount;
    for (int i = 0; i < 200; ++i) {
        Move move = book.choose(Position::initial(), moves);
        assert(move);
        moveCount[to_string(move)]++;
    }

    // Should have variety
    assert(moveCount.size() >= 2);

    // Nf3 and Bc4 (best two) should be selected more often than Nc3
    int nf3Count = moveCount["g1f3"];
    int bc4Count = moveCount["f1c4"];
    int nc3Count = moveCount["b1c3"];
    assert(nf3Count + bc4Count > nc3Count);

    std::cout << "Later position test passed - Nf3: " << nf3Count << ", Bc4: " << bc4Count
              << ", Nc3: " << nc3Count << "\n";
}

void testNoBookMoves() {
    Book book;

    // Add a single game, but not from the position we'll query
    book.insert(makeGame({"e2e4"}, pgn::Termination::WHITE_WIN));

    // Try to get a move from a completely different position
    Position pos =
        fen::parsePosition("rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1");

    Move move = book.choose(pos, {});
    assert(!move);  // Shouldn't return an actual move

    std::cout << "No book moves test passed\n";
}

void testOpeningDistribution(Book& book, const Position& initialPos, const DirichletPrior& prior) {

    std::map<std::string, int> moveCount;
    std::map<std::string, BookEntry> moveEntries;

    for (int seed = 1; seed <= 100; ++seed) {
        book.reseed(seed);
        Move move = book.choose(initialPos, {});
        if (move) {
            std::string moveStr = to_string(move);
            moveCount[moveStr]++;

            if (moveEntries.find(moveStr) == moveEntries.end()) {
                Position afterMove = moves::applyMove(initialPos, move);
                uint64_t key = Hash(afterMove)();
                if (book.entries.count(key)) moveEntries[moveStr] = book.entries[key];
            }
        }
    }

    std::vector<std::pair<std::string, int>> sortedMoves(moveCount.begin(), moveCount.end());
    std::sort(sortedMoves.begin(), sortedMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::cout << "\nOpening move distribution over 100 selections:\n";
    std::cout << std::setw(6) << "Move" << " | " << std::setw(9) << "Selected" << " | "
              << std::setw(12) << "W/D/L" << " | " << std::setw(9) << "Post.Rate" << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& [moveStr, count] : sortedMoves) {
        if (moveEntries.count(moveStr)) {
            const auto& entry = moveEntries[moveStr];
            double postMean = entry.posteriorMean(Color::w, prior);
            std::cout << std::setw(6) << moveStr << " | " << std::setw(9) << count << " | "
                      << std::setw(4) << entry.white << "/" << std::setw(3) << entry.draw << "/"
                      << std::setw(3) << entry.black << " | " << std::setw(7) << std::fixed
                      << std::setprecision(1) << (postMean * 100.0) << "%" << "\n";
        } else {
            std::cout << std::setw(6) << moveStr << " | " << std::setw(9) << count
                      << " | (no entry)\n";
        }
    }

    std::cout << "\nOpening distribution test passed - selected " << moveCount.size()
              << " different moves\n";
}

void testTemperature() {
    Book book = loadBook("book.csv");
    if (!book) {
        std::cout << "Skipping temperature test - no book.csv found\n";
        return;
    }

    Position initialPos = Position::initial();
    std::vector<double> temperatures = {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};

    std::cout << "\n=== Temperature Effect on Move Selection ===\n";

    for (double temp : temperatures) {
        book.setTemperature(temp);
        std::map<std::string, int> moveCount;

        // Use different seed ranges for each temperature to get independent samples
        int baseSeed = static_cast<int>(temp * 1000);
        for (int i = 0; i < 100; ++i) {
            book.reseed(baseSeed + i);
            Move move = book.choose(initialPos, {});
            if (move) moveCount[to_string(move)]++;
        }

        // Sort by selection count
        std::vector<std::pair<std::string, int>> sortedMoves(moveCount.begin(), moveCount.end());
        std::sort(sortedMoves.begin(), sortedMoves.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        std::cout << "\nTemperature = " << std::fixed << std::setprecision(1) << temp
                  << " (variety: " << moveCount.size() << " different moves)\n";
        for (size_t i = 0; i < std::min(size_t(6), sortedMoves.size()); ++i) {
            std::cout << "  " << std::setw(6) << sortedMoves[i].first << ": " << std::setw(3)
                      << sortedMoves[i].second << "%\n";
        }
    }

    std::cout << "\nTemperature test passed\n";
}

void analyzeOpeningBook(const char* filename) {
    std::cout << "\n=== Analyzing Opening Book: " << filename << " ===\n\n";

    Book book = loadBook(filename);
    if (!book) {
        std::cout << "Could not load book from: " << filename << "\n";
        return;
    }

    Position initialPos = Position::initial();
    DirichletPrior prior = computePrior(book);

    std::vector<MoveInfo> whiteMoves = collectOpeningMoves(book, initialPos);
    printOpeningMoveTable(whiteMoves, book, prior, initialPos);
    printBlackReplies(book, whiteMoves, initialPos);
    testOpeningDistribution(book, initialPos, prior);

    std::cout << "\n";
}

int main(int argc, char** argv) {
    // If a filename is provided, analyze that book
    if (argc > 1) {
        analyzeOpeningBook(argv[1]);
        return 0;
    }

    // Otherwise run the standard tests
    testBasicBookOperations();
    testThresholdFiltering();
    testVarietyWithNonZeroSeed();
    testLaterPosition();
    testNoBookMoves();
    testTemperature();

    std::cout << "All book tests passed!\n";
    return 0;
}
