#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#pragma once

#ifdef DEBUG
constexpr bool debug = 1;
#else
constexpr bool debug = 0;
#endif

static constexpr uint8_t kNumFiles = 8, kNumRanks = 8;
static constexpr uint8_t kNumSquares = kNumFiles * kNumRanks;

class Square {
    uint8_t square = 0;

public:
    // rank and file range from 0 to 7
    constexpr Square(int rank, int file) : square(rank * kNumFiles + file) {}
    constexpr Square(int index) : square(index) {}

    int rank() const { return square / kNumFiles; }
    int file() const { return square % kNumRanks; }
    int index() const { return square; }

    Square operator++() { return ++square, *this; }

    bool operator==(Square other) const { return square == other.square; }
    bool operator!=(Square other) const { return square != other.square; }

    // Conversion to std::string: file to letter ('a' to 'h') and rank to digit ('1' to '8')
    operator std::string() const {
        std::string str = "";
        str += 'a' + file();
        str += '1' + rank();
        return str;
    }
};
constexpr Square operator"" _sq(const char* str, size_t len) {
    assert(len == 2);
    assert(str[0] >= 'a' && str[0] - 'a' < kNumFiles);
    assert(str[1] >= '1' && str[1] - '1' < kNumRanks);
    return Square(str[1] - '1', str[0] - 'a');
}

enum class Color : uint8_t { WHITE, BLACK };

inline std::string to_string(Color color) {
    return color == Color::WHITE ? "w" : "b";
}

