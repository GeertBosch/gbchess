#include <cassert>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>

#ifdef __SSE2__
constexpr bool haveSSE2 = true;
#include <emmintrin.h>
#else

#ifdef DEBUG
// If we don't have SSE2, use the SSE2 emulation for testing purposes
constexpr bool haveSSE2 = true;
#else
constexpr bool haveSSE2 = false;
#endif

typedef __attribute__((__vector_size__(2 * sizeof(long long)))) long long __m128i;
__m128i _mm_cmpeq_epi8(__m128i x, __m128i y) {
    __m128i z;
    char xm[16];
    char ym[16];
    char zm[16];

    memcpy(xm, &x, sizeof(x));
    memcpy(ym, &y, sizeof(y));

    for (int i = 0; i < sizeof(xm); ++i) zm[i] = xm[i] == ym[i] ? 0xff : 0;

    memcpy(&z, zm, sizeof(z));
    return z;
}
__m128i _mm_cmplt_epi8(__m128i x, __m128i y) {
    __m128i z;
    char xm[16];
    char ym[16];
    char zm[16];

    memcpy(xm, &x, sizeof(x));
    memcpy(ym, &y, sizeof(y));

    for (int i = 0; i < sizeof(xm); ++i) zm[i] = xm[i] < ym[i] ? 0xff : 0;

    memcpy(&z, zm, sizeof(z));
    return z;
}
__m128i _mm_cmpgt_epi8(__m128i x, __m128i y) {
    __m128i z;
    char xm[16];
    char ym[16];
    char zm[16];

    memcpy(xm, &x, sizeof(x));
    memcpy(ym, &y, sizeof(y));

    for (int i = 0; i < sizeof(xm); ++i) zm[i] = xm[i] > ym[i] ? 0xff : 0;

    memcpy(&z, zm, sizeof(z));
    return z;
}
int32_t _mm_movemask_epi8(__m128i x) {
    using uint64_t = unsigned long long;
    x = ~_mm_cmpeq_epi8(x, __m128i{0, 0});
    uint64_t xm = (static_cast<uint64_t>(x[0])) & 0x8040201008040201ull;
    uint64_t ym = (static_cast<uint64_t>(x[1])) & 0x8040201008040201ull;
    xm |= xm >> 8;
    ym |= ym >> 8;
    xm |= xm >> 16;
    ym |= ym >> 16;
    xm |= xm >> 32;
    ym |= ym >> 32;
    return static_cast<unsigned char>(xm) + 256ul * static_cast<unsigned char>(ym);
}
#endif

#include "moves.h"

struct MovesTable {
    // precomputed possible moves for each piece type on each square
    SquareSet moves[kNumPieces][kNumSquares];

    // precomputed possible captures for each piece type on each square
    SquareSet captures[kNumPieces][kNumSquares];

    // precomputed possible squares that each square can be attacked from
    SquareSet attackers[kNumSquares];

    // precomputed move kinds to deal with double pawn push and pawn queen promotion, under
    // promotions are expanded separately
    MoveKind moveKinds[kNumPieces][kNumRanks][kNumRanks];  // piece, from rank, to rank

    // precomputed capture kinds to deal with pawn queen promotion captures, under promotions are
    // expanded separately
    MoveKind captureKinds[kNumPieces][kNumRanks][kNumRanks];  // piece, from rank, to rank

    // precomputed horizontal, diagonal and vertical paths from each square to each other square
    SquareSet paths[kNumSquares][kNumSquares];  // from, to

    // precomputed squares required to be clear for castling
    SquareSet castlingClear[2][index(MoveKind::QUEEN_CASTLE) + 1];  // color, moveKind

    // precomputed from squares for en passant targets
    SquareSet enPassantFrom[2][kNumFiles];  // color, file

    MovesTable();

private:
    void initializePieceMovesAndCaptures();
    void initializePieceMoveAndCaptureKinds();
    void initializeAttackers();
    void initializePaths();
    void initializeCastlingMasks();
    void initializeEnPassantFrom();
} movesTable;

