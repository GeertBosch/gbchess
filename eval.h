#include <limits>
#include <map>
#include <sstream>
#include <string>

#include "common.h"

/**
 *  The Score type is used to represent the evaluation of a chess position. It is a signed integer,
 *  where the value is in centipawns (hundredths of a pawn). The ends of the range indicate
 *  checkmate. In debug mode, the type protect against overflow. The value is positive if the
 *  position is good for white, and negative if it is good for black. The value is zero if the
 *  position is even.
 */
class Score {
    using value_type = int16_t;
    value_type value = 0;  // centipawns
    constexpr Score(unsigned long long value) : value(value) {
        assert(value <= std::numeric_limits<value_type>::max());
    }
    constexpr Score(value_type value) : value(value) { this->value = check(value); }

    static constexpr value_type check(value_type value) {
        if (debug) assert(-9999 <= value && value <= 9999);
        return value;
    }

public:
    friend constexpr Score operator"" _cp(unsigned long long value);
    Score() = default;
    constexpr Score operator-() const { return check(-value); }
    Score operator+(Score rhs) const { return check(value + rhs.value); }
    Score operator-(Score rhs) const { return *this + -rhs; }
    Score& operator+=(Score rhs) { return *this = *this + rhs; }
    Score& operator-=(Score rhs) { return *this = *this - rhs; }
    bool operator==(Score rhs) const { return value == rhs.value; }
    bool operator<(Score rhs) const { return value < rhs.value; }
    bool mate() const { return std::abs(value) >= max().value / 100 * 100; }

    /**
     *  For scores indicating mate, reduce the value by one, so that a sooner mate is preferred over
     *  a later one.
     */
    Score adjustDepth() const { return int16_t(value > 0 ? value - 1 : value); }

    operator std::string() const {
        value_type pawns = value / 100;
        value_type cents = std::abs(value % 100);
        std::string result = std::to_string(pawns) + '.';
        result += '0' + cents / 10;
        result += '0' + cents % 10;
        return result;
    }
    static constexpr Score max() { return Score(99'99ull); }
    static constexpr Score min() { return -max(); }
};

// literal constructor for _cp (centipawn) suffix
constexpr Score operator"" _cp(unsigned long long value) {
    return Score(value);
}

static Score worstEval = -99'99_cp;
static Score drawEval = 0_cp;
static Score bestEval = 99'99_cp;

/**
 * The evaluation is always from the perspective of the active color. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 */
struct Eval {
    Move move = {};  // Defaults to an invalid move
    Score evaluation = worstEval;

    Eval() = default;
    Eval(Move move, Score evaluation) : move(move), evaluation(evaluation) {}
    Eval& operator=(const Eval& other) = default;
    Eval operator-() const {
        auto ret = *this;
        ret.evaluation = -ret.evaluation;

        return ret;
    }

    bool operator<(const Eval& rhs) const { return evaluation < rhs.evaluation; }
};

extern uint64_t evalCount;
extern uint64_t cacheCount;

/**
 * This function iterates over each square in the board, uses the pieceValues map to find
 * the value of the piece on that square, and adjusts the total value accordingly. White
 * pieces have positive values, and black pieces have negative values, so the returned value
 * represents the advantage to the white player: positive for white's advantage, negative
 * for black's advantage.
 */
Score evaluateBoard(const Board& board);

/**
 * Evaluates the best moves from a given chess position up to a certain depth.
 * Each move is evaluated based on the static evaluation of the board or by recursive calls
 * to this function, decreasing the depth until it reaches zero. It also accounts for checkmate
 * and stalemate situations.
 */
Eval computeBestMove(Position& position, int maxdepth);

/**
 *  a debugging function to walk the move generation tree of strictly legal moves to count all the
 *  leaf nodes of a certain depth, which can be compared to predetermined values and used to isolate
 *  bugs. (See https://www.chessprogramming.org/Perft)
 */
uint64_t perft(Turn turn, Board& board, int depth);
