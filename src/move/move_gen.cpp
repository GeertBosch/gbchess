#include "move_gen.h"

#include "core/castling_info.h"
#include "core/options.h"
#include "core/piece_set.h"
#include "magic/magic.h"
#include "move.h"
#include "move_table.h"

using namespace moves;

namespace {
SquareSet ipath(Square from, Square to) {
    return MovesTable::path(from, to) | SquareSet(from) | SquareSet(to);
}

template <typename F>
void expandPromos(const F& fun, Piece piece, Move move) {
    for (int i = 0; i < 4; ++i)
        fun(piece, Move{move.from, move.to, MoveKind(index(move.kind) - i)});
}

template <typename F>
void findPawnPushes(SearchState& state, const F& fun) {
    bool white = state.active() == Color::w;
    const auto doublePushRank = white ? SquareSet::rank(3) : SquareSet::rank(kNumRanks - 1 - 3);
    const auto promo = white ? SquareSet::rank(kNumRanks - 1) : SquareSet::rank(0);
    auto free = ~state.occupancy();
    auto singles = (white ? state.pawns << kNumRanks : state.pawns >> kNumRanks) & free;
    auto doubles = (white ? singles << kNumRanks : singles >> kNumRanks) & free & doublePushRank;
    auto piece = white ? Piece::P : Piece::p;
    for (auto to : singles - promo)
        fun(piece,
            Move{makeSquare(file(to), white ? rank(to) - 1 : rank(to) + 1),
                 to,
                 MoveKind::Quiet_Move});
    for (auto to : singles& promo)
        expandPromos(fun,
                     piece,
                     Move{makeSquare(file(to), white ? rank(to) - 1 : rank(to) + 1),
                          to,
                          MoveKind::Queen_Promotion});
    for (auto to : doubles)
        fun(piece,
            Move{makeSquare(file(to), white ? rank(to) - 2 : rank(to) + 2),
                 to,
                 MoveKind::Double_Push});
}

template <typename F>
void findPawnCaptures(SearchState& state, const F& fun) {
    bool white = state.active() == Color::w;
    const auto promo = white ? SquareSet::rank(kNumRanks - 1) : SquareSet::rank(0);
    auto leftPawns = state.pawns - SquareSet::file(0);
    auto rightPawns = state.pawns - SquareSet::file(7);
    auto left = (white ? leftPawns << 7 : leftPawns >> 9) & state.occupancy.theirs();
    auto right = (white ? rightPawns << 9 : rightPawns >> 7) & state.occupancy.theirs();
    auto piece = white ? Piece::P : Piece::p;
    for (auto to : left - promo)
        fun(piece,
            Move{makeSquare(file(to) + 1, white ? rank(to) - 1 : rank(to) + 1),
                 to,
                 MoveKind::Capture});
    for (auto to : left& promo)
        expandPromos(fun,
                     piece,
                     Move{makeSquare(file(to) + 1, white ? rank(to) - 1 : rank(to) + 1),
                          to,
                          MoveKind::Queen_Promotion_Capture});
    for (auto to : right - promo)
        fun(piece,
            Move{makeSquare(file(to) - 1, white ? rank(to) - 1 : rank(to) + 1),
                 to,
                 MoveKind::Capture});
    for (auto to : right& promo)
        expandPromos(fun,
                     piece,
                     Move{makeSquare(file(to) - 1, white ? rank(to) - 1 : rank(to) + 1),
                          to,
                          MoveKind::Queen_Promotion_Capture});
}

template <typename F>
void findNonPawnMoves(const Board& board, SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];

        if (!state.pinned.contains(from) && !state.inCheck) {
            if (sliders.contains(piece)) {
                // If the piece is a sliding piece, and not pinned, we can move it freely to all
                // target squares for its piece type that are not occupied by our own pieces.
                SquareSet toSquares;
                if (PieceSet{PieceType::BISHOP, PieceType::QUEEN}.contains(piece))
                    toSquares |= targets(from, true, state.occupancy());
                if (PieceSet{PieceType::ROOK, PieceType::QUEEN}.contains(piece))
                    toSquares |= targets(from, false, state.occupancy());
                toSquares -= state.occupancy();

                for (auto to : toSquares) fun(piece, Move{from, to, MoveKind::Quiet_Move});
            } else {
                auto possibleSquares = MovesTable::possibleMoves(piece, from) - state.occupancy();
                for (auto to : possibleSquares) fun(piece, Move{from, to, MoveKind::Quiet_Move});
            }
        } else {
            auto possibleSquares = MovesTable::possibleMoves(piece, from) - state.occupancy();
            for (auto to : possibleSquares)  // Check for moving through pieces
                if (clearPath(state.occupancy(), from, to))
                    fun(piece, Move{from, to, MoveKind::Quiet_Move});
        }
    }
}

