#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>

#include "moves.h"

std::set<Square> rookMoves(const Square& from) {
    std::set<Square> moves;
    for (int i = 0; i < 8; ++i) {
        if (i != from.rank) {
            moves.insert(Square(i, from.file));
        }
        if (i != from.file) {
            moves.insert(Square(from.rank, i));
        }
    }
    return moves;
}

std::set<Square> bishopMoves(const Square& from) {
    std::set<Square> moves;
    for (int i = 1; i < 8; ++i) {
        if (from.rank + i < 8 && from.file + i < 8) {
            moves.insert(Square(from.rank + i, from.file + i));
        }
        if (from.rank - i >= 0 && from.file - i >= 0) {
            moves.insert(Square(from.rank - i, from.file - i));
        }
        if (from.rank + i < 8 && from.file - i >= 0) {
            moves.insert(Square(from.rank + i, from.file - i));
        }
        if (from.rank - i >= 0 && from.file + i < 8) {
            moves.insert(Square(from.rank - i, from.file + i));
        }
    }
    return moves;
}

std::set<Square> queenMoves(const Square& from) {
    std::set<Square> moves;

    // Combine the moves of a rook and a bishop
    auto rookMvs = rookMoves(from);
    auto bishopMvs = bishopMoves(from);

    moves.insert(rookMvs.begin(), rookMvs.end());
    moves.insert(bishopMvs.begin(), bishopMvs.end());

    return moves;
}

std::set<Square> knightMoves(const Square& from) {
    std::set<Square> moves;
    int knight_moves[8][2] = {{-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};
    for (int i = 0; i < 8; ++i) {
        int new_rank = from.rank + knight_moves[i][0];
        int new_file = from.file + knight_moves[i][1];
        if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
            moves.insert(Square(new_rank, new_file));
        }
    }
    return moves;
}

std::set<Square> kingMoves(const Square& from) {
    std::set<Square> moves;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (i != 0 || j != 0) {
                int new_rank = from.rank + i;
                int new_file = from.file + j;
                if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
                    moves.insert(Square(new_rank, new_file));
                }
            }
        }
    }
    return moves;
}

std::set<Square> whitePawnMoves(const Square& from) {
    std::set<Square> moves;
    if (from.rank == 1 && from.rank + 2 < 8) {
        moves.insert(Square(from.rank + 2, from.file));
    }
    if (from.rank + 1 < 8) {
        moves.insert(Square(from.rank + 1, from.file));
    }
    return moves;
}

std::set<Square> blackPawnMoves(const Square& from) {
    std::set<Square> moves;
    if (from.rank == 6 && from.rank - 2 >= 0) {
        moves.insert(Square(from.rank - 2, from.file));
    }
    if (from.rank - 1 >= 0) {
        moves.insert(Square(from.rank - 1, from.file));
    }
    return moves;
}

std::set<Square> possibleMoves(char piece, const Square& from) {
    switch (piece) {
        case 'P':
            return whitePawnMoves(from);
        case 'p':
            return blackPawnMoves(from);
        case 'N': case 'n':
            return knightMoves(from);
        case 'B': case 'b':
            return bishopMoves(from);
        case 'R': case 'r':
            return rookMoves(from);
        case 'Q': case 'q':
            return queenMoves(from);
        case 'K': case 'k':
            return kingMoves(from);
        default:
            return {};
    }
}

void addCaptureIfOnBoard(std::set<Square>& captures, int rank, int file) {
    if (rank >= 0 && rank < 8 && file >= 0 && file < 8) {
        captures.insert({rank, file});
    }
}

std::set<Square> possibleCaptures(char piece, const Square& from) {
    switch (piece) {
        case 'P':  // White Pawn
        {
            std::set<Square> captures;
            addCaptureIfOnBoard(captures, from.rank - 1, from.file - 1);  // Diagonal left
            addCaptureIfOnBoard(captures, from.rank - 1, from.file + 1);  // Diagonal right
            return captures;
        }
        case 'p':  // Black Pawn
        {
            std::set<Square> captures;
            addCaptureIfOnBoard(captures, from.rank + 1, from.file - 1);  // Diagonal left
            addCaptureIfOnBoard(captures, from.rank + 1, from.file + 1);  // Diagonal right
            return captures;
        }
        case 'N': case 'n':  // Knight (Both white and black)
            return knightMoves(from);
        case 'B': case 'b':  // Bishop (Both white and black)
            return bishopMoves(from);
        case 'R': case 'r':  // Rook (Both white and black)
            return rookMoves(from);
        case 'Q': case 'q':  // Queen (Both white and black)
            return queenMoves(from);
        case 'K': case 'k':  // King (Both white and black)
            return kingMoves(from);
        default:
            return {};
    }
}

bool movesThroughPieces(const ChessBoard& board, const Square& from, const Square& to) {
    int rankDiff = to.rank - from.rank;
    int fileDiff = to.file - from.file;

    // Check if the move isn't horizontal, vertical, or diagonal
    if (rankDiff != 0 && fileDiff != 0 && abs(rankDiff) != abs(fileDiff)) {
        return false; // It's not in straight line, thus no need to check further
    }

    // Calculate the direction of movement for rank and file
    int rankStep = (rankDiff != 0) ? rankDiff / abs(rankDiff) : 0;
    int fileStep = (fileDiff != 0) ? fileDiff / abs(fileDiff) : 0;

    int rankPos = from.rank + rankStep;
    int filePos = from.file + fileStep;

    while (rankPos != to.rank || filePos != to.file) {
        if (board[{rankPos, filePos}] != ' ') {
            return true; // There's a piece in the way
        }
        rankPos += rankStep;
        filePos += fileStep;
    }
    return false;
}

