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

#define dassert(condition)                      \
    {                                           \
        if constexpr (debug) assert(condition); \
    };

static constexpr uint8_t kNumFiles = 8, kNumRanks = 8;
static constexpr uint8_t kNumSquares = kNumFiles * kNumRanks;

enum class Square : uint16_t {
    // clang-format off
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8,
    // clang-format on
};

constexpr Square makeSquare(int file, int rank) {
    return Square(rank * kNumFiles + file);
}

constexpr int rank(Square square) {
    return int(square) / kNumFiles;
}
constexpr int file(Square square) {
    return int(square) % kNumRanks;
}
constexpr int index(Square square) {
    return int(square);
}

// Conversion to std::string: file to letter ('a' to 'h') and rank to digit ('1' to '8')
inline std::string to_string(Square square) {
    return {char('a' + file(square)), char('1' + rank(square))};
}

constexpr Square operator""_sq(const char* str, size_t len) {
    assert(len == 2);
    assert(str[0] >= 'a' && str[0] - 'a' < kNumFiles);
    assert(str[1] >= '1' && str[1] - '1' < kNumRanks);
    return makeSquare(str[0] - 'a', str[1] - '1');
}

enum class Color : uint8_t { w, b };

inline std::string to_string(Color color) {
    return color == Color::w ? "w" : "b";
}

constexpr Color operator!(Color color) {
    return color == Color::w ? Color::b : Color::w;
}

constexpr Color color(char color) {
    assert(color == 'w' || color == 'b');
    return color == 'b' ? Color::b : Color::w;
}

constexpr int index(Color color) {
    return static_cast<int>(color);
}

enum class PieceType : uint8_t { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
constexpr uint8_t index(PieceType type) {
    return static_cast<uint8_t>(type);
}
static constexpr uint8_t kNumPieceTypes = index(PieceType::KING) + 1;

enum class Piece : uint8_t { P, N, B, R, Q, K, _, p, n, b, r, q, k };
static constexpr uint8_t kNumPieces = static_cast<uint8_t>(Piece::k) + 1;

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
static constexpr Range pieces(Piece::P, Piece::k);

constexpr uint8_t index(Piece piece) {
    return static_cast<uint8_t>(piece);
}
static const std::string pieceChars = "PNBRQK.pnbrqk";

constexpr PieceType type(Piece piece) {
    return static_cast<PieceType>(index(piece) % (kNumPieceTypes + 1));
}

constexpr Piece addColor(PieceType type, Color color) {
    return static_cast<Piece>(static_cast<uint8_t>(type) +
                              (color == Color::w ? 0 : kNumPieceTypes + 1));
}

constexpr Color color(Piece piece) {
    return piece <= Piece::K ? Color::w : Color::b;
}

constexpr char to_char(Piece piece) {
    return pieceChars[index(piece)];
}

constexpr Piece toPiece(char piece) {
    auto pieceIndex = pieceChars.find(piece);
    return pieceIndex == std::string::npos ? Piece::_ : static_cast<Piece>(pieceIndex);
}

enum class MoveKind : uint8_t {
    // Moves that don't capture or promote
    Quiet_Move = 0,
    Double_Push = 1,
    O_O = 2,
    O_O_O = 3,

    // Captures that don't promote
    Capture = 4,
    En_Passant = 5,

    // Promotions that don't capture
    Knight_Promotion = 8,
    Bishop_Promotion = 9,
    Rook_Promotion = 10,
    Queen_Promotion = 11,

    // Promotions that capture
    Knight_Promotion_Capture = 12,
    Bishop_Promotion_Capture = 13,
    Rook_Promotion_Capture = 14,
    Queen_Promotion_Capture = 15,

    // Masks last so debugger will prefer specific enum literals
    Capture_Mask = 4,
    Promotion_Mask = 8,
};
constexpr uint8_t index(MoveKind kind) {
    return static_cast<uint8_t>(kind);
}
constexpr bool isCapture(MoveKind kind) {
    return index(kind) & index(MoveKind::Capture_Mask);
}
constexpr bool isPromotion(MoveKind kind) {
    return index(kind) & index(MoveKind::Promotion_Mask);
}
constexpr bool isCastles(MoveKind kind) {
    return kind == MoveKind::O_O || kind == MoveKind::O_O_O;
}
constexpr uint8_t kNumMoveKinds = index(MoveKind::Queen_Promotion_Capture) + 1;
constexpr uint8_t kNumNoPromoMoveKinds = index(MoveKind::En_Passant) + 1;

struct Move {

    static_assert(kNumSquares * kNumSquares * kNumMoveKinds <= (1u << 16));
    static_assert(sizeof(Square) == 2, "Square must be derived from uint16_t for proper packing");

    // Square needs to be derived from uint16_t to allow proper packing
    Square from : 6;
    Square to : 6;
    MoveKind kind : 4;

    Move() = default;
    Move(Square from, Square to, MoveKind kind) : from(from), to(to), kind(kind) {}

    // String conversion operator
    operator std::string() const {
        if (*this == Move{}) return "0000";
        auto str = to_string(from) + to_string(to);
        if (isPromotion()) str += to_char(addColor(PieceType((index(kind) & 3) + 1), Color::b));
        return str;
    }

    using Tuple = std::tuple<Square, Square, MoveKind>;
    operator Tuple() const { return {from, to, kind}; }

    operator bool() const { return from != to; }

    bool operator==(const Move& other) const {
        return from == other.from && to == other.to && kind == other.kind;
    }

