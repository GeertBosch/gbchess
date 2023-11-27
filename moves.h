#include <cstring>
#include <iterator>
#include <vector>

#include "common.h"

/**
 * Represents a set of squares on a chess board. This class is like std::set<Square>, but
 * uses a bitset represented by a uint64_t to store the squares, which is more efficient.
 */
class SquareSet {
    uint64_t _squares = 0;

public:
    SquareSet(uint64_t squares) : _squares(squares) {}
    class iterator;

    SquareSet() = default;

    static SquareSet path(Square from, Square to);

    /**
     * Returns the set of non-empty fields on the board.
     */
    static SquareSet occupancy(const Board& board);

    static SquareSet findPieces(const Board& board, Piece piece);

    void erase(Square square) { _squares &= ~(1ull << square.index()); }
    void insert(Square square) { _squares |= (1ull << square.index()); }
    void insert(SquareSet other) { _squares |= other._squares; }

    void insert(iterator begin, iterator end) {
        for (auto it = begin; it != end; ++it) insert(*it);
    }

    bool empty() const { return _squares == 0; }
    size_t size() const { return __builtin_popcountll(_squares); }
    size_t contains(Square square) const { return (_squares >> square.index()) & 1; }

    SquareSet operator&(SquareSet other) const { return _squares & other._squares; }
    SquareSet operator!(void) const { return ~_squares; }

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
        bool operator!=(const iterator& other) {
                return !(_squares == other._squares);
        }
    };

    iterator begin() {
        return {*this};
    }

    iterator end() {
        return SquareSet();
    }
};

using MoveVector = std::vector<Move>;
using ComputedMove = std::pair<Move, Position>;
using ComputedMoveVector = std::vector<ComputedMove>;

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
 * Computes all legal moves from a given chess position, mapping each move to the resulting
 * chess position after the move is applied. This function checks for moves that do not leave
 * or place the king of the active color in check.
 *
 * @param position The starting chess position.
 * @return A map where each key is a legal move and the corresponding value is the new chess
 *         position resulting from that move.
 */
ComputedMoveVector computeAllLegalMoves(const Position& position);

bool isAttacked(const Board& board, Square square);
bool isAttacked(const Board& board, SquareSet squares);

/**
 * Updates the board with the given move, which may be a capture.
 * Does not perform any legality checks.
 */
void applyMove(Board& board, Move move);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
[[nodiscard]] Position applyMove(Position position, Move move);
