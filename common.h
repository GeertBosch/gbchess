#include <string>

struct ChessPosition {
    std::string piecePlacement;
    char activeColor;
    std::string castlingAvailability;
    std::string enPassantTarget;
    int halfmoveClock;
    int fullmoveNumber;
};

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

    bool operator==(const Square& other) const {
	return rank == other.rank && file == other.file;
    }
};

struct Move {
    Square from;
    Square to;

    Move(const Square& fromSquare, const Square& toSquare) : from(fromSquare), to(toSquare) {}

    // Necessary for using the Move struct in a std::set
    bool operator<(const Move& other) const {
        if (from < other.from) return true;
        if (from == other.from) return to < other.to;
        return false;
    }
};

struct ChessBoard {
    char squares[8][8];

    ChessBoard() {
        for (auto& row : squares) {
            for (char& cell : row) {
                cell = ' ';
            }
        }
    }

    char& operator[](const Square& sq) {
        return squares[sq.rank][sq.file];
    }

    const char& operator[](const Square& sq) const {
        return squares[sq.rank][sq.file];
    }
};
