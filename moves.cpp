#include <vector>
#include <cctype>

struct Square {
    int rank; // From 0 to 7
    int file; // From 0 to 7

    Square(int r = 0, int f = 0) : rank(r), file(f) {}
};

std::vector<Square> rookMoves(const Square& from) {
    std::vector<Square> moves;
    for (int i = 0; i < 8; ++i) {
        if (i != from.file) moves.emplace_back(from.rank, i);
        if (i != from.rank) moves.emplace_back(i, from.file);
    }
    return moves;
}

std::vector<Square> bishopMoves(const Square& from) {
    std::vector<Square> moves;
    for (int i = 1; i < 8; ++i) {
        if (from.rank + i < 8 && from.file + i < 8) moves.emplace_back(from.rank + i, from.file + i);
        if (from.rank + i < 8 && from.file - i >= 0) moves.emplace_back(from.rank + i, from.file - i);
        if (from.rank - i >= 0 && from.file + i < 8) moves.emplace_back(from.rank - i, from.file + i);
        if (from.rank - i >= 0 && from.file - i >= 0) moves.emplace_back(from.rank - i, from.file - i);
    }
    return moves;
}

std::vector<Square> knightMoves(const Square& from) {
    const int dr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
    const int df[] = {-1, 1, -2, 2, -2, 2, -1, 1};
    std::vector<Square> moves;
    for (int i = 0; i < 8; ++i) {
        int newRank = from.rank + dr[i];
        int newFile = from.file + df[i];
        if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
            moves.emplace_back(newRank, newFile);
        }
    }
    return moves;
}

std::vector<Square> kingMoves(const Square& from) {
    std::vector<Square> moves;
    for (int r = -1; r <= 1; ++r) {
        for (int f = -1; f <= 1; ++f) {
            int newRank = from.rank + r;
            int newFile = from.file + f;
            if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                if (r != 0 || f != 0) moves.emplace_back(newRank, newFile);
            }
        }
    }
    return moves;
}

std::vector<Square> pawnMoves(char piece, const Square& from) {
    std::vector<Square> moves;

    if (piece == 'P') {
        if (from.rank == 1 && from.rank + 2 < 8) {
            moves.emplace_back(from.rank + 2, from.file);
        }
        if (from.rank + 1 < 8) {
            moves.emplace_back(from.rank + 1, from.file);
        }
    } else if (piece == 'p') {
        if (from.rank == 6 && from.rank - 2 >= 0) {
            moves.emplace_back(from.rank - 2, from.file);
        }
        if (from.rank - 1 >= 0) {
            moves.emplace_back(from.rank - 1, from.file);
        }
    }

    return moves;
}

std::vector<Square> possibleMoves(char piece, const Square& from) {
    switch (std::tolower(piece)) {
        case 'r': return rookMoves(from);
        case 'b': return bishopMoves(from);
        case 'n': return knightMoves(from);
        case 'k': return kingMoves(from);
        case 'q': {
            std::vector<Square> moves = rookMoves(from);
            std::vector<Square> bishop_moves = bishopMoves(from);
            moves.insert(moves.end(), bishop_moves.begin(), bishop_moves.end());
            return moves;
        }
        case 'p': return pawnMoves(piece, from);
        default:
            return {}; // Invalid piece
    }
}

