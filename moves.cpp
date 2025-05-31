#include "common.h"
#include <cassert>
#include <cstring>

#include "options.h"
#include "sse2.h"

#include "moves.h"

// Unconditionally use actual SSE2 or emulated SSE2: it turns out that the emulated CPU has more
// benefits for ARM (on at least Apple Silicon M1) than real SSE2 has for x86.
constexpr bool haveSSE2 = true;

struct CompoundMove {
    Square to = 0;
    uint8_t promo = 0;
    TwoSquares second = {0, 0};
};

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

    // precomputed delta in occupancy as result of a move, but only for non-promotion moves
    // to save on memory: convert promotion kinds using the noPromo function
    Occupancy occupancyDelta[kNumNoPromoMoveKinds][kNumSquares][kNumSquares];  // moveKind, from, to

    // precomputed horizontal, diagonal and vertical paths from each square to each other square
    SquareSet paths[kNumSquares][kNumSquares];  // from, to

    // precomputed squares required to be clear for castling
    SquareSet castlingClear[2][index(MoveKind::QUEEN_CASTLE) + 1];  // color, moveKind

    // precomputed from squares for en passant targets
    SquareSet enPassantFrom[2][kNumFiles];  // color, file

    // precompute compound moves for castling, en passant and promotions
    CompoundMove compound[16][kNumSquares];  // moveKind, to square

    MovesTable();

private:
    void initializePieceMovesAndCaptures();
    void initializePieceMoveAndCaptureKinds();
    void initializeAttackers();
    void initializeOccupancyDeltas();
    void initializePaths();
    void initializeCastlingMasks();
    void initializeEnPassantFrom();
    void initializeCompound();
} movesTable;

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::WHITE),
                                                 CastlingInfo(Color::BLACK)};
}  // namespace

namespace init {
Occupancy occupancyDelta(Move move) {
    auto [from, to, kind] = Move::Tuple(move);
    SquareSet ours;
    ours.insert(from);
    ours.insert(to);
    SquareSet theirs;
    switch (kind) {
    case MoveKind::KING_CASTLE: {
        auto info = castlingInfo[from.rank() != 0];
        ours.insert(info.kingSide[1][1]);
        ours.insert(info.kingSide[1][0]);
        break;
    }
    case MoveKind::QUEEN_CASTLE: {
        auto info = castlingInfo[from.rank() != 0];
        ours.insert(info.queenSide[1][1]);
        ours.insert(info.queenSide[1][0]);
        break;
    }
    case MoveKind::CAPTURE:
    case MoveKind::KNIGHT_PROMOTION_CAPTURE:
    case MoveKind::BISHOP_PROMOTION_CAPTURE:
    case MoveKind::ROOK_PROMOTION_CAPTURE:
    case MoveKind::QUEEN_PROMOTION_CAPTURE: theirs.insert(to); break;
    case MoveKind::EN_PASSANT: theirs.insert(Square(to.file(), from.rank())); break;
    default: break;
    }
    return Occupancy::delta(theirs, ours);
}
SquareSet castlingPath(Color color, MoveKind side) {
    SquareSet path;
    auto info = castlingInfo[int(color)];

    // Note the paths are reversed, so the start point is excluded and the endpoint included.
    if (side == MoveKind::KING_CASTLE)
        path |= SquareSet::path(info.kingSide[0][1], info.kingSide[0][0]) |
            SquareSet::path(info.kingSide[1][1], info.kingSide[1][0]);
    else
        path |= SquareSet::path(info.queenSide[0][1], info.queenSide[0][0]) |
            SquareSet::path(info.queenSide[1][1], info.queenSide[1][0]);

    return path;
}
SquareSet rookMoves(Square from) {
    SquareSet moves;
    for (int rank = 0; rank < kNumRanks; ++rank)
        if (rank != from.rank()) moves.insert(Square(from.file(), rank));

    for (int file = 0; file < kNumFiles; ++file)
        if (file != from.file()) moves.insert(Square(file, from.rank()));

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
    case Piece::_: break;
    case Piece::P: return whitePawnMoves(from);
    case Piece::p: return blackPawnMoves(from);
    case Piece::N:
    case Piece::n: return knightMoves(from);
    case Piece::B:
    case Piece::b: return bishopMoves(from);
    case Piece::R:
    case Piece::r: return rookMoves(from);
    case Piece::Q:
    case Piece::q: return queenMoves(from);
    case Piece::K:
    case Piece::k: return kingMoves(from);
    }
    return {};
}

