#include <limits>
#include <map>
#include <string>

#include "common.h"

#pragma once

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
        assert(-9999 <= value && value <= 9999);
        return value;
    }

public:
    friend constexpr Score operator"" _cp(unsigned long long value);
    Score() = default;
    constexpr Score operator-() const { return check(-value); }
    Score operator+(Score rhs) const { return check(value + rhs.value); }
    Score operator-(Score rhs) const { return *this + -rhs; }
    Score operator*(Score rhs) const { return check(value * rhs.value / 100); }
    Score& operator+=(Score rhs) { return *this = *this + rhs; }
    Score& operator-=(Score rhs) { return *this = *this - rhs; }
    Score& operator*=(Score rhs) { return *this = *this * rhs; }
    bool operator==(Score rhs) const { return value == rhs.value; }
    bool operator!=(Score rhs) const { return value != rhs.value; }
    bool operator<(Score rhs) const { return value < rhs.value; }
    bool operator>(Score rhs) const { return value > rhs.value; }
    bool operator<=(Score rhs) const { return value <= rhs.value; }
    bool operator>=(Score rhs) const { return value >= rhs.value; }
    // The mate score is the number of moves to mate, with the sign indicating the winner.
    int mate() const {
        return value < 0 ? -operator-().mate()
                         : (value < max().value / 100 * 100 ? 0 : 100 - value % 100);
    }
    int pawns() const { return value / 100; }
    int cp() const { return value; }

    operator std::string() const {
        value_type pawns = check(value) / 100;
        value_type cents = value % 100;

        // For mate scores, the number of moves to mate is encoded in the cents part
        if (value < 0) return "-" + std::string(-*this);
        if (mate()) return "M" + std::to_string(100 - cents);

        std::string result = std::to_string(pawns) + '.';
        result += '0' + cents / 10;
        result += '0' + cents % 10;
        return result;
    }
    static constexpr Score mateIn(int moves) {
        assert(moves > 0 && moves < 100);
        return Score(value_type(Score::max().cp() - moves + 1));
    }
    static constexpr Score max() { return Score(99'99ull); }
    static constexpr Score min() { return -max(); }
    static constexpr Score fromCP(value_type cp) { return Score(cp); }
};

// literal constructor for _cp (centipawn) suffix
constexpr Score operator"" _cp(unsigned long long value) {
    return Score(value);
}

using SquareTable = std::array<Score, kNumSquares>;
SquareTable operator+(SquareTable lhs, Score rhs);
SquareTable operator+(SquareTable lhs, SquareTable rhs);
SquareTable operator*(SquareTable table, Score score);

/** Reverse the ranks in the table and negate the scores, so tables made from the viewpoint
 *  of one side can be used for the other side.
 */
void flip(SquareTable& table);

using PieceSquareTable = std::array<SquareTable, kNumPieces>;
struct EvalTable {
    PieceSquareTable pieceSquareTable{};
    EvalTable();
    EvalTable(const Board& board, bool usePieceSquareTables);
    EvalTable& operator=(const EvalTable& other) = default;

    const SquareTable& operator[](Piece piece) const { return pieceSquareTable[index(piece)]; }
};

/**
 * The evaluation is always from the perspective of the active color. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 */
struct Eval {
    Move move = {};  // Defaults to an invalid move
    Score score = Score::min();

    Eval() = default;
    Eval(Move move, Score score) : move(move), score(score) {}
    Eval& operator=(const Eval& other) = default;
    Eval operator-() const {
        auto ret = *this;
        ret.score = -ret.score;

        return ret;
    }
    explicit operator bool() const { return move; }

    bool operator==(const Eval& rhs) const { return move == rhs.move && score == rhs.score; }
    bool operator!=(const Eval& rhs) const { return !(*this == rhs); }
    bool operator<(const Eval& rhs) const { return score < rhs.score; }
    bool operator>(const Eval& rhs) const { return score > rhs.score; }

    operator std::string() const {
        return static_cast<std::string>(move) + "@" + static_cast<std::string>(score);
    }
};

/**
 * This function iterates over each square in the board, uses the pieceValues map to find
 * the value of the piece on that square, and adjusts the total value accordingly. White
 * pieces have positive values, and black pieces have negative values, so the returned value
 * represents the advantage to the white player: positive for white's advantage, negative
 * for black's advantage.
 * If usePieceSquareTables is true, additionally adjust the evaluation of each piece based on
 * its position and the phase of the game.
 */
Score evaluateBoard(const Board& board, bool usePieceSquareTables);

/**
 * Same as above, but uses a pre-computed evaluation table.
 */
Score evaluateBoard(const Board& board, const EvalTable& table);

/**
 * Return the delta in the Score as result of the move from white's perspective: positive scores
 * indicate an advantage for white. The board past reflects the state before the move.
 */
inline Score evaluateMove(const Board& before, BoardChange move, const EvalTable& table) {
    Score delta = 0_cp;
    // Go over all changes in order, as we have the board before the move was made.

    // The first move is a regular capture or move, so adjust delta accordingly.
    Piece first = before[move.first[0]];
    delta -= table[move.captured][move.first[1].index()];  // Remove captured piece
    delta -= table[first][move.first[0].index()];          // Remove piece from original square
    delta += table[first][move.first[1].index()];          // Add the piece to its target square

    // The second move is either a quiet move for en passant or castling, or a promo move.
    Piece second = first;
    if (move.second[0] != move.first[1]) second = before[move.second[0]];  // Castling

    delta -= table[second][move.second[0].index()];  // Remove piece before promotion
    second = Piece(index(second) + move.promo);      // Apply any promotion
    delta += table[second][move.second[1].index()];  // Add piece with any promotions

    return delta;
}

inline Score evaluateMove(const Board& board,
                          Color activePlayer,
                          BoardChange move,
                          EvalTable& table) {
    Score eval = evaluateMove(board, move, table);
    return activePlayer == Color::WHITE ? eval : -eval;
}

/**
 * Same as the two functions above, but with player-relative scores
 */
template <typename TableArg>
Score evaluateBoard(const Board& board, Color activePlayer, TableArg&& arg) {
    Score eval = evaluateBoard(board, arg);
    return activePlayer == Color::WHITE ? eval : -eval;
}

/**
 * Returns true if and only if the side whose turn it is is in check.
 */
bool isInCheck(const Position& position);

/**
 * Returns true if and only if the size whose turn it is has no legal moves.
 */
bool isMate(const Position& position);

/**
 * Returns true if and only if the size whose turn it is has been checkmated.
 */
bool isCheckmate(const Position& position);
/**
 * Returns true if and only if the game is in stalemate.
 */
bool isStalemate(const Position& position);