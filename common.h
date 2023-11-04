#include <string>

#pragma once

struct Square {
    int rank;
    int file;

    Square(int r, int f) : rank(r), file(f) {}

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

    // Conversion to std::string
    operator std::string() const {
        std::string squareStr;
        squareStr += char('a' + file);  // Convert file to letter ('a' to 'h')
        squareStr += char('1' + rank);  // Convert rank to digit ('1' to '8')
        return squareStr;
    }
};

struct Move {
    Square from;
    Square to;
    char promotion = 'Q'; // Default promotion to Queen

    Move(const Square& fromSquare, const Square& toSquare) : from(fromSquare), to(toSquare) {}

    bool operator<(const Move& rhs) const {
        if (from < rhs.from) return true;
        if (rhs.from < from) return false;
        if (to < rhs.to) return true;
        if (rhs.to < to) return false;
        return promotion < rhs.promotion;
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

struct ChessPosition {
    ChessBoard board;
    char activeColor;
    std::string castlingAvailability;
    std::string enPassantTarget;
    int halfmoveClock;
    int fullmoveNumber;
};