void MovesTable::initializePieceMovesAndCaptures() {
    for (auto piece : pieces) {
        for (Square from = 0; from != kNumSquares; ++from) {
            moves[int(piece)][from.index()] = possibleMoves(Piece(piece), from);
            captures[int(piece)][from.index()] = possibleCaptures(Piece(piece), from);
        }
    }
}
void MovesTable::initializePieceMoveAndCaptureKinds() {
    for (auto piece : pieces) {
        for (int fromRank = 0; fromRank != kNumRanks; ++fromRank) {
            for (int toRank = 0; toRank != kNumRanks; ++toRank) {
                MoveKind moveKind = MoveKind::QUIET_MOVE;
                MoveKind captureKind = MoveKind::CAPTURE;
                switch (piece) {
                case Piece::WHITE_PAWN:
                    if (fromRank == 1 && toRank == 3) moveKind = MoveKind::DOUBLE_PAWN_PUSH;
                    if (toRank == kNumRanks - 1) {
                        moveKind = MoveKind::QUEEN_PROMOTION;
                        captureKind = MoveKind::QUEEN_PROMOTION_CAPTURE;
                    }
                    break;
                case Piece::BLACK_PAWN:
                    if (fromRank == kNumRanks - 2 && toRank == kNumRanks - 4)
                        moveKind = MoveKind::DOUBLE_PAWN_PUSH;
                    if (toRank == 0) {
                        moveKind = MoveKind::QUEEN_PROMOTION;
                        captureKind = MoveKind::QUEEN_PROMOTION_CAPTURE;
                    }
                    break;
                default: break;
                }
                moveKinds[int(piece)][fromRank][toRank] = moveKind;
                captureKinds[int(piece)][fromRank][toRank] = captureKind;
            }
        }
    }
}

void MovesTable::initializeAttackers() {
    // Initialize attackers
    for (Square from = 0; from != kNumSquares; ++from) {
        SquareSet toSquares;
        // Gather all possible squares that can be attacked by any piece
        for (auto piece : pieces) toSquares |= captures[int(piece)][from.index()];
        // Distribute attackers over the target squares
        for (auto to : toSquares) attackers[to.index()].insert(from);
    }
}

void MovesTable::initializePaths() {
    // Initialize paths
    for (int from = 0; from < kNumSquares; ++from) {
        for (int to = 0; to < kNumSquares; ++to) {
            paths[from][to] = SquareSet::path(Square(from), Square(to));
        }
    }
}

void MovesTable::initializeCastlingMasks() {
    // Initialize castling masks and en passant from squares
    for (int color = 0; color < 2; ++color) {
        castlingClear[color][index(MoveKind::QUEEN_CASTLE)] =
            castlingPath(Color(color), MoveKind::QUEEN_CASTLE);
        castlingClear[color][index(MoveKind::KING_CASTLE)] =
            castlingPath(Color(color), MoveKind::KING_CASTLE);
    }
}

void MovesTable::initializeEnPassantFrom() {
    // Initialize en passant from squares
    for (int color = 0; color < 2; ++color) {
        int fromRank = color == 0 ? kNumRanks - 4 : 3;  // skipping 3 ranks from either side
        for (int fromFile = 0; fromFile < kNumFiles; ++fromFile) {
            enPassantFrom[color][fromFile] = {SquareSet::valid(fromRank, fromFile - 1) |
                                              SquareSet::valid(fromRank, fromFile + 1)};
        }
    }
}

MovesTable::MovesTable() {
    initializePieceMovesAndCaptures();
    initializePieceMoveAndCaptureKinds();
    initializeAttackers();
    initializePaths();
    initializeCastlingMasks();
    initializeEnPassantFrom();
}

/**
 * Returns the bits corresponding to the bytes in the input that contain the nibble.
 * Note: nibble is assumed to be at most 4 bits.
 * The implementation really finds everything unequal and then inverts the result.
 */
uint64_t equalSet(uint64_t input, uint8_t nibble) {
    input ^= 0x01010101'01010101ull * nibble;  // 0 nibbles in the input indicate the piece
    input += 0x7f3f1f0f'7f3f1f0full;           // Cause overflow in the right bits
    input &= 0x80402010'80402010ull;           // These are them, the 8 occupancy bits
    input >>= 4;                               // The low nibbles is where the action is
    input += input >> 16;
    input += input >> 8;
    input += (input >> 28);  // Merge in the high nibbel from the high word
    input &= 0xffull;
    input ^= 0xffull;  // Invert to get the bits that are equal
    return input;
}

