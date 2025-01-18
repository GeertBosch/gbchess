#include <iostream>
#include <string>

#include "eval.h"
#include "eval_tables.h"
#include "fen.h"
#include "hash.h"
#include "moves.h"

#ifdef DEBUG
#include "debug.h"
#endif

std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << std::string(mv);
}
std::ostream& operator<<(std::ostream& os, Square sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, Color color) {
    return os << (color == Color::BLACK ? 'b' : 'w');
}
std::ostream& operator<<(std::ostream& os, Score score) {
    return os << std::string(score);
}
std::ostream& operator<<(std::ostream& os, const Eval& eval) {
    return os << eval.move << " " << eval.score;
}
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    for (const auto& move : moves) os << " " << std::string(move);
    return os;
}

PieceSquareTable operator+(PieceSquareTable lhs, Score rhs) {
    for (auto& value : lhs) value += rhs;
    return lhs;
}

PieceSquareTable operator+(PieceSquareTable lhs, PieceSquareTable rhs) {
    for (size_t i = 0; i < lhs.size(); ++i) lhs[i] += rhs[i];
    return lhs;
}

PieceSquareTable operator*(PieceSquareTable table, Score score) {
    for (auto& value : table) value *= score;
    return table;
}

void flip(PieceSquareTable& table) {
    // Flip the table vertically (king side remains king side, queen side remains queen side)
    for (auto sq = Square(0); sq != kNumSquares / 2; ++sq) {
        auto other = Square(kNumRanks - 1 - sq.rank(), sq.file());
        std::swap(table[sq.index()], table[other.index()]);
    };
    // Invert the values
    for (auto& sq : table) sq = -sq;
}

// Values of pieces, in centipawns
static std::array<Score, kNumPieces> pieceValues = {
    100_cp,   // White pawn
    300_cp,   // White knight
    300_cp,   // White bishop
    500_cp,   // White rook
    900_cp,   // White queen
    0_cp,     // Not counting the white king
    0_cp,     // No piece
    -100_cp,  // Black pawn
    -300_cp,  // Black knight
    -300_cp,  // Black bishop
    -500_cp,  // Black rook
    -900_cp,  // Black queen
    0_cp,     // Not counting the black king
};
// Values of moves, in addition to the value of the piece captured, in centipawns
static std::array<Score, kNumMoveKinds> moveValues = {
    0_cp,    //  0 Quiet move
    0_cp,    //  1 Double pawn push
    0_cp,    //  2 King castle
    0_cp,    //  3 Queen castle
    0_cp,    //  4 Capture
    0_cp,    //  5 En passant
    0_cp,    //  6 (unused)
    0_cp,    //  7 (unused)
    200_cp,  //  8 Knight promotion
    200_cp,  //  9 Bishop promotion
    400_cp,  // 10 Rook promotion
    800_cp,  // 11 Queen promotion
    200_cp,  // 12 Knight promotion capture
    200_cp,  // 13 Bishop promotion capture
    400_cp,  // 14 Rook promotion capture
    800_cp,  // 15 Queen promotion capture
};

/** The GamePhase reflects the current phase of the game, ranging from opening to endgame.
 * The phase is used to adjust the evaluation function based on the amount of material left on the
 * board. We follow the method of the Rookie 2.0 chess program by M.N.J. van Kervinck.
 * The transition uses multiple steps to avoid sudden changes in the evaluation function.
 * Unlike in Rookie 2.0, we use the phase of the least advanced player as overall phase, rather
 * than maintain a separate phase per side.
 */
struct GamePhase {
    static constexpr int kOpening = 7;
    static constexpr int kEndgame = 0;
    static constexpr Score weights[kOpening + 1] = {
        0_cp,    // Endgame
        14_cp,   // 1
        28_cp,   // 2
        42_cp,   // 3
        58_cp,   // 4
        72_cp,   // 5
        86_cp,   // 6
        100_cp,  // Opening
    };

    uint8_t phase = 0;  // ranges from 7 (opening) down to 0 (endgame)
    GamePhase(int phase) : phase(std::clamp(phase, kEndgame, kOpening)) {}
    GamePhase(const Board& board) {
        int material[2] = {0, 0};  // per color, in pawns
        for (auto piece : board.squares()) {
            auto val = pieceValues[index(piece)].pawns();
            // Ignore pawns
            if (val < -1) material[0] -= val;
            if (val > 1) material[1] += val;
        }
        *this = GamePhase((std::max(material[0], material[1]) - 10) / 2);
    }

    PieceSquareTable interpolate(PieceSquareTable opening, PieceSquareTable endgame) const {
        return opening * weights[phase] + endgame * (100_cp - weights[phase]);
    }
};

EvalTable::EvalTable() {
    for (auto piece : pieces) {
        auto& table = pieceSquareTables[index(piece)];
        table = {};
        table = table + pieceValues[index(piece)];
    }
}

EvalTable::EvalTable(const Board& board, bool usePieceSquareTables) {
    auto phase = GamePhase(board);
    for (auto piece : pieces) {
        auto& table = pieceSquareTables[index(piece)];
        table = {};
        if (piece != Piece::NONE && usePieceSquareTables) {
            switch (type(piece)) {
            case PieceType::PAWN: table = pawnScores; break;
            case PieceType::KNIGHT: table = knightScores; break;
            case PieceType::BISHOP: table = bishopScores; break;
            case PieceType::ROOK: table = rookScores; break;
            case PieceType::QUEEN: table = queenScores; break;
            case PieceType::KING:
                table = phase.interpolate(kingOpeningScores, kingEndgameScore);
                break;
            default: break;
            }
            if (color(piece) == Color::BLACK) flip(table);
        }
        table = table + pieceValues[index(piece)];
    }
}


Score evaluateBoard(const Board& board, const EvalTable& table) {
    Score value = 0_cp;
    auto occupied = SquareSet::occupancy(board);

    for (auto square : occupied) value += table[board[square]][square.index()];

    return value;
}

Score evaluateBoard(const Board& board, bool usePieceSquareTables) {
    auto table = EvalTable(board, usePieceSquareTables);
    auto occupancy = SquareSet::occupancy(board);
    Score value = 0_cp;
    for (auto sq : occupancy) {
        auto piece = board[sq];
        auto score = table[piece][sq.index()];
        value += score;
    }
    return value;
}

bool isInCheck(const Position& position) {
    auto kingPos =
        SquareSet::find(position.board, addColor(PieceType::KING, position.activeColor()));
    return isAttacked(position.board, kingPos, Occupancy(position.board, position.activeColor()));
}

bool isMate(const Position& position) {
    auto board = position.board;
    return allLegalMovesAndCaptures(position.turn, board).size() == 0;
}