template <typename F>
void findMoves(const Board& board, SearchState& state, const F& fun) {
    findPawnPushes(state, fun);
    findNonPawnMoves(board, state, fun);
}

/** For use in quiescent search: allow pawn moves that promote or near promotion */
template <typename F>
void findPromotionMoves(SearchState& state, const F& fun) {
    bool white = state.active() == Color::w;
    auto masked = white ? SquareSet::rank(kNumRanks - 2) | SquareSet::rank(kNumRanks - 3)
                        : SquareSet::rank(1) | SquareSet::rank(2);
    auto pawns = state.pawns;
    state.pawns &= masked;
    findPawnPushes(state, fun);
    state.pawns = pawns;
}

template <typename F>
void findCastles(SearchState& state, const F& fun) {
    if (state.inCheck) return;  // No castling while in check
    auto occupancy = state.occupancy;
    auto turn = state.turn;
    auto color = turn.activeColor();
    auto& info = castlingInfo[index(color)];

    // Check for king side castling
    if ((turn.castling() & info.kingSideMask) != CastlingMask::_) {
        auto path = MovesTable::castlingClear(color, MoveKind::O_O);
        if ((occupancy() & path).empty())
            fun(info.king, {info.kingSide[0].from, info.kingSide[0].to, MoveKind::O_O});
    }
    // Check for queen side castling
    if ((turn.castling() & info.queenSideMask) != CastlingMask::_) {
        auto path = MovesTable::castlingClear(color, MoveKind::O_O_O);
        if ((occupancy() & path).empty())
            fun(info.king, Move{info.queenSide[0].from, info.queenSide[0].to, MoveKind::O_O_O});
    }
}

template <typename F>
void findNonPawnCaptures(const Board& board, SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];
        if (sliders.contains(piece) && !state.pinned.contains(from) && !state.inCheck) {
            // If the piece is a sliding piece, and not pinned, we can move it freely to all target
            // squares for its piece type that are not occupied by our own pieces.
            SquareSet toSquares;
            if (PieceSet{PieceType::BISHOP, PieceType::QUEEN}.contains(piece))
                toSquares |= targets(from, true, state.occupancy().bits());
            if (PieceSet{PieceType::ROOK, PieceType::QUEEN}.contains(piece))
                toSquares |= targets(from, false, state.occupancy().bits());
            toSquares &= state.occupancy.theirs();

            for (auto to : toSquares) fun(piece, Move{from, to, MoveKind::Capture});
        } else {
            auto possibleSquares =
                MovesTable::possibleCaptures(piece, from) & state.occupancy.theirs();
            for (auto to : possibleSquares) {
                // Exclude captures that move through pieces
                if (clearPath(state.occupancy(), from, to))
                    fun(piece, Move{from, to, MoveKind::Capture});
            }
        }
    }
}

template <typename F>
void findCaptures(const Board& board, SearchState& state, const F& fun) {
    findPawnCaptures(state, fun);
    findNonPawnCaptures(board, state, fun);
}

template <typename F>
void findEnPassant(const Board& board, Turn turn, const F& fun) {
    auto enPassantTarget = turn.enPassant();
    if (turn.enPassant() == noEnPassantTarget) return;

    // For a given en passant target, there are two potential from squares. If either or
    // both have a pawn of the active color, then capture is possible.
    auto active = turn.activeColor();
    auto pawn = addColor(PieceType::PAWN, active);

    for (auto from : MovesTable::enPassantFrom(active, enPassantTarget))
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::En_Passant});
}
}  // namespace

