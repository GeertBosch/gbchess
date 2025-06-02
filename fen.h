#include "common.h"
namespace fen {
static constexpr auto emptyPiecePlacement = "8/8/8/8/8/8/8/8";
static constexpr auto initialPiecePlacement = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
static constexpr auto initialPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct ParseError : public std::exception {
    std::string message;
    ParseError(std::string message) : message(message) {}
    const char* what() const noexcept override { return message.c_str(); }
};

/**
 * Converts a Board object to a FEN piece placement string. The resulting string represents the
 * piece placement in FEN notation.
 */
std::string to_string(const Board& board);

/**
 * Converts a Position object to a full FEN string. The resulting string includes piece placement,
 * active color, castling rights, en passant target, halfmove clock, and fullmove number.
 */
std::string to_string(const Position& position);

/**
 * Checks if a given string might be a valid FEN string. This function performs a basic validation
 * of the input string.
 */
bool maybeFEN(std::string fen);

/**
 * Checks if the UCI move by itself could possibly be a valid move. This returns a quiet move for a
 * string of size 4, and a promotion move or promotion capture for a string of size 5. This function
 * checks what can be checked without the board state, such as:
 *  - The from and to squares are valid and not the same.
 *  - The move distance is valid for a knight, bishop or rook. This implicitly covers the pawn, king
 *    and queen moves as well.
 *  - If the move is a promotion, the rank of the to square must be at the top or botton of the
 *    board, and the from rank must be one step away. Return a promotion move if the file step is
 *    zero, and a promotiom capture if it is one.
 *
 * Throws ParseError if the move cannot possibly be valid regardless of the board state.
 */
Move parseUCIMove(const std::string& move);

/**
 * Parses a move string in UCI notation and returns a Move of the appropriate kind. In addition to
 * the parsing mentioned above, this function does the following board dependent checks:
 *   - The from square must not be empty.
 *   - If the to square is occupied, the piece must be of the opposite color.
 *   - For promotions, the piece must be a pawn.
 *   - If a pawn moves two squares vertically, it's a double push.
 *   - If a pawn moves diagonally and the target square is empty, it's an en passant move.
 *   - If the king moves two squares horizontally, it's castling. This doesn't check legality of the
 *     move in terms of moving through pieces, check conditions, castling rights or valid en passant
 *     targets. It only checks the basic movement rules for each piece type.
 *
 * For an invalid move, a MoveError exception is thrown.
 */
Move parseUCIMove(const Board& board, const std::string& move);

/**
 * Parses a FEN string and returns the corresponding Position object. The input string must follow
 * the FEN format.
 */
Position parsePosition(const std::string& fen);

/**
 * Parses the piece placement part of a FEN string and returns a Board object. The input string
 * should represent only the piece placement section of the FEN notation.
 */
Board parsePiecePlacement(const std::string& piecePlacement);
}  // namespace fen
