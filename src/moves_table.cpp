#include <cassert>
#include <cstring>

#include "castling_info.h"

#include "moves_table.h"

namespace init {
/** Compute the delta in occupancy for the given move */
Occupancy occupancyDelta(Square from, Square to, MoveKind kind) {
    SquareSet ours;
    ours.insert(from);
    ours.insert(to);
    SquareSet theirs;
    switch (kind) {
    case MoveKind::O_O: {
        auto& info = castlingInfo[rank(from) != 0];
        ours.insert(info.kingSide[1].to);
        ours.insert(info.kingSide[1].from);
        break;
    }
    case MoveKind::O_O_O: {
        auto& info = castlingInfo[rank(from) != 0];
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
        path |= MovesTable::path(info.kingSide[0].to, info.kingSide[0].from) |
            MovesTable::path(info.kingSide[1].to, info.kingSide[1].from);
    else
        path |= MovesTable::path(info.queenSide[0].to, info.queenSide[0].from) |
            MovesTable::path(info.queenSide[1].to, info.queenSide[1].from);

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

void MovesTable::initializePieceMovesAndCaptures() {
    for (auto piece : pieces) {
        for (auto from : SquareSet::all()) {
            _moves[int(piece)][from] = init::possibleMoves(Piece(piece), from);
            _captures[int(piece)][from] = init::possibleCaptures(Piece(piece), from);
        }
    }
}

void MovesTable::initializeAttackers() {
    // Initialize attackers
    for (auto from : SquareSet::all()) {
        SquareSet toSquares;
        // Gather all possible squares that can be attacked by any piece
        for (auto piece : pieces) toSquares |= _captures[int(piece)][from];
        // Distribute attackers over the target squares
        for (auto to : toSquares) _attackers[to].insert(from);
    }
}

void MovesTable::initializeOccupancyDeltas() {
    // Initialize occupancy changes
    for (int moveKind = 0; moveKind < kNumNoPromoMoveKinds; ++moveKind)
        for (int from = 0; from < kNumSquares; ++from)
            for (int to = 0; to < kNumSquares; ++to)
                _occupancyDelta[moveKind][from][to] =
                    init::occupancyDelta(Square(from), Square(to), MoveKind(moveKind));
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
        _castlingClear[color][index(MoveKind::O_O_O)] =
            init::castlingPath(Color(color), MoveKind::O_O_O);
        _castlingClear[color][index(MoveKind::O_O)] =
            init::castlingPath(Color(color), MoveKind::O_O);
    }
}

void MovesTable::initializeEnPassantFrom() {
    // Initialize en passant from squares
    for (int color = 0; color < 2; ++color) {
        int fromRank = color == 0 ? kNumRanks - 4 : 3;  // skipping 3 ranks from either side
        for (int fromFile = 0; fromFile < kNumFiles; ++fromFile)
            _enPassantFrom[color][fromFile] = {SquareSet::valid(fromRank, fromFile - 1) |
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
            _compound[kind][to] = {Square(to), 0, {Square(to), Square(to)}};

    // Initialize castles
    _compound[index(MK::O_O)][g1] = {g1, 0, {h1, f1}};
    _compound[index(MK::O_O)][g8] = {g8, 0, {h8, f8}};
    _compound[index(MK::O_O_O)][c1] = {c1, 0, {a1, d1}};
    _compound[index(MK::O_O_O)][c8] = {c8, 0, {a8, d8}};

    // En passant helper functions
    auto epRank = [](int rank) -> int { return rank < kNumRanks / 2 ? rank + 1 : rank - 1; };
    auto epTarget = [=](Square to) -> Square { return makeSquare(file(to), epRank(rank(to))); };
    auto epCompound = [=](Square to) -> CM { return {epTarget(to), 0, {epTarget(to), to}}; };

    // Initialize en passant capture for white and black
    for (auto to : (paths[a6][h6] | SquareSet(a6) | SquareSet(h6)) |
             (paths[a3][h3] | SquareSet(a3) | SquareSet(h3)))
        _compound[index(MK::En_Passant)][to] = epCompound(to);

    // Initialize promotion moves
    auto pm = index(MK::Knight_Promotion);
    auto pc = index(MK::Knight_Promotion_Capture);
    for (auto promo = index(PT::KNIGHT); promo <= index(PT::QUEEN); ++promo, ++pm, ++pc)
        for (auto to : (paths[a8][h8] | SquareSet(a8) | SquareSet(h8)) |
                 (paths[a1][h1] | SquareSet(a1) | SquareSet(h1)))
            _compound[pc][to] = _compound[pm][to] = {to, promo, {to, to}};
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

// Define the static member
MovesTable MovesTable::movesTable;
