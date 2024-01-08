#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "moves.h"

struct MovesTable {
    // precomputed possible moves for each piece type on each square
    SquareSet moves[kNumPieces][kNumSquares];

    // precomputed possible captures for each piece type on each square
    SquareSet captures[kNumPieces][kNumSquares];

    // precomputed paths from each square to each other square
    SquareSet paths[kNumSquares][kNumSquares];  // from, to

    // precomputed squares required to be clear for castling
    SquareSet castlingClear[2][index(MoveKind::QUEEN_CASTLE) + 1];  // color, moveKind

    // precomputed from squares for en passant targets
    SquareSet enPassantFrom[2][kNumFiles];  // color, file

    MovesTable();
} movesTable;

MovesTable::MovesTable() {
    for (Square from = 0; from != kNumSquares; ++from) {
        for (int piece = 0; piece != kNumPieces; ++piece) {
            moves[piece][from.index()] = possibleMoves(Piece(piece), from);
            captures[piece][from.index()] = possibleCaptures(Piece(piece), from);
        }
    }
    for (int from = 0; from < kNumSquares; ++from) {
        for (int to = 0; to < kNumSquares; ++to) {
            paths[from][to] = SquareSet::path(Square(from), Square(to));
        }
    }
    for (int color = 0; color < 2; ++color) {
        int fromRank = color == 0 ? kNumRanks - 4 : 3;  // skipping 3 ranks from either side
        for (int fromFile = 0; fromFile < kNumFiles; ++fromFile) {
            enPassantFrom[color][fromFile] = {SquareSet::valid(fromRank, fromFile - 1) |
                                              SquareSet::valid(fromRank, fromFile + 1)};
        }
        castlingClear[color][index(MoveKind::QUEEN_CASTLE)] =
            castlingPath(Color(color), MoveKind::QUEEN_CASTLE);
        castlingClear[color][index(MoveKind::KING_CASTLE)] =
            castlingPath(Color(color), MoveKind::KING_CASTLE);
    }
}

struct Occupancy {
    SquareSet theirs;
    SquareSet ours;
    Occupancy(const Board& board, Color activeColor) {
        theirs = SquareSet::occupancy(board, !activeColor);
        ours = SquareSet::occupancy(board, activeColor);
    }
};

/**
 * Returns the bits corresponding to the bytes in the input that contain the nibble.
 * Note: nibble is assumed to be at most 4 bits.
 * The implementation really finds everything unequal and then inverts the result.
 */
uint64_t equalSet(uint64_t input, uint8_t nibble) {
    const uint64_t occupancyNibbles = 0x80402010'08040201ull;  // high nibbles to indicate occupancy
    const uint64_t oneNibbles = 0x01010101'01010101ull;        // low nibbles set to one
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

uint64_t equalSet(std::array<Piece, 64> squares, Piece piece, bool invert) {
    static_assert(kNumPieces <= 16, "Piece must fit in 4 bits");

    uint64_t set = 0;
    for (int j = 0; j < sizeof(squares); j += sizeof(uint64_t)) {
        uint64_t input;
        memcpy(&input, &squares[j], sizeof(input));
        set |= equalSet(input, static_cast<uint8_t>(piece)) << j;
    }

    return invert ? ~set : set;
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

/**
 *  Returns the bits corresponding to the bytes in the input that are less than the piece.
 */
uint64_t lessSet(std::array<Piece, 64> squares, Piece piece, bool invert) {
    static_assert(kNumPieces <= 16, "Piece must fit in 4 bits");
    if (!int(piece)) return invert ? 0xffffffffffffffffull : 0;  // Special case for empty squares

    uint64_t set = 0;
    for (int j = 0; j < sizeof(squares); j += sizeof(uint64_t)) {
        uint64_t input;
        memcpy(&input, &squares[j], sizeof(input));
        set |= lessSet(input, static_cast<uint8_t>(piece)) << j;
    }

    return invert ? ~set : set;
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
    // If promoted, add all possible promotions
    if (type(piece) == PieceType::PAWN && (move.to.rank() == 0 || move.to.rank() == 7)) {
        for (auto promotion : {MoveKind::KNIGHT_PROMOTION,
                               MoveKind::BISHOP_PROMOTION,
                               MoveKind::ROOK_PROMOTION,
                               MoveKind::QUEEN_PROMOTION}) {
            move.kind = promotion;
            moves.emplace_back(move);
        }
    } else {
        moves.emplace_back(move);
    }
}

template <typename F>
void findMoves(const Board& board, Color activeColor, const F& fun) {
    auto occupied = SquareSet::occupancy(board);
    for (auto from : occupied) {
        auto piece = board[from];

        // Skip if piece isn't the active color
        if (color(piece) != activeColor) continue;

        auto possibleSquares = movesTable.moves[index(piece)][from.index()] & !occupied;
        for (auto to : possibleSquares) {
            auto kind = type(piece) == PieceType::PAWN && std::abs(from.rank() - to.rank()) == 2
                ? MoveKind::DOUBLE_PAWN_PUSH
                : MoveKind::QUIET_MOVE;
            // Check for occupied target square or moving through pieces
            if (clearPath(occupied, from, to)) fun(piece, Move{from, to, kind});
        }
    }
}

template <typename F>
void findCastles(const Board& board, Color activeColor, CastlingMask mask, const F& fun) {
    auto occupied = SquareSet::occupancy(board);
    auto color = int(activeColor);
    auto& info = castlingInfo[color];
    if ((mask & info.kingSideMask) != CastlingMask::NONE) {
        auto path = movesTable.castlingClear[color][index(MoveKind::KING_CASTLE)];
        if ((occupied & path).empty())
            fun(info.king, {info.kingFrom, info.kingToKingSide, MoveKind::KING_CASTLE});
    }
    if ((mask & info.queenSideMask) != CastlingMask::NONE) {
        auto path = movesTable.castlingClear[color][index(MoveKind::QUEEN_CASTLE)];
        if ((occupied & path).empty())
            fun(info.king, Move{info.kingFrom, info.kingToQueenSide, MoveKind::QUEEN_CASTLE});
    }
}

template <typename F>
void findCaptures(const Board& board, Color activeColor, const F& fun) {
    auto occupied = SquareSet::occupancy(board);
    for (auto from : occupied) {
        auto piece = board[from];

        // Check if the piece is of the active color
        if (color(piece) != activeColor) continue;

        auto possibleSquares = movesTable.captures[index(piece)][from.index()] & occupied;
        for (auto to : possibleSquares) {
            // Exclude self-capture and moves that move through pieces
            if (color(board[to]) != activeColor && clearPath(occupied, from, to))
                fun(piece, Move{from, to, MoveKind::CAPTURE});
        }
    }
}

template <typename F>
void findEnPassant(const Board& board, Color activeColor, Square enPassantTarget, const F& fun) {
    if (enPassantTarget == noEnPassantTarget) return;

    // For a given en passant target, there are two potential from squares. If either or
    // both have a pawn of the active color, then capture is possible.
    auto pawn = addColor(PieceType::PAWN, activeColor);
    for (auto from : movesTable.enPassantFrom[int(activeColor)][enPassantTarget.file()])
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::EN_PASSANT});
}

