#include <array>
#include <cassert>
#include <string>

#pragma once

struct Square {
    uint8_t _rank;
    uint8_t _file;

    Square(int r, int f) : _rank(r), _file(f) {}
    Square(int index) : _rank(index / 8), _file(index % 8) {}

    // Overload the < operator for Square
    bool operator<(const Square& other) const {
        if (_rank == other._rank) {
            return _file < other._file;
        }
        return _rank < other._rank;
    }

    int rank() const {
        return _rank;
    }

    int file() const {
        return _file;
    }

    int index() const {
        return _rank * 8 + _file;
    }

    bool operator==(const Square& other) const {
	    return _rank == other._rank && _file == other._file;
    }

    // Conversion to std::string
    operator std::string() const {
        std::string squareStr;
        squareStr += char('a' + _file);  // Convert file to letter ('a' to 'h')
        squareStr += char('1' + _rank);  // Convert rank to digit ('1' to '8')
        return squareStr;
    }
};

struct Move {
    Square from;
    Square to;
    char promotion = 'Q'; // Default promotion to Queen

    Move() : from(Square(-1, -1)), to(Square(-1, -1)) {}
    Move(const Square& fromSquare, const Square& toSquare) : from(fromSquare), to(toSquare) {}
    Move(const Square& fromSquare, const Square& toSquare, char promotionPiece) : from(fromSquare), to(toSquare), promotion(promotionPiece) {}

    bool operator<(const Move& rhs) const {
        if (from < rhs.from) return true;
        if (rhs.from < from) return false;
        if (to < rhs.to) return true;
        if (rhs.to < to) return false;
        return promotion < rhs.promotion;
    }

    // String conversion operator
    operator std::string() const {
        return static_cast<std::string>(from) + " " + static_cast<std::string>(to);
    }

    operator bool() const {
        return from._rank != to._rank || from._file != to._file;
    }
};

class ChessBoard {
    std::array<char, 64> squares;

public:
    ChessBoard() {
        squares.fill(' ');
    }

    char& operator[](const Square& sq) {
        int index = sq._rank * 8 + sq._file;
        return squares[index];
    }

    const char& operator[](const Square& sq) const {
        int index = sq._rank * 8 + sq._file;
        return squares[index];
    }
};

enum class Color : uint8_t {
    WHITE,
    BLACK
};

inline std::string to_string(Color color) {
    return color == Color::WHITE ? "w" : "b";
}

inline Color operator!(Color color) {
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

inline Color color (char color) {
    assert(color == 'w' || color == 'b');
    return color == 'b' ? Color::BLACK : Color::WHITE;
}

enum CastlingAvailability : uint8_t {
    WHITE_KINGSIDE = 1,
    WHITE_QUEENSIDE = 2,
    BLACK_KINGSIDE = 4,
    BLACK_QUEENSIDE = 8
};

struct ChessPosition {
    ChessBoard board;
    uint8_t castlingAvailability;
    Color activeColor;
    uint8_t halfmoveClock; // If the clock is used, we'll draw before it overflows
    uint16_t fullmoveNumber; // >65,535 moves is a lot of moves
    Square enPassantTarget = 0; // 0 indicates no en passant target
};