uint64_t equalSet(__m128i input, uint8_t nibble) {
    __m128i cmp = {0x01010101'01010101ll * nibble, 0x01010101'01010101ll * nibble};
    __m128i res = _mm_cmpeq_epi8(input, cmp);
    return static_cast<uint16_t>(_mm_movemask_epi8(res));  // avoid sign extending
}

template <typename T>
uint64_t equalSetT(std::array<Piece, 64> squares, Piece piece, bool invert) {
    static_assert(kNumPieces <= 16, "Piece must fit in 4 bits");

    uint64_t set = 0;
    for (int j = 0; j < sizeof(squares); j += sizeof(T)) {
        T input;
        memcpy(&input, &squares[j], sizeof(T));
        set |= equalSet(input, static_cast<uint8_t>(piece)) << j;
    }

    return invert ? ~set : set;
}

uint64_t equalSet(std::array<Piece, 64> squares, Piece piece, bool invert) {
    uint64_t res;
    if constexpr (haveSSE2)
        res = equalSetT<__m128i>(squares, piece, invert);
    else
        res = equalSetT<uint64_t>(squares, piece, invert);

    if constexpr (debug && haveSSE2) {
        auto ref = equalSetT<uint64_t>(squares, piece, invert);
        assert(res == ref);
    }
    return res;
}

/**
 * Returns the bits corresponding to the bytes in the input that are less than the nibble.
 * Note: nibble is assumed to be at most 4 bits and not zero.
 * The implementation really finds everything greater than or equal and then inverts the result.
 */
uint64_t lessSet(uint64_t input, uint8_t nibble) {
    nibble = 0x10 - nibble;  // The amount to add to overflow into the high nibble for >=
    input += 0x01010101'01010101ull * nibble;  // broadcast to all bytes
    input += 0x70301000'70301000ull;           // Cause overflow in the right bits
    input &= 0x80402010'80402010ull;           // These are them, the 8 occupancy bits
    input >>= 4;                               // The low nibbles is where the action is
    input += input >> 16;
    input += input >> 8;
    input += (input >> 28);  // Merge in the high nibbel from the high word
    input &= 0xffull;
    input ^= 0xffull;  // Invert to get the bits that are less
    return input;
}

uint64_t lessSet(__m128i input, uint8_t nibble) {
    __m128i cmp = {0x01010101'01010101ll * nibble, 0x01010101'01010101ll * nibble};
    __m128i res = _mm_cmplt_epi8(input, cmp);
    return static_cast<uint16_t>(_mm_movemask_epi8(res));  // avoid sign extending
}

/**
 *  Returns the bits corresponding to the bytes in the input that are less than the piece.
 */
template <typename T>
uint64_t lessSetT(std::array<Piece, 64> squares, Piece piece, bool invert) {
    static_assert(kNumPieces <= 16, "Piece must fit in 4 bits");
    if (!int(piece)) return invert ? 0xffffffffffffffffull : 0;  // Special case for empty squares

    uint64_t set = 0;
    for (int j = 0; j < sizeof(squares); j += sizeof(T)) {
        T input;
        memcpy(&input, &squares[j], sizeof(T));
        set |= lessSet(input, static_cast<uint8_t>(piece)) << j;
    }

    return invert ? ~set : set;
}
uint64_t lessSet(std::array<Piece, 64> squares, Piece piece, bool invert) {
    uint64_t res;
    if constexpr (haveSSE2)
        res = lessSetT<__m128i>(squares, piece, invert);
    else
        res = lessSetT<uint64_t>(squares, piece, invert);

    if constexpr (debug && haveSSE2) {
        auto ref = lessSetT<uint64_t>(squares, piece, invert);
        assert(res == ref);
    }

    return res;
}

SquareSet SquareSet::occupancy(const Board& board) {
    return equalSet(board.squares(), Piece::NONE, true);
}

SquareSet SquareSet::occupancy(const Board& board, Color color) {
    bool black = color == Color::BLACK;
    return lessSet(board.squares(), black ? Piece::BLACK_PAWN : Piece::NONE, black);
}

SquareSet SquareSet::find(const Board& board, Piece piece) {
    return equalSet(board.squares(), piece, false);
}

SquareSet rookMoves(Square from) {
    SquareSet moves;
    for (int rank = 0; rank < kNumRanks; ++rank)
        if (rank != from.rank()) moves.insert(Square(rank, from.file()));

    for (int file = 0; file < kNumFiles; ++file)
        if (file != from.file()) moves.insert(Square(from.rank(), file));

    return moves;
}