    bool isPromotion() const { return kind >= MoveKind::Promotion_Mask; }
};
static_assert(sizeof(Move) == 2);

using MoveVector = std::vector<Move>;
inline std::string to_string(MoveVector moves) {
    std::string str = "";
    for (auto&& move : moves) str += std::string(move) + " ";
    if (!str.empty()) str.pop_back();
    return str;
}

class Board {
    using Squares = std::array<Piece, kNumSquares>;
    Squares _squares;

public:
    Board() { _squares.fill(Piece::_); }

    Piece& operator[](Square sq) { return _squares[index(sq)]; }
    const Piece& operator[](Square sq) const { return _squares[index(sq)]; }
    const auto& squares() const { return _squares; }
    bool operator==(const Board& other) const { return _squares == other._squares; }
    bool operator!=(const Board& other) const { return _squares != other._squares; }

    using iterator = Squares::iterator;
    iterator begin() { return _squares.begin(); }
    iterator end() { return _squares.end(); }
};

enum class CastlingMask : uint8_t {
    _ = 0,
    K = 1,
    Q = 2,
    k = 4,
    q = 8,
    KQ = K | Q,       // 3
    Kk = K | k,       // 5
    Qk = Q | k,       // 6
    KQk = K | Q | k,  // 7
    Kq = K | q,       // 9
    Qq = Q | q,       // 10
    KQq = K | Q | q,  // 11
    kq = k | q,       // 12
    Kkq = K | k | q,  // 13
    Qkq = Q | k | q,  // 14
    KQkq = KQ | kq,   // 15
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
    if ((mask & CastlingMask::K) != CastlingMask::_) str += "K";
    if ((mask & CastlingMask::Q) != CastlingMask::_) str += "Q";
    if ((mask & CastlingMask::k) != CastlingMask::_) str += "k";
    if ((mask & CastlingMask::q) != CastlingMask::_) str += "q";
    if (str == "") str = "-";
    return str;
}

// Square to indicate no enpassant target
static constexpr auto noEnPassantTarget = Square(0);

struct FromTo {
    Square from;
    Square to;
};

/**
 * Succinct representation of data needed to make or unmake a move. It is sufficient to recreate a
 * Board from before the move, starting from a board after the move was made and the other way
 * around. The second and promo fields are needed for complex moves, such as castling, promotion and
 * en passant.
 */
struct BoardChange {
    Piece captured;
    uint8_t promo;
    FromTo first = {Square(0), Square(0)};
    FromTo second = {Square(0), Square(0)};
};

class alignas(4) Turn {
    enum EnPassantTarget : uint16_t {  // 16 bits to allow better packing
        _ = 0,
        // clang-format off
        a3 = 16, b3 = 17, c3 = 18, d3 = 19, e3 = 20, f3 = 21, g3 = 22, h3 = 23,
        a6 = 24, b6 = 25, c6 = 26, d6 = 27, e6 = 28, f6 = 29, g6 = 30, h6 = 31,
        // clang-format on
    };
    static constexpr auto noEnPassantTarget = EnPassantTarget::_;
    static Square toSquare(EnPassantTarget target) {
        uint16_t value = static_cast<uint16_t>(target);
        value += (value & 8) * 2;  // Shift rank 4 to rank 6, not affecting rank 3 or value 0.
        return Square(value);
    }
    static EnPassantTarget toEnPassantTarget(Square square) {
        uint16_t value = index(square);
        value -= (value & 32) / 2;  // Shift rank 6 to rank 4, not affecting rank 3 or value 0.
        return EnPassantTarget(value);
    }

    // EnPassant target square (5 bits)
    EnPassantTarget enPassantTarget : 5;
    // Halfmove clock (7 bits)
    uint16_t halfmoveClock : 7;
    // Castling availability (4 bits)
    CastlingMask castlingAvailability : 4;

    // Rather than using a separate full move number and active color bit, we can just use the
    // number of half moves to both determine the active side (white on even, black on odd) and
    // the full move number (half moves / 2 + 1).
    uint16_t fullmoveNumber : 15;
    Color active : 1;

public:
    Turn(Color active,
         CastlingMask castlingAvailability,
         Square enPassantTarget,
         int halfmoveClock = 0,
         int fullmoveNumber = 1)
        : enPassantTarget(toEnPassantTarget(enPassantTarget)),
          halfmoveClock(halfmoveClock),
          castlingAvailability(castlingAvailability),
          fullmoveNumber(fullmoveNumber),
          active(static_cast<Color>(active)) {
        // This code path is suprisingly hot, so only assert in debug mode
        dassert(halfmoveClock >= 0 && halfmoveClock < 128);
        dassert(fullmoveNumber > 0 && fullmoveNumber < 32768);
    }

    Turn(Color color) : Turn(color, CastlingMask::KQkq, Square(noEnPassantTarget), 0, 1) {}

    Color activeColor() const { return active; };
    void setActive(Color color) { active = color; };

    CastlingMask castling() const { return castlingAvailability; }
    void setCastling(CastlingMask castling) { castlingAvailability = castling; }

    Square enPassant() const { return toSquare(enPassantTarget); }
    void setEnPassant(Square enPassant) { enPassantTarget = toEnPassantTarget(enPassant); }

    uint8_t halfmove() const { return halfmoveClock; }
    void resetHalfmove() { halfmoveClock = 0; }

    uint16_t fullmove() const { return fullmoveNumber; }

    void tick() {
        ++halfmoveClock;
        // Flip active color and increment fullmove number if active color was black
        active = !active;
        fullmoveNumber += active == Color::w;
    }
};
static_assert(sizeof(Turn) == 4);

struct alignas(8) Position {
    Board board;
    Turn turn = {Color::w};
    Color active() const { return turn.activeColor(); }
};
