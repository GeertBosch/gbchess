#include <climits>
#include <cstring>
#include <functional>
#include <iterator>
#include <vector>

#include "common.h"

#pragma once

/**
 * Represents a set of squares on a chess board. This class is like std::set<Square>, but
 * uses a bitset represented by a uint64_t to store the squares, which is more efficient.
 */
class SquareSet {
    uint64_t _squares = 0;
    static_assert(kNumSquares <= sizeof(uint64_t) * CHAR_BIT);

public:
    SquareSet(uint64_t squares) : _squares(squares) {}
    SquareSet(Square square) : _squares(1ull << square.index()) {}
    class iterator;

    SquareSet() = default;

    /**
     * Returns the set of squares between two squares, exclusive of the `to` square.
     */
    static SquareSet path(Square from, Square to);

    /**
     * Returns the set of non-empty fields on the board, possibly limited to one color.
     */
    static SquareSet occupancy(const Board& board);
    static SquareSet occupancy(const Board& board, Color color);

    static SquareSet find(const Board& board, Piece piece);

    static SquareSet valid(int rank, int file) {
        return rank >= 0 && rank < kNumRanks && file >= 0 && file < kNumFiles
            ? SquareSet(Square(rank, file))
            : SquareSet();
    }

    void erase(Square square) { _squares &= ~(1ull << square.index()); }
    void insert(Square square) { _squares |= (1ull << square.index()); }
    void insert(SquareSet other) { _squares |= other._squares; }

    void insert(iterator begin, iterator end) {
        for (auto it = begin; it != end; ++it) insert(*it);
    }

    operator bool() const { return _squares; }
    bool empty() const { return _squares == 0; }
    size_t size() const { return __builtin_popcountll(_squares); }
    bool contains(Square square) const { return *this & SquareSet(square); }

    SquareSet operator&(SquareSet other) const { return _squares & other._squares; }
    SquareSet operator|(SquareSet other) const { return _squares | other._squares; }
    SquareSet operator^(SquareSet other) const { return _squares ^ other._squares; }
    SquareSet operator!(void) const { return ~_squares; }

    SquareSet operator|=(SquareSet other) { return _squares |= other._squares; }
    SquareSet operator&=(SquareSet other) { return _squares &= other._squares; }
    SquareSet operator^=(SquareSet other) { return _squares ^= other._squares; }

    bool operator==(SquareSet other) const { return _squares == other._squares; }

    class iterator {
        friend class SquareSet;
        uint64_t _squares;
        iterator(SquareSet squares) : _squares(squares._squares) {}
        using iterator_category = std::forward_iterator_tag;

    public:
        iterator operator++() {
            _squares &= _squares - 1;  // Clear the least significant bit
            return *this;
        }
        Square operator*() {
            return Square(__builtin_ctzll(_squares));  // Count trailing zeros
        }
        bool operator==(const iterator& other) { return _squares == other._squares; }
        bool operator!=(const iterator& other) { return !(_squares == other._squares); }
    };

    iterator begin() { return {*this}; }
    iterator end() { return SquareSet(); }
};

struct Occupancy {
    SquareSet theirs;
    SquareSet ours;
    constexpr Occupancy() = default;
    constexpr Occupancy(const Board& board, Color activeColor) {
        theirs = SquareSet::occupancy(board, !activeColor);
        ours = SquareSet::occupancy(board, activeColor);
    }
    constexpr Occupancy(SquareSet theirs, SquareSet ours) : theirs(theirs), ours(ours) {}

    SquareSet operator()(void) const { return SquareSet{theirs | ours}; }
    Occupancy operator!() const { return {ours, theirs}; }
    Occupancy operator^(Occupancy other) const {
        return {theirs ^ other.theirs, ours ^ other.ours};
    }
    bool operator==(Occupancy other) const { return theirs == other.theirs && ours == other.ours; }
    bool operator!=(Occupancy other) const { return !(*this == other); }
};

/**
 * Returns the set of squares that needs to be empty for castling to be legal.
 */
SquareSet castlingPath(Color color, MoveKind side);

/**
 * Compute the delta in occupancy due to the given move
 */
Occupancy occupancyDelta(Move move);

