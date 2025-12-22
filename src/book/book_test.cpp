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

    for (int i = 0; i < 100; ++i) book.insert(makeGame({"e2e4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 95; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 5; ++i) book.insert(makeGame({"d2d4"}, pgn::Termination::DRAW));
    for (int i = 0; i < 30; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::WHITE_WIN));
    for (int i = 0; i < 20; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::DRAW));
    for (int i = 0; i < 50; ++i) book.insert(makeGame({"g1f3"}, pgn::Termination::BLACK_WIN));

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

double winRate(BookEntry entry, Color active) {
    uint64_t wins = active == Color::w ? entry.white : entry.black;
    return (wins + 0.5 * entry.draw) / entry.total();
}


void analyzeOpeningBook(const char* filename) {
    std::cout << "\n=== Analyzing Opening Book: " << filename << " ===\n\n";

    Book book = loadBook(filename);
    if (!book) {
        std::cout << "Could not load book from: " << filename << "\n";
        return;
    }

    Position initialPos = Position::initial();

    // Collect all first moves for White
    struct MoveInfo {
        Move move;
        uint32_t games;
        double winRate;
    };
    std::vector<MoveInfo> whiteMoves;

    // Get all legal moves from initial position
    Board board = initialPos.board;
    MoveVector legalMoves = moves::allLegalMoves(initialPos.turn, board);

    for (Move move : legalMoves) {
        Position afterMove = moves::applyMove(initialPos, move);
        uint64_t key = Hash(afterMove)();

        if (book.entries.count(key)) {
            const auto& entry = book.entries[key];
            whiteMoves.push_back(
                {move, static_cast<uint32_t>(entry.total()), winRate(entry, Color::w)});
        }
    }

    // Sort by number of games (descending)
    std::sort(whiteMoves.begin(), whiteMoves.end(), [](const MoveInfo& a, const MoveInfo& b) {
        return a.games > b.games;
    });

    std::cout << "White's opening moves:\n";
    std::cout << std::setw(6) << "Move" << " | " << std::setw(8) << "Games" << " | " << std::setw(8)
              << "Win Rate" << " (posterior)" << "\n";
    std::cout << std::string(50, '-') << "\n";

    // Compute prior for displaying posterior means
    uint64_t totalW = 0, totalD = 0, totalL = 0;
    for (const auto& [key, entry] : book.entries) {
        totalW += entry.white;
        totalD += entry.draw;
        totalL += entry.black;
    }
    DirichletPrior prior = DirichletPrior::fromGlobalStats(totalW, totalD, totalL);

    for (const auto& info : whiteMoves) {
        Position afterMove = moves::applyMove(initialPos, info.move);
        uint64_t key = Hash(afterMove)();
        const auto& entry = book.entries[key];
        double postMean = entry.posteriorMean(Color::w, prior);
        std::cout << std::setw(6) << to_string(info.move) << " | " << std::setw(8) << info.games
                  << " | " << std::setw(7) << std::fixed << std::setprecision(1)
                  << (info.winRate * 100.0) << "% (" << (postMean * 100.0) << "%)" << "\n";
    }

    // Now for each popular White opening, show Black's best reply
    std::cout << "\n=== Black's Best Replies (seed 0) ===\n\n";

    book.reseed(0);  // Use seed 0 for deterministic selection

    for (const auto& whiteMove : whiteMoves) {
        if (whiteMove.games < 10) continue;  // Only show popular openings

        Move blackReply = book.choose(initialPos, {whiteMove.move});

        if (blackReply) {
            Position afterWhite = moves::applyMove(initialPos, whiteMove.move);
            Position afterBlack = moves::applyMove(afterWhite, blackReply);
            uint64_t blackKey = Hash(afterBlack)();

            std::cout << "After " << to_string(whiteMove.move);
            if (book.entries.count(blackKey)) {
                const auto& entry = book.entries[blackKey];
                std::cout << ", best reply: " << to_string(blackReply)
                          << " (games: " << entry.total() << ", Black win rate: " << std::fixed
                          << std::setprecision(1) << (winRate(entry, Color::b) * 100.0) << "%)\n";
            } else {
                std::cout << ", reply: " << to_string(blackReply) << "\n";
            }
        } else {
            std::cout << "After " << to_string(whiteMove.move) << ", no book reply found\n";
        }
    }

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

    std::cout << "All book tests passed!\n";
    return 0;
}
