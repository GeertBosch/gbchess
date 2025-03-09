#include <iostream>
#include <string>

#include "eval_tables.h"
#include "fen.h"
#include "hash.h"
#include "moves.h"

#include "eval.h"

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
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    for (const auto& move : moves) os << " " << std::string(move);
    return os;
}

SquareTable operator+(SquareTable lhs, Score rhs) {
    for (auto& value : lhs) value += rhs;
    return lhs;
}

SquareTable operator+(SquareTable lhs, SquareTable rhs) {
    for (size_t i = 0; i < lhs.size(); ++i) lhs[i] += rhs[i];
    return lhs;
}

SquareTable operator*(SquareTable table, Score score) {
    for (auto& value : table) value *= score;
    return table;
}

void flip(SquareTable& table) {
    // Flip the table vertically (king side remains king side, queen side remains queen side)
    for (auto sq = Square(0); sq != kNumSquares / 2; ++sq) {
        auto other = Square(sq.file(), kNumRanks - 1 - sq.rank());
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

    SquareTable interpolate(SquareTable opening, SquareTable endgame) const {
        return opening * weights[phase] + endgame * (100_cp - weights[phase]);
    }
};

int computePhase(const Board& board) {
    return GamePhase(board).phase;
}

struct EvalTables {
    struct Entry {
        TaperedPieceSquareTable tapered;
        PieceValueTable pieceValues;
    };
    std::map<std::string, Entry> entries;
    EvalTables() {
        entries.insert({"Bill Jordan", {Bill_Jordan::tapered, Bill_Jordan::pieceValues}});
        entries.insert(
            {"Tomasz Michniewski", {Tomasz_Michniewski::tapered, Tomasz_Michniewski::pieceValues}});
    }
    const Entry& operator[](std::string name) const {
        auto it = entries.find(name);
        if (it != entries.end()) return it->second;
        throw std::invalid_argument("Unknown eval table set: " + name);
    }
} evalTables;

EvalTable::EvalTable() {
    for (auto piece : pieces) {
        auto& table = pieceSquareTable[index(piece)];
        table = {};
        table = table + pieceValues[index(piece)];
    }
}

EvalTable::EvalTable(const Board& board, bool usePieceSquareTables) {
    auto phase = GamePhase(board);
    auto entry = evalTables["Bill Jordan"];
    auto tapered = entry.tapered;
    auto pieceValues = entry.pieceValues;

    for (auto piece : pieces) {
        auto& table = pieceSquareTable[index(piece)];
        table = {};
        if (piece == Piece::_) continue;
        if (usePieceSquareTables) {
            auto middlegame = tapered[0][index(type(piece))];
            auto endgame = tapered[1][index(type(piece))];
            table = phase.interpolate(middlegame, endgame);
        }
        table = table + pieceValues[index(type(piece))];
        if (color(piece) == Color::BLACK) flip(table);
    }
}

Score evaluateBoard(const Board& board, const EvalTable& table) {
    Score value = 0_cp;
    for (auto sq : SquareSet::occupancy(board)) value += table[board[sq]][sq.index()];

    return value;
}

Score evaluateBoard(const Board& board, bool usePieceSquareTables) {
    auto table = EvalTable(board, usePieceSquareTables);
    return evaluateBoard(board, table);
}

bool isInCheck(const Position& position) {
    auto kingPos =
        SquareSet::find(position.board, addColor(PieceType::KING, position.activeColor()));
    return isAttacked(position.board, kingPos, Occupancy(position.board, position.activeColor()));
}

bool isMate(const Position& position) {
    auto board = position.board;
    return countLegalMovesAndCaptures(position.turn, board) == 0;
}

bool isCheckmate(const Position& position) {
    return isInCheck(position) && isMate(position);
}

bool isStalemate(const Position& position) {
    return !isInCheck(position) && isMate(position);
}