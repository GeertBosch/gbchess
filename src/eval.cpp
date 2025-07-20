#include "common.h"
#include <algorithm>
#include <string>

#include "eval_tables.h"
#include "magic.h"
#include "moves.h"
#include "moves_gen.h"
#include "options.h"
#include "piece_set.h"

#include "eval.h"

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
    for (auto sq : Range(Square(0), Square(kNumSquares / 2 - 1))) {
        auto other = makeSquare(file(sq), kNumRanks - 1 - rank(sq));
        std::swap(table[sq], table[other]);
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
        for (auto piece : board) {
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
        if (color(piece) == Color::b) flip(table);
    }
}

Score evaluateBoard(const Board& board, const EvalTable& table) {
    Score value = 0_cp;
    for (auto sq : occupancy(board)) value += table[board[sq]][sq];

    return value;
}

Score evaluateBoard(const Board& board, bool usePieceSquareTables) {
    auto table = EvalTable(board, usePieceSquareTables);
    return evaluateBoard(board, table);
}

Score evaluateBoardSimple(const Board& board) {
    return evaluateBoard(board, false);
}
Score evaluateBoard(const Board& board) {
    return evaluateBoard(board, true);
}
bool isInCheck(const Position& position) {
    auto kingPos = find(position.board, addColor(PieceType::KING, position.active()));
    return isAttacked(position.board, kingPos, Occupancy(position.board, position.active()));
}

bool isMate(const Position& position) {
    auto board = position.board;
    auto state = SearchState(board, position.turn);
    return countLegalMovesAndCaptures(board, state) == 0;
}

bool isCheckmate(const Position& position) {
    return isInCheck(position) && isMate(position);
}

bool isStalemate(const Position& position) {
    return !isInCheck(position) && isMate(position);
}

/**
 * Returns new discovered attackers of square 'to' due to x-rays
 * revealed after removing a piece from the board.
 */
SquareSet discoverXRayAttackers(const Board& board, Square to, SquareSet occ, Color side) {
    SquareSet xrayAttackers;

    // Diagonal directions (bishop, queen)
    SquareSet bishopAttackers = targets(to, /*bishop=*/true, occ) & occ;
    PieceSet bishopPieces = {addColor(PieceType::BISHOP, side), addColor(PieceType::QUEEN, side)};
    for (auto from : bishopAttackers)
        if (bishopPieces.contains(board[from])) xrayAttackers.insert(from);

    // Orthogonal directions (rook, queen)
    SquareSet rookAttackers = targets(to, /*bishop=*/false, occ) & occ;
    PieceSet rookPieces = {addColor(PieceType::ROOK, side), addColor(PieceType::QUEEN, side)};
    for (auto from : rookAttackers)
        if (rookPieces.contains(board[from])) xrayAttackers.insert(from);

    return xrayAttackers;
}

SquareSet leastValuableAttacker(const Board& board,
                                SquareSet attackers,
                                Color side,
                                Piece& nextAttacker) {
    int bestValue = 0;
    Square nextSquare;
    nextAttacker = Piece::_;

    for (Square sq : attackers) {
        Piece piece = board[sq];
        if (color(piece) != side) continue;

        int val = std::abs(pieceValues[index(piece)].cp());
        if (bestValue && bestValue <= val) continue;

        bestValue = val;
        nextSquare = sq;
        nextAttacker = piece;
    }

    return bestValue ? SquareSet{nextSquare} : SquareSet{};
}

/**
 * Static Exchange Evaluation - see https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
 * Returns the material gain/loss from the perspective of the active player.
 */
