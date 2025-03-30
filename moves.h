#include <climits>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "common.h"
#include "sse2.h"

#pragma once

/**
 * Represents a set of squares on a chess board. This class is like std::set<Square>, but
 * uses a bitset represented by a uint64_t to store the squares, which is more efficient.
 */
class SquareSet {
    using T = uint64_t;
    T _squares = 0;
    static_assert(kNumSquares <= sizeof(T) * CHAR_BIT);

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
     * Returns the set of squares between two squares, inclusive of the `to` square.
     */
    static SquareSet ipath(Square from, Square to) {
        return path(from, to) | SquareSet(from) | SquareSet(to);
    }

    /**
     * Returns the set of non-empty fields on the board, possibly limited to one color.
     */
    static SquareSet occupancy(const Board& board);
    static SquareSet occupancy(const Board& board, Color color);

    static SquareSet find(const Board& board, Piece piece);
    static SquareSet rank(int rank) {  //
        int shift = rank * 8;
        uint64_t ret = 0xffull;
        ret <<= shift;
        return SquareSet(ret);
    }
    static SquareSet file(int file) { return SquareSet(0x0101010101010101ull << file); }

    static SquareSet valid(int rank, int file) {
        return rank >= 0 && rank < kNumRanks && file >= 0 && file < kNumFiles
            ? SquareSet(Square(file, rank))
            : SquareSet();
    }
    static SquareSet all() { return SquareSet(0xffff'ffff'ffff'ffffull); }

    void erase(Square square) { _squares &= ~(1ull << square.index()); }
    void insert(Square square) { _squares |= (1ull << square.index()); }
    void insert(SquareSet other) { _squares |= other._squares; }

    void insert(iterator begin, iterator end) {
        for (auto it = begin; it != end; ++it) insert(*it);
    }

    explicit operator bool() const { return _squares; }
    T bits() const { return _squares; }
    bool empty() const { return _squares == 0; }
    size_t size() const { return __builtin_popcountll(_squares); }
    bool contains(Square square) const { return bool(*this & SquareSet(square)); }

    SquareSet operator&(SquareSet other) const { return _squares & other._squares; }
    SquareSet operator|(SquareSet other) const { return _squares | other._squares; }
    SquareSet operator^(SquareSet other) const { return _squares ^ other._squares; }
    SquareSet operator!(void) const { return ~_squares; }
    SquareSet operator>>(int shift) const { return _squares >> shift; }
    SquareSet operator<<(int shift) const { return _squares << shift; }
    SquareSet operator-(SquareSet other) const { return _squares & ~other._squares; }

    SquareSet operator|=(SquareSet other) { return _squares |= other._squares; }
    SquareSet operator&=(SquareSet other) { return _squares &= other._squares; }
    SquareSet operator^=(SquareSet other) { return _squares ^= other._squares; }
    SquareSet operator-=(SquareSet other) { return _squares &= ~other._squares; }
    SquareSet operator>>=(int shift) { return _squares >>= shift; }
    SquareSet operator<<=(int shift) { return _squares <<= shift; }

    bool operator==(SquareSet other) const { return _squares == other._squares; }

    class iterator {
        friend class SquareSet;
        SquareSet::T _squares;
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

    iterator begin() const { return {*this}; }
    iterator end() const { return SquareSet(); }
};


class Occupancy {
    SquareSet _theirs;
    SquareSet _ours;

    // Private constructor to prevent creation of invalid occupancies
    constexpr Occupancy(SquareSet theirs, SquareSet ours) : _theirs(theirs), _ours(ours) {}

public:
    constexpr Occupancy() = default;
    constexpr Occupancy(const Board& board, Color activeColor)
        : Occupancy(SquareSet::occupancy(board, !activeColor),
                    SquareSet::occupancy(board, activeColor)) {}

    // Deltas may have corresponding elements set in both theirs and ourss
    static Occupancy delta(SquareSet theirs, SquareSet ours) { return Occupancy(theirs, ours); }

    SquareSet theirs() const { return _theirs; }
    SquareSet ours() const { return _ours; }

    SquareSet operator()(void) const { return SquareSet{_theirs | _ours}; }
    Occupancy operator!() const { return {_ours, _theirs}; }
    Occupancy operator^(Occupancy other) const {
        return {_theirs ^ other._theirs, _ours ^ other._ours};
    }
    Occupancy operator^=(Occupancy other) {
        _theirs ^= other._theirs;
        _ours ^= other._ours;
        return *this;
    }
    bool operator==(Occupancy other) const {
        return _theirs == other._theirs && _ours == other._ours;
    }
    bool operator!=(Occupancy other) const { return !(*this == other); }
};

/**
 * Returns true if the given square is attacked by a piece of the given opponent color.
 */
bool isAttacked(const Board& board, Square square, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Color opponentColor);

struct SearchState {
    SearchState(const Board& board, Turn turn)
        : occupancy(Occupancy(board, turn.activeColor())),
          pawns(SquareSet::find(board, addColor(PieceType::PAWN, turn.activeColor()))),
          turn(turn),
          kingSquare(
              *SquareSet::find(board, addColor(PieceType::KING, turn.activeColor())).begin()),
          inCheck(isAttacked(board, kingSquare, occupancy)) {}
    Color active() const { return turn.activeColor(); }

    Occupancy occupancy;
    SquareSet pawns;
    Turn turn;
    Square kingSquare;
    bool inCheck;
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
void addAvailableMoves(MoveVector& moves, const Board& board, Turn turn);

/**
 * This function follows the same structure as availableMoves but focuses on captures. It
 * loops through each square on the board, determines if there's a piece of the active color
 * on it, and then finds its possible captures. The result is filtered to exclude self-
 * captures, and those that move through other pieces, adding valid captures to the result
 * set.
 */
void addAvailableCaptures(MoveVector& captures, const Board& board, Turn turn);

void addAvailableEnPassant(MoveVector& captures, const Board& board, Turn turn);


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

/**
 * Returns true if the given side has a pawn that may promote on the next move.
 * Does not check for legality of the promotion move.
 */
bool mayHavePromoMove(Color side, Board& board, Occupancy occupancy);

struct MoveWithPieces {
    Move move;
    Piece piece;
    Piece captured;
};

using MoveFun = std::function<void(Board&, MoveWithPieces)>;
void forAllLegalQuiescentMoves(Turn turn, Board& board, int depthleft, MoveFun action);
void forAllLegalMovesAndCaptures(Board& board, SearchState& state, MoveFun action);
inline void forAllLegalMovesAndCaptures(Turn turn, Board& board, MoveFun action) {
    auto state = SearchState(board, turn);
    forAllLegalMovesAndCaptures(board, state, action);
}

size_t countLegalMovesAndCaptures(Board& board, SearchState& state);

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, int depthleft);

struct ParseError : public std::exception {
    std::string message;
    ParseError(std::string message) : message(message) {}
    const char* what() const noexcept override { return message.c_str(); }
};

/**
 * Parses a move string in UCI notation and returns a Move or MoveVector of the appropriate kind.
 * The UCI string is a 4 or 5 character string representing from/to squares and promotion piece, if
 * any. Returns the corresponding Move object, or an empty move if the string does not specify a
 * legal move. Throws a ParseError if the string is not a legal move for the position.
 */
Move parseUCIMove(Position position, const std::string& move);
MoveVector parseUCIMoves(Position position, const std::vector<std::string>& moves);
MoveVector parseUCIMoves(Position position, const std::string& moves);

/**
 * Similar to above, but applies the move to a position and returns the updated position.
 * Throws a ParseError if the string is not a legal move for the position.
 */
Position applyUCIMove(Position position, const std::string& move);

struct UndoPosition {
    UndoPosition() : board(), turn(Color::WHITE) {}
    UndoPosition(BoardChange board, Turn turn) : board(board), turn(turn) {}
    BoardChange board;
    Turn turn;
};

/**
 * Decompose a possibly complex move (castling, promotion, en passant) into two simpler moves
 * that allow making and unmaking the change to the board without complex conditional logic.
 */
BoardChange prepareMove(Board& board, Move move);

/**
 * Updates the board with the given move, which may be a capture.
 * Does not perform any legality checks. Any captured piece is returned.
 */
BoardChange makeMove(Board& board, Move move);
BoardChange makeMove(Board& board, BoardChange change);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
UndoPosition makeMove(Position& position, Move move);
UndoPosition makeMove(Position& position, BoardChange change, Move move);

/**
 * Undoes the given move, restoring the captured piece to the captured square.
 */
void unmakeMove(Board& board, BoardChange undo);
void unmakeMove(Position& position, UndoPosition undo);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
[[nodiscard]] Position applyMove(Position position, Move move);
[[nodiscard]] Position applyMoves(Position position, MoveVector const& moves);

Turn applyMove(Turn turn, MoveWithPieces mwp);
void applyMove(SearchState& state, MoveWithPieces mwp);

/**
 *  Returns the castling mask for the castling rights cancelled by the given move.
 */
CastlingMask castlingMask(Square from, Square to);