std::set<Move> availableMoves(const ChessBoard& board, char activeColor) {
    std::set<Move> moves;

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square currentSquare{rank, file};
            char piece = board[currentSquare];

            // Skip if the square is empty or if the piece isn't the active color
            if (piece == ' ' || (activeColor == 'w' && std::islower(piece)) || (activeColor == 'b' && std::isupper(piece))) {
                continue;
            }

            auto possibleSquares = possibleMoves(piece, currentSquare);
            for (const auto& dest : possibleSquares) {
                // Check for self-blocking or moving through pieces
                if (board[dest] == ' ' && !movesThroughPieces(board, currentSquare, dest)) {
                    moves.insert({currentSquare, dest});
                }
            }
        }
    }

    return moves;
}


std::set<Move> availableCaptures(const ChessBoard& board, char activeColor) {
    std::set<Move> captures;

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square from = {rank, file};
            char piece = board[from];

            // Check if the piece is of the active color
            if ((std::isupper(piece) && activeColor == 'w') || (std::islower(piece) && activeColor == 'b')) {
                std::set<Square> possibleCaptureSquares = possibleCaptures(piece, from);

                for (const Square& to : possibleCaptureSquares) {
                    char targetPiece = board[to];

                    // Exclude self-capture and moves that move through pieces
                    if (targetPiece == ' ') continue; // No piece to capture

                    if ((std::isupper(piece) && std::islower(targetPiece)) || (std::islower(piece) && std::isupper(targetPiece))) {
                        if (!movesThroughPieces(board, from, to)) {
                            captures.insert(Move{from, to});
                        }
                    }
                }
            }
        }
    }
    return captures;
}

void applyMove(ChessBoard& board, const Move& move) {
    char& piece = board[move.from];
    char& target = board[move.to];

    // Check if it's a pawn promotion
    if ((piece == 'P' && move.to.rank == 0) || (piece == 'p' && move.to.rank == 7)) {
        target = move.promotion; // Promote the pawn to the desired piece
        if (piece == 'p') // If it's a black pawn, make the promoted piece lowercase
            target = std::tolower(target);
    } else {
        target = piece; // Move the piece to the target square
    }
    piece = ' '; // Empty the source square
}

void applyMove(ChessPosition& position, const Move& move) {
    // Apply the move to the board
    applyMove(position.board, move);

    // Update halfMoveClock
    // Reset on pawn advance or capture, else increment
    char piece = position.board[move.from];
    if (tolower(piece) == 'p' || position.board[move.to] != ' ') {
        position.halfmoveClock = 0;
    } else {
        position.halfmoveClock++;
    }

    // Update fullMoveNumber
    // Increment after black's move
    if (position.activeColor == 'b') {
        position.fullmoveNumber++;
    }

    // Update activeColor
    position.activeColor = (position.activeColor == 'w') ? 'b' : 'w';

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

bool isAttacked(const ChessBoard& board, const Square& square) {
    char piece = board[square];
    if (piece == ' ') return false; // The square is empty, so it is not attacked.

    char opponentColor = std::isupper(piece) ? 'b' : 'w';
    auto captures = availableCaptures(board, opponentColor);

    for(const auto& move : captures) {
        if(move.to == square)
            return true; // The square is attacked by some opponent piece.
    }
    return false; // The square is not attacked by any opponent piece.
}

std::set<Square> findPieces(const ChessBoard& board, char piece) {
    std::set<Square> squares;
    for(int rank = 0; rank < 8; ++rank) {
        for(int file = 0; file < 8; ++file) {
            Square sq{rank, file};
            if(board[sq] == piece) {
                squares.insert(sq);
            }
        }
    }
    return squares;
}

bool isInCheck(const ChessBoard& board, char activeColor) {
    char king = (activeColor == 'w') ? 'K' : 'k';
    auto kingSquares = findPieces(board, king);

    if(kingSquares.empty()) return false; // No king of the active color is present.

    Square kingSquare = *kingSquares.begin(); // Get the square where the king is located.
    return isAttacked(board, kingSquare);
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
    auto moves = availableMoves(position.board, position.activeColor);
    auto captures = availableCaptures(position.board, position.activeColor);

    // Combine moves and captures into one set
    moves.insert(captures.begin(), captures.end());

    // Iterate over all moves and captures
    for (const auto& move : moves) {
        // Make a copy of the position to apply the move
        ChessPosition newPosition = position;
        applyMove(newPosition, move);

        // Check if the move would result in our king being in check
        // Adjusted to use newPosition.piecePlacement to pass to isInCheck
        if (!isInCheck(newPosition.board, position.activeColor)) {
            // If not in check, add the move to the map of legal moves
            legalMoves[move] = newPosition;
        }
    }

    return legalMoves;
}
