#include <cassert>
#include <cstring>

#include "castling_info.h"
#include "common.h"
#include "magic.h"
#include "moves_table.h"
#include "piece_set.h"

#include "moves.h"

namespace {

struct PinData {
    SquareSet captures;
    PieceSet pinningPieces;
};
}  // namespace

namespace moves {
SquareSet pinnedPieces(const Board& board,
                       Occupancy occupancy,
                       Square kingSquare,
                       const PinData& pinData) {
    SquareSet pinned;
    for (auto pinner : pinData.captures & occupancy.theirs()) {
        // Check if the pin is a valid sliding piece
        if (!pinData.pinningPieces.contains(board[pinner])) continue;

        auto pieces = occupancy() & MovesTable::path(kingSquare, pinner);
        if (pieces.size() == 1) pinned.insert(pieces);
    }
    return pinned & occupancy.ours();
}

SquareSet pinnedPieces(const Board& board, Occupancy occupancy, Square kingSquare) {
    // For the purpose of pinning, we only consider sliding pieces, not knights.

    // Define pinning piece sets and corresponding capture sets
    PinData pinData[] = {
        {MovesTable::possibleCaptures(Piece::R, kingSquare), {PieceType::ROOK, PieceType::QUEEN}},
        {MovesTable::possibleCaptures(Piece::B, kingSquare), {PieceType::BISHOP, PieceType::QUEEN}}};

    auto pinned = SquareSet();

    for (const auto& data : pinData) pinned |= pinnedPieces(board, occupancy, kingSquare, data);

    return pinned;
}

void addMove(MoveVector& moves, Move move) {
    moves.emplace_back(move);
}

BoardChange prepareMove(Board& board, Move move) {
    // Lookup the compound move for the given move kind and target square. This breaks moves like
    // castling, en passant and promotion into a simple capture/move and a second move that can be a
    // no-op move, a quiet move or a promotion. The target square is taken from the compound move.
    auto compound = MovesTable::compoundMove(move);
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
    constexpr struct MaskTable {
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
    for (auto from : occupancy.theirs() & MovesTable::attackers(square))
        if (clearPath(occupancy(), from, square) &&
            MovesTable::possibleCaptures(board[from], from).contains(square))
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
    return MovesTable::possibleCaptures(board[from], from).contains(to);
}

SquareSet attackers(const Board& board, Square target, SquareSet occupancy) {
    SquareSet result;

    auto knightAttackers = MovesTable::possibleCaptures(Piece::N, target) & occupancy;
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
    auto otherAttackers = (occupancy - result) & MovesTable::possibleCaptures(Piece::K, target);
    for (auto from : otherAttackers)
        if (attacks(board, from, target)) result |= from;
    return result;
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
}  // namespace moves