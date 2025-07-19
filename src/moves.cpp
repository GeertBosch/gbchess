#include <cassert>
#include <cstring>


#include "common.h"
#include "magic.h"
#include "options.h"

#include "moves.h"

struct CompoundMove {
    Square to : 8;  // TODO C++20: Can use default initialization for bit-fields
    uint8_t promo = 0;
    FromTo second = {Square(0), Square(0)};
    CompoundMove() = default;
    CompoundMove(Square to, uint8_t promo, FromTo second) : to(to), promo(promo), second(second) {}
};

struct alignas(64) MovesTable {
    // precomputed possible moves for each piece type on each square
    SquareSet moves[kNumPieces][kNumSquares];

    // precomputed possible captures for each piece type on each square
    SquareSet captures[kNumPieces][kNumSquares];

    // precomputed possible squares that each square can be attacked from
    SquareSet attackers[kNumSquares];

    // precomputed delta in occupancy as result of a move, but only for non-promotion moves
    // to save on memory: convert promotion kinds using the noPromo function
    Occupancy occupancyDelta[kNumNoPromoMoveKinds][kNumSquares][kNumSquares];  // moveKind, from, to

    // precomputed horizontal, diagonal and vertical paths from each square to each other square
    SquareSet paths[kNumSquares][kNumSquares];  // from, to

    // precomputed squares required to be clear for castling
    SquareSet castlingClear[2][index(MoveKind::O_O_O) + 1];  // color, moveKind

    // precomputed from squares for en passant targets
    SquareSet enPassantFrom[2][kNumFiles];  // color, file

    // precompute compound moves for castling, en passant and promotions
    CompoundMove compound[kNumMoveKinds][kNumSquares];  // moveKind, to square

    MovesTable();

private:
    void initializePieceMovesAndCaptures();
    void initializeAttackers();
    void initializeOccupancyDeltas();
    void initializePaths();
    void initializeCastlingMasks();
    void initializeEnPassantFrom();
    void initializeCompound();
} movesTable;

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::w), CastlingInfo(Color::b)};
}  // namespace