SquareSet bishopMoves(Square from) {
    SquareSet moves;
    for (int i = 1; i < std::min(kNumRanks, kNumFiles); ++i) {
        moves.insert(SquareSet::valid(from.rank() + i, from.file() + i));
        moves.insert(SquareSet::valid(from.rank() - i, from.file() - i));
        moves.insert(SquareSet::valid(from.rank() + i, from.file() - i));
        moves.insert(SquareSet::valid(from.rank() - i, from.file() + i));
    }
    return moves;
}

SquareSet queenMoves(Square from) {
    // Combine the moves of a rook and a bishop
    return rookMoves(from) | bishopMoves(from);
}

SquareSet knightMoves(Square from) {
    int vectors[8][2] = {{-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};

    SquareSet moves;
    for (auto [rank, file] : vectors)
        moves.insert(SquareSet::valid(from.rank() + rank, from.file() + file));
    return moves;
}

SquareSet kingMoves(Square from) {
    int vectors[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

    SquareSet moves;
    for (auto [rank, file] : vectors)
        moves.insert(SquareSet::valid(from.rank() + rank, from.file() + file));
    return moves;
}

SquareSet whitePawnMoves(Square from) {
    SquareSet moves = SquareSet::valid(from.rank() + 1, from.file());
    if (from.rank() == 1) moves.insert(SquareSet::valid(from.rank() + 2, from.file()));
    return moves;
}

SquareSet blackPawnMoves(Square from) {
    SquareSet moves = SquareSet::valid(from.rank() - 1, from.file());
    if (from.rank() == kNumRanks - 2) moves.insert(SquareSet::valid(from.rank() - 2, from.file()));
    return moves;
}

SquareSet possibleMoves(Piece piece, Square from) {
    switch (piece) {
    case Piece::NONE: break;
    case Piece::WHITE_PAWN: return whitePawnMoves(from);
    case Piece::BLACK_PAWN: return blackPawnMoves(from);
    case Piece::WHITE_KNIGHT:
    case Piece::BLACK_KNIGHT: return knightMoves(from);
    case Piece::WHITE_BISHOP:
    case Piece::BLACK_BISHOP: return bishopMoves(from);
    case Piece::WHITE_ROOK:
    case Piece::BLACK_ROOK: return rookMoves(from);
    case Piece::WHITE_QUEEN:
    case Piece::BLACK_QUEEN: return queenMoves(from);
    case Piece::WHITE_KING:
    case Piece::BLACK_KING: return kingMoves(from);
    }
    return {};
}

SquareSet possibleCaptures(Piece piece, Square from) {
    switch (piece) {
    case Piece::NONE: break;
    case Piece::WHITE_PAWN:                                        // White Pawn
        return SquareSet::valid(from.rank() + 1, from.file() - 1)  // Diagonal left
            | SquareSet::valid(from.rank() + 1, from.file() + 1);  // Diagonal right
    case Piece::BLACK_PAWN:                                        // Black Pawn
        return SquareSet::valid(from.rank() - 1, from.file() - 1)  // Diagonal left
            | SquareSet::valid(from.rank() - 1, from.file() + 1);  // Diagonal right
    case Piece::WHITE_KNIGHT:
    case Piece::BLACK_KNIGHT: return knightMoves(from);
    case Piece::WHITE_BISHOP:
    case Piece::BLACK_BISHOP: return bishopMoves(from);
    case Piece::WHITE_ROOK:
    case Piece::BLACK_ROOK: return rookMoves(from);
    case Piece::WHITE_QUEEN:
    case Piece::BLACK_QUEEN: return queenMoves(from);
    case Piece::WHITE_KING:
    case Piece::BLACK_KING: return kingMoves(from);
    }
    return {};
}

SquareSet SquareSet::path(Square from, Square to) {
    SquareSet path;
    int rankDiff = to.rank() - from.rank();
    int fileDiff = to.file() - from.file();

    // Check if the move isn't horizontal, vertical, or diagonal
    if (rankDiff != 0 && fileDiff != 0 && abs(rankDiff) != abs(fileDiff)) {
        return path;  // It's not in straight line, thus no need to check further
    }

    // Calculate the direction of movement for rank and file
    int rankStep = (rankDiff != 0) ? rankDiff / abs(rankDiff) : 0;
    int fileStep = (fileDiff != 0) ? fileDiff / abs(fileDiff) : 0;

    int rankPos = from.rank() + rankStep;
    int filePos = from.file() + fileStep;

    while (rankPos != to.rank() || filePos != to.file()) {
        path.insert(Square{rankPos, filePos});
        rankPos += rankStep;
        filePos += fileStep;
    }
    return path;
}

SquareSet castlingPath(Color color, MoveKind side) {
    SquareSet path;
    int rank = color == Color::WHITE ? 0 : kNumRanks - 1;

    if (side == MoveKind::QUEEN_CASTLE) {
        // Note the paths are reversed, so the start point is excluded and the endpoint included.
        path |= SquareSet::path(Square(rank, Position::kKingCastledQueenSideFile),
                                Square(rank, Position::kKingFile));
        path |= SquareSet::path(Square(rank, Position::kRookCastledQueenSideFile),
                                Square(rank, Position::kQueenSideRookFile));
    } else {
        assert(side == MoveKind::KING_CASTLE);
        // Note the paths are reversed, so the start point is excluded and the endpoint included.
        path |= SquareSet::path(Square(rank, Position::kKingCastledKingSideFile),
                                Square(rank, Position::kKingFile));
        path |= SquareSet::path(Square(rank, Position::kRookCastledKingSideFile),
                                Square(rank, Position::kKingSideRookFile));
    }

    // Explicitly exclude the king's and rook's starting square for chess960
    path.erase(Square(rank, Position::kKingFile));
    path.erase(Square(rank, Position::kKingSideRookFile));
    return path;
}

bool clearPath(SquareSet occupancy, Square from, Square to) {
    auto path = movesTable.paths[from.index()][to.index()];
    return (occupancy & path).empty();
}

void addMove(MoveVector& moves, Piece piece, Move move) {
    moves.emplace_back(move);
}

template <typename F>
void expandMovePromotions(Piece piece, Move move, const F& fun) {
    fun(piece, move);

    // If promoted as queen, add all possible under promotions
    for (auto kind = int(move.kind()); --kind >= int(MoveKind::KNIGHT_PROMOTION);)
        fun(piece, Move{move.from(), move.to(), MoveKind(kind)});
}

template <typename F>
void expandCapturePromotions(Piece piece, Move move, const F& fun) {
    fun(piece, move);

    // If promoted as queen, add all possible under promotions
    for (auto kind = int(move.kind()); --kind >= int(MoveKind::KNIGHT_PROMOTION_CAPTURE);)
        fun(piece, Move{move.from(), move.to(), MoveKind(kind)});
}

template <typename F>
void findMoves(const Board& board, Occupancy occupied, const F& fun) {
    for (auto from : occupied.ours) {
        auto piece = board[from];
        auto possibleSquares = movesTable.moves[index(piece)][from.index()] & !occupied();
        for (auto to : possibleSquares) {
            // Check for moving through pieces
            if (clearPath(occupied(), from, to)) {
                auto kind = movesTable.moveKinds[index(piece)][from.rank()][to.rank()];
                expandMovePromotions(piece, Move{from, to, kind}, fun);
            }
        }
    }
}

template <typename F>
void findCastles(const Board& board, Occupancy occupied, Turn turn, const F& fun) {
    auto color = int(turn.activeColor);
    auto& info = castlingInfo[color];
    // Check for king side castling
    if ((turn.castlingAvailability & info.kingSideMask) != CastlingMask::NONE) {
        auto path = movesTable.castlingClear[color][index(MoveKind::KING_CASTLE)];
        if ((occupied() & path).empty())
            fun(info.king, {info.kingFrom, info.kingToKingSide, MoveKind::KING_CASTLE});
    }
    // Check for queen side castling
    if ((turn.castlingAvailability & info.queenSideMask) != CastlingMask::NONE) {
        auto path = movesTable.castlingClear[color][index(MoveKind::QUEEN_CASTLE)];
        if ((occupied() & path).empty())
            fun(info.king, Move{info.kingFrom, info.kingToQueenSide, MoveKind::QUEEN_CASTLE});
    }
}

template <typename F>
void findCaptures(const Board& board, Occupancy occupied, const F& fun) {
    for (auto from : occupied.ours) {
        auto piece = board[from];

        auto possibleSquares = movesTable.captures[index(piece)][from.index()] & occupied.theirs;
        for (auto to : possibleSquares) {
            // Exclude captures that move through pieces
            if (clearPath(occupied(), from, to)) {
                auto kind = movesTable.captureKinds[index(piece)][from.rank()][to.rank()];
                expandCapturePromotions(piece, Move{from, to, kind}, fun);
            }
        }
    }
}

template <typename F>
void findEnPassant(const Board& board, Turn turn, const F& fun) {
    auto enPassantTarget = turn.enPassantTarget;
    if (turn.enPassantTarget == noEnPassantTarget) return;

    // For a given en passant target, there are two potential from squares. If either or
    // both have a pawn of the active color, then capture is possible.
    auto activeColor = turn.activeColor;
    auto pawn = addColor(PieceType::PAWN, activeColor);
    for (auto from : movesTable.enPassantFrom[int(activeColor)][enPassantTarget.file()])
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::EN_PASSANT});
}