SquareSet possibleCaptures(Piece piece, Square from) {
    switch (piece) {
    case Piece::_: break;
    case Piece::P:                                                 // White Pawn
        return SquareSet::valid(from.rank() + 1, from.file() - 1)  // Diagonal left
            | SquareSet::valid(from.rank() + 1, from.file() + 1);  // Diagonal right
    case Piece::p:                                                 // Black Pawn
        return SquareSet::valid(from.rank() - 1, from.file() - 1)  // Diagonal left
            | SquareSet::valid(from.rank() - 1, from.file() + 1);  // Diagonal right
    case Piece::N:
    case Piece::n: return knightMoves(from);
    case Piece::B:
    case Piece::b: return bishopMoves(from);
    case Piece::R:
    case Piece::r: return rookMoves(from);
    case Piece::Q:
    case Piece::q: return queenMoves(from);
    case Piece::K:
    case Piece::k: return kingMoves(from);
    }
    return {};
}
SquareSet path(Square from, Square to) {
    SquareSet path;
    int rankDiff = to.rank() - from.rank();
    int fileDiff = to.file() - from.file();

    // Check that the move is horizontal, vertical, or diagonal. If not, no path.
    if (rankDiff && fileDiff && abs(rankDiff) != abs(fileDiff))
        return {};  // It's not in straight line, thus no need to check further

    // Calculate the direction of movement for rank and file
    int rankStep = rankDiff ? rankDiff / abs(rankDiff) : 0;
    int fileStep = fileDiff ? fileDiff / abs(fileDiff) : 0;

    int rankPos = from.rank() + rankStep;
    int filePos = from.file() + fileStep;

    while (rankPos != to.rank() || filePos != to.file()) {
        path.insert(Square{filePos, rankPos});
        rankPos += rankStep;
        filePos += fileStep;
    }
    return path;
}
}  // namespace init

SquareSet SquareSet::path(Square from, Square to) {
    return movesTable.paths[from.index()][to.index()];
}

