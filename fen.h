#include "common.h"

std::string toString(const ChessBoard& board);
std::ostream& operator<<(std::ostream& os, const ChessBoard& board);

/**
 * @brief Parses the piece placement string of a FEN notation and returns a ChessBoard object.
 *
 * @param piecePlacement The piece placement string of a FEN notation.
 * @return ChessBoard The ChessBoard object representing the parsed FEN notation.
 */
ChessBoard parsePiecePlacement(const std::string& piecePlacement);

/**
 * Parses a FEN string and returns the corresponding ChessPosition object.
 *
 * @param fen The FEN string to parse.
 * @return The ChessPosition object corresponding to the given FEN string.
 */
ChessPosition parseFEN(const std::string& fen);