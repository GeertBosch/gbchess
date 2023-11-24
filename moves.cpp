#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>

#include "moves.h"

struct MovesTable {
    // precomputed possible moves for each piece type on each square
    SquareSet moves[kNumPieces][kNumSquares];
    // precomputed possible captures for each piece type on each square
    SquareSet captures[kNumPieces][kNumSquares];
    // precomputed paths from each square to each other square
    SquareSet paths[kNumSquares][kNumSquares];

    MovesTable();
} movesTable;

MovesTable::MovesTable() {
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square from{rank, file};
            for (int piece = 0; piece < kNumPieces; ++piece) {
                moves[piece][from.index()] = possibleMoves(Piece(piece), from);
                captures[piece][from.index()] = possibleCaptures(Piece(piece), from);
            }
        }
    }
    for (int from = 0; from < kNumSquares; ++from) {
        for (int to = 0; to < kNumSquares; ++to) {
            paths[from][to] = SquareSet::path(Square(from), Square(to));
        }
    }
}

SquareSet SquareSet::occupancy(const ChessBoard& board) {
    auto squares = board.squares();
    auto bitmask = [](uint64_t input) -> uint64_t {
        input += 0x7f3f1f0f'7f3f1f0full;  // Cause overflow in the right bits
        input &= 0x80402010'80402010ull;  // These are them, the 8 occupancy bits
        input >>= 4;                      // The low nibbles is where the action is
        input += input >> 16;
        input += input >> 8;
        input += (input >> 28);  // Merge in the high nibbel from the high word
        input &= 0xffull;
        return input;
    };

    uint64_t set = 0;
    uint64_t input;

    for (int j = 0; j < sizeof(squares); j += sizeof(input)) {
        memcpy(&input, &squares[j], sizeof(input));
        set |= bitmask(input) << j;
    }

    return set;
}

SquareSet SquareSet::findPieces(const ChessBoard& board, Piece piece) {
    SquareSet squares;
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square sq{rank, file};
            if (board[sq] == piece) {
                squares.insert(sq);
            }
        }
    }
    return squares;
}


SquareSet rookMoves(Square from) {
    SquareSet moves;
    for (int i = 0; i < 8; ++i) {
        if (i != from.rank()) {
            moves.insert(Square(i, from.file()));
        }
        if (i != from.file()) {
            moves.insert(Square(from.rank(), i));
        }
    }
    return moves;
}

SquareSet bishopMoves(Square from) {
    SquareSet moves;
    for (int i = 1; i < 8; ++i) {
        if (from.rank() + i < 8 && from.file() + i < 8) {
            moves.insert(Square(from.rank() + i, from.file() + i));
        }
        if (from.rank() - i >= 0 && from.file() - i >= 0) {
            moves.insert(Square(from.rank() - i, from.file() - i));
        }
        if (from.rank() + i < 8 && from.file() - i >= 0) {
            moves.insert(Square(from.rank() + i, from.file() - i));
        }
        if (from.rank() - i >= 0 && from.file() + i < 8) {
            moves.insert(Square(from.rank() - i, from.file() + i));
        }
    }
    return moves;
}

SquareSet queenMoves(Square from) {
    SquareSet moves;

    // Combine the moves of a rook and a bishop
    auto rookMvs = rookMoves(from);
    auto bishopMvs = bishopMoves(from);

    moves.insert(rookMvs.begin(), rookMvs.end());
    moves.insert(bishopMvs.begin(), bishopMvs.end());

    return moves;
}

SquareSet knightMoves(Square from) {
    SquareSet moves;
    int knight_moves[8][2] = {
        {-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};
    for (int i = 0; i < 8; ++i) {
        int new_rank = from.rank() + knight_moves[i][0];
        int new_file = from.file() + knight_moves[i][1];
        if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
            moves.insert(Square(new_rank, new_file));
        }
    }
    return moves;
}

SquareSet kingMoves(Square from) {
    SquareSet moves;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (i != 0 || j != 0) {
                int new_rank = from.rank() + i;
                int new_file = from.file() + j;
                if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
                    moves.insert(Square(new_rank, new_file));
                }
            }
        }
    }
    return moves;
}