void addAvailableMoves(MoveVector& moves, const Board& board, Color activeColor) {
    auto occupancy = Occupancy(board, activeColor);
    findMoves(board, occupancy, [&](Piece piece, Move move) { addMove(moves, piece, move); });
}

void addAvailableCaptures(MoveVector& captures, const Board& board, Color activeColor) {
    auto occupancy = Occupancy(board, activeColor);
    findCaptures(board, occupancy, [&](Piece piece, Move move) { addMove(captures, piece, move); });
}

void addAvailableEnPassant(MoveVector& captures,
                           const Board& board,
                           Color activeColor,
                           Square enPassantTarget) {
    Turn turn;
    turn.activeColor = activeColor;
    turn.enPassantTarget = enPassantTarget;
    findEnPassant(board, turn, [&](Piece piece, Move move) { addMove(captures, piece, move); });
}

void addAvailableCastling(MoveVector& captures,
                          const Board& board,
                          Color activeColor,
                          CastlingMask mask) {
    Turn turn;
    turn.activeColor = activeColor;
    turn.castlingAvailability = mask;
    auto occupancy = Occupancy(board, activeColor);
    findCastles(
        board, occupancy, turn, [&](Piece piece, Move move) { addMove(captures, piece, move); });
}

Piece makeMove(Board& board, Move move) {
    auto& piece = board[move.from()];
    auto& target = board[move.to()];

    // En passant capture
    switch (move.kind()) {
    case MoveKind::KING_CASTLE: {
        auto rank = move.from().rank();
        auto rook = board[Square(rank, Position::kKingSideRookFile)];
        auto king = board[Square(rank, Position::kKingFile)];

        // Remove king and rook from their original squares
        board[Square(rank, Position::kKingSideRookFile)] = Piece::NONE;
        board[Square(rank, Position::kKingFile)] = Piece::NONE;

        // Place king and rook on their new squares
        board[Square(rank, Position::kKingCastledKingSideFile)] = king;
        board[Square(rank, Position::kRookCastledKingSideFile)] = rook;
        return Piece::NONE;
    }

    case MoveKind::QUEEN_CASTLE: {
        auto rank = move.from().rank();
        auto rook = board[Square(rank, Position::kQueenSideRookFile)];
        auto king = board[Square(rank, Position::kKingFile)];

        // Remove king and rook from their original squares
        board[Square(rank, Position::kQueenSideRookFile)] = Piece::NONE;
        board[Square(rank, Position::kKingFile)] = Piece::NONE;

        // Place king and rook on their new squares
        board[Square(rank, Position::kKingCastledQueenSideFile)] = king;
        board[Square(rank, Position::kRookCastledQueenSideFile)] = rook;
        return Piece::NONE;
    }

    case MoveKind::EN_PASSANT:
        // The pawns target is empty, so move the piece to capture en passant there.
        std::swap(board[Square{move.from().rank(), move.to().file()}], target);
        break;

    case MoveKind::KNIGHT_PROMOTION:
    case MoveKind::BISHOP_PROMOTION:
    case MoveKind::ROOK_PROMOTION:
    case MoveKind::QUEEN_PROMOTION:
    case MoveKind::KNIGHT_PROMOTION_CAPTURE:
    case MoveKind::BISHOP_PROMOTION_CAPTURE:
    case MoveKind::ROOK_PROMOTION_CAPTURE:
    case MoveKind::QUEEN_PROMOTION_CAPTURE:
        piece = addColor(promotionType(move.kind()), color(piece));  // Promote the pawn
        [[fallthrough]];
    case MoveKind::QUIET_MOVE:
    case MoveKind::DOUBLE_PAWN_PUSH:
    case MoveKind::CAPTURE:;
    }

    // Update the target, and empty the source square
    auto captured = target;
    target = piece;
    piece = Piece::NONE;
    return captured;
}