void MovesTable::initializePieceMovesAndCaptures() {
    for (auto piece : pieces) {
        for (Square from = 0; from != kNumSquares; ++from) {
            moves[int(piece)][from.index()] = init::possibleMoves(Piece(piece), from);
            captures[int(piece)][from.index()] = init::possibleCaptures(Piece(piece), from);
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
                case Piece::P:
                    if (fromRank == 1 && toRank == 3) moveKind = MoveKind::DOUBLE_PAWN_PUSH;
                    if (toRank == kNumRanks - 1) {
                        moveKind = MoveKind::QUEEN_PROMOTION;
                        captureKind = MoveKind::QUEEN_PROMOTION_CAPTURE;
                    }
                    break;
                case Piece::p:
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

void MovesTable::initializeOccupancyDeltas() {
    // Initialize occupancy changes
    for (int moveKind = 0; moveKind < kNumNoPromoMoveKinds; ++moveKind)
        for (int from = 0; from < kNumSquares; ++from)
            for (int to = 0; to < kNumSquares; ++to)
                occupancyDelta[moveKind][from][to] =
                    init::occupancyDelta(Move{Square(from), Square(to), MoveKind(moveKind)});
}

void MovesTable::initializePaths() {
    // Initialize paths
    for (int from = 0; from < kNumSquares; ++from)
        for (int to = 0; to < kNumSquares; ++to)
            paths[from][to] = init::path(Square(from), Square(to));
}

void MovesTable::initializeCastlingMasks() {
    // Initialize castling masks and en passant from squares
    for (int color = 0; color < 2; ++color) {
        castlingClear[color][index(MoveKind::QUEEN_CASTLE)] =
            init::castlingPath(Color(color), MoveKind::QUEEN_CASTLE);
        castlingClear[color][index(MoveKind::KING_CASTLE)] =
            init::castlingPath(Color(color), MoveKind::KING_CASTLE);
    }
}

void MovesTable::initializeEnPassantFrom() {
    // Initialize en passant from squares
    for (int color = 0; color < 2; ++color) {
        int fromRank = color == 0 ? kNumRanks - 4 : 3;  // skipping 3 ranks from either side
        for (int fromFile = 0; fromFile < kNumFiles; ++fromFile)
            enPassantFrom[color][fromFile] = {SquareSet::valid(fromRank, fromFile - 1) |
                                              SquareSet::valid(fromRank, fromFile + 1)};
    }
}

void MovesTable::initializeCompound() {
    using MK = MoveKind;
    using PT = PieceType;
    using CM = CompoundMove;

    // Initialize non-compound moves
    for (auto kind = 0; kind < kNumMoveKinds; ++kind)
        for (auto to = 0; to < kNumSquares; ++to)
            compound[kind][to] = {Square(to), 0, {Square(to), Square(to)}};

    // Initialize castles
    compound[index(MK::KING_CASTLE)]["g1"_sq.index()] = {"g1"_sq, 0, {"h1"_sq, "f1"_sq}};
    compound[index(MK::KING_CASTLE)]["g8"_sq.index()] = {"g8"_sq, 0, {"h8"_sq, "f8"_sq}};
    compound[index(MK::QUEEN_CASTLE)]["c1"_sq.index()] = {"c1"_sq, 0, {"a1"_sq, "d1"_sq}};
    compound[index(MK::QUEEN_CASTLE)]["c8"_sq.index()] = {"c8"_sq, 0, {"a8"_sq, "d8"_sq}};

    // En passant helper functions
    auto epRank = [](int rank) -> int { return rank < kNumRanks / 2 ? rank + 1 : rank - 1; };
    auto epTarget = [=](Square to) -> Square { return Square(to.file(), epRank(to.rank())); };
    auto epCompound = [=](Square to) -> CM { return {epTarget(to), 0, {epTarget(to), to}}; };

    // Initialize en passant capture for white
    for (auto to : SquareSet::ipath("a6"_sq, "h6"_sq) | SquareSet::ipath("a3"_sq, "h3"_sq))
        compound[index(MK::EN_PASSANT)][to.index()] = epCompound(to);

    // Initialize promotion moves
    auto pm = index(MK::KNIGHT_PROMOTION);
    auto pc = index(MK::KNIGHT_PROMOTION_CAPTURE);
    for (auto promo = index(PT::KNIGHT); promo <= index(PT::QUEEN); ++promo, ++pm, ++pc)
        for (auto to : SquareSet::ipath("a8"_sq, "h8"_sq) | SquareSet::ipath("a1"_sq, "h1"_sq))
            compound[pc][to.index()] = compound[pm][to.index()] = {to, promo, {to, to}};
}

MovesTable::MovesTable() {
    initializePieceMovesAndCaptures();
    initializePieceMoveAndCaptureKinds();
    initializeAttackers();
    initializeOccupancyDeltas();
    initializePaths();
    initializeCastlingMasks();
    initializeEnPassantFrom();
    initializeCompound();
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
    for (size_t j = 0; j < sizeof(squares); j += sizeof(T)) {
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
    for (size_t j = 0; j < sizeof(squares); j += sizeof(T)) {
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
    return equalSet(board.squares(), Piece::_, true);
}

SquareSet SquareSet::occupancy(const Board& board, Color color) {
    bool black = color == Color::BLACK;
    return lessSet(board.squares(), black ? Piece::p : Piece::_, black);
}

SquareSet SquareSet::find(const Board& board, Piece piece) {
    return equalSet(board.squares(), piece, false);
}

struct PieceSet {
    uint16_t pieces = 0;
    PieceSet() = default;
    PieceSet(Piece piece) : pieces(1 << index(piece)) {}
    PieceSet(PieceType pieceType)
        : pieces((1 << index(addColor(pieceType, Color::WHITE))) |
                 (1 << index(addColor(pieceType, Color::BLACK)))) {}
    constexpr PieceSet(uint16_t pieces) : pieces(pieces) {}
    constexpr PieceSet operator|(PieceSet other) const { return PieceSet(pieces | other.pieces); }
    bool has(Piece piece) const { return pieces & (1 << index(piece)); }
};

struct PinData {
    SquareSet captures;
    PieceSet pinningPieces;
};

SquareSet pinnedPieces(const Board& board,
                       Occupancy occupancy,
                       Square kingSquare,
                       const PinData& pinData) {
    SquareSet pinned;
    for (auto pinner : pinData.captures & occupancy.theirs()) {
        // Check if the pin is a valid sliding piece
        if (!pinData.pinningPieces.has(board[pinner])) continue;

        auto path = movesTable.paths[kingSquare.index()][pinner.index()];
        auto pieces = occupancy() & path;
        if (pieces.size() == 1) pinned.insert(pieces);
    }
    return pinned & occupancy.ours();
}

SquareSet pinnedPieces(const Board& board, Occupancy occupancy, Square kingSquare) {
    // For the purpose of pinning, we only consider sliding pieces, not knights.

    // Define pinning piece sets and corresponding capture sets
    PinData pinData[] = {{movesTable.captures[index(Piece::R)][kingSquare.index()],
                          PieceSet(PieceType::ROOK) | PieceSet(PieceType::QUEEN)},
                         {movesTable.captures[index(Piece::B)][kingSquare.index()],
                          PieceSet(PieceType::BISHOP) | PieceSet(PieceType::QUEEN)}};

    auto pinned = SquareSet();

    for (const auto& data : pinData) pinned |= pinnedPieces(board, occupancy, kingSquare, data);

    return pinned;
}

SearchState::SearchState(const Board& board, Turn turn)
    : occupancy(Occupancy(board, turn.activeColor())),
      pawns(SquareSet::find(board, addColor(PieceType::PAWN, turn.activeColor()))),
      turn(turn),
      kingSquare(*SquareSet::find(board, addColor(PieceType::KING, turn.activeColor())).begin()),
      inCheck(isAttacked(board, kingSquare, occupancy)),
      pinned(pinnedPieces(board, occupancy, kingSquare)) {}

bool clearPath(SquareSet occupancy, Square from, Square to) {
    auto path = movesTable.paths[from.index()][to.index()];
    return (occupancy & path).empty();
}

void addMove(MoveVector& moves, Move move) {
    moves.emplace_back(move);
}

int noPromo(MoveKind kind) {
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

Occupancy occupancyDelta(Move move) {
    return movesTable.occupancyDelta[noPromo(move.kind())][move.from().index()][move.to().index()];
}

template <typename F>
void expandPromos(const F& fun, Piece piece, Move move) {
    for (int i = 0; i < 4; ++i)
        fun(piece, Move{move.from(), move.to(), MoveKind(index(move.kind()) - i)});
}

template <typename F>
void findPawnPushes(SearchState& state, const F& fun) {
    bool white = state.active() == Color::WHITE;
    const auto doublePushRank = white ? SquareSet::rank(3) : SquareSet::rank(kNumRanks - 1 - 3);
    const auto promo = white ? SquareSet::rank(kNumRanks - 1) : SquareSet::rank(0);
    auto free = !state.occupancy();
    auto singles = (white ? state.pawns << kNumRanks : state.pawns >> kNumRanks) & free;
    auto doubles = (white ? singles << kNumRanks : singles >> kNumRanks) & free & doublePushRank;
    auto piece = white ? Piece::P : Piece::p;
    for (auto to : singles - promo)
        fun(piece,
            Move{{to.file(), white ? to.rank() - 1 : to.rank() + 1}, to, MoveKind::QUIET_MOVE});
    for (auto to : singles& promo)
        expandPromos(fun,
                     piece,
                     Move{{to.file(), white ? to.rank() - 1 : to.rank() + 1},
                          to,
                          MoveKind::QUEEN_PROMOTION});
    for (auto to : doubles)
        fun(piece,
            Move{{to.file(), white ? to.rank() - 2 : to.rank() + 2},
                 to,
                 MoveKind::DOUBLE_PAWN_PUSH});
}

template <typename F>
void findPawnCaptures(SearchState& state, const F& fun) {
    bool white = state.active() == Color::WHITE;
    const auto promo = white ? SquareSet::rank(kNumRanks - 1) : SquareSet::rank(0);
    auto leftPawns = state.pawns - SquareSet::file(0);
    auto rightPawns = state.pawns - SquareSet::file(7);
    auto left = (white ? leftPawns << 7 : leftPawns >> 9) & state.occupancy.theirs();
    auto right = (white ? rightPawns << 9 : rightPawns >> 7) & state.occupancy.theirs();
    auto piece = white ? Piece::P : Piece::p;
    for (auto to : left - promo)
        fun(piece,
            Move{{to.file() + 1, white ? to.rank() - 1 : to.rank() + 1}, to, MoveKind::CAPTURE});
    for (auto to : left& promo)
        expandPromos(fun,
                     piece,
                     Move{{to.file() + 1, white ? to.rank() - 1 : to.rank() + 1},
                          to,
                          MoveKind::QUEEN_PROMOTION_CAPTURE});
    for (auto to : right - promo)
        fun(piece,
            Move{{to.file() - 1, white ? to.rank() - 1 : to.rank() + 1}, to, MoveKind::CAPTURE});
    for (auto to : right& promo)
        expandPromos(fun,
                     piece,
                     Move{{to.file() - 1, white ? to.rank() - 1 : to.rank() + 1},
                          to,
                          MoveKind::QUEEN_PROMOTION_CAPTURE});
}

template <typename F>
void findNonPawnMoves(const Board& board, SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];
        auto possibleSquares = movesTable.moves[index(piece)][from.index()] - state.occupancy();
        for (auto to : possibleSquares) {
            // Check for moving through pieces
            if (clearPath(state.occupancy(), from, to)) fun(piece, Move{from, to, Move::QUIET});
        }
    }
}

template <typename F>
void findMoves(const Board& board, SearchState& state, const F& fun) {
    findPawnPushes(state, fun);
    findNonPawnMoves(board, state, fun);
}

/** For use in quiescent search: allow pawn moves that promote or near promotion */
template <typename F>
void findPromotionMoves(SearchState& state, const F& fun) {
    bool white = state.active() == Color::WHITE;
    auto masked = white ? SquareSet::rank(kNumRanks - 2) | SquareSet::rank(kNumRanks - 3)
                        : SquareSet::rank(1) | SquareSet::rank(2);
    auto pawns = state.pawns;
    state.pawns &= masked;
    findPawnPushes(state, fun);
    state.pawns = pawns;
}


template <typename F>
void findCastles(Occupancy occupancy, Turn turn, const F& fun) {
    auto color = int(turn.activeColor());
    auto& info = castlingInfo[color];

    // Check for king side castling
    if ((turn.castling() & info.kingSideMask) != CastlingMask::_) {
        auto path = movesTable.castlingClear[color][index(MoveKind::KING_CASTLE)];
        if ((occupancy() & path).empty())
            fun(info.king, {info.kingSide[0][0], info.kingSide[0][1], MoveKind::KING_CASTLE});
    }
    // Check for queen side castling
    if ((turn.castling() & info.queenSideMask) != CastlingMask::_) {
        auto path = movesTable.castlingClear[color][index(MoveKind::QUEEN_CASTLE)];
        if ((occupancy() & path).empty())
            fun(info.king,
                Move{info.queenSide[0][0], info.queenSide[0][1], MoveKind::QUEEN_CASTLE});
    }
}

template <typename F>
void findNonPawnCaptures(const Board& board, SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];
        auto possibleSquares =
            movesTable.captures[index(piece)][from.index()] & state.occupancy.theirs();
        for (auto to : possibleSquares) {
            // Exclude captures that move through pieces
            if (clearPath(state.occupancy(), from, to))
                fun(piece, Move{from, to, MoveKind::CAPTURE});
        }
    }
}

template <typename F>
void findCaptures(const Board& board, SearchState& state, const F& fun) {
    findPawnCaptures(state, fun);
    findNonPawnCaptures(board, state, fun);
}

template <typename F>
void findEnPassant(const Board& board, Turn turn, const F& fun) {
    auto enPassantTarget = turn.enPassant();
    if (turn.enPassant() == noEnPassantTarget) return;

    // For a given en passant target, there are two potential from squares. If either or
    // both have a pawn of the active color, then capture is possible.
    auto active = turn.activeColor();
    auto pawn = addColor(PieceType::PAWN, active);

    for (auto from : movesTable.enPassantFrom[int(active)][enPassantTarget.file()])
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::EN_PASSANT});
}