SquareSet whitePawnMoves(Square from) {
    SquareSet moves;
    if (from.rank() == 1 && from.rank() + 2 < 8) {
        moves.insert(Square(from.rank() + 2, from.file()));
    }
    if (from.rank() + 1 < 8) {
        moves.insert(Square(from.rank() + 1, from.file()));
    }
    return moves;
}

SquareSet blackPawnMoves(Square from) {
    SquareSet moves;
    if (from.rank() == 6 && from.rank() - 2 >= 0) {
        moves.insert(Square(from.rank() - 2, from.file()));
    }
    if (from.rank() - 1 >= 0) {
        moves.insert(Square(from.rank() - 1, from.file()));
    }
    return moves;
}

SquareSet possibleMoves(Piece piece, Square from) {
    switch (piece) {
        case Piece::NONE:
            break;
        case Piece::WHITE_PAWN:
            return whitePawnMoves(from);
        case Piece::BLACK_PAWN:
            return blackPawnMoves(from);
        case Piece::WHITE_KNIGHT:
        case Piece::BLACK_KNIGHT:
            return knightMoves(from);
        case Piece::WHITE_BISHOP:
        case Piece::BLACK_BISHOP:
            return bishopMoves(from);
        case Piece::WHITE_ROOK:
        case Piece::BLACK_ROOK:
            return rookMoves(from);
        case Piece::WHITE_QUEEN:
        case Piece::BLACK_QUEEN:
            return queenMoves(from);
        case Piece::WHITE_KING:
        case Piece::BLACK_KING:
            return kingMoves(from);
    }
    return {};
}

void addCaptureIfOnBoard(SquareSet& captures, int rank, int file) {
    if (rank >= 0 && rank < 8 && file >= 0 && file < 8) {
        captures.insert(Square{rank, file});
    }
}

SquareSet possibleCaptures(Piece piece, Square from) {
    switch (piece) {
        case Piece::NONE:
            break;
        case Piece::WHITE_PAWN:  // White Pawn
        {
            SquareSet captures;
            addCaptureIfOnBoard(captures, from.rank() - 1, from.file() - 1);  // Diagonal left
            addCaptureIfOnBoard(captures, from.rank() - 1, from.file() + 1);  // Diagonal right
            return captures;
        }
        case Piece::BLACK_PAWN:  // Black Pawn
        {
            SquareSet captures;
            addCaptureIfOnBoard(captures, from.rank() + 1, from.file() - 1);  // Diagonal left
            addCaptureIfOnBoard(captures, from.rank() + 1, from.file() + 1);  // Diagonal right
            return captures;
        }
        case Piece::WHITE_KNIGHT:
        case Piece::BLACK_KNIGHT:
            return knightMoves(from);
        case Piece::WHITE_BISHOP:
        case Piece::BLACK_BISHOP:
            return bishopMoves(from);
        case Piece::WHITE_ROOK:
        case Piece::BLACK_ROOK:
            return rookMoves(from);
        case Piece::WHITE_QUEEN:
        case Piece::BLACK_QUEEN:
            return queenMoves(from);
        case Piece::WHITE_KING:
        case Piece::BLACK_KING:
            return kingMoves(from);
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

bool movesThroughPieces(SquareSet occupancy, Square from, Square to) {
    auto path = movesTable.paths[from.index()][to.index()];
    return !(occupancy & path).empty();
}

void addMove(MoveVector& moves, Piece piece, Square from, Square to) {
    // If promoted, add all possible promotions
    if (type(piece) == PieceType::PAWN && (to.rank() == 0 || to.rank() == 7)) {
        for (auto promotion :
             {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN}) {
            moves.emplace_back(Move{from, to, promotion});
        }
    } else {
        moves.emplace_back(Move{from, to});
    }
}

void addAvailableMoves(MoveVector& moves, const ChessBoard& board, Color activeColor) {
    auto occupied = SquareSet::occupancy(board);
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square sq{rank, file};
            auto piece = board[sq];

            // Skip if the square is empty or if the piece isn't the active color
            if (piece == Piece::NONE || activeColor != color(piece))
                continue;

            auto pieceIndex = static_cast<uint8_t>(piece);
            auto possibleSquares = movesTable.moves[pieceIndex][sq.index()];
            for (auto dest : possibleSquares) {
                // Check for occupied target square or moving through pieces
                if (board[dest] == Piece::NONE && !movesThroughPieces(occupied, sq, dest)) {
                    addMove(moves, piece, sq, dest);
                }
            }
        }
    }
}

