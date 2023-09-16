#include <vector>
struct Square {
    int rank; // From 0 to 7
    int file; // From 0 to 7

    Square(int r = 0, int f = 0) : rank(r), file(f) {}
};


std::vector<Square> possibleMoves(char piece, const Square& from) {
    std::vector<Square> moves;

    switch (piece) {
        case 'R': // Rook
            for (int i = 0; i < 8; ++i) {
                if (i != from.file) moves.emplace_back(from.rank, i); // Horizontal moves
                if (i != from.rank) moves.emplace_back(i, from.file); // Vertical moves
            }
            break;

        case 'B': // Bishop
            for (int i = 1; i < 8; ++i) {
                if (from.rank + i < 8 && from.file + i < 8) moves.emplace_back(from.rank + i, from.file + i);
                if (from.rank + i < 8 && from.file - i >= 0) moves.emplace_back(from.rank + i, from.file - i);
                if (from.rank - i >= 0 && from.file + i < 8) moves.emplace_back(from.rank - i, from.file + i);
                if (from.rank - i >= 0 && from.file - i >= 0) moves.emplace_back(from.rank - i, from.file - i);
            }
            break;

        case 'N': // Knight
            {
                const int dr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
                const int df[] = {-1, 1, -2, 2, -2, 2, -1, 1};
                for (int i = 0; i < 8; ++i) {
                    int newRank = from.rank + dr[i];
                    int newFile = from.file + df[i];
                    if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                        moves.emplace_back(newRank, newFile);
                    }
                }
            }
            break;

        case 'K': // King
            for (int r = -1; r <= 1; ++r) {
                for (int f = -1; f <= 1; ++f) {
                    int newRank = from.rank + r;
                    int newFile = from.file + f;
                    if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                        if (r != 0 || f != 0) moves.emplace_back(newRank, newFile);
                    }
                }
            }
            break;

        case 'Q': // Queen
            moves = possibleMoves('R', from);
            std::vector<Square> bishopMoves = possibleMoves('B', from);
            moves.insert(moves.end(), bishopMoves.begin(), bishopMoves.end());
            break;

        case 'P': // Pawn (assuming it's white for simplicity)
            if (from.rank + 1 < 8) moves.emplace_back(from.rank + 1, from.file);
            // For capturing, we would need additional info about the board.
            // So, only the basic move is added for now.
            break;

        default:
            // Handle error: invalid piece type
            break;
    }

    return moves;
}

