#pragma once
#include <cassert>
#include <cstdint>

#include "core/core.h"

/**
 * Structure representing time control settings for a chess game. All timings are in milliseconds.
 * The maximum increment per move is 31 days, or 2,678,400,000 milliseconds, fitting uint32_t. The
 * maximum time per side is 50 years, or 1,577,923,200 seconds assuming 13 leap days. Note that the
 * time is signed to allow players to exceed their time without flagging.
 *
 * For fixed time controls, each turn can always use the full time indicated by whiteMillis and
 * blackMillis, and no time is deducted for moves made. The movesToGo and increments are ignored in
 * that case.
 */
struct TimeControl {
    static constexpr int64_t kSecondMillis = 1000;
    static constexpr int64_t kMinuteMillis = 60 * kSecondMillis;
    static constexpr int64_t kHourMillis = 60 * kMinuteMillis;
    static constexpr int64_t kDayMillis = 24 * kHourMillis;
    static constexpr int64_t kYearMillis = 365 * kDayMillis;  // Leap days not included
    static constexpr int64_t kMaxTimeMillis = 50 * kYearMillis + 13 * kDayMillis;  // Leap days
    static constexpr int64_t kMaxIncrementMillis = 31 * kDayMillis;

    static constexpr int8_t kUseIncrementPercent = 80;  // Use 80% of increment time
    static constexpr int8_t kMinDefaultMovesToGo = 10;  // Assume at least 10 moves to go if unknown
    static constexpr int8_t kExpectedGameMoves = 20;    // Expected moves in a game

    int64_t whiteMillis;
    int64_t blackMillis;
    uint32_t whiteIncrementMillis;
    uint32_t blackIncrementMillis;
    uint16_t movesToGo;  // Number of moves to go until next time control, or 0 if unknown
    bool fixedTime;      // True if movetime is used for 'movetime' or 'go infinite'

    constexpr TimeControl(int64_t timeMillis, uint32_t incrementMillis = 0)
        : whiteMillis(timeMillis),
          blackMillis(timeMillis),
          whiteIncrementMillis(incrementMillis),
          blackIncrementMillis(incrementMillis),
          movesToGo(0),
          fixedTime(false) {
        assert(timeMillis >= -kMaxTimeMillis && timeMillis < kMaxTimeMillis);
        assert(incrementMillis <= kMaxIncrementMillis);
    }

    // assignment operator
    TimeControl& operator=(const TimeControl& other) = default;

    int64_t getMillis(Color color) const { return color == Color::w ? whiteMillis : blackMillis; }
    int64_t getIncrementMillis(Color color) const {
        return color == Color::w ? whiteIncrementMillis : blackIncrementMillis;
    }

    void setTimeMillis(Color color, int64_t millis) {
        if (color == Color::w)
            whiteMillis = millis;
        else
            blackMillis = millis;
    }

    void setIncrementMillis(Color color, uint32_t millis) {
        if (color == Color::w)
            whiteIncrementMillis = millis;
        else
            blackIncrementMillis = millis;
    }
    void setFixedTimeMillis(int64_t whiteFixedMillis, int64_t blackFixedMillis) {
        whiteMillis = whiteFixedMillis;
        blackMillis = blackFixedMillis;
        fixedTime = true;
    }

    void setFixedTimeMillis(int64_t fixedMillis) { setFixedTimeMillis(fixedMillis, fixedMillis); }

    int64_t computeMillisForMove(Turn turn) const {
        if (fixedTime) return getMillis(turn.activeColor());

        int movesToGo = this->movesToGo;
        // If no moves to go is specified, estimate it based on expected game length and number of
        // moves played so far. Toward the end of the game, never assume less than 10 moves left.
        if (!movesToGo)
            movesToGo = kMinDefaultMovesToGo +
                std::max(0, kExpectedGameMoves - kMinDefaultMovesToGo - turn.fullmove());

        int64_t baseMillis = getMillis(turn.activeColor()) / movesToGo;
        int64_t incrementMillis = getIncrementMillis(turn.activeColor());
        incrementMillis = incrementMillis * kUseIncrementPercent / 100;
        return baseMillis + incrementMillis;
    }
};
