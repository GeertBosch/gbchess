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

/**
 * Returns the bits corresponding to the bytes in the input that contain the nibble.
 * Note: nibble is assumed to be at most 4 bits.
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
    input ^= 0xffull;
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

SquareSet SquareSet::occupancy(const Board& board) {
    return equalSet(board.squares(), Piece::NONE, true);
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

void addMove(MoveVector& moves, Piece piece, Square from, Square to, MoveKind kind) {
    // If promoted, add all possible promotions
    if (type(piece) == PieceType::PAWN && (to.rank() == 0 || to.rank() == 7)) {
        for (auto promotion : {MoveKind::KNIGHT_PROMOTION,
                               MoveKind::BISHOP_PROMOTION,
                               MoveKind::ROOK_PROMOTION,
                               MoveKind::QUEEN_PROMOTION}) {
            moves.emplace_back(Move{from, to, promotion | kind});
        }
    } else {
        moves.emplace_back(Move{from, to, kind});
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
            // Check for occupied target square or moving through pieces
            if (clearPath(occupied, from, to)) fun(piece, from, to);
        }
    }
}

template <typename F>
void findCastles(const Board& board, Color activeColor, CastlingMask mask, const F& fun) {
    auto occupied = SquareSet::occupancy(board);
    if (activeColor == Color::WHITE) {
        if ((mask & CastlingMask::WHITE_KINGSIDE) != CastlingMask::NONE) {
            auto path = movesTable.castlingClear[0][index(MoveKind::KING_CASTLE)];
            if ((occupied & path).empty())
                fun(Piece::WHITE_KING, Position::whiteKing, Position::whiteKingSideRook);
        }
        if ((mask & CastlingMask::WHITE_QUEENSIDE) != CastlingMask::NONE) {
            auto path = movesTable.castlingClear[0][index(MoveKind::QUEEN_CASTLE)];
            if ((occupied & path).empty())
                fun(Piece::WHITE_KING, Position::whiteKing, Position::whiteQueenSideRook);
        }
    } else {
        assert(activeColor == Color::BLACK);
        if ((mask & CastlingMask::BLACK_KINGSIDE) != CastlingMask::NONE) {
            auto path = movesTable.castlingClear[1][index(MoveKind::KING_CASTLE)];
            if ((occupied & path).empty())
                fun(Piece::BLACK_KING, Position::blackKing, Position::blackKingSideRook);
        }
        if ((mask & CastlingMask::BLACK_QUEENSIDE) != CastlingMask::NONE) {
            auto path = movesTable.castlingClear[1][index(MoveKind::QUEEN_CASTLE)];
            if ((occupied & path).empty())
                fun(Piece::BLACK_KING, Position::blackKing, Position::blackQueenSideRook);
        }
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
                fun(piece, from, to);
        }
    }
}

template <typename F>
void findEnPassant(const Board& board, Color activeColor, Square enPassantTarget, const F& fun) {
    if (enPassantTarget != Position::noEnPassantTarget) {
        if (false)
            std::cout << "En passant target: " << std::string(enPassantTarget) << " (rank "
                      << enPassantTarget.rank() << ")" << std::endl;
        assert((activeColor == Color::WHITE && enPassantTarget.rank() == 5) ||
               (activeColor == Color::BLACK && enPassantTarget.rank() == 2));
        // For a given en passant target, there are two potential from squares. If either or
        // both have a pawn of the active color, then capture is possible.
        auto pawn = activeColor == Color::WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN;
        for (auto from : movesTable.enPassantFrom[int(activeColor)][enPassantTarget.file()]) {
            // std::cout << "trying from square " << std::string(from) << std::endl;
            if (board[from] == pawn) {
                if (false)
                    std::cout << "En passant from " << std::string(from) << " to "
                              << std::string(enPassantTarget) << " (" << to_string(activeColor)
                              << ")" << std::endl;
                fun(pawn, from, enPassantTarget);
            }
        }
    }
}

void addAvailableMoves(MoveVector& moves, const Board& board, Color activeColor) {
    findMoves(board, activeColor, [&moves](Piece piece, Square from, Square to) {
        addMove(moves, piece, from, to, MoveKind::QUIET_MOVE);
    });
}

void addAvailableCaptures(MoveVector& captures, const Board& board, Color activeColor) {
    findCaptures(board, activeColor, [&captures](Piece piece, Square from, Square to) {
        addMove(captures, piece, from, to, MoveKind::CAPTURE);
    });
}

void addAvailableEnPassant(MoveVector& captures,
                           const Board& board,
                           Color activeColor,
                           Square enPassantTarget) {
    findEnPassant(
        board, activeColor, enPassantTarget, [&captures](Piece piece, Square from, Square to) {
            addMove(captures, piece, from, to, MoveKind::EN_PASSANT);
        });
}

void applyMove(Board& board, Move move) {
    auto& piece = board[move.from];
    auto& target = board[move.to];
    if (type(piece) == PieceType::PAWN && target == Piece::NONE &&
        move.from.file() != move.to.file()) {
        // En passant capture
        board[Square{move.from.rank(), move.to.file()}] = Piece::NONE;
    }

    // Update the target, including promotion if applicable
    target = move.isPromotion() ? addColor(promotionType(move.kind), color(piece)) : piece;
    piece = Piece::NONE;  // Empty the source square
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

Position applyMove(Position position, Move move) {
    // Check if the move is a capture or pawn move before applying it to the board
    bool capture = position.board[move.to] != Piece::NONE;
    auto piece = position.board[move.from];
    bool pawnMove = type(piece) == PieceType::PAWN;

    // Apply the move to the board
    applyMove(position.board, move);

    // Update enPassantTarget
    // Set the en passant target if a pawn moves two squares forward, otherwise reset it.
    position.enPassantTarget = Position::noEnPassantTarget;
    if (pawnMove && abs(move.from.rank() - move.to.rank()) == 2) {
        position.enPassantTarget = {(move.from.rank() + move.to.rank()) / 2, move.from.file()};
        if (false)
            std::cout << "Setting en passant target: " << std::string(position.enPassantTarget)
                      << std::endl;
    }
    // Update castlingAvailability
    position.castlingAvailability &= ~castlingMask(move.from, move.to);

    // Update halfMoveClock
    // Reset on pawn advance or capture, else increment
    ++position.halfmoveClock;
    if (pawnMove || capture) position.halfmoveClock = 0;

    // Update fullMoveNumber
    // Increment after black's move
    if (position.activeColor == Color::BLACK) ++position.fullmoveNumber;

    // Update activeColor
    auto oldColor = position.activeColor;
    position.activeColor = !position.activeColor;

    return position;
}

bool isAttacked(const Board& board, Square square) {
    auto piece = board[square];
    if (piece == Piece::NONE) return false;  // The square is empty, so it is not attacked.

    Color opponentColor = !color(piece);
    auto occupancy = SquareSet::occupancy(board);
    for (Square from : occupancy) {
        auto piece = board[from];

        // Check if the piece is of the opponent's color
        if (color(piece) != opponentColor) continue;

        auto possibleCaptureSquares = movesTable.captures[index(piece)][from.index()];
        if (possibleCaptureSquares.contains(square) && clearPath(occupancy, from, square))
            return true;
    }
    return false;
}

bool isAttacked(const Board& board, SquareSet squares) {
    for (auto square : squares) {
        if (isAttacked(board, square)) return true;
    }
    return false;
}

/**
 * Computes all legal moves from a given chess position, mapping each move to the resulting
 * chess position after the move is applied. This function checks for moves that do not leave
 * or place the king of the active color in check.
 *
 * @param position The starting chess position.
 * @return A map where each key is a legal move and the corresponding value is the new chess
 *         position resulting from that move.
 */