/**
 * This availableMoves function iterates over each square on the board. If a piece of the active
 * color is found, it calculates its possible moves using the possibleMoves function you already
 * have. For each possible destination square, it checks if the move would target an occupied square
 * or move through other pieces. If neither condition is true, the move is added to the set.
 */
void addAvailableMoves(MoveVector& moves, const Board& board, Color activeColor);

/**
 * This function follows the same structure as availableMoves but focuses on captures. It
 * loops through each square on the board, determines if there's a piece of the active color
 * on it, and then finds its possible captures. The result is filtered to exclude self-
 * captures, and those that move through other pieces, adding valid captures to the result
 * set.
 */
void addAvailableCaptures(MoveVector& captures, const Board& board, Color activeColor);

void addAvailableEnPassant(MoveVector& captures,
                           const Board& board,
                           Color activeColor,
                           Square enPassantTarget);

/**
 * This function adds castling moves to the result set. It checks if the active color has
 * castling rights, and if so, whether the path for castling is unobstructed. It does not check
 * for legality in terms of check conditions.
 */
void addAvailableCastling(MoveVector& moves,
                          const Board& board,
                          Color activeColor,
                          CastlingMask castlingAvailability);

/**
 * Calculates all possible moves for a given chess piece on the board.
 * This function does not account for the legality of the move in terms of check conditions,
 * but merely provides possible moves based on the movement rules for each piece type.
 *
 * @param piece The type of the chess piece ('R', 'N', 'B', 'Q', 'K', or 'P' for white,
 *              'r', 'n', 'b', 'q', 'k', or 'p' for black).
 * @param from The starting square of the piece for which to calculate possible moves.
 * @return A set of squares to which the piece can potentially move.
 */
SquareSet possibleMoves(Piece piece, Square from);

/**
 * Calculates all possible capture moves for a given chess piece on the board.
 * This function focuses on the potential captures a piece can make, including pawn's
 * diagonal captures. It does not take into account en passant or legality in terms of
 * check conditions.
 *
 * @param piece The type of the chess piece ('R', 'N', 'B', 'Q', 'K', or 'P' for white,
 *              'r', 'n', 'b', 'q', 'k', or 'p' for black).
 * @param from The starting square of the piece for which to calculate possible capture moves.
 * @return A set of squares from which the piece can potentially make a capture.
 */

SquareSet possibleCaptures(Piece piece, Square from);

/**
 * Computes all legal moves from a given chess position. This function checks for moves
 * that do not leave or place the king of the active color in check, including the special
 * case of castling.
 */
MoveVector allLegalMovesAndCaptures(Turn turn, Board& board);

struct MoveWithPieces {
    Move move;
    Piece piece;
    Piece captured;
};

using MoveFun = std::function<void(Board&, MoveWithPieces)>;
void forAllLegalQuiescentMoves(Turn turn, Board& board, int depthleft, MoveFun action);
void forAllLegalMovesAndCaptures(Turn turn, Board& board, MoveFun action);
size_t countLegalMovesAndCaptures(Turn turn, Board& board);

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, int depthleft);

/**
 * Returns true if the given square is attacked by a piece of the given opponent color.
 */
bool isAttacked(const Board& board, Square square, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Color opponentColor);

/**
 * Parses a move string in UCI notation and returns a Move of the appropriate kind. The move is a 4
 * or 5 character string representing from/to squares and promotion piece, if any. Returns the
 * corresponding Move object, or an empty move if the string does not specify a legal move.
 */
Move parseMoveUCI(Position position, const std::string& move);

/**
 * Updates the board with the given move, which may be a capture.
 * Does not perform any legality checks. Any captured piece is returned.
 */
Piece makeMove(Board& board, Move move);

/**
 * Undoes the given move, restoring the captured piece to the captured square.
 */
void unmakeMove(Board& board, Move move, Piece captured);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
[[nodiscard]] Position applyMove(Position position, Move move);
[[nodiscard]] Position applyMoves(Position position, MoveVector const& moves);

Turn applyMove(Turn turn, Piece piece, Move move);

/**
 *  Returns the castling mask for the castling rights cancelled by the given move.
 */
CastlingMask castlingMask(Square from, Square to);
