#include "common.h"


namespace fen {
static constexpr auto emptyPiecePlacement = "8/8/8/8/8/8/8/8";
static constexpr auto initialPiecePlacement = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
static constexpr auto initialPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/**
 * @brief Converts a Board object to a FEN piece placement string.
 *
 * @param board The Board object to convert.
 * @return std::string The FEN string representing the piece placement.
 */
std::string to_string(const Board& board);

/**
 * @brief Converts a Position object to a FEN string.
 *
 * @param position The Position object to convert.
 * @return std::string The FEN string representing the position.
 */
std::string to_string(const Position& position);

bool maybeFEN(std::string fen);

/**
 * Parses a FEN string and returns the corresponding Position object.
 *
 * @param fen The FEN string to parse.
 * @return The Position object corresponding to the given FEN string.
 */
Position parsePosition(const std::string& fen);

/**
 * @brief Parses the piece placement string of a FEN notation and returns a Board object.
 *
 * @param piecePlacement The piece placement string of a FEN notation.
 * @return Board The Board object representing the parsed FEN notation.
 */
Board parsePiecePlacement(const std::string& piecePlacement);
}  // namespace fen
