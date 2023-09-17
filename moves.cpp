#include <cassert>
#include <cmath>
#include <iostream>
#include <set>

#include "common.h"

std::set<Square> rookMoves(const Square& from) {
    std::set<Square> moves;
    for (int i = 0; i < 8; ++i) {
        if (i != from.rank) {
            moves.insert(Square(i, from.file));
        }
        if (i != from.file) {
            moves.insert(Square(from.rank, i));
        }
    }
    return moves;
}

std::set<Square> bishopMoves(const Square& from) {
    std::set<Square> moves;
    for (int i = 1; i < 8; ++i) {
        if (from.rank + i < 8 && from.file + i < 8) {
            moves.insert(Square(from.rank + i, from.file + i));
        }
        if (from.rank - i >= 0 && from.file - i >= 0) {
            moves.insert(Square(from.rank - i, from.file - i));
        }
        if (from.rank + i < 8 && from.file - i >= 0) {
            moves.insert(Square(from.rank + i, from.file - i));
        }
        if (from.rank - i >= 0 && from.file + i < 8) {
            moves.insert(Square(from.rank - i, from.file + i));
        }
    }
    return moves;
}

std::set<Square> queenMoves(const Square& from) {
    std::set<Square> moves;

    // Combine the moves of a rook and a bishop
    auto rookMvs = rookMoves(from);
    auto bishopMvs = bishopMoves(from);

    moves.insert(rookMvs.begin(), rookMvs.end());
    moves.insert(bishopMvs.begin(), bishopMvs.end());

    return moves;
}

std::set<Square> knightMoves(const Square& from) {
    std::set<Square> moves;
    int knight_moves[8][2] = {{-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};
    for (int i = 0; i < 8; ++i) {
        int new_rank = from.rank + knight_moves[i][0];
        int new_file = from.file + knight_moves[i][1];
        if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
            moves.insert(Square(new_rank, new_file));
        }
    }
    return moves;
}

std::set<Square> kingMoves(const Square& from) {
    std::set<Square> moves;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (i != 0 || j != 0) {
                int new_rank = from.rank + i;
                int new_file = from.file + j;
                if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
                    moves.insert(Square(new_rank, new_file));
                }
            }
        }
    }
    return moves;
}

std::set<Square> whitePawnMoves(const Square& from) {
    std::set<Square> moves;
    if (from.rank == 1 && from.rank + 2 < 8) {
        moves.insert(Square(from.rank + 2, from.file));
    }
    if (from.rank + 1 < 8) {
        moves.insert(Square(from.rank + 1, from.file));
    }
    return moves;
}

std::set<Square> blackPawnMoves(const Square& from) {
    std::set<Square> moves;
    if (from.rank == 6 && from.rank - 2 >= 0) {
        moves.insert(Square(from.rank - 2, from.file));
    }
    if (from.rank - 1 >= 0) {
        moves.insert(Square(from.rank - 1, from.file));
    }
    return moves;
}

std::set<Square> possibleMoves(char piece, const Square& from) {
    switch (piece) {
        case 'P':
            return whitePawnMoves(from);
        case 'p':
            return blackPawnMoves(from);
        case 'N': case 'n':
            return knightMoves(from);
        case 'B': case 'b':
            return bishopMoves(from);
        case 'R': case 'r':
            return rookMoves(from);
        case 'Q': case 'q':
            return queenMoves(from);
        case 'K': case 'k':
            return kingMoves(from);
        default:
            return {};
    }
}

void addCaptureIfOnBoard(std::set<Square>& captures, int rank, int file) {
    if (rank >= 0 && rank < 8 && file >= 0 && file < 8) {
        captures.insert({rank, file});
    }
}

std::set<Square> possibleCaptures(char piece, const Square& from) {
    switch (piece) {
        case 'P':  // White Pawn
        {
            std::set<Square> captures;
            addCaptureIfOnBoard(captures, from.rank - 1, from.file - 1);  // Diagonal left
            addCaptureIfOnBoard(captures, from.rank - 1, from.file + 1);  // Diagonal right
            return captures;
        }
        case 'p':  // Black Pawn
        {
            std::set<Square> captures;
            addCaptureIfOnBoard(captures, from.rank + 1, from.file - 1);  // Diagonal left
            addCaptureIfOnBoard(captures, from.rank + 1, from.file + 1);  // Diagonal right
            return captures;
        }
        case 'N': case 'n':  // Knight (Both white and black)
            return knightMoves(from);
        case 'B': case 'b':  // Bishop (Both white and black)
            return bishopMoves(from);
        case 'R': case 'r':  // Rook (Both white and black)
            return rookMoves(from);
        case 'Q': case 'q':  // Queen (Both white and black)
            return queenMoves(from);
        case 'K': case 'k':  // King (Both white and black)
            return kingMoves(from);
        default:
            return {};
    }
}

