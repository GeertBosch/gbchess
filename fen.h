#include "common.h"

std::string toString(const ChessBoard& board);
std::ostream& operator<<(std::ostream& os, const ChessBoard& board);

ChessBoard parsePiecePlacement(const std::string& piecePlacement);
ChessPosition parseFEN(const std::string& fen);