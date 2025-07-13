#pragma once

#include <limits>
#include <map>
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
protected:
    using value_type = int16_t;
    constexpr Score(unsigned long long value) : value(value) {
        assert(value <= std::numeric_limits<value_type>::max());
    }

    constexpr value_type operator()() const { return value; }

private:
    value_type value = 0;  // centipawns
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
        if (moves < 0) return mateIn(-moves);

        assert(moves > 0 && moves < 100);
        return Score(value_type(Score::max().cp() - moves + 1));
    }
    static constexpr Score max() { return Score(99'99ull); }
    static constexpr Score min() { return -max(); }
    static constexpr Score draw() { return Score(0ull); }
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

using PieceValueTable = std::array<Score, kNumPieceTypes>;
using PieceSquareTable = std::array<SquareTable, kNumPieces>;
using TaperedPieceSquareTable = std::array<PieceSquareTable, 2>;  // Middle game and end game
struct EvalTable {
    PieceSquareTable pieceSquareTable{};
    EvalTable();
    EvalTable(const Board& board, bool usePieceSquareTables);
    EvalTable& operator=(const EvalTable& other) = default;

    const SquareTable& operator[](Piece piece) const { return pieceSquareTable[index(piece)]; }
};

// Returns the phase (7 for opening, 0 for endgame) based on the material on the board.
int computePhase(const Board& board);

/**
 * Return the delta in the Score as result of the move from white's perspective: positive scores
 * indicate an advantage for white. The board past reflects the state before the move.
 */
inline Score evaluateMove(const Board& before, BoardChange move, const EvalTable& table) {
    Score delta = 0_cp;
    // Go over all changes in order, as we have the board before the move was made.

    // The first move is a regular capture or move, so adjust delta accordingly.
    Piece first = before[move.first.from];
    delta -= table[move.captured][move.first.to];  // Remove captured piece
    delta -= table[first][move.first.from];        // Remove piece from original square
    delta += table[first][move.first.to];          // Add the piece to its target square

    // The second move is either a quiet move for en passant or castling, or a promo move.
    Piece second = first;
    if (move.second.from != move.first.to) second = before[move.second.from];  // Castling

    delta -= table[second][move.second.from];      // Remove piece before promotion
    second = Piece(uint8_t(second) + move.promo);  // Apply any promotion
    delta += table[second][move.second.to];        // Add piece with any promotions

    return delta;
}

inline Score evaluateMove(const Board& board,
                          Color activePlayer,
                          BoardChange move,
                          EvalTable& table) {
    Score eval = evaluateMove(board, move, table);
    return activePlayer == Color::w ? eval : -eval;
}


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
 * Same as the two functions above, but with player-relative scores
 */
template <typename TableArg>
Score evaluateBoard(const Board& board, Color activePlayer, TableArg&& arg) {
    Score eval = evaluateBoard(board, arg);
    return activePlayer == Color::w ? eval : -eval;
}

/**
 * Static Exchange Evaluation (SEE) - evaluates the material outcome of a capture sequence
 * on a given square. Returns the net material gain/loss from the perspective of the
 * side to move. Used in quiescence search to determine if a capture is worth considering.
 */
Score staticExchangeEvaluation(const Board& board, Square from, Square to);

/** The following three functions return a relative score for moves for ordering */
int scoreMVVLVA(const Board& board, Move move);
int scoreSEE(const Board& board, Move move);
int scoreMove(const Board& board, Move move);

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

/**
 * Returns true if the side to move has non-pawn material (useful for null move pruning).
 * In positions with only pawns and king, null move pruning can be unreliable due to zugzwang.
 */
bool hasNonPawnMaterial(const Position& position);