BoardChange prepareMove(Board& board, Move move) {
    // Lookup the compound move for the given move kind and target square. This breaks moves like
    // castling, en passant and promotion into a simple capture/move and a second move that can be a
    // no-op move, a quiet move or a promotion. The target square is taken from the compound move.
    auto compound = movesTable.compound[index(move.kind())][move.to().index()];
    auto captured = board[compound.to];
    BoardChange undo = {captured, {move.from(), compound.to}, compound.promo, compound.second};
    return undo;
}
BoardChange makeMove(Board& board, BoardChange change) {
    Piece first = Piece::_;
    std::swap(first, board[change.first[0]]);
    board[change.first[1]] = first;

    // The following statements don't change the board for non-compound moves
    auto second = Piece::_;
    std::swap(second, board[change.second[0]]);
    second = Piece(index(second) + change.promo);
    board[change.second[1]] = second;
    return change;
}

BoardChange makeMove(Board& board, Move move) {
    auto change = prepareMove(board, move);
    return makeMove(board, change);
}

UndoPosition makeMove(Position& position, BoardChange change, Move move) {
    auto ours = position.board[change.first[0]];
    auto undo = UndoPosition{makeMove(position.board, change), position.turn};
    MoveWithPieces mwp = {move, ours, undo.board.captured};
    position.turn = applyMove(position.turn, mwp);
    return undo;
}
UndoPosition makeMove(Position& position, Move move) {
    return makeMove(position, prepareMove(position.board, move), move);
}
void unmakeMove(Board& board, BoardChange undo) {
    Piece ours = Piece::_;
    std::swap(board[undo.second[1]], ours);
    ours = Piece(index(ours) - undo.promo);
    board[undo.second[0]] = ours;
    auto piece = undo.captured;
    std::swap(piece, board[undo.first[1]]);
    board[undo.first[0]] = piece;
}

