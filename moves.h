#include <bitset>
#include <map>
#include <set>

#include "common.h"

/**
 * Represents a set of squares on a chess board. This class is like std::set<Square>, but
 * uses a bitset represented by a uint64_t to store the squares, which is more efficient.
 */
class SquareSet {
    uint64_t _squares = 0;
public:
    class iterator;

    void insert(Square square) {
        _squares |= (1ull << square.index());
    }
    void insert(SquareSet other) {
        _squares |= other._squares;
    }

    void insert (iterator begin, iterator end) {
        for (auto it = begin; it != end; ++it) {
            insert(*it);
        }
    }

    bool empty() const {
        return _squares == 0;
    }

    size_t size() const {
        return __builtin_popcountll(_squares);
    }

    size_t count(Square square) const {
        return (_squares >> square.index()) & 1;
    }

    class iterator {
        friend class SquareSet;
        uint64_t _squares;
        iterator(SquareSet squares) : _squares(squares._squares) {}
    public:
        iterator operator++() {
            _squares &= _squares - 1; // Clear the least significant bit
            return *this;
        }
        Square operator*() {
            return Square(__builtin_ctzll(_squares)); // Count trailing zeros
        }
        bool operator==(const iterator& other) {
            return _squares == other._squares;
        }
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


/**
 * This availableMoves function iterates over each square on the board. If a piece of the
 * active color is found, it calculates its possible moves using the possibleMoves function
 * you already have. For each possible destination square, it checks if the move would
 * result in self-blocking or moving through other pieces using the movesThroughPieces
 * function. If neither condition is true, the move is added to the set.
 */
std::set<Move> availableMoves(const ChessBoard& board, char activeColor);

/**
 * This function follows the same structure as availableMoves but focuses on captures. It
 * loops through each square on the board, determines if there's a piece of the active color
 * on it, and then finds its possible captures. The result is filtered to exclude self-
 * captures, and those that move through other pieces, adding valid captures to the result
 * set.
 */
std::set<Move> availableCaptures(const ChessBoard& board, char activeColor);

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
SquareSet possibleMoves(char piece, const Square& from);

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

SquareSet possibleCaptures(char piece, const Square& from);
/**
 * Computes all legal moves from a given chess position, mapping each move to the resulting
 * chess position after the move is applied. This function checks for moves that do not leave
 * or place the king of the active color in check.
 *
 * @param position The starting chess position.
 * @return A map where each key is a legal move and the corresponding value is the new chess
 *         position resulting from that move.
 */
std::map<Move, ChessPosition> computeAllLegalMoves(const ChessPosition& position);

/**
 * Checks if the specified color is in check on the given chess board.
 *
 * @param board The chess board to check.
 * @param activeColor The color to check for being in check.
 * @return True if the specified color is in check, false otherwise.
 */
bool isInCheck(const ChessBoard& board, Color activeColor);

/**
 * Updates the board with the given move, which may be a capture.
 * Does not perform any legality checks.
 */
void applyMove(ChessBoard& board, const Move& move);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
void applyMove(ChessPosition& position, const Move& move);