constexpr Color operator!(Color color) {
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

constexpr Color color(char color) {
    assert(color == 'w' || color == 'b');
    return color == 'b' ? Color::BLACK : Color::WHITE;
}

constexpr uint8_t baseRank(Color color) {
    return color == Color::WHITE ? 0 : kNumRanks - 1;
}

enum class PieceType : uint8_t { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
inline constexpr uint8_t index(PieceType type) {
    return static_cast<uint8_t>(type);
}
inline constexpr char to_char(PieceType type) {
    return "pnbrqk"[index(type)];
}
static constexpr uint8_t kNumPiecesTypes = index(PieceType::KING) + 1;

enum class Piece : uint8_t {
    WHITE_PAWN,
    WHITE_KNIGHT,
    WHITE_BISHOP,
    WHITE_ROOK,
    WHITE_QUEEN,
    WHITE_KING,
    NONE,
    BLACK_PAWN,
    BLACK_KNIGHT,
    BLACK_BISHOP,
    BLACK_ROOK,
    BLACK_QUEEN,
    BLACK_KING
};
static constexpr uint8_t kNumPieces = static_cast<uint8_t>(Piece::BLACK_KING) + 1;

template <typename T>
class Range {
public:
    class iterator {
        T value;

    public:
        constexpr iterator(T value) : value(value) {}
        T operator*() const { return value; }
        iterator& operator++() {
            value = static_cast<T>(static_cast<size_t>(value) + 1);
            return *this;
        }
        bool operator==(iterator other) const { return value == other.value; }
        bool operator!=(iterator other) const { return value != other.value; }
    };
    constexpr Range(T first, T last)
        : _begin(first), _end(static_cast<T>(static_cast<size_t>(last) + 1)) {}

    iterator begin() const { return _begin; }
    iterator end() const { return _end; }

private:
    const iterator _begin;
    const iterator _end;
};
static constexpr Range pieces(Piece::WHITE_PAWN, Piece::BLACK_KING);

inline constexpr uint8_t index(Piece piece) {
    return static_cast<uint8_t>(piece);
}
static const std::string pieceChars = "PNBRQK.pnbrqk";

constexpr PieceType type(Piece piece) {
    return static_cast<PieceType>(index(piece) % (kNumPiecesTypes + 1));
}

constexpr Piece addColor(PieceType type, Color color) {
    return static_cast<Piece>(static_cast<uint8_t>(type) +
                              (color == Color::WHITE ? 0 : kNumPiecesTypes + 1));
}

constexpr Color color(Piece piece) {
    return piece <= Piece::WHITE_KING ? Color::WHITE : Color::BLACK;
}

inline char to_char(Piece piece) {
    return pieceChars[index(piece)];
}

inline Piece toPiece(char piece) {
    auto pieceIndex = pieceChars.find(piece);
    return pieceIndex == std::string::npos ? Piece::NONE : static_cast<Piece>(pieceIndex);
}

enum class MoveKind : uint8_t {
    // Moves that don't capture or promote
    QUIET_MOVE = 0,
    DOUBLE_PAWN_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,

    // Captures that don't promote
    CAPTURE_MASK = 4,
    CAPTURE = 4,
    EN_PASSANT = 5,

    // Promotions that don't capture
    PROMOTION_MASK = 8,
    KNIGHT_PROMOTION = 8,
    BISHOP_PROMOTION = 9,
    ROOK_PROMOTION = 10,
    QUEEN_PROMOTION = 11,

    // Promotions that capture
    KNIGHT_PROMOTION_CAPTURE = 12,
    BISHOP_PROMOTION_CAPTURE = 13,
    ROOK_PROMOTION_CAPTURE = 14,
    QUEEN_PROMOTION_CAPTURE = 15,
};
inline constexpr uint8_t index(MoveKind kind) {
    return static_cast<uint8_t>(kind);
}
inline constexpr bool isCapture(MoveKind kind) {
    return (static_cast<uint8_t>(kind) & static_cast<uint8_t>(MoveKind::CAPTURE_MASK)) != 0;
}
inline constexpr bool isPromotion(MoveKind kind) {
    return (static_cast<uint8_t>(kind) & static_cast<uint8_t>(MoveKind::PROMOTION_MASK)) != 0;
}
inline constexpr bool isCastle(MoveKind kind) {
    return kind == MoveKind::KING_CASTLE || kind == MoveKind::QUEEN_CASTLE;
}
static constexpr uint8_t kNumMoveKinds = index(MoveKind::QUEEN_PROMOTION_CAPTURE) + 1;
static constexpr uint8_t kNumNoPromoMoveKinds = index(MoveKind::EN_PASSANT) + 1;

inline int noPromo(MoveKind kind) {
    MoveKind noPromoKinds[] = {MoveKind::QUIET_MOVE,
                               MoveKind::DOUBLE_PAWN_PUSH,
                               MoveKind::KING_CASTLE,
                               MoveKind::QUEEN_CASTLE,
                               MoveKind::CAPTURE,
                               MoveKind::EN_PASSANT,
                               MoveKind::QUIET_MOVE,
                               MoveKind::QUIET_MOVE,
                               MoveKind::QUIET_MOVE,
                               MoveKind::QUIET_MOVE,
                               MoveKind::QUIET_MOVE,
                               MoveKind::QUIET_MOVE,
                               MoveKind::CAPTURE,
                               MoveKind::CAPTURE,
                               MoveKind::CAPTURE,
                               MoveKind::CAPTURE};
    return index(noPromoKinds[index(kind)]);
}

inline PieceType promotionType(MoveKind kind) {
    return static_cast<PieceType>((static_cast<uint8_t>(kind) & 3) + 1);
}

class Move {
    uint16_t move = 0;
    static_assert(kNumSquares * kNumSquares * kNumMoveKinds <= (1u << 16));

public:
    static constexpr MoveKind QUIET = MoveKind::QUIET_MOVE;
    static constexpr MoveKind CAPTURE = MoveKind::CAPTURE;
    Move() = default;
    Move(Square from, Square to, MoveKind kind)
        : move(from.index() + to.index() * kNumSquares + index(kind) * kNumSquares * kNumSquares) {}

    // String conversion operator
    operator std::string() const {
        if (!move) return "0000";
        auto str = static_cast<std::string>(from()) + static_cast<std::string>(to());
        if (isPromotion()) str += to_char(promotionType(kind()));
        return str;
    }

    using Tuple = std::tuple<Square, Square, MoveKind>;
    operator Tuple() const { return {from(), to(), kind()}; }

    Square from() const { return move % kNumSquares; }
    Square to() const { return (move / kNumSquares) % kNumSquares; }
    MoveKind kind() const { return MoveKind(move / (kNumSquares * kNumSquares)); }

    operator bool() const { return from().index() != to().index(); }

    bool operator==(Move other) { return move == other.move; }

    bool isPromotion() const { return kind() >= MoveKind::PROMOTION_MASK; }
};
using MoveVector = std::vector<Move>;

class Board {
    using Squares = std::array<Piece, kNumSquares>;
    Squares _squares;

    // The minimal space required is 64 bits for occupied squares (8 bytes), plus 6 bits for white
    // king pos, plus 6 bits for black king, plus 30 * log2(10) = 102 bits for identification of the
    // pieces, which would be 21 bytes. For practical purposes, we can just use 4 bits per piece and
    // avoid the special king encoding, so we'd end up with 8 bytes for the occupied squares and
    // 32*4 = 128 bits for the pieces, which would be 24 bytes in total. The question is whether the
    // advantage of having the occupancy bitset available outweighs the disadvantage of having to do
    // the bit twiddling to get the piece type and color.

public:
    Board() { _squares.fill(Piece::NONE); }

    Piece& operator[](Square sq) { return _squares[sq.index()]; }
    const Piece& operator[](Square sq) const { return _squares[sq.index()]; }
    const auto& squares() const { return _squares; }
    bool operator==(const Board& other) const { return _squares == other._squares; }
    bool operator!=(const Board& other) const { return _squares != other._squares; }

    using iterator = Squares::iterator;
    iterator begin() { return _squares.begin(); }
    iterator end() { return _squares.end(); }
};

enum class CastlingMask : uint8_t {
    NONE = 0,
    WHITE_KINGSIDE = 1,
    WHITE_QUEENSIDE = 2,
    WHITE = WHITE_KINGSIDE | WHITE_QUEENSIDE,
    BLACK_KINGSIDE = 4,
    BLACK_QUEENSIDE = 8,
    BLACK = BLACK_KINGSIDE | BLACK_QUEENSIDE,
    ALL = WHITE | BLACK,
};
inline CastlingMask operator&(CastlingMask lhs, CastlingMask rhs) {
    return CastlingMask(uint8_t(lhs) & uint8_t(rhs));
}
inline CastlingMask operator|(CastlingMask lhs, CastlingMask rhs) {
    return CastlingMask(uint8_t(lhs) | uint8_t(rhs));
}
inline CastlingMask operator&=(CastlingMask& lhs, CastlingMask rhs) {
    return lhs = CastlingMask(uint8_t(lhs) & uint8_t(rhs));
}
inline CastlingMask operator|=(CastlingMask& lhs, CastlingMask rhs) {
    return lhs = CastlingMask(uint8_t(lhs) | uint8_t(rhs));
}
inline CastlingMask operator~(CastlingMask lhs) {
    return static_cast<CastlingMask>(~static_cast<uint8_t>(lhs));
}
inline std::string to_string(CastlingMask mask) {
    std::string str = "";
    if ((mask & CastlingMask::WHITE_KINGSIDE) != CastlingMask::NONE) str += "K";
    if ((mask & CastlingMask::WHITE_QUEENSIDE) != CastlingMask::NONE) str += "Q";
    if ((mask & CastlingMask::BLACK_KINGSIDE) != CastlingMask::NONE) str += "k";
    if ((mask & CastlingMask::BLACK_QUEENSIDE) != CastlingMask::NONE) str += "q";
    if (str == "") str = "-";
    return str;
}

struct CastlingInfo {
    const Piece rook;
    const Piece king;
    const CastlingMask kingSideMask;
    const CastlingMask queenSideMask;
    const Square rookFromKingSide;
    const Square rookToKingSide;
    const Square rookFromQueenSide;
    const Square rookToQueenSide;
    const Square kingFrom;
    const Square kingToKingSide;
    const Square kingToQueenSide;

    constexpr CastlingInfo(Color color)
        : rook(addColor(PieceType::ROOK, color)),
          king(addColor(PieceType::KING, color)),
          kingSideMask(color == Color::WHITE ? CastlingMask::WHITE_KINGSIDE
                                             : CastlingMask::BLACK_KINGSIDE),
          queenSideMask(color == Color::WHITE ? CastlingMask::WHITE_QUEENSIDE
                                              : CastlingMask::BLACK_QUEENSIDE),
          rookFromKingSide(Square(baseRank(color), kNumFiles - 1)),
          rookToKingSide(Square(baseRank(color), kNumFiles - 3)),
          rookFromQueenSide(Square(baseRank(color), 0)),
          rookToQueenSide(Square(baseRank(color), 3)),
          kingFrom(Square(baseRank(color), kNumFiles / 2)),
          kingToKingSide(Square(baseRank(color), kNumFiles - 2)),
          kingToQueenSide(Square(baseRank(color), 2)){};
};

static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::WHITE),
                                                 CastlingInfo(Color::BLACK)};

