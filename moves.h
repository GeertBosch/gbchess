#include <set>

#include "common.h"

std::set<Move> availableCaptures(const ChessBoard& board, char activeColor);

std::set<Square> possibleMoves(char piece, const Square& from);
std::set<Square> possibleCaptures(char piece, const Square& from);