void unmakeMove(Board& board, Move move, Piece captured) {
    switch (move.kind()) {
    case MoveKind::KING_CASTLE: {
        auto rank = move.from().rank();
        auto rook = board[Square(rank, Position::kRookCastledKingSideFile)];
        auto king = board[Square(rank, Position::kKingCastledKingSideFile)];

        // Remove king and rook from their original squares
        board[Square(rank, Position::kKingCastledKingSideFile)] = Piece::NONE;
        board[Square(rank, Position::kRookCastledKingSideFile)] = Piece::NONE;

        // Place king and rook on their new squares
        board[Square(rank, Position::kKingFile)] = king;
        board[Square(rank, Position::kKingSideRookFile)] = rook;
        break;
    }
    case MoveKind::QUEEN_CASTLE: {
        auto rank = move.from().rank();
        auto rook = board[Square(rank, Position::kRookCastledQueenSideFile)];
        auto king = board[Square(rank, Position::kKingCastledQueenSideFile)];

        // Remove king and rook from their original squares
        board[Square(rank, Position::kKingCastledQueenSideFile)] = Piece::NONE;
        board[Square(rank, Position::kRookCastledQueenSideFile)] = Piece::NONE;

        // Place king and rook on their new squares
        board[Square(rank, Position::kKingFile)] = king;
        board[Square(rank, Position::kQueenSideRookFile)] = rook;
        break;
    }
    case MoveKind::EN_PASSANT:
        // The pawns target is empty, so move the piece to capture en passant there.
        std::swap(board[Square{move.from().rank(), move.to().file()}], captured);
        board[move.from()] = board[move.to()];
        board[move.to()] = captured;
        break;
    case MoveKind::KNIGHT_PROMOTION:
    case MoveKind::BISHOP_PROMOTION:
    case MoveKind::ROOK_PROMOTION:
    case MoveKind::QUEEN_PROMOTION:
    case MoveKind::KNIGHT_PROMOTION_CAPTURE:
    case MoveKind::BISHOP_PROMOTION_CAPTURE:
    case MoveKind::ROOK_PROMOTION_CAPTURE:
    case MoveKind::QUEEN_PROMOTION_CAPTURE:
        board[move.to()] = addColor(PieceType::PAWN, color(board[move.to()]));  // Demote the piece
        [[fallthrough]];
    case MoveKind::QUIET_MOVE:
    case MoveKind::DOUBLE_PAWN_PUSH:
    case MoveKind::CAPTURE:
        board[move.from()] = board[move.to()];
        board[move.to()] = captured;
        break;
    }
}

