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

public:
    // rank and file range from 0 to 7
    constexpr Square(int file, int rank) : square(rank * kNumFiles + file) {}
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

private:
    // lldb summary-string "${var.rep}"
    enum Rep : uint8_t {
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
    union {
        uint8_t square = 0;
        Rep rep;
    };
};
constexpr Square operator""_sq(const char* str, size_t len) {
    assert(len == 2);
    assert(str[0] >= 'a' && str[0] - 'a' < kNumFiles);
    assert(str[1] >= '1' && str[1] - '1' < kNumRanks);
    return Square(str[0] - 'a', str[1] - '1');
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

inline constexpr uint8_t index(Piece piece) {
    return static_cast<uint8_t>(piece);
}
static const std::string pieceChars = "PNBRQK.pnbrqk";

constexpr PieceType type(Piece piece) {
    return static_cast<PieceType>(index(piece) % (kNumPieceTypes + 1));
}

constexpr Piece addColor(PieceType type, Color color) {
    return static_cast<Piece>(static_cast<uint8_t>(type) +
                              (color == Color::WHITE ? 0 : kNumPieceTypes + 1));
}

constexpr Color color(Piece piece) {
    return piece <= Piece::K ? Color::WHITE : Color::BLACK;
}

inline char to_char(Piece piece) {
    return pieceChars[index(piece)];
}

inline Piece toPiece(char piece) {
    auto pieceIndex = pieceChars.find(piece);
    return pieceIndex == std::string::npos ? Piece::_ : static_cast<Piece>(pieceIndex);
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
inline constexpr bool isCastles(MoveKind kind) {
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
    // lldb summary-string
    // "${var.rep.fromFile}${var.rep.fromRank} ${var.rep.toFile}${var.rep.toRank}${var.rep.kind}"

    static_assert(kNumSquares * kNumSquares * kNumMoveKinds <= (1u << 16));
    struct Rep {
        // lldb summary-string: "${var.from}${var.to} ${var.kind}"

        // Square needs to be derived from uint16_t to allow proper packing
        enum Square : uint16_t {
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

        enum Kind : uint16_t {
            // clang-format off
            quiet_move, double_push, o_o, o_o_o,
            capture, en_passant,
            N_promo = 8, B_promo, R_promo, Q_promo,
            N_promo_capture, B_promo_capture, R_promo_capture, Q_promo_capture
            // clang-format on
        };

        Square from : 6;
        Square to : 6;
        Kind kind : 4;
    };
    union {
        uint16_t move = 0;
        Rep rep;
    };

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
inline std::string to_string(MoveVector moves) {
    std::string str = "";
    for (auto&& move : moves) str += std::string(move) + " ";
    if (!str.empty()) str.pop_back();
    return str;
}

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
    Board() { _squares.fill(Piece::_); }

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
    _ = 0,
    K = 1,
    Q = 2,
    KQ = K | Q,  // 3
    k = 4,
    Kk = K | k,       // 5
    Qk = Q | k,       // 6
    KQk = K | Q | k,  // 7
    q = 8,
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
          kingSideMask(color == Color::WHITE ? CastlingMask::K : CastlingMask::k),
          queenSideMask(color == Color::WHITE ? CastlingMask::Q : CastlingMask::q),
          rookFromKingSide(Square(kNumFiles - 1, baseRank(color))),
          rookToKingSide(Square(kNumFiles - 3, baseRank(color))),
          rookFromQueenSide(Square(0, baseRank(color))),
          rookToQueenSide(Square(3, baseRank(color))),
          kingFrom(Square(kNumFiles / 2, baseRank(color))),
          kingToKingSide(Square(kNumFiles - 2, baseRank(color))),
          kingToQueenSide(Square(2, baseRank(color))) {};
};

static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::WHITE),
                                                 CastlingInfo(Color::BLACK)};

// Square to indicate no enpassant target
static constexpr auto noEnPassantTarget = Square(0);

using TwoSquares = std::array<Square, 2>;

/**
 * Succinct representation of data needed to make or unmake a move. It is sufficient to recreate a
 * Board from before the move, starting from a board after the move was made and the other way
 * around. The second and promo fields are needed for complex moves, such as castling, promotion and
 * en passant.
 */
struct BoardChange {
    Piece captured;
    TwoSquares first = {0, 0};
    uint8_t promo;
    TwoSquares second = {0, 0};
    operator std::string() {
        auto piece2str = [](Piece piece) -> std::string {
            return std::string(1, pieceChars[index(piece)]);
        };
        std::string promoChars = " nbrq";
        auto promo2str = [&promoChars](uint8_t promo) -> std::string {
            return promo ? std::string(1, promoChars[promo]) : "";
        };

        return "captured = " + piece2str(captured) + " " + std::string(first[0]) +
            std::string(first[1]) + std::string(second[0]) + std::string(second[1]) +
            promo2str(promo);
    }
};

class Turn {
    Color activeColor = Color::WHITE;
    CastlingMask castlingAvailability = CastlingMask::KQkq;  // Bitmask of CastlingMask
    Square enPassantTarget = noEnPassantTarget;
    uint8_t halfmoveClock = 0;  // If the clock is used, we'll draw at 100, well before it overflows
    uint16_t fullmoveNumber = 1;  // >65,535 moves is a lot of moves

public:
    Turn(Color active) : activeColor(active) {}
    Turn(Color active, CastlingMask castlingAvailability)
        : activeColor(active), castlingAvailability(castlingAvailability) {}
    Turn(Color active, Square enPassantTarget)
        : activeColor(active), enPassantTarget(enPassantTarget) {}

    Color active() const { return activeColor; };
    void setActive(Color active) { activeColor = active; };

    CastlingMask castling() const { return castlingAvailability; }
    void setCastling(CastlingMask castling) { castlingAvailability = castling; }

    Square enPassant() const { return enPassantTarget; }
    void setEnPassant(Square enPassant) { enPassantTarget = enPassant; }

    uint8_t halfmove() const { return halfmoveClock; }
    void setHalfmove(uint8_t halfmove) { halfmoveClock = halfmove; }

    uint16_t fullmove() const { return fullmoveNumber; }
    void setFullmove(uint16_t fullmove) { fullmoveNumber = fullmove; }
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
    Turn turn = {Color::WHITE};
    Color active() const { return turn.active(); }
};