namespace moves {
SearchState::SearchState(const Board& board, Turn turn)
    : occupancy(Occupancy(board, turn.activeColor())),
      pawns(find(board, addColor(PieceType::PAWN, turn.activeColor()))),
      turn(turn),
      kingSquare(*find(board, addColor(PieceType::KING, turn.activeColor())).begin()),
      inCheck(isAttacked(board, kingSquare, occupancy)),
      pinned(pinnedPieces(board, occupancy, kingSquare)) {}


bool doesNotCheck(Board& board, const SearchState& state, Move move) {
    auto [from, to, kind] = move;

    SquareSet checkSquares = state.kingSquare;
    if (from == state.kingSquare)
        checkSquares = isCastles(kind) ? ipath(from, to) : to;
    else if (!state.pinned.contains(from) && !state.inCheck && kind != MoveKind::En_Passant)
        return true;
    auto delta = MovesTable::occupancyDelta(move);
    auto check = isAttacked(board, checkSquares, state.occupancy ^ delta);
    return !check;
}

void forAllLegalQuiescentMoves(Turn turn,
                               Board& board,
                               int depthleft,
                               std::function<void(Move)> action) {
    // Iterate over all moves and captures
    auto state = SearchState(board, turn);
    auto doMove = [&](Piece, Move move) {
        auto change = makeMove(board, move);

        if (doesNotCheck(board, state, move)) action(move);

        unmakeMove(board, change);
    };
    findCaptures(board, state, doMove);

    // Check if the opponent may promote
    bool otherMayPromote = depthleft > options::promotionMinDepthLeft &&
        mayHavePromoMove(!turn.activeColor(), board, !state.occupancy);

    // Avoid horizon effect: don't promote in the last plies
    if (depthleft >= options::promotionMinDepthLeft) findPromotionMoves(state, doMove);

    findEnPassant(board, turn, doMove);
    if (state.inCheck || otherMayPromote) findMoves(board, state, doMove);
}

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, int depthleft) {
    MoveVector legalQuiescentMoves;
    forAllLegalQuiescentMoves(
        turn, board, depthleft, [&](Move move) { legalQuiescentMoves.emplace_back(move); });
    return legalQuiescentMoves;
}

void forAllLegalMovesAndCaptures(Board& board, SearchState& state, MoveFun action) {
    // Iterate over all moves and captures
    auto doMove = [&](Piece piece, Move move) {
        auto change = makeMove(board, move);
        if (doesNotCheck(board, state, move))
            action(board, MoveWithPieces{move, piece, change.captured});
        unmakeMove(board, change);
    };
    findCaptures(board, state, doMove);
    findEnPassant(board, state.turn, doMove);
    findMoves(board, state, doMove);
    findCastles(state, doMove);
}

size_t countLegalMovesAndCaptures(Board& board, SearchState& state) {
    size_t count = 0;
    auto countMove = [&count, &board, &state](Piece, Move move) {
        count += doesNotCheck(board, state, move);
    };
    findNonPawnCaptures(board, state, countMove);
    findNonPawnMoves(board, state, countMove);

    findPawnCaptures(state, countMove);
    findEnPassant(board, state.turn, countMove);
    findPawnPushes(state, countMove);

    findCastles(state, countMove);

    return count;
}

Move checkMove(Position position, Move move) {
    auto moves = allLegalMovesAndCaptures(position.turn, position.board);

    // Try to find a legal move or capture corresponding to the given move string.
    for (auto m : moves)
        if (m == move) return m;

    throw MoveError("Invalid move: " + to_string(move));
}

MoveVector allLegalCaptures(Turn turn, Board board) {
    MoveVector captures;
    for (auto move : allLegalMovesAndCaptures(turn, board))
        if (isCapture(move.kind)) captures.emplace_back(move);
    return captures;
}

MoveVector allLegalMoves(Turn turn, Board board) {
    MoveVector moves;
    for (auto move : allLegalMovesAndCaptures(turn, board))
        if (!isCapture(move.kind)) moves.emplace_back(move);
    return moves;
}

MoveVector allLegalMovesAndCaptures(Turn turn, Board& board) {
    MoveVector legalMoves;
    forAllLegalMovesAndCaptures(
        turn, board, [&](Board&, MoveWithPieces move) { legalMoves.emplace_back(move.move); });
    return legalMoves;
}
}  // namespace moves