ComputedMoveVector allLegalMoves(const Position& position) {
    ComputedMoveVector legalMoves;

    auto ourKing = addColor(PieceType::KING, position.activeColor);
    auto oldKing = SquareSet::find(position.board, ourKing);

    // Iterate over all moves and captures
    auto addIfLegal = [&](Piece piece, Square from, Square to) {
        // If we move the king, reflect that in the king squares
        auto newKing = oldKing;
        if (piece == ourKing) {
            newKing.erase(from);
            newKing.insert(to);
        }

        auto kind = position.board[to] == Piece::NONE ? MoveKind::QUIET_MOVE : MoveKind::CAPTURE;
        Move move = {from, to, kind};  // For now assume no promotion applies

        // Make a copy of the position to apply the move
        auto newPosition = applyMove(position, move);

        // Check if the move would result in our king being in check.
        if (isAttacked(newPosition.board, newKing)) return;

        // If promoted, add all possible promotions, legality is not affected
        if (type(piece) == PieceType::PAWN && (to.rank() == 0 || to.rank() == kNumRanks - 1)) {
            for (auto promotion : {MoveKind::KNIGHT_PROMOTION,
                                   MoveKind::BISHOP_PROMOTION,
                                   MoveKind::ROOK_PROMOTION,
                                   MoveKind::QUEEN_PROMOTION}) {
                newPosition.board[to] = addColor(promotionType(promotion), position.activeColor);
                legalMoves.emplace_back(Move{from, to, promotion}, newPosition);
            }
        } else {
            legalMoves.emplace_back(Move{from, to, kind}, newPosition);
        }
    };

    findCaptures(position.board, position.activeColor, addIfLegal);
    findEnPassant(position.board, position.activeColor, position.enPassantTarget, addIfLegal);
    findMoves(position.board, position.activeColor, addIfLegal);

    return legalMoves;
}
