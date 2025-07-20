#include <cassert>
#include <cstring>

#include "castling_info.h"
#include "common.h"
#include "magic.h"
#include "moves_table.h"
#include "options.h"
#include "piece_set.h"

#include "moves.h"

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::w), CastlingInfo(Color::b)};

struct PinData {
    SquareSet captures;
    PieceSet pinningPieces;
};
}  // namespace

SquareSet pinnedPieces(const Board& board,
                       Occupancy occupancy,
                       Square kingSquare,
                       const PinData& pinData) {
    SquareSet pinned;
    for (auto pinner : pinData.captures & occupancy.theirs()) {
        // Check if the pin is a valid sliding piece
        if (!pinData.pinningPieces.contains(board[pinner])) continue;

        auto pieces = occupancy() & path(kingSquare, pinner);
        if (pieces.size() == 1) pinned.insert(pieces);
    }
    return pinned & occupancy.ours();
}

SquareSet pinnedPieces(const Board& board, Occupancy occupancy, Square kingSquare) {
    // For the purpose of pinning, we only consider sliding pieces, not knights.

    // Define pinning piece sets and corresponding capture sets
    PinData pinData[] = {
        {possibleCaptures(Piece::R, kingSquare), {PieceType::ROOK, PieceType::QUEEN}},
        {possibleCaptures(Piece::B, kingSquare), {PieceType::BISHOP, PieceType::QUEEN}}};

    auto pinned = SquareSet();

    for (const auto& data : pinData) pinned |= pinnedPieces(board, occupancy, kingSquare, data);

    return pinned;
}

SearchState::SearchState(const Board& board, Turn turn)
    : occupancy(Occupancy(board, turn.activeColor())),
      pawns(find(board, addColor(PieceType::PAWN, turn.activeColor()))),
      turn(turn),
      kingSquare(*find(board, addColor(PieceType::KING, turn.activeColor())).begin()),
      inCheck(isAttacked(board, kingSquare, occupancy)),
      pinned(pinnedPieces(board, occupancy, kingSquare)) {}

bool clearPath(SquareSet occupancy, Square from, Square to) {
    return (occupancy & path(from, to)).empty();
}

void addMove(MoveVector& moves, Move move) {
    moves.emplace_back(move);
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
                auto possibleSquares = possibleMoves(piece, from) - state.occupancy();
                for (auto to : possibleSquares) fun(piece, Move{from, to, MoveKind::Quiet_Move});
            }
        } else {
            auto possibleSquares = possibleMoves(piece, from) - state.occupancy();
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
        auto path = castlingClear(color, MoveKind::O_O);
        if ((occupancy() & path).empty())
            fun(info.king, {info.kingSide[0].from, info.kingSide[0].to, MoveKind::O_O});
    }
    // Check for queen side castling
    if ((turn.castling() & info.queenSideMask) != CastlingMask::_) {
        auto path = castlingClear(color, MoveKind::O_O_O);
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
                possibleCaptures(piece, from) & state.occupancy.theirs();
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

    for (auto from : enPassantFrom(active, enPassantTarget))
        if (board[from] == pawn) fun(pawn, {from, enPassantTarget, MoveKind::En_Passant});
}

BoardChange prepareMove(Board& board, Move move) {
    // Lookup the compound move for the given move kind and target square. This breaks moves like
    // castling, en passant and promotion into a simple capture/move and a second move that can be a
    // no-op move, a quiet move or a promotion. The target square is taken from the compound move.
    auto compound = compoundMove(move);
    auto captured = board[compound.to];
    BoardChange undo = {captured, compound.promo, {move.from, compound.to}, compound.second};
    return undo;
}

BoardChange makeMove(Board& board, BoardChange change) {
    Piece first = Piece::_;
    std::swap(first, board[change.first.from]);
    board[change.first.to] = first;

    // The following statements don't change the board for non-compound moves
    auto second = Piece::_;
    std::swap(second, board[change.second.from]);
    second = Piece(index(second) + change.promo);
    board[change.second.to] = second;
    return change;
}

BoardChange makeMove(Board& board, Move move) {
    auto change = prepareMove(board, move);
    return makeMove(board, change);
}

UndoPosition makeMove(Position& position, BoardChange change, Move move) {
    auto ours = position.board[change.first.from];
    auto undo = UndoPosition{makeMove(position.board, change), position.turn};
    MoveWithPieces mwp = {move, ours, undo.board.captured};
    position.turn = applyMove(position.turn, mwp);
    return undo;
}

UndoPosition makeMove(Position& position, Move move) {
    return makeMove(position, prepareMove(position.board, move), move);
}

void unmakeMove(Board& board, BoardChange undo) {
    Piece ours = Piece::_;
    std::swap(board[undo.second.to], ours);
    ours = Piece(index(ours) - undo.promo);
    board[undo.second.from] = ours;
    auto piece = undo.captured;
    std::swap(piece, board[undo.first.to]);
    board[undo.first.from] = piece;
}

void unmakeMove(Position& position, UndoPosition undo) {
    unmakeMove(position.board, undo.board);
    position.turn = undo.turn;
}