void unmakeMove(Position& position, UndoPosition undo) {
    unmakeMove(position.board, undo.board);
    position.turn = undo.turn;
}

CastlingMask castlingMask(Square from, Square to) {
    using CM = CastlingMask;

    const struct MaskTable {
        std::array<CM, kNumSquares> mask;
        constexpr MaskTable() : mask{} {
            mask[castlingInfo[0].kingSide[0][0].index()] = CM::KQ;  // White King
            mask[castlingInfo[0].kingSide[1][0].index()] = CM::K;   // White King Side Rook
            mask[castlingInfo[0].queenSide[1][0].index()] = CM::Q;  // White Queen Side Rook
            mask[castlingInfo[1].kingSide[0][0].index()] = CM::kq;  // Black King
            mask[castlingInfo[1].kingSide[1][0].index()] = CM::k;   // Black King Side Rook
            mask[castlingInfo[1].queenSide[1][0].index()] = CM::q;  // Black Queen Side Rook
        }
        CM operator[](Square sq) const { return mask[sq.index()]; }
    } maskTable;

    return maskTable[from] | maskTable[to];
}

Turn applyMove(Turn turn, MoveWithPieces mwp) {
    // Update enPassantTarget
    // Set the en passant target if a pawn moves two squares forward, otherwise reset it.
    turn.setEnPassant(noEnPassantTarget);
    auto move = mwp.move;
    if (move.kind() == MoveKind::DOUBLE_PAWN_PUSH)
        turn.setEnPassant({move.from().file(), (move.from().rank() + move.to().rank()) / 2});

    // Update castlingAvailability
    turn.setCastling(turn.castling() & ~castlingMask(move.from(), move.to()));

    // Update halfmoveClock and halfmoveNumber, and switch the active side.
    turn.tick();
    // Reset halfmove clock on pawn advance or capture
    if (type(mwp.piece) == PieceType::PAWN || isCapture(move.kind())) turn.resetHalfmove();

    return turn;
}

