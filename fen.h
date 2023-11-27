#include "common.h"

std::string toString(const Board& board);
std::ostream& operator<<(std::ostream& os, const Board& board);

/**
 * @brief Parses the piece placement string of a FEN notation and returns a Board object.
 *
 * @param piecePlacement The piece placement string of a FEN notation.
 * @return Board The Board object representing the parsed FEN notation.
 */
Board parsePiecePlacement(const std::string& piecePlacement);

/**
 * Parses a FEN string and returns the corresponding Position object.
 *
 * @param fen The FEN string to parse.
 * @return The Position object corresponding to the given FEN string.
 */
Position parseFEN(const std::string& fen);