CastlingMask castlingMask(Square from, Square to) {
    const struct MaskTable {
        using CM = CastlingMask;
        std::array<CM, kNumSquares> mask;
        constexpr MaskTable() : mask{} {
            mask[castlingInfo[0].kingSide[0].from] = CM::KQ;  // White King
            mask[castlingInfo[0].kingSide[1].from] = CM::K;   // White King Side Rook
            mask[castlingInfo[0].queenSide[1].from] = CM::Q;  // White Queen Side Rook
            mask[castlingInfo[1].kingSide[0].from] = CM::kq;  // Black King
            mask[castlingInfo[1].kingSide[1].from] = CM::k;   // Black King Side Rook
            mask[castlingInfo[1].queenSide[1].from] = CM::q;  // Black Queen Side Rook
        }
        CM operator[](Square sq) const { return mask[sq]; }
    } maskTable;

    return maskTable[from] | maskTable[to];
}

Turn applyMove(Turn turn, MoveWithPieces mwp) {
    // Update enPassantTarget
    // Set the en passant target if a pawn moves two squares forward, otherwise reset it.
    turn.setEnPassant(noEnPassantTarget);
    auto move = mwp.move;
    if (move.kind == MoveKind::Double_Push)
        turn.setEnPassant(makeSquare(file(move.from), (rank(move.from) + rank(move.to)) / 2));

    // Update castlingAvailability
    turn.setCastling(turn.castling() & ~castlingMask(move.from, move.to));

    // Update halfmoveClock and halfmoveNumber, and switch the active side.
    turn.tick();
    // Reset halfmove clock on pawn advance or capture
    if (type(mwp.piece) == PieceType::PAWN || isCapture(move.kind)) turn.resetHalfmove();

    return turn;
}

Position applyMove(Position position, Move move) {
    // Remember the piece being moved, before applying the move to the board
    auto piece = position.board[move.from];

    // Apply the move to the board
    auto undo = makeMove(position.board, move);
    MoveWithPieces mwp = {move, piece, undo.captured};
    position.turn = applyMove(position.turn, mwp);

    return position;
}

bool isAttacked(const Board& board, Square square, Occupancy occupancy) {
    // We're using this function to find out if empty squares are attacked for determining
    // legality of castling, so we can't assume that the capture square is occupied.
    for (auto from : occupancy.theirs() & attackers(square))
        if (clearPath(occupancy(), from, square) &&
            possibleCaptures(board[from], from).contains(square))
            return true;

    return false;
}

bool isAttacked(const Board& board, SquareSet squares, Occupancy occupancy) {
    for (auto square : squares)
        if (isAttacked(board, square, occupancy)) return true;

    return false;
}

bool isAttacked(const Board& board, SquareSet squares, Color opponentColor) {
    auto occupancy = Occupancy(board, opponentColor);
    return isAttacked(board, squares, occupancy);
}

bool attacks(const Board& board, Square from, Square to) {
    return possibleCaptures(board[from], from).contains(to);
}

SquareSet attackers(const Board& board, Square target, SquareSet occupancy) {
    SquareSet result;

    auto knightAttackers = possibleCaptures(Piece::N, target) & occupancy;
    for (auto from : knightAttackers)
        if (type(board[from]) == PieceType::KNIGHT) result |= from;

    auto rookAttackers = targets(target, false, occupancy);
    auto rookPieces = PieceSet{PieceType::ROOK, PieceType::QUEEN};
    for (auto from : rookAttackers)
        if (rookPieces.contains(board[from])) result |= from;

    auto bishopAttackers = targets(target, true, occupancy);
    auto bishopPieces = PieceSet{PieceType::BISHOP, PieceType::QUEEN};
    for (auto from : bishopAttackers)
        if (bishopPieces.contains(board[from])) result |= from;

    // See if there are pawns or kings that can attack the square.
    auto otherAttackers = (occupancy - result) & possibleCaptures(Piece::K, target);
    for (auto from : otherAttackers)
        if (attacks(board, from, target)) result |= from;
    return result;
}

Move checkMove(Position position, Move move) {
    auto moves = allLegalMovesAndCaptures(position.turn, position.board);

    // Try to find a legal move or capture corresponding to the given move string.
    for (auto m : moves)
        if (m == move) return m;

    throw MoveError("Invalid move: " + to_string(move));
}
 SquareSet ipath(Square from, Square to) {
    return path(from, to) | SquareSet(from) | SquareSet(to);
}

bool doesNotCheck(Board& board, const SearchState& state, Move move) {
    auto [from, to, kind] = move;

    SquareSet checkSquares = state.kingSquare;
    if (from == state.kingSquare)
        checkSquares = isCastles(kind) ? ipath(from, to) : to;
    else if (!state.pinned.contains(from) && !state.inCheck && kind != MoveKind::En_Passant)
        return true;
    auto delta = occupancyDelta(move);
    auto check = isAttacked(board, checkSquares, state.occupancy ^ delta);
    return !check;
}

bool mayHavePromoMove(Color side, Board& board, Occupancy occupancy) {
    Piece pawn;
    SquareSet from;
    if (side == Color::w) {
        from = SquareSet(0x00ff'0000'0000'0000ull) & occupancy.ours();
        auto to = SquareSet(0xff00'0000'0000'0000ull) - occupancy();
        from &= to >> 8;
        pawn = Piece::P;
    } else {
        from = SquareSet(0x0000'0000'0000'ff00ull) & occupancy.ours();
        auto to = SquareSet(0x0000'0000'0000'00ffull) - occupancy();
        from &= to << 8;
        pawn = Piece::p;
    }
    for (auto square : from)
        if (board[square] == pawn) return true;
    return false;
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

MoveVector allLegalMovesAndCaptures(Turn turn, Board& board) {
    MoveVector legalMoves;
    forAllLegalMovesAndCaptures(
        turn, board, [&](Board&, MoveWithPieces move) { legalMoves.emplace_back(move.move); });
    return legalMoves;
}