// Square to indicate no enpassant target
static constexpr auto noEnPassantTarget = Square(0);

struct Turn {
    Color activeColor = Color::WHITE;
    CastlingMask castlingAvailability = CastlingMask::ALL;  // Bitmask of CastlingMask
    Square enPassantTarget = noEnPassantTarget;
    uint8_t halfmoveClock = 0;  // If the clock is used, we'll draw at 100, well before it overflows
    uint16_t fullmoveNumber = 1;  // >65,535 moves is a lot of moves
};

struct Position {
    // File indices for standard castling, not chess960
    static const int kQueenSideRookFile = 0;
    static const int kKingSideRookFile = kNumFiles - 1;
    static const int kKingFile = kNumFiles / 2;

    static const int kRookCastledQueenSideFile = 3;
    static const int kKingCastledQueenSideFile = 2;
    static const int kRookCastledKingSideFile = kNumFiles - 3;
    static const int kKingCastledKingSideFile = kNumFiles - 2;

    // Base positions of pieces involved in castling
    static constexpr auto whiteQueenSideRook = "a1"_sq;
    static constexpr auto whiteKing = "e1"_sq;
    static constexpr auto whiteKingSideRook = "h1"_sq;
    static constexpr auto blackQueenSideRook = "a8"_sq;
    static constexpr auto blackKing = "e8"_sq;
    static constexpr auto blackKingSideRook = "h8"_sq;

    // Positions of castled pieces
    static constexpr auto whiteRookCastledQueenSide = "c1"_sq;
    static constexpr auto whiteRookCastledKingSide = "f1"_sq;
    static constexpr auto whiteKingCastledQueenSide = "b1"_sq;
    static constexpr auto whiteKingCastledKingSide = "g1"_sq;
    static constexpr auto blackRookCastledQueenSide = "c8"_sq;
    static constexpr auto blackRookCastledKingSide = "f8"_sq;
    static constexpr auto blackKingCastledQueenSide = "b8"_sq;
    static constexpr auto blackKingCastledKingSide = "g8"_sq;

    Board board;
    Turn turn;
    Color activeColor() const { return turn.activeColor; }
};
