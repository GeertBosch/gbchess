#pragma once
#include <functional>

#include "core/core.h"
#include "core/square_set/occupancy.h"

namespace moves {
struct SearchState {
    SearchState(const Board& board, Turn turn);
    Color active() const { return turn.activeColor(); }

    Occupancy occupancy;
    SquareSet pawns;
    Turn turn;
    Square kingSquare;
    bool inCheck;
    SquareSet pinned;
};

/**
 * Computes all legal moves from a given chess position. This function checks for moves
 * that do not leave or place the king of the active color in check, including the special
 * case of castling.
 */
MoveVector allLegalMoves(Turn turn, Board board);
MoveVector allLegalCaptures(Turn turn, Board board);
MoveVector allLegalMovesAndCaptures(Turn turn, Board& board);

/** Flags for quiescent move generation extensions */
enum class QuiescentFlags : uint8_t {
    None = 0,
    IncludeChecks = 1,        // Include moves that give check
    IncludeAllMoves = 2,      // Include all quiet moves (for non-quiet positions)
    IncludePromotions = 4,    // Include pawn moves that may promote
};
constexpr QuiescentFlags operator|(QuiescentFlags a, QuiescentFlags b) {
    return static_cast<QuiescentFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr bool operator&(QuiescentFlags a, QuiescentFlags b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, QuiescentFlags flags);

size_t countLegalMovesAndCaptures(Board& board, SearchState& state);

using MoveFun = std::function<void(Board&, MoveWithPieces)>;
void forAllLegalQuiescentMoves(Turn turn, Board& board, QuiescentFlags flags, MoveFun action);
void forAllLegalMovesAndCaptures(Board& board, SearchState& state, MoveFun action);

inline void forAllLegalMovesAndCaptures(Turn turn, Board& board, MoveFun action) {
    auto state = SearchState(board, turn);
    forAllLegalMovesAndCaptures(board, state, action);
}

/**
 * Returns true if the given move does not leave the king in check.
 */
bool doesNotCheck(Board& board, const SearchState& state, Move move);
}  // namespace moves