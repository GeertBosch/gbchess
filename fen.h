#include "common.h"

namespace fen {
static constexpr auto emptyPiecePlacement = "8/8/8/8/8/8/8/8";
static constexpr auto initialPiecePlacement = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
static constexpr auto initialPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

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
