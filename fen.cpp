#include <iostream>
#include <sstream>
#include <string>

#include "common.h"
#include "fen.h"

namespace fen {
MoveKind parsePromotionType(char promo) {
    switch (promo) {
    case 'q': return MoveKind::Queen_Promotion;
    case 'r': return MoveKind::Rook_Promotion;
    case 'b': return MoveKind::Bishop_Promotion;
    case 'n': return MoveKind::Knight_Promotion;
    default: throw ParseError("Invalid promotion format: " + std::string(1, promo));
    }
}

/** Check if to to and from squares could be used for promotion */
bool validPromotion(Square from, Square to) {
    bool validToRank = rank(to) == 0 || rank(to) == kNumRanks - 1;
    bool validRankStep = abs(rank(from) - rank(to)) == 1;
    bool validFileStep = abs(file(from) - file(to)) <= 1;
    return validToRank && validRankStep && validFileStep;
}

/** Check that the (from, to) move can be valid for any piece */
bool validMove(Square from, Square to) {
    int rankDist = abs(rank(to) - rank(from));
    int fileDist = abs(file(to) - file(from));
    // A move is valid if it is not to the same square and the rank and file steps are valid for
    // either a knight, bishop or rook move.
    bool validKnightMove = (rankDist == 2 && fileDist == 1) || (rankDist == 1 && fileDist == 2);
    bool validBishopMove = from != to && rankDist == fileDist;
    bool validRookMove = (rankDist == 0) ^ (fileDist == 0);
    return validKnightMove || validBishopMove || validRookMove;
}

/** Check if the move by itself could possibly be a valid move, and returns either
 * a quiet move or a promotion move, based on the passed string. */
Move parseUCIMove(const std::string& move) {
    if (move.size() != 4 && move.size() != 5)
        throw ParseError("Invalid length for a UCI move, should be 4 or 5: " + move);
    int field[4] = {move[0] - 'a', move[1] - '1', move[2] - 'a', move[3] - '1'};
    if (field[0] < 0 || field[0] >= kNumFiles || field[1] < 0 || field[1] >= kNumRanks ||
        field[2] < 0 || field[2] >= kNumFiles || field[3] < 0 || field[3] >= kNumRanks)
        throw ParseError("Invalid UCI move format: " + move);
    Square from = makeSquare(field[0], field[1]);
    Square to = makeSquare(field[2], field[3]);
    if (!validMove(from, to)) throw ParseError("Invalid UCI move for any piece: " + move);
    auto kind = move.size() == 5 ? parsePromotionType(move[4]) : MoveKind::Quiet_Move;
    if (isPromotion(kind) && !validPromotion(from, to))
        throw ParseError("Invalid promotion move: " + move);
    return Move(from, to, kind);
}

MoveKind setCapture(MoveKind kind) {
    return MoveKind(index(kind) | index(MoveKind::Capture_Mask));  // Set the capture bit
}

Move parseUCIMove(const Board& board, const std::string& move) {
    auto [from, to, kind] = parseUCIMove(move);

    // Any move to an occupied square is a capture
    if (board[to] != Piece::_) return Move(from, to, setCapture(kind));

    auto piece = board[from];
    if (piece == Piece::_) throw ParseError("No piece on the board for UCI move: " + move);
    int rankStep = rank(to) - rank(from);
    int fileStep = file(to) - file(from);

    // If the king moves two squares, it's castling
    if (type(piece) == PieceType::KING && rankStep == 0 && abs(fileStep) == 2)
        return Move(from, to, fileStep < 0 ? MoveKind::O_O_O : MoveKind::O_O);

    // If a pawn moves two squares it's a double push
    if (type(piece) == PieceType::PAWN && fileStep == 0 && abs(rankStep) == 2)
        return Move(from, to, MoveKind::Double_Push);

    // If a pawn moves diagonally and the target square is empty, it must be en passant
    if (type(piece) == PieceType::PAWN && abs(fileStep) == 1 && abs(rankStep) == 1)
        return Move(from, to, MoveKind::En_Passant);

    // Finally, this must be a quiet or promoting move, as determined during parsing
    return Move(from, to, kind);
}

Board parsePiecePlacement(const std::string& piecePlacement) {
    Board board;

    int rank = 0, file = 0;

    for (char ch : piecePlacement) {
        if (ch == '/') {
            rank++;
            file = 0;
        } else if (std::isdigit(ch)) {
            file += ch - '0';  // Move the file by the number of empty squares
        } else {
            Square sq = makeSquare(
                file, 7 - rank);  // Adjust the rank based on how it's stored in the Board
            board[sq] = toPiece(ch);
            file++;
        }
    }

    return board;
}
Turn parseTurn(std::stringstream ss) {
    std::string activeColorStr;
    std::string castlingAvailabilityStr;
    std::string enPassantTargetStr;
    std::string halfmoveClockStr;
    std::string fullmoveNumberStr;


    // Read other components of the FEN string
    ss >> activeColorStr >> castlingAvailabilityStr >> enPassantTargetStr >> halfmoveClockStr >>
        fullmoveNumberStr;

    Color active = activeColorStr == "b" ? Color::b : Color::w;
    CastlingMask castling = CastlingMask::_;
    for (char ch : castlingAvailabilityStr) {
        switch (ch) {
        case 'K': castling |= CastlingMask::K; break;
        case 'Q': castling |= CastlingMask::Q; break;
        case 'k': castling |= CastlingMask::k; break;
        case 'q': castling |= CastlingMask::q; break;
        }
    }
    Turn turn(active,
              castling,
              noEnPassantTarget,
              std::stoi(halfmoveClockStr),
              std::stoi(fullmoveNumberStr));

    if (enPassantTargetStr != "-") {
        int file = enPassantTargetStr[0] - 'a';
        int rank = enPassantTargetStr[1] - '1';
        turn.setEnPassant(makeSquare(file, rank));
    }

    return turn;
}

Position parsePosition(std::stringstream ss) {
    Position position;
    std::string piecePlacementStr;
    // Read piece placement from the FEN string
    std::getline(ss, piecePlacementStr, ' ');
    if (piecePlacementStr == "startpos") return parsePosition(fen::initialPosition);
    position.board = parsePiecePlacement(piecePlacementStr);
    position.turn = parseTurn(std::move(ss));

    return position;
}

Position parsePosition(const std::string& fen) {
    return parsePosition(std::stringstream(fen));
}

bool maybeFEN(std::string fen) {
    if (fen == "startpos") return true;
    std::string startChars = "rnbqkpRNBQKP12345678";
    int slashCount = 0;
    for (char ch : fen) slashCount += ch == '/';
    return fen != "" && startChars.find(fen[0]) != std::string::npos && slashCount == 7;
}

std::string to_string(const Board& board) {
    std::stringstream fen;
    for (int rank = 7; rank >= 0; --rank) {  // Start from the 8th rank and go downwards
        int emptyCount = 0;                  // Count of consecutive empty squares
        for (int file = 0; file < 8; ++file) {
            Square sq =
                makeSquare(file, rank);  // Adjust the rank based on how it's stored in the Board
            auto piece = board[sq];
            if (piece == Piece::_) {  // Empty square
                ++emptyCount;
            } else {
                if (emptyCount > 0) {
                    fen << emptyCount;  // Add the count of empty squares
                    emptyCount = 0;     // Reset the count
                }
                fen << to_char(piece);  // Add the piece
            }
        }
        if (emptyCount > 0)
            fen << emptyCount;     // Add remaining empty squares at the end of the rank
        if (rank > 0) fen << '/';  // Separate ranks by '/'
    }
    return fen.str();
}

std::string to_string(CastlingMask mask) {
    std::string str = "";
    if ((mask & CastlingMask::K) != CastlingMask::_) str += "K";
    if ((mask & CastlingMask::Q) != CastlingMask::_) str += "Q";
    if ((mask & CastlingMask::k) != CastlingMask::_) str += "k";
    if ((mask & CastlingMask::q) != CastlingMask::_) str += "q";
    if (str == "") str = "-";
    return str;
}

std::string to_string(const Turn& turn) {
    std::stringstream str;
    str << to_string(turn.activeColor()) << " ";
    str << to_string(turn.castling()) << " ";
    str << (turn.enPassant() ? to_string(turn.enPassant()) : "-") << " ";
    str << (int)turn.halfmove() << " ";
    str << turn.fullmove();
    return str.str();
}

std::string to_string(const Position& position) {
    std::stringstream fen;
    fen << to_string(position.board) << " " << to_string(position.turn);
    return fen.str();
}

}  // namespace fen