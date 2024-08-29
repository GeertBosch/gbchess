#include <iostream>
#include <string>
#include <unordered_set>

#include "eval.h"
#include "eval_tables.h"
#include "fen.h"
#include "hash.h"
#include "moves.h"
#include "options.h"

#ifdef DEBUG
#include "debug.h"
#endif
#define D \
    if constexpr (debug) std::cerr

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
    return os << eval.move << " " << eval.evaluation;
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
/**
 * The transposition table is a hash table that stores the best move found for a position, so it can
 * be reused in subsequent searches. The table is indexed by the hash of the position, and stores
 * the best move found so far, along with the evaluation of the position after that move.
 * Evaluations are always from the perspective of the player to move. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 *
 * Generations are used to be able to tell stale entries from fresh ones. When a new search is
 * started, the generation is incremented, and all new entries are marked with the new generation.
 * This allows us to still use the old entries for things like move ordering, but prefer new ones.
 */
struct TranspositionTable {
    static constexpr size_t kNumEntries = Options::transpositionTableEntries;

    enum EntryType { EXACT, LOWERBOUND, UPPERBOUND };

    struct Entry {
        Hash hash;
        Eval move;
        uint8_t generation = 0;
        uint8_t depth = 0;
        EntryType type = EXACT;
        Entry() = default;

        Entry(Hash hash, Eval move, uint8_t generation, uint8_t depth, EntryType type)
            : hash(hash), move(move), generation(generation), depth(depth), type(type) {}
    };

    std::vector<Entry> entries{kNumEntries};
    // std::array<Entry, kNumEntries> entries{};
    uint64_t numInserted = 0;
    uint64_t numOccupied = 0;   // Number of non-obsolete entries
    uint64_t numObsoleted = 0;  // Number of entries in the table from previous generations
    uint64_t numCollisions = 0;
    uint64_t numImproved = 0;
    uint64_t numHits = 0;
    uint64_t numStale = 0;  // Hits on entries from previous generations
    uint64_t numMisses = 0;

    uint8_t generation = 0;

    ~TranspositionTable() {
        if (debug && numInserted) printStats();
    }

    Eval find(Hash hash) {
        ++numHits;
        auto& entry = entries[hash() % kNumEntries];
        if (entry.hash() == hash()) return entry.move;
        --numHits;
        ++numMisses;
        return {Move(), drawEval};  // no move found
    }

    Entry* lookup(Hash hash, int depth) {
        if constexpr (kNumEntries == 0) return nullptr;

        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numHits;
        if (entry.hash == hash && entry.generation == generation && entry.depth >= depth) {
            return &entry;
        }
        --numHits;
        ++numMisses;
        return nullptr;
    }

    void insert(Hash hash, Eval move, uint8_t depthleft, EntryType type) {
        auto idx = hash() % kNumEntries;
        auto& entry = entries[idx];
        ++numInserted;

        if (entry.generation != generation && entry.move) {
            --numObsoleted;
            entry.move = {};
        }

        if (entry.type == EXACT && entry.depth > depthleft && entry.move &&
            (type != EXACT || entry.move.evaluation >= move.evaluation))
            return;

        // 3 cases: improve existing entry, collision with unrelated entry, or occupy an empty slot
        if (entry.move && entry.hash == hash)
            ++numImproved;
        else if (entry.move)
            ++numCollisions;
        else
            ++numOccupied;

        entry.hash = hash;
        entry.move = move;
        entry.generation = generation;
        entry.depth = depthleft;
        entry.type = type;
    }

    void clear() {
        if (debug && numInserted) {
            printStats();
            std::cout << "Cleared transposition table\n";
        }
        *this = {};
    };

    void newGeneration() {
        ++generation;
        numObsoleted += numOccupied;
        numOccupied = 0;
    }

    void printStats() {
        std::cout << "Transposition table stats:\n";
        std::cout << "  Inserted: " << numInserted << "\n";
        std::cout << "  Collisions: " << numCollisions << "\n";
        std::cout << "  Occupied: " << numOccupied << "\n";
        std::cout << "  Obsoleted: " << numObsoleted << "\n";
        std::cout << "  Improved: " << numImproved << "\n";
        std::cout << "  Hits: " << numHits << "\n";
        std::cout << "  Stale: " << numStale << "\n";
        std::cout << "  Misses: " << numMisses << "\n";
    }

} transpositionTable;

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
 * The transitiion uses multiple steps to avoid sudden changes in the evaluation function.
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

