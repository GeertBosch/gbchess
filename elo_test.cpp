#include "elo.h"

#include <cassert>
#include <iostream>

void testBasicELO() {
    const ELO player;
    assert(player() == ELO::kInitialRating);
    std::cout << "Initial player rating: " << player() << "\n";

    // Simulate a win against an opponent rated 400 points higher
    ELO opponent(ELO::kInitialRating + 400);
    assert(opponent() == ELO::kInitialRating + 400);
    ELO playerAfterWin = player;
    playerAfterWin.updateOne(opponent, ELO::WIN);
    std::cout << "After win over opponent rated " << opponent() << ": " << playerAfterWin() << "\n";
    assert(playerAfterWin() > player());
    assert(playerAfterWin() < opponent());

    // Simulate both a win and a loss over the stronger opponent
    ELO playerAfterWinAndLoss = playerAfterWin;
    playerAfterWinAndLoss.updateOne(opponent, ELO::LOSS);
    std::cout << "After subsequent loss against this opponent : " << playerAfterWinAndLoss()
              << "\n";
    assert(playerAfterWinAndLoss() > player());  // Should still be higher than initial rating
    assert(playerAfterWinAndLoss() < playerAfterWin());  // Should be lower than after the win
}

void testWin25PercentAgainstOne() {
    const int kNumRounds = 100;
    const int kOpponentDiff = 400;
    const int kOpponentRating = ELO::kInitialRating + kOpponentDiff;
    ELO player;

    // After a player wins 25% of the time against a given opponent rated 400 points higher,
    // their rating difference should converge to about 200 points.
    ELO opponent(kOpponentRating);  // Same opponent every time
    for (int round = 0; round < kNumRounds; ++round) {
        player.updateBoth(opponent, round % 4 == 0 ? ELO::WIN : ELO::LOSS);
    }

    std::cout << "After winning 25% of " << kNumRounds << " rounds against a " << kOpponentRating
              << " rated opponens: " << player() << " vs " << opponent() << "\n";
    // No ELO points should get lost
    assert(player() + opponent() == ELO::kInitialRating + kOpponentRating);

    // At a 25% win rate, expect to be 200 points below the opponent
    const int kExpectedDiff = -200;
    int diff = player() - opponent();
    assert(diff > kExpectedDiff - ELO::K);
    assert(diff < kExpectedDiff + ELO::K);
}

void testWinDrawLoseDrawAgainstOne() {
    const int kNumRounds = 100;
    const int kOpponentDiff = -100;
    const int kOpponentRating = ELO::kInitialRating + kOpponentDiff;
    ELO player;

    // After a player sequentially wins/draws/loses/draws many times against an opponent rated 100
    // points lower, their rating should converge toward their average.
    ELO opponent(kOpponentRating);  // New opponent every time
    for (int round = 0; round < kNumRounds; ++round) {
        player.updateBoth(opponent,
                          round % 4 == 0       ? ELO::WIN
                              : round % 2 == 1 ? ELO::DRAW
                                               : ELO::LOSS);
    }

    std::cout << "After win/draw/lose/draw " << kNumRounds << " rounds against " << kOpponentRating
              << " rated opponents: " << player() << " vs " << opponent() << "\n";
    assert(player() + opponent() == ELO::kInitialRating + kOpponentRating);
    assert(player() < ELO::kInitialRating);  // Lost some
    const int kExpectedDiff = kOpponentDiff / 2;
    int diff = player() - opponent();
    assert(diff > -ELO::K);
    assert(diff < +ELO::K);
}

void testWin50PercentAgainstMany() {
    const int kNumRounds = 100;
    const int kOpponentRating = ELO::kInitialRating + 400;
    ELO player;

    // After a player wins half of the time against opponents rated 400 points higher,
    // their rating should converge toward the opponent rating.
    for (int round = 0; round < kNumRounds; ++round) {
        ELO opponent(kOpponentRating);  // New opponent every time
        player.updateOne(opponent, round % 2 == 0 ? ELO::WIN : ELO::LOSS);
    }

    std::cout << "After winning 50% of " << kNumRounds << " rounds against " << kOpponentRating
              << " rated opponents: " << player() << "\n";
    assert(player() > ELO::kInitialRating);
    assert(player() > kOpponentRating - ELO::K);
    assert(player() < kOpponentRating + ELO::K);
}

void testWin90PercentAgainstMany() {
    const int kNumRounds = 250;
    const int kOpponentRating = 1600;
    ELO player;

    // Simulate a player who wins 90% of the time against opponents at a 1600 rating.
    for (int round = 0; round < kNumRounds; ++round) {
        ELO opponent(kOpponentRating);  // New opponent every time
        player.updateOne(opponent, round % 10 == 0 ? ELO::LOSS : ELO::WIN);
    }

    std::cout << "After winning 90% of " << kNumRounds << " rounds against " << kOpponentRating
              << " rated opponents: " << player() << "\n";

    const int kExpectedDiff = 400;
    int diff = player() - kOpponentRating;
    assert(diff > kExpectedDiff - ELO::K);
    assert(diff < kExpectedDiff + ELO::K);
}

int main() {
    testBasicELO();
    testWin50PercentAgainstMany();
    testWin25PercentAgainstOne();
    testWin90PercentAgainstMany();
    testWinDrawLoseDrawAgainstOne();
    std::cout << "All ELO tests passed.\n";
    return 0;
}