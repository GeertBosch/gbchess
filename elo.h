#include <algorithm>
#include <cmath>

#pragma once

class ELO {
    int rating = kInitialRating;

public:
    static constexpr int kInitialRating = 800;
    static constexpr int kMinRating = 100;
    static constexpr int kMaxRating = 4000;
    static constexpr int K = 32;  // Maximum rating change per game
    enum Result { LOSS = 0, DRAW = 1, WIN = 2 };

    ELO() = default;
    ELO(const ELO& other) = default;
    ELO& operator=(const ELO& other) = default;

    ELO(int rating) : rating(std::clamp(rating, kMinRating, kMaxRating)) {}

    int operator()() const { return rating; }

    // Update the rating after a game against an opponent with the given rating, where the opponent
    // is an unchanging reference.
    void updateOne(ELO opponent, Result win) { updateBoth(opponent, win); }

    // Transfer rating between both opponents based on their existing ratings and the result
    void updateBoth(ELO& opponent, Result win) {
        int ratingDiff = opponent() - rating;
        double expected = 1.0 / (1.0 + pow(10.0, ratingDiff / 400.0));
        int change = std::round(K * (win * 0.5 - expected));
        rating += change;
        opponent.rating -= change;
    }
};