void addAvailableMoves(MoveVector& moves, const Board& board, Color activeColor) {
    findMoves(board, activeColor, [&](Piece piece, Move move) { addMove(moves, piece, move); });
}

void addAvailableCaptures(MoveVector& captures, const Board& board, Color activeColor) {
    findCaptures(
        board, activeColor, [&](Piece piece, Move move) { addMove(captures, piece, move); });
}

void addAvailableEnPassant(MoveVector& captures,
                           const Board& board,
                           Color activeColor,
                           Square enPassantTarget) {
    findEnPassant(board, activeColor, enPassantTarget, [&](Piece piece, Move move) {
        addMove(captures, piece, move);
    });
}

void addAvailableCastling(MoveVector& captures,
                          const Board& board,
                          Color activeColor,
                          CastlingMask mask) {
    findCastles(
        board, activeColor, mask, [&](Piece piece, Move move) { addMove(captures, piece, move); });
}

Piece makeMove(Board& board, Move move) {
    auto& piece = board[move.from];
    auto& target = board[move.to];

    // En passant capture
    switch (move.kind) {
    case MoveKind::KING_CASTLE: {
        auto rank = move.from.rank();
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
        auto rank = move.from.rank();
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
        std::swap(board[Square{move.from.rank(), move.to.file()}], target);
        break;

    case MoveKind::KNIGHT_PROMOTION:
    case MoveKind::BISHOP_PROMOTION:
    case MoveKind::ROOK_PROMOTION:
    case MoveKind::QUEEN_PROMOTION:
    case MoveKind::KNIGHT_PROMOTION_CAPTURE:
    case MoveKind::BISHOP_PROMOTION_CAPTURE:
    case MoveKind::ROOK_PROMOTION_CAPTURE:
    case MoveKind::QUEEN_PROMOTION_CAPTURE:
        piece = addColor(promotionType(move.kind), color(piece));  // Promote the pawn
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
    switch (move.kind) {
    case MoveKind::KING_CASTLE: {
        auto rank = move.from.rank();
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
        auto rank = move.from.rank();
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
        std::swap(board[Square{move.from.rank(), move.to.file()}], captured);
        board[move.from] = board[move.to];
        board[move.to] = captured;
        break;
    case MoveKind::KNIGHT_PROMOTION:
    case MoveKind::BISHOP_PROMOTION:
    case MoveKind::ROOK_PROMOTION:
    case MoveKind::QUEEN_PROMOTION:
    case MoveKind::KNIGHT_PROMOTION_CAPTURE:
    case MoveKind::BISHOP_PROMOTION_CAPTURE:
    case MoveKind::ROOK_PROMOTION_CAPTURE:
    case MoveKind::QUEEN_PROMOTION_CAPTURE:
        board[move.to] = addColor(PieceType::PAWN, color(board[move.to]));  // Demote the piece
        [[fallthrough]];
    case MoveKind::QUIET_MOVE:
    case MoveKind::DOUBLE_PAWN_PUSH:
    case MoveKind::CAPTURE:
        board[move.from] = board[move.to];
        board[move.to] = captured;
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
    if (move.kind == MoveKind::DOUBLE_PAWN_PUSH) {
        turn.enPassantTarget = {(move.from.rank() + move.to.rank()) / 2, move.from.file()};
    }
    // Update castlingAvailability
    turn.castlingAvailability &= ~castlingMask(move.from, move.to);

    // Update halfMoveClock: reset on pawn advance or capture, else increment
    ++turn.halfmoveClock;
    if (type(piece) == PieceType::PAWN || isCapture(move.kind)) turn.halfmoveClock = 0;

    // Update fullMoveNumber: increment after black's move
    if (turn.activeColor == Color::BLACK) ++turn.fullmoveNumber;

    // Update activeColor
    turn.activeColor = !turn.activeColor;
    return turn;
}

Position applyMove(Position position, Move move) {
    // Check if the move is a capture or pawn move before applying it to the board
    auto piece = position.board[move.from];

    // Apply the move to the board
    makeMove(position.board, move);
    position.turn = applyMove(position.turn, piece, move);

    return position;
}

bool isAttacked(const Board& board, Square square, SquareSet opponentSquares) {
    // We're using this function to find out if empty squares are attacked for determining
    // legality of castling, so the code below is incorrect.
    auto occupancy = SquareSet::occupancy(board);
    for (Square from : opponentSquares) {
        auto piece = board[from];
        auto possibleCaptureSquares = movesTable.captures[index(piece)][from.index()];
        if (possibleCaptureSquares.contains(square) && clearPath(occupancy, from, square))
            return true;
    }
    return false;
}

bool isAttacked(const Board& board, SquareSet squares, SquareSet opponentSquares) {
    for (auto square : squares) {
        if (isAttacked(board, square, opponentSquares)) return true;
    }
    return false;
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

/**
 * Computes all legal moves from a given chess position, mapping each move to the resulting
 * chess position after the move is applied. This function checks for moves that do not leave
 * or place the king of the active color in check.
 *
 * @param position The starting chess position.
 * @return A map where each key is a legal move and the corresponding value is the new chess
 *         position resulting from that move.
 */
ComputedMoveVector allLegalMoves(Position position) {
    ComputedMoveVector legalMoves;
    auto turn = position.turn;
    auto& board = position.board;

    auto ourKing = addColor(PieceType::KING, turn.activeColor);
    auto oldKing = SquareSet::find(board, ourKing);
    auto occupancy = Occupancy(board, turn.activeColor);

    // Iterate over all moves and captures
    auto addIfLegal = [&](Piece piece, Move move) {
        auto [from, to, kind] = move;
        // If we move the king, reflect that in the king squares
        auto newKing = oldKing;
        if (piece == ourKing) {
            if (kind == MoveKind::KING_CASTLE || kind == MoveKind::QUEEN_CASTLE)
                newKing |= movesTable.paths[from.index()][to.index()];
            else
                newKing.erase(from);
            newKing.insert(to);
        }

        // Make a copy of the position to apply the move
        auto captured = makeMove(board, move);

        // Check if the move would result in our king being in check.
        auto newTurn = applyMove(turn, piece, move);
        auto newOpponentSquares = occupancy.theirs & !SquareSet(to);
        // Adjust the opponent squares for the en passant case.
        if (move.kind == MoveKind::EN_PASSANT) newOpponentSquares.erase(turn.enPassantTarget);

        if (isAttacked(board, newKing, newOpponentSquares))
            ;  // Ignore the move
        else if (type(piece) == PieceType::PAWN && (to.rank() == 0 || to.rank() == kNumRanks - 1)) {
            // If promoted, add all possible promotions, legality is not affected
            for (auto promotion : {MoveKind::KNIGHT_PROMOTION,
                                   MoveKind::BISHOP_PROMOTION,
                                   MoveKind::ROOK_PROMOTION,
                                   MoveKind::QUEEN_PROMOTION}) {
                board[to] = addColor(promotionType(promotion), turn.activeColor);
                legalMoves.emplace_back(Move{from, to, promotion}, Position{board, newTurn});
                board[to] = piece;  // undo the promotion
            }
        } else {
            legalMoves.emplace_back(Move{from, to, kind}, Position{board, newTurn});
        }
        unmakeMove(board, move, captured);
    };

    findCaptures(board, turn.activeColor, addIfLegal);
    findEnPassant(board, turn.activeColor, turn.enPassantTarget, addIfLegal);
    findMoves(board, turn.activeColor, addIfLegal);
    findCastles(board, turn.activeColor, turn.castlingAvailability, addIfLegal);

    return legalMoves;
}
