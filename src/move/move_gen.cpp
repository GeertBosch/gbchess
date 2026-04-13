#include "move_gen.h"

#include "core/castling_info.h"
#include "core/options.h"
#include "core/piece_set.h"
#include "magic/magic.h"
#include "move.h"
#include "move_table.h"

using namespace moves;

namespace {

static constexpr PieceSet sliders = {Piece::B, Piece::b, Piece::R, Piece::r, Piece::Q, Piece::q};

SquareSet ipath(Square from, Square to) {
    return MovesTable::path(from, to) | SquareSet(from) | SquareSet(to);
}

template <typename F>
void expandPromos(const F& fun, Piece piece, Move move) {
    for (int i = 0; i < 4; ++i)
        fun(piece, Move{move.from, move.to, MoveKind(index(move.kind) - i)});
}

template <Color active, typename F>
void findPawnPushes(const SearchState& state, const F& fun) {
    constexpr bool white = active == Color::w;
    constexpr auto doublePushRank = white ? SquareSet::rank(3) : SquareSet::rank(kNumRanks - 1 - 3);
    constexpr auto promo = SquareSet::rank(white ? kNumRanks - 1 : 0);
    constexpr auto piece = white ? Piece::P : Piece::p;
    auto free = ~state.occupancy();
    auto singles = (white ? state.pawns << kNumRanks : state.pawns >> kNumRanks) & free;
    auto doubles = (white ? singles << kNumRanks : singles >> kNumRanks) & free & doublePushRank;
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
void findPawnPushes(const SearchState& state, const F& fun) {
    return state.active() == Color::w ? findPawnPushes<Color::w>(state, fun)
                                      : findPawnPushes<Color::b>(state, fun);
}

template <Color active, typename F>
void findPawnCaptures(const SearchState& state, const F& fun) {
    constexpr bool white = active == Color::w;
    constexpr auto promo = SquareSet::rank(white ? kNumRanks - 1 : 0);
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
void findPawnCaptures(const SearchState& state, const F& fun) {
    return state.active() == Color::w ? findPawnCaptures<Color::w>(state, fun)
                                      : findPawnCaptures<Color::b>(state, fun);
}

template <Color active, typename F>
void findPawnPushesAndCaptures(const SearchState& state, const F& fun) {
    findPawnPushes<active>(state, fun);
    findPawnCaptures<active>(state, fun);
}

template <typename F>
void findNonPawnMoves(const Board& board, const SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];

        if (!state.pinned.contains(from) && !state.inCheck) {
            if (sliders.contains(piece)) {
                // If the piece is a sliding piece, and not pinned, we can move it freely to all
                // target squares for its piece type that are not occupied by our own pieces.
                SquareSet toSquares;
                if (PieceSet{PieceType::BISHOP, PieceType::QUEEN}.contains(piece))
                    toSquares |= bishopTargets(from, state.occupancy());
                if (PieceSet{PieceType::ROOK, PieceType::QUEEN}.contains(piece))
                    toSquares |= rookTargets(from, state.occupancy());
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
void findMoves(const Board& board, const SearchState& state, const F& fun) {
    findPawnPushes(state, fun);
    findNonPawnMoves(board, state, fun);
}

template <typename F>
void findCastles(const SearchState& state, const F& fun) {
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
void findNonPawnCaptures(const Board& board, const SearchState& state, const F& fun) {
    for (auto from : state.occupancy.ours() - state.pawns) {
        auto piece = board[from];
        if (sliders.contains(piece) && !state.pinned.contains(from) && !state.inCheck) {
            // If the piece is a sliding piece, and not pinned, we can move it freely to all target
            // squares for its piece type that are not occupied by our own pieces.
            SquareSet toSquares;
            if (PieceSet{PieceType::BISHOP, PieceType::QUEEN}.contains(piece))
                toSquares |= bishopTargets(from, state.occupancy());
            if (PieceSet{PieceType::ROOK, PieceType::QUEEN}.contains(piece))
                toSquares |= rookTargets(from, state.occupancy());
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
void findNonPawnMovesAndCaptures(const Board& board, const SearchState& state, const F& fun) {
    findNonPawnCaptures(board, state, fun);
    findNonPawnMoves(board, state, fun);
    findCastles(state, fun);
}

template <typename F>
void findCaptures(const Board& board, const SearchState& state, const F& fun) {
    findPawnCaptures(state, fun);
    findNonPawnCaptures(board, state, fun);
}

template <Color active, typename F>
void findEnPassant(const Board& board, Turn turn, const F& fun) {
    auto enPassantTarget = turn.enPassant();
    if (enPassantTarget == noEnPassantTarget) return;

    // For a given en passant target, there are two potential from squares. If either or
    // both have a pawn of the active color, then capture is possible.
    constexpr auto pawn = addColor(PieceType::PAWN, active);

    for (auto from : MovesTable::enPassantFrom(active, enPassantTarget))
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::En_Passant});
}

template <typename F>
void findEnPassant(const Board& board, Turn turn, const F& fun) {
    return turn.activeColor() == Color::w ? findEnPassant<Color::w>(board, turn, fun)
                                          : findEnPassant<Color::b>(board, turn, fun);
}

template <typename F>
void findChecks(const Board& board, SearchState& state, const F& fun) {
    auto otherKing = state.turn.activeColor() == Color::w ? Piece::k : Piece::K;
    auto otherKingSquare =
        *find(board, otherKing).begin();  // There is always exactly one king of each color
    auto doIfCheck = [&](Piece piece, Move move) {
        if (attacks(piece, move.to, otherKingSquare)) fun(piece, move);
    };
    findPawnPushes(state, doIfCheck);
    findNonPawnMoves(board, state, doIfCheck);
    // TODO: Consider castling that gives check
}

/** For use in quiescent search: allow pawn moves that promote or near promotion */
template <Color active, typename F>
void findPromotionMoves(SearchState& state, const F& fun) {
    constexpr bool white = active == Color::w;
    constexpr auto masked = white ? SquareSet::rank(kNumRanks - 2) | SquareSet::rank(kNumRanks - 3)
                                  : SquareSet::rank(1) | SquareSet::rank(2);
    auto pawns = state.pawns;
    state.pawns &= masked;
    findPawnPushes<active>(state, fun);
    state.pawns = pawns;
}

template <typename F>
void findPromotionMoves(SearchState& state, const F& fun) {
    return state.active() == Color::w ? findPromotionMoves<Color::w>(state, fun)
                                      : findPromotionMoves<Color::b>(state, fun);
}

/** For use in perft: no need to actually generate moves at depth 1, just count them */
template <Color active>
size_t countPawnMovesAndCaptures(const Board& board, const SearchState& state) {
    size_t count = 0;

    // For a given en passant target, there are two potential from squares. If either or
    // both have a pawn of the active color, then capture is possible.
    if (auto enPassantTarget = state.turn.enPassant(); enPassantTarget != noEnPassantTarget)
        for (auto from : MovesTable::enPassantFrom(active, enPassantTarget))
            if (auto pawn = addColor(PieceType::PAWN, active); board[from] == pawn)
                count += doesNotCheck(board, state, {from, enPassantTarget, MoveKind::En_Passant});

    // Need to check for legality if we're in check or have pinned pieces, can't just count.
    if (state.inCheck || state.pinned)
        return findPawnPushesAndCaptures<active>(
                   state, [&](Piece, Move move) { count += doesNotCheck(board, state, move); }),
               count;

    constexpr bool white = active == Color::w;
    constexpr auto promo = SquareSet::rank(white ? kNumRanks - 1 : 0);
    constexpr auto doublePushRank = white ? SquareSet::rank(3) : SquareSet::rank(kNumRanks - 1 - 3);

    auto free = ~state.occupancy();
    auto singles = (white ? state.pawns << kNumRanks : state.pawns >> kNumRanks) & free;
    auto doubles = (white ? singles << kNumRanks : singles >> kNumRanks) & free & doublePushRank;
    count += singles.size() + 3 * (singles & promo).size() + doubles.size();

    auto leftPawns = state.pawns - SquareSet::file(0);
    auto rightPawns = state.pawns - SquareSet::file(7);
    auto left = (white ? leftPawns << 7 : leftPawns >> 9) & state.occupancy.theirs();
    auto right = (white ? rightPawns << 9 : rightPawns >> 7) & state.occupancy.theirs();
    count += left.size() + 3 * (left & promo).size();
    count += right.size() + 3 * (right & promo).size();

    return count;
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

bool castlingDoesNotCheck(const Board& board, const SearchState& state, Move move) {
    auto [from, to, kind] = move;
    dassert(isCastles(kind));
    auto checkSquares = ipath(from, to);
    auto delta = MovesTable::occupancyDelta(move);
    auto check = isAttacked(board, checkSquares, state.occupancy ^ delta);
    return !check;
}

bool doesNotCheck(const Board& board, const SearchState& state, Move move) {
    auto [from, to, kind] = move;

    if (isCastles(kind)) return castlingDoesNotCheck(board, state, move);

    if (!state.pinned.contains(from) && !state.inCheck && from != state.kingSquare &&
        kind != MoveKind::En_Passant)
        return true;  // En passant can result in check without moving the king or a pinned piece

    auto delta = MovesTable::occupancyDelta(move);
    auto squareToCheck = from == state.kingSquare ? to : state.kingSquare;
    auto check = isAttacked(board, squareToCheck, state.occupancy ^ delta);
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

    // In endgames, allow finding checks in the first plies of quiescence search
    bool endGame = state.occupancy.size() <= options::checksMaxPiecesLeft;

    // Allow limited quiescence search in endgames to find checks. Don't double up in case of
    // being in check or promotion threats.
    // TODO: Move this check search, as well as the promotion search, to regular search where we
    // should extend the search depth if we have a forcing check or when our opponent can promote.
    // This should not be in quiescence search.
    if (depthleft >= options::checksMinDepthLeft && !otherMayPromote && !state.inCheck && endGame)
        findChecks(board, state, doMove);

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

void forAllLegalMovesAndCaptures(Board& board, const SearchState& state, MoveFun action) {
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

size_t countLegalMovesAndCaptures(const Board& board, const SearchState& state) {
    size_t count = 0;
    auto king = state.active() == Color::w ? Piece::K : Piece::k;

    count += state.active() == Color::w ? countPawnMovesAndCaptures<Color::w>(board, state)
                                        : countPawnMovesAndCaptures<Color::b>(board, state);

    auto completeCount = [&](Piece, Move move) { count += doesNotCheck(board, state, move); };
    auto optimizedCount = [&](Piece piece, Move move) {
        count += piece == king ? doesNotCheck(board, state, move) : 1;
    };
    return state.inCheck || state.pinned
        ? (findNonPawnMovesAndCaptures(board, state, completeCount), count)
        : (findNonPawnMovesAndCaptures(board, state, optimizedCount), count);
}

size_t countLegalMovesAndCaptures(const Position& position) {
    return countLegalMovesAndCaptures(position.board, {position.board, position.turn});
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