void addAvailableCaptures(MoveVector& captures, const ChessBoard& board, Color activeColor) {
    auto occupancy = SquareSet::occupancy(board);
    for (Square from = 0; from != Square(kNumSquares); ++from) {
        auto piece = board[from];

        // Check if the piece is of the active color
        if (color(piece) != activeColor)
            continue;

        auto pieceIndex = static_cast<uint8_t>(piece);
        SquareSet possibleCaptureSquares = movesTable.captures[pieceIndex][from.index()];
        for (Square to : possibleCaptureSquares) {
            auto targetPiece = board[to];

            if (targetPiece == Piece::NONE)
                continue;  // No piece to capture

            // Exclude self-capture and moves that move through pieces
            if (color(piece) != color(targetPiece) && !movesThroughPieces(occupancy, from, to))
                addMove(captures, piece, from, to);
        }
    }
}

void applyMove(ChessBoard& board, Move move) {
    auto& piece = board[move.from];
    auto& target = board[move.to];

    // Update the target, including promotion if applicable
    target = move.promotion == PieceType::PAWN ? piece : addColor(move.promotion, color(piece));
    piece = Piece::NONE;  // Empty the source square
}

void applyMove(ChessPosition& position, Move move) {
    // Check if the move is a capture or pawn move before applying it to the board
    bool capture = position.board[move.to] != Piece::NONE;
    auto piece = position.board[move.from];
    bool pawnMove = type(piece) == PieceType::PAWN;

    // Apply the move to the board
    applyMove(position.board, move);

    // Update halfMoveClock
    ++position.halfmoveClock;
    // Reset on pawn advance or capture, else increment
    if (pawnMove || capture)
        position.halfmoveClock = 0;

    // Update fullMoveNumber
    // Increment after black's move
    if (position.activeColor == Color::BLACK)
        ++position.fullmoveNumber;

    // Update activeColor
    auto oldColor = position.activeColor;
    position.activeColor = !position.activeColor;
    assert(oldColor != position.activeColor);

    // Update castlingAvailability
    // Here, you should handle the logic to update the castling rights
    // based on the move made. This includes disabling castling if a rook
    // or king is moved, or if a rook is captured.

    // Update enPassantTarget
    // Set the en passant target if a pawn moves two squares forward,
    // otherwise reset it.
    // This logic depends on whether a pawn has moved two squares from its original rank.

    // ... add logic for enPassantTarget here ...
}
bool isAttacked(const ChessBoard& board, Square square) {
    auto piece = board[square];
    if (piece == Piece::NONE)
        return false;  // The square is empty, so it is not attacked.

    Color opponentColor = !color(piece);
    MoveVector captures;
    addAvailableCaptures(captures, board, opponentColor);

    for (auto move : captures) {
        if (move.to == square)
            return true;  // The square is attacked by some opponent piece.
    }
    return false;  // The square is not attacked by any opponent piece.
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
std::map<Move, ChessPosition> computeAllLegalMoves(const ChessPosition& position) {
    std::map<Move, ChessPosition> legalMoves;

    // Gather all possible moves and captures
    MoveVector moves;
    addAvailableCaptures(moves, position.board, position.activeColor);
    addAvailableMoves(moves, position.board, position.activeColor);

    auto ourKing = addColor(PieceType::KING, position.activeColor);
    auto oldKing = SquareSet::findPieces(position.board, ourKing);

    // Iterate over all moves and captures
    for (auto move : moves) {
        // If we move the king, reflect that in the king squares
        auto newKing = oldKing;
        if (position.board[move.from] == ourKing) {
            newKing.erase(move.from);
            newKing.insert(move.to);
        }

        // Make a copy of the position to apply the move
        ChessPosition newPosition = position;
        applyMove(newPosition, move);

        // Check if the move would result in our king being in check.
        bool inCheck = [&]() {
            for (auto sq : newKing)
                if (isAttacked(newPosition.board, sq))
                    return true;
            return false;
        }();

        if (!inCheck)
            legalMoves[move] = newPosition;
    }

    return legalMoves;
}
