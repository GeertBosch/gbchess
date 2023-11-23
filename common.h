#include <array>
#include <cassert>
#include <string>

#pragma once

class Square {
    uint8_t _index;
public:
    Square(int rank, int file) : _index(rank * 8 + file) {}
    Square(int index) : _index(index) {}

    int rank() const {
        return _index / 8;
    }

    int file() const {
        return _index % 8;
    }
    int index() const {
        return _index;
    }

    Square operator++() {
        ++_index;
        return *this;
    }

    bool operator==(Square other) const {
        return _index == other._index;
    }
    bool operator!=(Square other) const {
        return _index != other._index;
    }

    // Conversion to std::string
    operator std::string() const {
        std::string squareStr;
        squareStr += char('a' + file());  // Convert file to letter ('a' to 'h')
        squareStr += char('1' + rank());  // Convert rank to digit ('1' to '8')
        return squareStr;
    }
};
static constexpr uint8_t kNumSquares = 64;

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
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};
static constexpr uint8_t kNumPiecesTypes = static_cast<uint8_t>(PieceType::KING) + 1;

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

enum class Piece : uint8_t {
    NONE,
    WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
    BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING
};
static constexpr uint8_t kNumPieces = static_cast<uint8_t>(Piece::BLACK_KING) + 1;

inline char to_char(Piece piece) {
    char pieceChars[] = {'.', 'P', 'N', 'B', 'R', 'Q', 'K', 'p', 'n', 'b', 'r', 'q', 'k'};
    return pieceChars[static_cast<uint8_t>(piece)];
}

inline Color color(Piece piece) {
    return piece <= Piece::WHITE_KING ? Color::WHITE : Color::BLACK;
}

inline Piece toPiece(char piece) {
    std::string pieceChars = " PNBRQKpnbrqk";
    auto pieceIndex = pieceChars.find(piece);
    return pieceIndex == std::string::npos ? Piece::NONE : static_cast<Piece>(pieceIndex);
}

inline PieceType type(Piece piece) {
    return static_cast<PieceType>((static_cast<uint8_t>(piece) - 1) % kNumPiecesTypes);
}

inline Piece addColor(PieceType type, Color color) {
    return static_cast<Piece>(static_cast<uint8_t>(type)
      + (color == Color::WHITE ? 0 : kNumPiecesTypes) + 1);
}

struct Move {
    Square from;
    Square to;
    PieceType promotion = PieceType::PAWN; // PAWN indicates no promotion (default)

    Move() : from(Square(-1, -1)), to(Square(-1, -1)) {}
    Move(Square fromSquare, Square toSquare) : from(fromSquare), to(toSquare) {}
    Move(Square fromSquare, Square toSquare, PieceType promotion)
        : from(fromSquare), to(toSquare), promotion(promotion) {}

    // String conversion operator
    operator std::string() const {
        return static_cast<std::string>(from) + " " + static_cast<std::string>(to);
    }

    operator bool() const {
        return from.index() != to.index();
    }

    bool operator<(Move other) const {
        if (from.index() < other.from.index()) {
            return true;
        } else if (from.index() == other.from.index()) {
            if (to.index() < other.to.index()) {
                return true;
            } else if (to.index() == other.to.index()) {
                return promotion < other.promotion;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
};

class ChessBoard {
    std::array<Piece, 64> _squares;

public:
    ChessBoard() {
        _squares.fill(Piece::NONE);
    }

    Piece& operator[](Square sq) {
        return _squares[sq.index()];
    }

    const Piece operator[](Square sq) const {
        return _squares[sq.index()];
    }

    const auto& squares() const {
        return _squares;
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