namespace init {
Occupancy occupancyDelta(Move move) {
    auto [from, to, kind] = move;
    SquareSet ours;
    ours.insert(from);
    ours.insert(to);
    SquareSet theirs;
    switch (kind) {
    case MoveKind::O_O: {
        auto info = castlingInfo[rank(from) != 0];
        ours.insert(info.kingSide[1].to);
        ours.insert(info.kingSide[1].from);
        break;
    }
    case MoveKind::O_O_O: {
        auto info = castlingInfo[rank(from) != 0];
        ours.insert(info.queenSide[1].to);
        ours.insert(info.queenSide[1].from);
        break;
    }
    case MoveKind::Capture:
    case MoveKind::Knight_Promotion_Capture:
    case MoveKind::Bishop_Promotion_Capture:
    case MoveKind::Rook_Promotion_Capture:
    case MoveKind::Queen_Promotion_Capture: theirs.insert(to); break;
    case MoveKind::En_Passant: theirs.insert(makeSquare(file(to), rank(from))); break;
    default: break;
    }
    return Occupancy::delta(theirs, ours);
}
SquareSet castlingPath(Color color, MoveKind side) {
    SquareSet path;
    auto info = castlingInfo[int(color)];

    // Note the paths are reversed, so the start point is excluded and the endpoint included.
    if (side == MoveKind::O_O)
        path |= ::path(info.kingSide[0].to, info.kingSide[0].from) |
            ::path(info.kingSide[1].to, info.kingSide[1].from);
    else
        path |= ::path(info.queenSide[0].to, info.queenSide[0].from) |
            ::path(info.queenSide[1].to, info.queenSide[1].from);

    return path;
}
SquareSet rookMoves(Square from) {
    SquareSet moves;
    for (int rank = 0; rank < kNumRanks; ++rank)
        if (rank != ::rank(from)) moves.insert(makeSquare(file(from), rank));

    for (int file = 0; file < kNumFiles; ++file)
        if (file != ::file(from)) moves.insert(makeSquare(file, rank(from)));

    return moves;
}

SquareSet bishopMoves(Square from) {
    SquareSet moves;
    for (int i = 1; i < std::min(kNumRanks, kNumFiles); ++i) {
        moves.insert(SquareSet::valid(rank(from) + i, file(from) + i));
        moves.insert(SquareSet::valid(rank(from) - i, file(from) - i));
        moves.insert(SquareSet::valid(rank(from) + i, file(from) - i));
        moves.insert(SquareSet::valid(rank(from) - i, file(from) + i));
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
        moves.insert(SquareSet::valid(::rank(from) + rank, ::file(from) + file));
    return moves;
}

SquareSet kingMoves(Square from) {
    int vectors[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

    SquareSet moves;
    for (auto [rank, file] : vectors)
        moves.insert(SquareSet::valid(::rank(from) + rank, ::file(from) + file));
    return moves;
}

SquareSet whitePawnMoves(Square from) {
    SquareSet moves = SquareSet::valid(rank(from) + 1, file(from));
    if (rank(from) == 1) moves.insert(SquareSet::valid(rank(from) + 2, file(from)));
    return moves;
}

SquareSet blackPawnMoves(Square from) {
    SquareSet moves = SquareSet::valid(rank(from) - 1, file(from));
    if (rank(from) == kNumRanks - 2) moves.insert(SquareSet::valid(rank(from) - 2, file(from)));
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
    case Piece::P:                                               // White Pawn
        return SquareSet::valid(rank(from) + 1, file(from) - 1)  // Diagonal left
            | SquareSet::valid(rank(from) + 1, file(from) + 1);  // Diagonal right
    case Piece::p:                                               // Black Pawn
        return SquareSet::valid(rank(from) - 1, file(from) - 1)  // Diagonal left
            | SquareSet::valid(rank(from) - 1, file(from) + 1);  // Diagonal right
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
}  // namespace init

SquareSet path(Square from, Square to) {
    return movesTable.paths[from][to];
}
SquareSet ipath(Square from, Square to) {
    return path(from, to) | SquareSet(from) | SquareSet(to);
}

void MovesTable::initializePieceMovesAndCaptures() {
    for (auto piece : pieces) {
        for (auto from : SquareSet::all()) {
            moves[int(piece)][from] = init::possibleMoves(Piece(piece), from);
            captures[int(piece)][from] = init::possibleCaptures(Piece(piece), from);
        }
    }
}

void MovesTable::initializeAttackers() {
    // Initialize attackers
    for (auto from : SquareSet::all()) {
        SquareSet toSquares;
        // Gather all possible squares that can be attacked by any piece
        for (auto piece : pieces) toSquares |= captures[int(piece)][from];
        // Distribute attackers over the target squares
        for (auto to : toSquares) attackers[to].insert(from);
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
            paths[from][to] = SquareSet::makePath(Square(from), Square(to));
}

void MovesTable::initializeCastlingMasks() {
    // Initialize castling masks and en passant from squares
    for (int color = 0; color < 2; ++color) {
        castlingClear[color][index(MoveKind::O_O_O)] =
            init::castlingPath(Color(color), MoveKind::O_O_O);
        castlingClear[color][index(MoveKind::O_O)] =
            init::castlingPath(Color(color), MoveKind::O_O);
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
    compound[index(MK::O_O)][g1] = {g1, 0, {h1, f1}};
    compound[index(MK::O_O)][g8] = {g8, 0, {h8, f8}};
    compound[index(MK::O_O_O)][c1] = {c1, 0, {a1, d1}};
    compound[index(MK::O_O_O)][c8] = {c8, 0, {a8, d8}};

    // En passant helper functions
    auto epRank = [](int rank) -> int { return rank < kNumRanks / 2 ? rank + 1 : rank - 1; };
    auto epTarget = [=](Square to) -> Square { return makeSquare(file(to), epRank(rank(to))); };
    auto epCompound = [=](Square to) -> CM { return {epTarget(to), 0, {epTarget(to), to}}; };

    // Initialize en passant capture for white
    for (auto to : ipath(a6, h6) | ipath(a3, h3))
        compound[index(MK::En_Passant)][to] = epCompound(to);

    // Initialize promotion moves
    auto pm = index(MK::Knight_Promotion);
    auto pc = index(MK::Knight_Promotion_Capture);
    for (auto promo = index(PT::KNIGHT); promo <= index(PT::QUEEN); ++promo, ++pm, ++pc)
        for (auto to : ipath(a8, h8) | ipath(a1, h1))
            compound[pc][to] = compound[pm][to] = {to, promo, {to, to}};
}

MovesTable::MovesTable() {
    initializePieceMovesAndCaptures();
    initializeAttackers();
    initializeOccupancyDeltas();
    initializePaths();
    initializeCastlingMasks();
    initializeEnPassantFrom();
    initializeCompound();
}

namespace {
struct PinData {
    SquareSet captures;
    PieceSet pinningPieces;
};
}  // namespace

SquareSet pinnedPieces(const Board& board,
                       Occupancy occupancy,
                       Square kingSquare,
                       const PinData& pinData) {
    SquareSet pinned;
    for (auto pinner : pinData.captures & occupancy.theirs()) {
        // Check if the pin is a valid sliding piece
        if (!pinData.pinningPieces.contains(board[pinner])) continue;

        auto path = movesTable.paths[kingSquare][pinner];
        auto pieces = occupancy() & path;
        if (pieces.size() == 1) pinned.insert(pieces);
    }
    return pinned & occupancy.ours();
}

SquareSet pinnedPieces(const Board& board, Occupancy occupancy, Square kingSquare) {
    // For the purpose of pinning, we only consider sliding pieces, not knights.

    // Define pinning piece sets and corresponding capture sets
    PinData pinData[] = {
        {movesTable.captures[index(Piece::R)][kingSquare], {PieceType::ROOK, PieceType::QUEEN}},
        {movesTable.captures[index(Piece::B)][kingSquare], {PieceType::BISHOP, PieceType::QUEEN}}};

    auto pinned = SquareSet();

    for (const auto& data : pinData) pinned |= pinnedPieces(board, occupancy, kingSquare, data);

    return pinned;
}

SearchState::SearchState(const Board& board, Turn turn)
    : occupancy(Occupancy(board, turn.activeColor())),
      pawns(find(board, addColor(PieceType::PAWN, turn.activeColor()))),
      turn(turn),
      kingSquare(*find(board, addColor(PieceType::KING, turn.activeColor())).begin()),
      inCheck(isAttacked(board, kingSquare, occupancy)),
      pinned(pinnedPieces(board, occupancy, kingSquare)) {}

bool clearPath(SquareSet occupancy, Square from, Square to) {
    auto path = movesTable.paths[from][to];
    return (occupancy & path).empty();
}

void addMove(MoveVector& moves, Move move) {
    moves.emplace_back(move);
}

int noPromo(MoveKind kind) {
    MoveKind noPromoKinds[] = {MoveKind::Quiet_Move,
                               MoveKind::Double_Push,
                               MoveKind::O_O,
                               MoveKind::O_O_O,
                               MoveKind::Capture,
                               MoveKind::En_Passant,
                               MoveKind::Quiet_Move,
                               MoveKind::Quiet_Move,
                               MoveKind::Quiet_Move,
                               MoveKind::Quiet_Move,
                               MoveKind::Quiet_Move,
                               MoveKind::Quiet_Move,
                               MoveKind::Capture,
                               MoveKind::Capture,
                               MoveKind::Capture,
                               MoveKind::Capture};
    return index(noPromoKinds[index(kind)]);
}

Occupancy occupancyDelta(Move move) {
    return movesTable.occupancyDelta[noPromo(move.kind)][move.from][move.to];
}

template <typename F>
void expandPromos(const F& fun, Piece piece, Move move) {
    for (int i = 0; i < 4; ++i)
        fun(piece, Move{move.from, move.to, MoveKind(index(move.kind) - i)});
}

template <typename F>
void findPawnPushes(SearchState& state, const F& fun) {
    bool white = state.active() == Color::w;
    const auto doublePushRank = white ? SquareSet::rank(3) : SquareSet::rank(kNumRanks - 1 - 3);
    const auto promo = white ? SquareSet::rank(kNumRanks - 1) : SquareSet::rank(0);
    auto free = ~state.occupancy();
    auto singles = (white ? state.pawns << kNumRanks : state.pawns >> kNumRanks) & free;
    auto doubles = (white ? singles << kNumRanks : singles >> kNumRanks) & free & doublePushRank;
    auto piece = white ? Piece::P : Piece::p;
    for (auto to : singles - promo)
        fun(piece,
            Move{makeSquare(file(to), white ? rank(to) - 1 : rank(to) + 1),
                 to,
                 MoveKind::Quiet_Move});
    for (auto to : singles& promo)
        expandPromos(fun,
                     piece,
                     Move{makeSquare(file(to), white ? rank(to) - 1 : rank(to) + 1),
                          to,
                          MoveKind::Queen_Promotion});
    for (auto to : doubles)
        fun(piece,
            Move{makeSquare(file(to), white ? rank(to) - 2 : rank(to) + 2),
                 to,
                 MoveKind::Double_Push});
}

template <typename F>
void findPawnCaptures(SearchState& state, const F& fun) {
    bool white = state.active() == Color::w;
    const auto promo = white ? SquareSet::rank(kNumRanks - 1) : SquareSet::rank(0);
    auto leftPawns = state.pawns - SquareSet::file(0);
    auto rightPawns = state.pawns - SquareSet::file(7);
    auto left = (white ? leftPawns << 7 : leftPawns >> 9) & state.occupancy.theirs();
    auto right = (white ? rightPawns << 9 : rightPawns >> 7) & state.occupancy.theirs();
    auto piece = white ? Piece::P : Piece::p;
    for (auto to : left - promo)
        fun(piece,
            Move{makeSquare(file(to) + 1, white ? rank(to) - 1 : rank(to) + 1),
                 to,
                 MoveKind::Capture});
    for (auto to : left& promo)
        expandPromos(fun,
                     piece,
                     Move{makeSquare(file(to) + 1, white ? rank(to) - 1 : rank(to) + 1),
                          to,
                          MoveKind::Queen_Promotion_Capture});
    for (auto to : right - promo)
        fun(piece,
            Move{makeSquare(file(to) - 1, white ? rank(to) - 1 : rank(to) + 1),
                 to,
                 MoveKind::Capture});
    for (auto to : right& promo)
        expandPromos(fun,
                     piece,
                     Move{makeSquare(file(to) - 1, white ? rank(to) - 1 : rank(to) + 1),
                          to,
                          MoveKind::Queen_Promotion_Capture});
}

template <typename F>
void findNonPawnMoves(const Board& board, SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];

        if (!state.pinned.contains(from) && !state.inCheck) {
            if (sliders.contains(piece)) {
                // If the piece is a sliding piece, and not pinned, we can move it freely to all
                // target squares for its piece type that are not occupied by our own pieces.
                SquareSet toSquares;
                if (PieceSet{PieceType::BISHOP, PieceType::QUEEN}.contains(piece))
                    toSquares |= targets(from, true, state.occupancy());
                if (PieceSet{PieceType::ROOK, PieceType::QUEEN}.contains(piece))
                    toSquares |= targets(from, false, state.occupancy());
                toSquares -= state.occupancy();

                for (auto to : toSquares) fun(piece, Move{from, to, MoveKind::Quiet_Move});
            } else {
                auto possibleSquares = movesTable.moves[index(piece)][from] - state.occupancy();
                for (auto to : possibleSquares) fun(piece, Move{from, to, MoveKind::Quiet_Move});
            }
        } else {
            auto possibleSquares = movesTable.moves[index(piece)][from] - state.occupancy();
            for (auto to : possibleSquares)  // Check for moving through pieces
                if (clearPath(state.occupancy(), from, to))
                    fun(piece, Move{from, to, MoveKind::Quiet_Move});
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
    bool white = state.active() == Color::w;
    auto masked = white ? SquareSet::rank(kNumRanks - 2) | SquareSet::rank(kNumRanks - 3)
                        : SquareSet::rank(1) | SquareSet::rank(2);
    auto pawns = state.pawns;
    state.pawns &= masked;
    findPawnPushes(state, fun);
    state.pawns = pawns;
}

template <typename F>
void findCastles(SearchState& state, const F& fun) {
    if (state.inCheck) return;  // No castling while in check
    auto occupancy = state.occupancy;
    auto turn = state.turn;
    auto color = int(turn.activeColor());
    auto& info = castlingInfo[color];

    // Check for king side castling
    if ((turn.castling() & info.kingSideMask) != CastlingMask::_) {
        auto path = movesTable.castlingClear[color][index(MoveKind::O_O)];
        if ((occupancy() & path).empty())
            fun(info.king, {info.kingSide[0].from, info.kingSide[0].to, MoveKind::O_O});
    }
    // Check for queen side castling
    if ((turn.castling() & info.queenSideMask) != CastlingMask::_) {
        auto path = movesTable.castlingClear[color][index(MoveKind::O_O_O)];
        if ((occupancy() & path).empty())
            fun(info.king, Move{info.queenSide[0].from, info.queenSide[0].to, MoveKind::O_O_O});
    }
}

template <typename F>
void findNonPawnCaptures(const Board& board, SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];
        if (sliders.contains(piece) && !state.pinned.contains(from) && !state.inCheck) {
            // If the piece is a sliding piece, and not pinned, we can move it freely to all target
            // squares for its piece type that are not occupied by our own pieces.
            SquareSet toSquares;
            if (PieceSet{PieceType::BISHOP, PieceType::QUEEN}.contains(piece))
                toSquares |= targets(from, true, state.occupancy().bits());
            if (PieceSet{PieceType::ROOK, PieceType::QUEEN}.contains(piece))
                toSquares |= targets(from, false, state.occupancy().bits());
            toSquares &= state.occupancy.theirs();

            for (auto to : toSquares) fun(piece, Move{from, to, MoveKind::Capture});
        } else {
            auto possibleSquares =
                movesTable.captures[index(piece)][from] & state.occupancy.theirs();
            for (auto to : possibleSquares) {
                // Exclude captures that move through pieces
                if (clearPath(state.occupancy(), from, to))
                    fun(piece, Move{from, to, MoveKind::Capture});
            }
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

    for (auto from : movesTable.enPassantFrom[int(active)][file(enPassantTarget)])
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::En_Passant});
}

BoardChange prepareMove(Board& board, Move move) {
    // Lookup the compound move for the given move kind and target square. This breaks moves like
    // castling, en passant and promotion into a simple capture/move and a second move that can be a
    // no-op move, a quiet move or a promotion. The target square is taken from the compound move.
    auto compound = movesTable.compound[index(move.kind)][move.to];
    auto captured = board[compound.to];
    BoardChange undo = {captured, compound.promo, {move.from, compound.to}, compound.second};
    return undo;
}

BoardChange makeMove(Board& board, BoardChange change) {
    Piece first = Piece::_;
    std::swap(first, board[change.first.from]);
    board[change.first.to] = first;

    // The following statements don't change the board for non-compound moves
    auto second = Piece::_;
    std::swap(second, board[change.second.from]);
    second = Piece(index(second) + change.promo);
    board[change.second.to] = second;
    return change;
}

BoardChange makeMove(Board& board, Move move) {
    auto change = prepareMove(board, move);
    return makeMove(board, change);
}

UndoPosition makeMove(Position& position, BoardChange change, Move move) {
    auto ours = position.board[change.first.from];
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
    std::swap(board[undo.second.to], ours);
    ours = Piece(index(ours) - undo.promo);
    board[undo.second.from] = ours;
    auto piece = undo.captured;
    std::swap(piece, board[undo.first.to]);
    board[undo.first.from] = piece;
}

void unmakeMove(Position& position, UndoPosition undo) {
    unmakeMove(position.board, undo.board);
    position.turn = undo.turn;
}

CastlingMask castlingMask(Square from, Square to) {
    const struct MaskTable {
        using CM = CastlingMask;
        std::array<CM, kNumSquares> mask;
        constexpr MaskTable() : mask{} {
            mask[castlingInfo[0].kingSide[0].from] = CM::KQ;  // White King
            mask[castlingInfo[0].kingSide[1].from] = CM::K;   // White King Side Rook
            mask[castlingInfo[0].queenSide[1].from] = CM::Q;  // White Queen Side Rook
            mask[castlingInfo[1].kingSide[0].from] = CM::kq;  // Black King
            mask[castlingInfo[1].kingSide[1].from] = CM::k;   // Black King Side Rook
            mask[castlingInfo[1].queenSide[1].from] = CM::q;  // Black Queen Side Rook
        }
        CM operator[](Square sq) const { return mask[sq]; }
    } maskTable;

    return maskTable[from] | maskTable[to];
}

Turn applyMove(Turn turn, MoveWithPieces mwp) {
    // Update enPassantTarget
    // Set the en passant target if a pawn moves two squares forward, otherwise reset it.
    turn.setEnPassant(noEnPassantTarget);
    auto move = mwp.move;
    if (move.kind == MoveKind::Double_Push)
        turn.setEnPassant(makeSquare(file(move.from), (rank(move.from) + rank(move.to)) / 2));

    // Update castlingAvailability
    turn.setCastling(turn.castling() & ~castlingMask(move.from, move.to));

    // Update halfmoveClock and halfmoveNumber, and switch the active side.
    turn.tick();
    // Reset halfmove clock on pawn advance or capture
    if (type(mwp.piece) == PieceType::PAWN || isCapture(move.kind)) turn.resetHalfmove();

    return turn;
}

Position applyMove(Position position, Move move) {
    // Remember the piece being moved, before applying the move to the board
    auto piece = position.board[move.from];

    // Apply the move to the board
    auto undo = makeMove(position.board, move);
    MoveWithPieces mwp = {move, piece, undo.captured};
    position.turn = applyMove(position.turn, mwp);

    return position;
}

bool isAttacked(const Board& board, Square square, Occupancy occupancy) {
    // We're using this function to find out if empty squares are attacked for determining
    // legality of castling, so we can't assume that the capture square is occupied.
    auto attackers = occupancy.theirs() & movesTable.attackers[square];
    for (auto from : attackers)
        if (clearPath(occupancy(), from, square) &&
            movesTable.captures[index(board[from])][from].contains(square))
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

bool attacks(const Board& board, Square from, Square to) {
    return movesTable.captures[index(board[from])][from].contains(to);
}

SquareSet attackers(const Board& board, Square target, SquareSet occupancy) {
    SquareSet result;

    auto knightAttackers = movesTable.captures[index(Piece::N)][target] & occupancy;
    for (auto from : knightAttackers)
        if (type(board[from]) == PieceType::KNIGHT) result |= from;

    auto rookAttackers = targets(target, false, occupancy);
    auto rookPieces = PieceSet{PieceType::ROOK, PieceType::QUEEN};
    for (auto from : rookAttackers)
        if (rookPieces.contains(board[from])) result |= from;

    auto bishopAttackers = targets(target, true, occupancy);
    auto bishopPieces = PieceSet{PieceType::BISHOP, PieceType::QUEEN};
    for (auto from : bishopAttackers)
        if (bishopPieces.contains(board[from])) result |= from;

    // See if there are pawns or kings that can attack the square.
    auto otherAttackers = (occupancy - result) & movesTable.captures[index(Piece::K)][target];
    for (auto from : otherAttackers)
        if (attacks(board, from, target)) result |= from;
    return result;
}

Move checkMove(Position position, Move move) {
    auto moves = allLegalMovesAndCaptures(position.turn, position.board);

    // Try to find a legal move or capture corresponding to the given move string.
    for (auto m : moves)
        if (m == move) return m;

    throw MoveError("Invalid move: " + to_string(move));
}

bool doesNotCheck(Board& board, const SearchState& state, Move move) {
    auto [from, to, kind] = move;

    SquareSet checkSquares = state.kingSquare;
    if (from == state.kingSquare)
        checkSquares = isCastles(kind) ? ipath(from, to) : to;
    else if (!state.pinned.contains(from) && !state.inCheck && kind != MoveKind::En_Passant)
        return true;
    auto delta = movesTable.occupancyDelta[noPromo(kind)][from][to];
    auto check = isAttacked(board, checkSquares, state.occupancy ^ delta);
    return !check;
}

bool mayHavePromoMove(Color side, Board& board, Occupancy occupancy) {
    Piece pawn;
    SquareSet from;
    if (side == Color::w) {
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
    findCastles(state, doMove);
}

size_t countLegalMovesAndCaptures(Board& board, SearchState& state) {
    size_t count = 0;
    auto countMove = [&count, &board, &state](Piece, Move move) {
        count += doesNotCheck(board, state, move);
    };
    findNonPawnCaptures(board, state, countMove);
    findNonPawnMoves(board, state, countMove);

    findPawnCaptures(state, countMove);
    findEnPassant(board, state.turn, countMove);
    findPawnPushes(state, countMove);

    findCastles(state, countMove);

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
    return movesTable.moves[index(piece)][from];
}
SquareSet possibleCaptures(Piece piece, Square from) {
    return movesTable.captures[index(piece)][from];
}
}  // namespace for_test