Position applyMove(Position position, Move move) {
    // Remember the piece being moved, before applying the move to the board
    auto piece = position.board[move.from()];

    // Apply the move to the board
    auto undo = makeMove(position.board, move);
    MoveWithPieces mwp = {move, piece, undo.captured};
    position.turn = applyMove(position.turn, mwp);

    return position;
}

Position applyMoves(Position position, MoveVector const& moves) {
    for (auto move : moves) position = applyMove(position, move);

    return position;
}

bool isAttacked(const Board& board, Square square, Occupancy occupancy) {
    // We're using this function to find out if empty squares are attacked for determining
    // legality of castling, so we can't assume that the capture square is occupied.
    auto attackers = occupancy.theirs() & movesTable.attackers[square.index()];
    for (auto from : attackers)
        if (clearPath(occupancy(), from, square) &&
            movesTable.captures[index(board[from])][from.index()].contains(square))
            return true;

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

Move parseUCIMove(Position position, const std::string& move) {
    auto moves = allLegalMovesAndCaptures(position.turn, position.board);

    // Try to find a legal move or capture corresponding to the given move string.
    for (auto m : moves)
        if (std::string(m) == move) return m;

    throw ParseError("Invalid UCI move: " + move);
}

Position applyUCIMove(Position position, const std::string& move) {
    return applyMove(position, parseUCIMove(position, move));
}

bool doesNotCheck(Board& board, const SearchState& state, Move move) {
    auto [from, to, kind] = Move::Tuple(move);

    SquareSet checkSquares = state.kingSquare;
    if (from == state.kingSquare)
        checkSquares = isCastles(kind) ? SquareSet::ipath(from, to) : to;
    else if (!state.pinned.contains(from) && !state.inCheck && kind != MoveKind::EN_PASSANT)
        return true;
    auto delta = movesTable.occupancyDelta[noPromo(kind)][from.index()][to.index()];
    auto check = isAttacked(board, checkSquares, state.occupancy ^ delta);
    return !check;
}

bool mayHavePromoMove(Color side, Board& board, Occupancy occupancy) {
    Piece pawn;
    SquareSet from;
    if (side == Color::WHITE) {
        from = SquareSet(0x00ff'0000'0000'0000ull) & occupancy.ours();
        auto to = SquareSet(0xff00'0000'0000'0000ull) - occupancy();
        from &= to >> 8;
        pawn = Piece::P;
    } else {
        from = SquareSet(0x0000'0000'0000'ff00ull) & occupancy.ours();
        auto to = SquareSet(0x0000'0000'0000'00ffull) - occupancy();
        from &= to << 8;
        pawn = Piece::p;
    }
    for (auto square : from)
        if (board[square] == pawn) return true;
    return false;
}

void forAllLegalQuiescentMoves(Turn turn,
                               Board& board,
                               int depthleft,
                               std::function<void(Move)> action) {
    // Iterate over all moves and captures
    auto state = SearchState(board, turn);
    auto doMove = [&](Piece, Move move) {
        auto change = makeMove(board, move);

        if (doesNotCheck(board, state, move)) action(move);

        unmakeMove(board, change);
    };
    findCaptures(board, state, doMove);

    // Check if the opponent may promote
    bool otherMayPromote = depthleft > options::promotionMinDepthLeft &&
        mayHavePromoMove(!turn.activeColor(), board, !state.occupancy);

    // Avoid horizon effect: don't promote in the last plies
    if (depthleft >= options::promotionMinDepthLeft) findPromotionMoves(state, doMove);

    findEnPassant(board, turn, doMove);
    if (state.inCheck || otherMayPromote) findMoves(board, state, doMove);
}

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, int depthleft) {
    MoveVector legalQuiescentMoves;
    forAllLegalQuiescentMoves(
        turn, board, depthleft, [&](Move move) { legalQuiescentMoves.emplace_back(move); });
    return legalQuiescentMoves;
}

void forAllLegalMovesAndCaptures(Board& board, SearchState& state, MoveFun action) {
    // Iterate over all moves and captures
    auto doMove = [&](Piece piece, Move move) {
        auto change = makeMove(board, move);
        if (doesNotCheck(board, state, move))
            action(board, MoveWithPieces{move, piece, change.captured});
        unmakeMove(board, change);
    };
    findCaptures(board, state, doMove);
    findEnPassant(board, state.turn, doMove);
    findMoves(board, state, doMove);
    if (!state.inCheck) findCastles(state.occupancy, state.turn, doMove);
}

size_t countLegalMovesAndCaptures(Board& board, SearchState& state) {
    size_t count = 0;
    auto countMove = [&count, &board, &state](Piece, Move move) {
        count += doesNotCheck(board, state, move);
    };
    findCaptures(board, state, countMove);
    findEnPassant(board, state.turn, countMove);
    findMoves(board, state, countMove);
    if (!state.inCheck) findCastles(state.occupancy, state.turn, countMove);

    return count;
}

MoveVector allLegalMovesAndCaptures(Turn turn, Board& board) {
    MoveVector legalMoves;
    forAllLegalMovesAndCaptures(
        turn, board, [&](Board&, MoveWithPieces move) { legalMoves.emplace_back(move.move); });
    return legalMoves;
}

namespace for_test {
SquareSet possibleMoves(Piece piece, Square from) {
    return movesTable.moves[index(piece)][from.index()];
}
SquareSet possibleCaptures(Piece piece, Square from) {
    return movesTable.captures[index(piece)][from.index()];
}

void addAvailableMoves(MoveVector& moves, const Board& board, Turn turn) {
    auto state = SearchState(board, turn);
    findMoves(board, state, [&](Piece, Move move) { addMove(moves, move); });
}

void addAvailableCaptures(MoveVector& captures, const Board& board, Turn turn) {
    auto state = SearchState(board, turn);
    findCaptures(board, state, [&](Piece, Move move) { addMove(captures, move); });
}

void addAvailableEnPassant(MoveVector& captures, const Board& board, Turn turn) {
    findEnPassant(board, turn, [&](Piece, Move move) { addMove(captures, move); });
}
}  // namespace for_test