struct EvalTable {
    std::array<PieceSquareTable, kNumPieces> pieceSquareTables{};
    EvalTable(GamePhase phase, bool usePieceSquareTables) {
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
    const PieceSquareTable& operator[](Piece piece) const {
        return pieceSquareTables[index(piece)];
    }
};

EvalTable evalTable(GamePhase::kOpening, false);
uint64_t evalCount = 0;
uint64_t cacheCount = 0;

Score evaluateBoard(const Board& board, const EvalTable& table) {
    Score value = 0_cp;
    auto occupied = SquareSet::occupancy(board);

    for (auto square : occupied) value += table[board[square]][square.index()];

    return value;
}

Score evaluateBoard(const Board& board, bool usePieceSquareTables) {
    auto phase = GamePhase(board);
    D << "phase: " << std::to_string((int)phase.phase) << std::endl;
    auto table = EvalTable(phase, usePieceSquareTables);
    auto occupancy = SquareSet::occupancy(board);
    Score value = 0_cp;
    for (auto sq : occupancy) {
        auto piece = board[sq];
        auto score = table[piece][sq.index()];
        D << to_char(piece) << " at " << std::string(sq) << ": " << score << "\n";
        value += score;
    }
    return value;
}

using MoveIt = MoveVector::iterator;

/**
 * Sorts the moves (including captures) in the range [begin, end) in place, so that the move or
 * capture from the transposition table, if any, comes first. Returns an iterator to the next move.
 */
MoveIt sortTransposition(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    // See if the current position is a transposition of a previously seen one
    auto cachedMove = transpositionTable.find(hash);
    if (!cachedMove) return begin;
    // If the move is not part of the current legal moves, nothing to do here.
    auto it = std::find(begin, end, cachedMove.move);
    if (it == end) return begin;

    // If the transposition is not at the beginning, shift preceeding moves to the right
    if (it != begin) {
        std::swap(*begin, *it);
    }

    // The transposition is now in the beginning, so return an iterator to the next move
    return std::next(begin);
}

/**
 * Sorts the moves in the range [begin, end) in place, so that the capture come before
 * non-captures, and captures are sorted by victim value, attacker value, and move kind.
 */
MoveIt sortCaptures(const Position& position, MoveIt begin, MoveIt end) {
    std::sort(begin, end, [&board = position.board](Move a, Move b) {
        if (!isCapture(a.kind())) return false;

        // Most valuable victims first
        auto ato = board[a.to()];
        auto bto = board[b.to()];
        if (bto < ato) return true;
        if (ato < bto) return false;

        // Least valuable attackers next
        auto afrom = board[a.from()];
        auto bfrom = board[b.from()];
        if (afrom < bfrom) return true;
        if (bfrom < afrom) return false;

        // Otherwise, sort by move kind, highest first
        return b.kind() < a.kind();
    });

    return begin;
}

void sortMoves(const Position& position, Hash hash, MoveIt begin, MoveIt end) {
    begin = sortTransposition(position, hash, begin, end);
    begin = sortCaptures(position, begin, end);
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

Eval staticEval(Position& position) {
    Eval best;  // Default to the worst possible move
    auto currentEval = evaluateBoard(position.board, evalTable);
    forAllLegalMovesAndCaptures(
        position.turn, position.board, [&](Board& board, MoveWithPieces mwp) {
            auto [move, piece, captured] = mwp;
            ++evalCount;
            auto newEval = currentEval;
            if (isCapture(move.kind())) newEval -= pieceValues[index(captured)];
            if (position.activeColor() == Color::BLACK) newEval = -newEval;
            newEval += moveValues[index(move.kind())];
            Eval ourMove{move, newEval};
            if (ourMove > best) best = ourMove;
        });
    if (!best.move) {
        auto moves = allLegalMovesAndCaptures(position.turn, position.board);
        Move first = moves.size() ? moves.front() : Move();
        assert(moves.size() == 0);
        if (!isInCheck(position)) best.evaluation = drawEval;
    }
    return best;
}

Score quiesce(Position& position, Score alpha, Score beta, int depthleft) {
    Score stand_pat = evaluateBoard(position.board, evalTable);
    ++evalCount;
    if (depthleft == 0) return stand_pat;
    if (position.activeColor() == Color::BLACK) stand_pat = -stand_pat;
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    auto moveList = allLegalQuiescentMoves(position.turn, position.board, depthleft);
    Hash hash(position);
    sortMoves(position, hash, moveList.begin(), moveList.end());

    Eval best;
    for (auto move : moveList) {
        auto newPosition = applyMove(position, move);
        auto score = -quiesce(newPosition, -beta, -alpha, depthleft - 1).adjustDepth();

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

/**
 * The alpha-beta search algorithm with fail-soft negamax search.
 */
Score alphaBeta(Position& position, Score alpha, Score beta, int depthleft) {
    if (depthleft == 0) return quiesce(position, alpha, beta, Options::quiescenceDepth);

    Hash hash(position);

    // Check the transposition table
    if (auto ttEntry = transpositionTable.lookup(hash, depthleft)) {
        auto move = ttEntry->move;
        ++cacheCount;
        switch (ttEntry->type) {
        case TranspositionTable::EXACT: alpha = beta = move.evaluation; break;
        case TranspositionTable::LOWERBOUND: alpha = std::max(alpha, move.evaluation); break;
        case TranspositionTable::UPPERBOUND: beta = std::min(beta, move.evaluation); break;
        }
        if (alpha >= beta) {
            return move.evaluation;
        }
    }

    auto moveList = allLegalMovesAndCaptures(position.turn, position.board);

    //  Forced moves don't count towards depth, but avoid infinite recursion
    if (moveList.size() == 1 && position.turn.halfmoveClock < 50) ++depthleft;

    sortMoves(position, hash, moveList.begin(), moveList.end());

    Eval best;
    TranspositionTable::EntryType type = TranspositionTable::EXACT;
    for (auto move : moveList) {
        auto newPosition = applyMove(position, move);
        auto score =
            -alphaBeta(newPosition, -beta, -std::max(alpha, best.evaluation), depthleft - 1)
                 .adjustDepth();
        if (score > best.evaluation) {
            best = {move, score};
        }
        if (best.evaluation >= beta) {
            type = TranspositionTable::LOWERBOUND;
            break;
        }
    }

    if (moveList.empty()) {
        auto kingPos =
            SquareSet::find(position.board, addColor(PieceType::KING, position.activeColor()));
        if (!isAttacked(position.board, kingPos, Occupancy(position.board, position.activeColor())))
            return drawEval;
    }

    if (best.evaluation <= alpha) {
        type = TranspositionTable::UPPERBOUND;
    }

    transpositionTable.insert(
        hash, best, depthleft, type);  // Cache the best move for this position
    return best.evaluation;
}
Eval computeBestMove(Position& position, int maxdepth) {
    evalTable = {GamePhase(position.board), true};
    transpositionTable.clear();

    Eval best;  // Default to the worst possible move

    best.evaluation = quiesce(position, worstEval, bestEval, 2);
    // std::cerr << "staticEval is " << best << "\n";

    for (auto depth = 1; depth <= maxdepth; ++depth) {
        transpositionTable.newGeneration();
        auto score = alphaBeta(position, worstEval, bestEval, depth);
        best = transpositionTable.find(Hash(position));
        assert(best.evaluation == score);
        // auto pv = principalVariation(position, depth);
        // std::cerr << "Depth " << depth << ": " << best << " pv " << pv << "\n";

        if (best.evaluation.mate()) break;
    }

    return best;
}

MoveVector principalVariation(Position position, int depth) {
    MoveVector pv;
    std::unordered_multiset<Hash> seen;
    Hash hash = Hash(position);
    while (auto found = transpositionTable.find(hash)) {
        pv.push_back(found.move);
        seen.insert(hash);
        // Limit to 3 repetitions, as that's a draw. This prevents unbounded recursion.
        if (seen.count(hash) == 3) break;
        position = applyMove(position, found.move);
        hash = Hash(position);
    }
    if (pv.size() < depth && !isMate(position)) {
        std::cout << "Hash " << hash() << " (FEN " << fen::to_string(position)
                  << ") not found in transposition table ["
                  << (hash() % TranspositionTable::kNumEntries) << "]\n";
    }
    return pv;
}

uint64_t perft(Turn turn, Board& board, int depth) {
    if (depth <= 0) return 1;
    uint64_t nodes = 0;
    forAllLegalMovesAndCaptures(turn, board, [&](Board& board, MoveWithPieces mwp) {
        auto newTurn = applyMove(turn, mwp.piece, mwp.move);
        nodes += perft(newTurn, board, depth - 1);
    });
    return nodes;
}