bool movesThroughPieces(const ChessBoard& board, const Square& from, const Square& to) {
    int rankDiff = to.rank - from.rank;
    int fileDiff = to.file - from.file;

    // Check if the move isn't horizontal, vertical, or diagonal
    if (rankDiff != 0 && fileDiff != 0 && abs(rankDiff) != abs(fileDiff)) {
        return false; // It's not in straight line, thus no need to check further
    }

    // Calculate the direction of movement for rank and file
    int rankStep = (rankDiff != 0) ? rankDiff / abs(rankDiff) : 0;
    int fileStep = (fileDiff != 0) ? fileDiff / abs(fileDiff) : 0;

    int rankPos = from.rank + rankStep;
    int filePos = from.file + fileStep;

    while (rankPos != to.rank || filePos != to.file) {
        if (board.squares[rankPos][filePos] != ' ') {
            return true; // There's a piece in the way
        }
        rankPos += rankStep;
        filePos += fileStep;
    }
    return false;
}

/**
 * This availableMoves function iterates over each square on the board. If a piece of the
 * active color is found, it calculates its possible moves using the possibleMoves function
 * you already have. For each possible destination square, it checks if the move would
 * result in self-blocking or moving through other pieces using the movesThroughPieces
 * function. If neither condition is true, the move is added to the set.
 */
std::set<Move> availableMoves(const ChessBoard& board, char activeColor) {
    std::set<Move> moves;

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square currentSquare{rank, file};
            char piece = board[currentSquare];

            // Skip if the square is empty or if the piece isn't the active color
            if (piece == ' ' || (activeColor == 'w' && std::islower(piece)) || (activeColor == 'b' && std::isupper(piece))) {
                continue;
            }

            auto possibleSquares = possibleMoves(piece, currentSquare);
            for (const auto& dest : possibleSquares) {
                // Check for self-blocking or moving through pieces
                if (board[dest] == ' ' && !movesThroughPieces(board, currentSquare, dest)) {
                    moves.insert({currentSquare, dest});
                }
            }
        }
    }

    return moves;
}

void testPossibleMoves() {
    // Test rook moves
    {
        auto moves = possibleMoves('R', Square(0, 0));
        assert(moves.size() == 14);  
        assert(moves.count(Square(0, 7)) == 1);  
        assert(moves.count(Square(7, 0)) == 1);  
    }

    // Test bishop moves
    {
        auto moves = possibleMoves('B', Square(0, 0));
        assert(moves.size() == 7);  
        assert(moves.count(Square(7, 7)) == 1);
    }

    // Test knight moves
    {
        auto moves = possibleMoves('N', Square(0, 0));
        assert(moves.size() == 2);  
        assert(moves.count(Square(1, 2)) == 1);  
        assert(moves.count(Square(2, 1)) == 1);  
    }

    // Test king moves
    {
        auto moves = possibleMoves('K', Square(0, 0));
        assert(moves.size() == 3);  
    }

    // Test queen moves
    {
        auto moves = possibleMoves('Q', Square(0, 0));
        assert(moves.size() == 21); 
    }

    // Test white pawn moves
    {
        auto moves = possibleMoves('P', Square(1, 0));
        assert(moves.size() == 2);  
        assert(moves.count(Square(2, 0)) == 1);  
        assert(moves.count(Square(3, 0)) == 1);  
    }

    // Test black pawn moves
    {
        auto moves = possibleMoves('p', Square(6, 0));
        assert(moves.size() == 2);  
        assert(moves.count(Square(5, 0)) == 1);  
        assert(moves.count(Square(4, 0)) == 1);  
    }

    std::cout << "All tests passed!" << std::endl;
}

int main() {
    testPossibleMoves();
    return 0;
}

