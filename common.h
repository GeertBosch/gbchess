#include <array>
#include <cassert>
#include <string>

#pragma once

struct Square {
    uint8_t _rank;
    uint8_t _file;

    Square(int r, int f) : _rank(r), _file(f) {}
    Square(int index) : _rank(index / 8), _file(index % 8) {}

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


enum class PieceType : uint8_t {
    INVALID,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

inline PieceType fromChar(char type) {
    switch (type) {
        case 'P':
        case 'p':
            return PieceType::PAWN;
        case 'N':
        case 'n':
            return PieceType::KNIGHT;
        case 'B':
        case 'b':
            return PieceType::BISHOP;
        case 'R':
        case 'r':
            return PieceType::ROOK;
        case 'Q':
        case 'q':
            return PieceType::QUEEN;
        case 'K':
        case 'k':
            return PieceType::KING;
        default:
            assert(false);
    }
}

inline char toChar(PieceType type, Color color) {
    switch (type) {
        case PieceType::PAWN:
            return color == Color::WHITE ? 'P' : 'p';
        case PieceType::KNIGHT:
            return color == Color::WHITE ? 'N' : 'n';
        case PieceType::BISHOP:
            return color == Color::WHITE ? 'B' : 'b';
        case PieceType::ROOK:
            return color == Color::WHITE ? 'R' : 'r';
        case PieceType::QUEEN:
            return color == Color::WHITE ? 'Q' : 'q';
        case PieceType::KING:
            return color == Color::WHITE ? 'K' : 'k';
        default:
            assert(false);
    }
}

struct Move {
    Square from;
    Square to;
    PieceType promotion = PieceType::INVALID;
    Move() : from(Square(-1, -1)), to(Square(-1, -1)) {}
    Move(const Square& fromSquare, const Square& toSquare) : from(fromSquare), to(toSquare) {}
    Move(const Square& fromSquare, const Square& toSquare, PieceType promotion) : from(fromSquare), to(toSquare), promotion(promotion) {}

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
