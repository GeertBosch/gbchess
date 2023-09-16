#include <iostream>
#include <cassert>
#include <set>

struct Square {
    int rank;
    int file;

    Square(int r = 0, int f = 0) : rank(r), file(f) {}

    // Overload the < operator for Square
    bool operator<(const Square& other) const {
        if (rank == other.rank) {
            return file < other.file;
        }
        return rank < other.rank;
    }
};

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
        case 'R':
        case 'r':
            return rookMoves(from);
        case 'N':
        case 'n':
            return knightMoves(from);
        case 'B':
        case 'b':
            return bishopMoves(from);
        case 'Q':
        case 'q':
            {
                auto rook = rookMoves(from);
                auto bishop = bishopMoves(from);
                rook.insert(bishop.begin(), bishop.end());
                return rook;
            }
        case 'K':
        case 'k':
            return kingMoves(from);
        case 'P':
            return whitePawnMoves(from);
        case 'p':
            return blackPawnMoves(from);
        default:
            return {};  // Return an empty set for invalid pieces
    }
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