CastlingMask castlingMask(Square from, Square to) {
    using P = Position;
    using CM = CastlingMask;

    // Remove castling availability if a rook moves or is captured
    if (from == P::whiteQueenSideRook || to == P::whiteQueenSideRook) return CM::WHITE_QUEENSIDE;
    if (from == P::whiteKingSideRook || to == P::whiteKingSideRook) return CM::WHITE_KINGSIDE;
    if (from == P::blackQueenSideRook || to == P::blackQueenSideRook) return CM::BLACK_QUEENSIDE;
    if (from == P::blackKingSideRook || to == P::blackKingSideRook) return CM::BLACK_KINGSIDE;

    // Remove castling availability if the king is moves
    if (from == P::whiteKing) return CM::WHITE;
    if (from == P::blackKing) return CM::BLACK;

    return CM::NONE;
}

Turn applyMove(Turn turn, Piece piece, Move move) {
    // Update enPassantTarget
    // Set the en passant target if a pawn moves two squares forward, otherwise reset it.
    turn.enPassantTarget = noEnPassantTarget;
    if (move.kind() == MoveKind::DOUBLE_PAWN_PUSH) {
        turn.enPassantTarget = {(move.from().rank() + move.to().rank()) / 2, move.from().file()};
    }
    // Update castlingAvailability
    turn.castlingAvailability &= ~castlingMask(move.from(), move.to());

    // Update halfMoveClock: reset on pawn advance or capture, else increment
    ++turn.halfmoveClock;
    if (type(piece) == PieceType::PAWN || isCapture(move.kind())) turn.halfmoveClock = 0;

    // Update fullMoveNumber: increment after black's move
    if (turn.activeColor == Color::BLACK) ++turn.fullmoveNumber;

    // Update activeColor
    turn.activeColor = !turn.activeColor;
    return turn;
}

