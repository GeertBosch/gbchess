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
    SquareSet(Square square) : _squares(1ull << square) {}
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
        int shift = rank * kNumFiles;
        uint64_t ret = (1ull << kNumFiles) - 1;
        ret <<= shift;
        return SquareSet(ret);
    }
    static SquareSet file(int file) { return SquareSet(0x0101010101010101ull << file); }

    static SquareSet valid(int rank, int file) {
        return rank >= 0 && rank < kNumRanks && file >= 0 && file < kNumFiles
            ? SquareSet(makeSquare(file, rank))
            : SquareSet();
    }
    static SquareSet all() { return SquareSet(0xffff'ffff'ffff'ffffull); }

    void erase(Square square) { _squares &= ~(1ull << square); }
    void insert(Square square) { _squares |= (1ull << square); }
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
    SquareSet operator-(SquareSet other) const {
        auto inv = ~other._squares;
        auto ret = _squares & inv;
        return ret;
    }

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

using CastlingMove = std::array<FromTo, 2>;  // King and Rook moves for castling
struct CastlingInfo {
    const Piece rook;
    const Piece king;
    const CastlingMask kingSideMask;
    const CastlingMask queenSideMask;

    const CastlingMove kingSide;
    const CastlingMove queenSide;

    constexpr CastlingInfo(Color color)
        : rook(addColor(PieceType::ROOK, color)),
          king(addColor(PieceType::KING, color)),
          kingSideMask(color == Color::w ? CastlingMask::K : CastlingMask::k),
          queenSideMask(color == Color::w ? CastlingMask::Q : CastlingMask::q),

          kingSide(color == Color::w ? CastlingMove{FromTo{e1, g1}, FromTo{h1, f1}}
                                     : CastlingMove{FromTo{e8, g8}, FromTo{h8, f8}}),
          queenSide(color == Color::w ? CastlingMove{FromTo{e1, c1}, FromTo{a1, d1}}
                                      : CastlingMove{FromTo{e8, c8}, FromTo{a8, d8}}) {};
};

struct MoveError : public std::exception {
    std::string message;
    MoveError(std::string message) : message(message) {}
    const char* what() const noexcept override { return message.c_str(); }
};

/**
 * Returns true if the given square is attacked by a piece of the given opponent color.
 */
bool isAttacked(const Board& board, Square square, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Color opponentColor);

/** Returns the set of pieces that would result in the king being checked,
    if the piece where to be removed from the board. */
SquareSet pinnedPieces(const Board& board, Occupancy occupancy, Square kingSquare);

struct SearchState {
    SearchState(const Board& board, Turn turn);
    Color active() const { return turn.activeColor(); }

    Occupancy occupancy;
    SquareSet pawns;
    Turn turn;
    Square kingSquare;
    bool inCheck;
    SquareSet pinned;
};

/**
 * Returns true if the given move does not leave the king in check.
 */
bool doesNotCheck(Board& board, const SearchState& state, Move move);

/**
 * Returns the set of squares that needs to be empty for castling to be legal.
 */
SquareSet castlingPath(Color color, MoveKind side);

/**
 * Compute the delta in occupancy due to the given move
 */
Occupancy occupancyDelta(Move move);

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

struct UndoPosition {
    UndoPosition() : board(), turn(Color::w) {}
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

Turn applyMove(Turn turn, MoveWithPieces mwp);
void applyMove(SearchState& state, MoveWithPieces mwp);

/**
 *  Returns the castling mask for the castling rights cancelled by the given move.
 */
CastlingMask castlingMask(Square from, Square to);

namespace for_test {

/**
 * Returns a set of squares to which a piece can potentially move. This function does not account
 * for the legality of the move in terms of check conditions and such, but merely provides possible
 * moves based on the movement rules for each piece type.
 */
SquareSet possibleMoves(Piece piece, Square from);

/**
 * Returns a set of squares on which a piece can potentially make a capture. It does not take into
 * account en passant or legality in terms of check conditions and such.
 */

SquareSet possibleCaptures(Piece piece, Square from);

}  // namespace for_test