Score staticExchangeEvaluation(const Board& board, Square from, Square to) {
    int gain[32];  // Max number of piece on the board
    int depth = 0;

    Piece attacker = board[from];
    Piece target = board[to];

    PieceSet mayXray = {PieceType::PAWN, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN};
    SquareSet occ = occupancy(board);
    SquareSet attackers = ::attackers(board, to, occ);
    SquareSet fromSet = SquareSet{from};

    gain[depth] = std::abs(pieceValues[index(target)].cp());
    Color sideToMove = color(attacker);

    Piece nextPiece = attacker;

    do {
        ++depth;
        gain[depth] = std::abs(pieceValues[index(nextPiece)].cp()) - gain[depth - 1];

        // Remove piece from occupancy and attackers
        occ ^= fromSet;
        attackers ^= fromSet;

        // If piece removed may reveal x-rays, re-scan x-ray attacks
        if (mayXray.contains(nextPiece))
            attackers |= discoverXRayAttackers(board, to, occ, sideToMove);

        // Get least valuable attacker of the opposite side
        fromSet = leastValuableAttacker(board, attackers, !sideToMove, nextPiece);
        sideToMove = !sideToMove;
    } while (!fromSet.empty());

    // Minimax backward resolution of speculative scores
    while (--depth) gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    return Score::fromCP(gain[0]);
}
namespace {
/**
 * Base scores for move ordering based on move kind.
 * Higher scores indicate moves that should be searched first.
 */
constexpr int moveKindBaseScores[kNumMoveKinds] = {
    // Non-capturing, non-promoting moves
    0,        // Quiet_Move
    0,        // Double_Push
    400'000,  // O_O (castling)
    400'000,  // O_O_O (castling)

    // Captures that don't promote
    1'000'000,  // Capture
    1'000'000,  // En_Passant
    0,
    0,  // unused slots

    // Promotions that don't capture
    5'000'300,   // Knight_Promotion (5M base + 300 piece value)
    5'000'300,   // Bishop_Promotion (5M base + 300 piece value)
    5'000'500,   // Rook_Promotion (5M base + 500 piece value)
    10'000'000,  // Queen_Promotion (10M base for queen promotions)

    // Promotions that capture
    6'000'300,   // Knight_Promotion_Capture (5M base + 300 piece + 1M capture)
    6'000'300,   // Bishop_Promotion_Capture (5M base + 300 piece + 1M capture)
    6'000'500,   // Rook_Promotion_Capture (5M base + 500 piece + 1M capture)
    11'000'000,  // Queen_Promotion_Capture (10M base + 1M capture)
};

}  // namespace

int scoreSEE(const Board& board, Move move) {
    auto score = staticExchangeEvaluation(board, move.from, move.to).cp();
    if (score < 0) score -= 500'000;  // Bad captures get lower priority but still above quiet moves
    return score;
};

/**
 * Calculates MVV-LVA (Most Valuable Victim - Least Valuable Attacker) score for a capture move.
 * Returns the MVV-LVA score component (not including base capture score).
 * Used as a fallback when Static Exchange Evaluation is disabled.
 */
int scoreMVVLVA(const Board& board, Move move) {
    Piece victim = board[move.to];
    Piece attacker = board[move.from];

    // Piece values for MVV-LVA: P=100, N=300, B=300, R=500, Q=900, K=10000
    auto getSimpleValue = [](Piece piece) -> int {
        switch (type(piece)) {
        case PieceType::PAWN: return 100;
        case PieceType::KNIGHT: return 300;
        case PieceType::BISHOP: return 300;
        case PieceType::ROOK: return 500;
        case PieceType::QUEEN: return 900;
        case PieceType::KING: return 10000;
        case PieceType::EMPTY: return 100;  // Empty square indicates en passant capture
        }
    };

    int victimValue = getSimpleValue(victim);
    int attackerValue = getSimpleValue(attacker);
    return victimValue * 10 - attackerValue;
}

/**
 * Calculates a score for move ordering. Higher scores indicate moves that should be searched first.
 */
int scoreMove(const Board& board, Move move) {
    // Start with base score from move kind table
    int baseScore = moveKindBaseScores[index(move.kind)];

    // For captures, add MVV-LVA or SEE score
    if (isCapture(move.kind))
        baseScore += options::staticExchangeEvaluation
            ? scoreSEE(board, move)      // Use SEE if enabled
            : scoreMVVLVA(board, move);  // Fallback to MVV-LVA

    return baseScore;
}

bool hasNonPawnMaterial(const Position& position) {
    Color activeColor = position.active();

    for (Square sq : Range(Square(0), Square(kNumSquares - 1))) {
        Piece piece = position.board[sq];
        if (piece == Piece::_ || color(piece) != activeColor) continue;

        PieceType pieceType = type(piece);
        if (pieceType != PieceType::PAWN && pieceType != PieceType::KING) {
            return true;  // Found a non-pawn, non-king piece
        }
    }
    return false;  // Only pawns and king remaining
}