Position applyMove(Position position, Move move) {
    // Remember the piece being moved, before applying the move to the board
    auto piece = position.board[move.from()];

    // Apply the move to the board
    makeMove(position.board, move);
    position.turn = applyMove(position.turn, piece, move);

    return position;
}

bool isAttacked(const Board& board, Square square, Occupancy occupancy) {
    // We're using this function to find out if empty squares are attacked for determining
    // legality of castling, so we ca't assume that the capture square is occupied.
    for (Square from : occupancy.theirs& movesTable.attackers[square.index()]) {
        auto piece = board[from];
        auto possibleCaptureSquares = movesTable.captures[index(piece)][from.index()];
        if (possibleCaptureSquares.contains(square) &&
            clearPath(SquareSet::occupancy(board), from, square))
            return true;
    }
    return false;
}

bool isAttacked(const Board& board, SquareSet squares, Occupancy occupancy) {
    for (auto square : squares)
        if (isAttacked(board, square, occupancy)) return true;

    return false;
}

bool isAttacked(const Board& board, SquareSet squares, Color opponentColor) {
    auto occupancy = Occupancy(board, opponentColor);
    return isAttacked(board, squares, occupancy);
}

namespace {
std::string toString(SquareSet squares) {
    std::string str;
    for (Square sq : squares) {
        str += static_cast<std::string>(sq);
        str += " ";
    }
    str.pop_back();  // Remove the last space
    return str;
}
}  // namespace

void forAllLegalMoves(Turn turn, Board& board, std::function<void(Board&, MoveWithPieces)> action) {

    auto ourKing = addColor(PieceType::KING, turn.activeColor);
    auto oldKing = SquareSet::find(board, ourKing);
    auto occupancy = Occupancy(board, turn.activeColor);

    // Iterate over all moves and captures
    auto addIfLegal = [&](Piece piece, Move move) {
        auto [from, to, kind] = Move::Tuple(move);
        // If we move the king, reflect that in the king squares
        auto newKing = oldKing;
        if (piece == ourKing) {
            if (kind == MoveKind::KING_CASTLE || kind == MoveKind::QUEEN_CASTLE)
                newKing |= movesTable.paths[from.index()][to.index()];
            else
                newKing.erase(from);
            newKing.insert(to);
        }

        // Apply the move to the board
        auto captured = makeMove(board, move);

        auto newOccupancy =
            Occupancy{occupancy.theirs & !SquareSet(to), (occupancy.ours & !SquareSet(from)) | to};

        // Adjust the opponent squares for the en passant and castling cases.
        switch (kind) {
        case MoveKind::KING_CASTLE:
            newOccupancy.ours.erase(Square(from.rank(), Position::kKingSideRookFile));
            newOccupancy.ours.insert(Square(from.rank(), Position::kRookCastledKingSideFile));
            break;
        case MoveKind::QUEEN_CASTLE:
            newOccupancy.ours.erase(Square(from.rank(), Position::kQueenSideRookFile));
            newOccupancy.ours.insert(Square(from.rank(), Position::kRookCastledQueenSideFile));
            break;
        case MoveKind::EN_PASSANT: newOccupancy = Occupancy(board, turn.activeColor); break;
        default: break;
        }

        // Check if the move would result in our king being in check.
        if (!isAttacked(board, newKing, newOccupancy))
            action(board, MoveWithPieces{Move{from, to, kind}, piece, captured});

        unmakeMove(board, move, captured);
    };

    findCaptures(board, occupancy, addIfLegal);
    findEnPassant(board, turn, addIfLegal);
    findMoves(board, occupancy, addIfLegal);
    findCastles(board, occupancy, turn, addIfLegal);
}

MoveVector allLegalMoves(Turn turn, Board& board) {
    MoveVector legalMoves;
    forAllLegalMoves(
        turn, board, [&](Board&, MoveWithPieces move) { legalMoves.emplace_back(move.move); });
    return legalMoves;
}