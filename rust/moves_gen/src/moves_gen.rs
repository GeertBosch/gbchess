use fen::{Board, Color, Piece, PieceType, Square, Turn};
use magic::targets;
use moves::{make_move, Move, MoveKind, MoveWithPieces, Occupancy};
use moves_table::{clear_path, MovesTable};
use square_set::{find_piece, SquareSet};

pub type MoveVector = Vec<Move>;

// Removed unused constants and functions for cleaner code

/// Options constants (simplified version of options.h)
pub mod options {
    pub const PROMOTION_MIN_DEPTH_LEFT: i32 = 3; // quiescence_depth - 2
}

#[derive(Debug, Clone)]
pub struct SearchState {
    pub occupancy: Occupancy,
    pub pawns: SquareSet,
    pub turn: Turn,
    pub king_square: Square,
    pub in_check: bool,
    pub pinned: SquareSet,
}

impl SearchState {
    pub fn new(board: &Board, turn: Turn) -> Self {
        let active = turn.active_color();
        let occupancy = Occupancy::from_board(board, active);
        let pawns = find_piece(board, Piece::from_type_and_color(PieceType::Pawn, active));
        let king_square = find_piece(board, Piece::from_type_and_color(PieceType::King, active))
            .iter()
            .next()
            .expect("King not found");
        let in_check = is_attacked(board, king_square, &occupancy);
        let pinned = pinned_pieces(board, &occupancy, king_square);

        Self {
            occupancy,
            pawns,
            turn,
            king_square,
            in_check,
            pinned,
        }
    }

    pub fn active(&self) -> Color {
        self.turn.active_color()
    }
}

// Castling information (from castling_info.h)
#[derive(Debug, Clone, Copy)]
pub struct CastlingInfo {
    pub rook: Piece,
    pub king: Piece,
    pub king_side_mask: u8,
    pub queen_side_mask: u8,
    pub king_side: [(Square, Square); 2], // King and Rook moves
    pub queen_side: [(Square, Square); 2],
}

impl CastlingInfo {
    pub fn new(color: Color) -> Self {
        let rook = Piece::from_type_and_color(PieceType::Rook, color);
        let king = Piece::from_type_and_color(PieceType::King, color);
        let (king_side_mask, queen_side_mask) = match color {
            Color::White => (1, 2), // K, Q
            Color::Black => (4, 8), // k, q
        };
        let (king_side, queen_side) = match color {
            Color::White => (
                [(Square::E1, Square::G1), (Square::H1, Square::F1)],
                [(Square::E1, Square::C1), (Square::A1, Square::D1)],
            ),
            Color::Black => (
                [(Square::E8, Square::G8), (Square::H8, Square::F8)],
                [(Square::E8, Square::C8), (Square::A8, Square::D8)],
            ),
        };

        Self {
            rook,
            king,
            king_side_mask,
            queen_side_mask,
            king_side,
            queen_side,
        }
    }
}

fn get_castling_info(color: Color) -> CastlingInfo {
    CastlingInfo::new(color)
}

#[derive(Debug, Clone, Copy)]
pub struct PieceSet {
    pieces: u16,
}

impl PieceSet {
    pub const fn new() -> Self {
        Self { pieces: 0 }
    }

    pub const fn from_pieces(pieces: &[Piece]) -> Self {
        let mut result = 0u16;
        let mut i = 0;
        while i < pieces.len() {
            result |= 1 << pieces[i] as u8;
            i += 1;
        }
        Self { pieces: result }
    }

    pub fn contains(&self, piece: Piece) -> bool {
        (self.pieces & (1 << piece as u8)) != 0
    }
}

pub const SLIDERS: PieceSet =
    PieceSet::from_pieces(&[Piece::B, Piece::b, Piece::R, Piece::r, Piece::Q, Piece::q]);

// Helper functions - removed unused functions for cleaner code

fn is_attacked(board: &Board, square: Square, occupancy: &Occupancy) -> bool {
    // Simple implementation - check if any opponent piece attacks this square
    let table = MovesTable::new();
    let opponent_pieces = occupancy.theirs();

    for from in opponent_pieces.iter() {
        let piece = board[from];
        if piece == Piece::Empty {
            continue;
        }

        // Check if this piece can attack the target square
        let attacks = if SLIDERS.contains(piece) {
            let mut attack_squares = SquareSet::new();

            // For bishops and queens, check diagonal attacks
            if matches!(piece, Piece::B | Piece::b | Piece::Q | Piece::q) {
                attack_squares = attack_squares
                    | (targets(from, true, occupancy.all()) & SquareSet::from_square(square));
            }

            // For rooks and queens, check straight-line attacks
            if matches!(piece, Piece::R | Piece::r | Piece::Q | Piece::q) {
                attack_squares = attack_squares
                    | (targets(from, false, occupancy.all()) & SquareSet::from_square(square));
            }

            attack_squares
        } else {
            table.possible_captures(piece, from) & SquareSet::from_square(square)
        };

        if !attacks.is_empty() && clear_path(occupancy.all(), from, square) {
            return true;
        }
    }
    false
}

fn pinned_pieces(_board: &Board, _occupancy: &Occupancy, _king_square: Square) -> SquareSet {
    // Simplified implementation
    SquareSet::new()
}

// Path functions - removed unused for cleaner code

fn expand_promos<F>(fun: &mut F, piece: Piece, mv: Move)
where
    F: FnMut(Piece, Move),
{
    for i in 0..4 {
        let new_kind = match mv.kind as u8 - i {
            11 => MoveKind::QueenPromotion,
            10 => MoveKind::RookPromotion,
            9 => MoveKind::BishopPromotion,
            8 => MoveKind::KnightPromotion,
            15 => MoveKind::QueenPromotionCapture,
            14 => MoveKind::RookPromotionCapture,
            13 => MoveKind::BishopPromotionCapture,
            12 => MoveKind::KnightPromotionCapture,
            _ => continue,
        };
        fun(piece, Move::new(mv.from, mv.to, new_kind));
    }
}

fn find_pawn_pushes<F>(state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    let white = state.active() == Color::White;
    let double_push_rank = if white {
        SquareSet::rank(2) // 3rd rank (0-indexed)
    } else {
        SquareSet::rank(5) // 6th rank
    };
    let promo_rank = if white {
        SquareSet::rank(7) // 8th rank
    } else {
        SquareSet::rank(0) // 1st rank
    };

    let free = !state.occupancy.all();
    let singles = shift_pawns_forward(state.pawns, white) & free;
    let doubles = shift_pawns_forward(singles, white) & free & double_push_rank;

    let piece = if white { Piece::P } else { Piece::p };

    // Non-promoting single pushes
    for to in (singles & !promo_rank).iter() {
        let from = shift_square_backward(to, white);
        fun(piece, Move::new(from, to, MoveKind::QuietMove));
    }

    // Promoting single pushes
    for to in (singles & promo_rank).iter() {
        let from = shift_square_backward(to, white);
        expand_promos(fun, piece, Move::new(from, to, MoveKind::QueenPromotion));
    }

    // Double pushes
    for to in doubles.iter() {
        let from = shift_square_backward(shift_square_backward(to, white), white);
        fun(piece, Move::new(from, to, MoveKind::DoublePush));
    }
}

fn shift_pawns_forward(pawns: SquareSet, white: bool) -> SquareSet {
    if white {
        SquareSet::from_bits(pawns.bits() << 8)
    } else {
        SquareSet::from_bits(pawns.bits() >> 8)
    }
}

fn shift_square_backward(square: Square, white: bool) -> Square {
    let index = square as usize;
    let new_index = if white { index - 8 } else { index + 8 };
    Square::make_square(new_index % 8, new_index / 8)
}

fn find_pawn_captures<F>(state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    let white = state.active() == Color::White;
    let promo_rank = if white {
        SquareSet::rank(7)
    } else {
        SquareSet::rank(0)
    };

    let left_pawns = state.pawns & !SquareSet::file(0);
    let right_pawns = state.pawns & !SquareSet::file(7);

    let left = shift_pawn_capture_left(left_pawns, white) & state.occupancy.theirs();
    let right = shift_pawn_capture_right(right_pawns, white) & state.occupancy.theirs();

    let piece = if white { Piece::P } else { Piece::p };

    // Left captures (non-promoting)
    for to in (left & !promo_rank).iter() {
        let from = capture_square_origin_left(to, white);
        fun(piece, Move::new(from, to, MoveKind::Capture));
    }

    // Left captures (promoting)
    for to in (left & promo_rank).iter() {
        let from = capture_square_origin_left(to, white);
        expand_promos(
            fun,
            piece,
            Move::new(from, to, MoveKind::QueenPromotionCapture),
        );
    }

    // Right captures (non-promoting)
    for to in (right & !promo_rank).iter() {
        let from = capture_square_origin_right(to, white);
        fun(piece, Move::new(from, to, MoveKind::Capture));
    }

    // Right captures (promoting)
    for to in (right & promo_rank).iter() {
        let from = capture_square_origin_right(to, white);
        expand_promos(
            fun,
            piece,
            Move::new(from, to, MoveKind::QueenPromotionCapture),
        );
    }
}

fn shift_pawn_capture_left(pawns: SquareSet, white: bool) -> SquareSet {
    if white {
        SquareSet::from_bits((pawns.bits() & 0xfefefefefefefefe) << 7)
    } else {
        SquareSet::from_bits((pawns.bits() & 0xfefefefefefefefe) >> 9)
    }
}

fn shift_pawn_capture_right(pawns: SquareSet, white: bool) -> SquareSet {
    if white {
        SquareSet::from_bits((pawns.bits() & 0x7f7f7f7f7f7f7f7f) << 9)
    } else {
        SquareSet::from_bits((pawns.bits() & 0x7f7f7f7f7f7f7f7f) >> 7)
    }
}

fn capture_square_origin_left(to: Square, white: bool) -> Square {
    let index = to as usize;
    let from_index = if white { index - 7 } else { index + 9 };
    Square::make_square(from_index % 8, from_index / 8)
}

fn capture_square_origin_right(to: Square, white: bool) -> Square {
    let index = to as usize;
    let from_index = if white { index - 9 } else { index + 7 };
    Square::make_square(from_index % 8, from_index / 8)
}

fn find_non_pawn_moves<F>(board: &Board, state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    let table = MovesTable::new();

    for from in (state.occupancy.ours() & !state.pawns).iter() {
        let piece = board[from];

        if SLIDERS.contains(piece) && !state.pinned.contains(from) && !state.in_check {
            // Sliding piece, not pinned
            let mut to_squares = SquareSet::new();

            if matches!(piece, Piece::B | Piece::b | Piece::Q | Piece::q) {
                to_squares = to_squares | targets(from, true, state.occupancy.all());
            }
            if matches!(piece, Piece::R | Piece::r | Piece::Q | Piece::q) {
                to_squares = to_squares | targets(from, false, state.occupancy.all());
            }
            to_squares = to_squares & !state.occupancy.all();

            for to in to_squares.iter() {
                fun(piece, Move::new(from, to, MoveKind::QuietMove));
            }
        } else {
            let possible_squares = table.possible_moves(piece, from) & !state.occupancy.all();
            for to in possible_squares.iter() {
                if clear_path(state.occupancy.all(), from, to) {
                    fun(piece, Move::new(from, to, MoveKind::QuietMove));
                }
            }
        }
    }
}

fn find_moves<F>(board: &Board, state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    find_pawn_pushes(state, fun);
    find_non_pawn_moves(board, state, fun);
}

fn find_promotion_moves<F>(state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    let white = state.active() == Color::White;
    let promo_rank = if white {
        SquareSet::rank(7) // 8th rank
    } else {
        SquareSet::rank(0) // 1st rank
    };

    let free = !state.occupancy.all();
    let promoting_pawns = shift_pawns_forward(state.pawns, white) & free & promo_rank;

    let piece = if white { Piece::P } else { Piece::p };

    for to in promoting_pawns.iter() {
        let from = shift_square_backward(to, white);
        expand_promos(fun, piece, Move::new(from, to, MoveKind::QueenPromotion));
    }
}

fn find_castles<F>(state: &SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    if state.in_check {
        return; // No castling while in check
    }

    let color = state.turn.active_color();
    let info = get_castling_info(color);
    let castling = state.turn.castling();

    // King side castling
    if (castling.value() & info.king_side_mask) != 0 {
        // Check if path is clear - simplified implementation
        let king_to = info.king_side[0].1;
        let rook_to = info.king_side[1].1;
        let clear_squares = SquareSet::from_square(rook_to) | SquareSet::from_square(king_to);
        if (state.occupancy.all() & clear_squares).is_empty() {
            fun(
                info.king,
                Move::new(info.king_side[0].0, info.king_side[0].1, MoveKind::OO),
            );
        }
    }

    // Queen side castling
    if (castling.value() & info.queen_side_mask) != 0 {
        // Check if path is clear - simplified implementation
        let king_to = info.queen_side[0].1;
        let rook_to = info.queen_side[1].1;
        let clear_squares = SquareSet::from_square(rook_to)
            | SquareSet::from_square(king_to)
            | SquareSet::from_square(Square::B1);
        if (state.occupancy.all() & clear_squares).is_empty() {
            fun(
                info.king,
                Move::new(info.queen_side[0].0, info.queen_side[0].1, MoveKind::OOO),
            );
        }
    }
}

fn find_non_pawn_captures<F>(board: &Board, state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    let table = MovesTable::new();

    for from in (state.occupancy.ours() & !state.pawns).iter() {
        let piece = board[from];

        if SLIDERS.contains(piece) && !state.pinned.contains(from) && !state.in_check {
            // Sliding piece, not pinned
            let mut to_squares = SquareSet::new();

            if matches!(piece, Piece::B | Piece::b | Piece::Q | Piece::q) {
                to_squares = to_squares | targets(from, true, state.occupancy.all());
            }
            if matches!(piece, Piece::R | Piece::r | Piece::Q | Piece::q) {
                to_squares = to_squares | targets(from, false, state.occupancy.all());
            }
            to_squares = to_squares & state.occupancy.theirs();

            for to in to_squares.iter() {
                fun(piece, Move::new(from, to, MoveKind::Capture));
            }
        } else {
            let possible_squares = table.possible_captures(piece, from) & state.occupancy.theirs();
            for to in possible_squares.iter() {
                if clear_path(state.occupancy.all(), from, to) {
                    fun(piece, Move::new(from, to, MoveKind::Capture));
                }
            }
        }
    }
}

fn find_captures<F>(board: &Board, state: &mut SearchState, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    find_pawn_captures(state, fun);
    find_non_pawn_captures(board, state, fun);
}

fn find_en_passant<F>(board: &Board, turn: Turn, fun: &mut F)
where
    F: FnMut(Piece, Move),
{
    let en_passant_target = turn.en_passant();
    if en_passant_target.is_none() {
        return;
    }
    let en_passant_target = en_passant_target.unwrap();

    let active = turn.active_color();
    let pawn = Piece::from_type_and_color(PieceType::Pawn, active);

    // Check adjacent files for pawns that can capture en passant
    let target_file = en_passant_target.file();
    let pawn_rank = if active == Color::White { 4 } else { 3 };

    // Check left adjacent file
    if target_file > 0 {
        let from = Square::make_square(target_file - 1, pawn_rank);
        if board[from] == pawn {
            fun(
                pawn,
                Move::new(from, en_passant_target, MoveKind::EnPassant),
            );
        }
    }

    // Check right adjacent file
    if target_file < 7 {
        let from = Square::make_square(target_file + 1, pawn_rank);
        if board[from] == pawn {
            fun(
                pawn,
                Move::new(from, en_passant_target, MoveKind::EnPassant),
            );
        }
    }
}

/**
 * Returns true if the given move does not leave the king in check.
 */
pub fn does_not_check(board: &mut Board, state: &SearchState, mv: Move) -> bool {
    let change = make_move(board, mv);
    let new_occupancy = Occupancy::from_board(board, state.turn.active_color());

    // Determine which square(s) to check for attack
    let check_square = if mv.from == state.king_square {
        // If the king moved, check the destination square
        // TODO: Handle castling path checking (ipath) when castling is implemented
        mv.to
    } else {
        // If another piece moved, check the original king square
        state.king_square
    };

    let result = !is_attacked(board, check_square, &new_occupancy);
    moves::unmake_move_board(board, change);
    result
}

/**
 * Computes all legal moves from a given chess position.
 */
pub fn all_legal_moves(turn: Turn, board: Board) -> MoveVector {
    let moves = all_legal_moves_and_captures(turn, &board);
    moves
        .into_iter()
        .filter(|mv| !mv.kind.is_capture())
        .collect()
}

pub fn all_legal_captures(turn: Turn, board: Board) -> MoveVector {
    let moves = all_legal_moves_and_captures(turn, &board);
    moves
        .into_iter()
        .filter(|mv| mv.kind.is_capture())
        .collect()
}

pub fn all_legal_moves_and_captures(turn: Turn, board: &Board) -> MoveVector {
    let mut legal_moves = Vec::new();
    for_all_legal_moves_and_captures(
        board,
        &SearchState::new(board, turn),
        &mut |_board: &mut Board, move_with_pieces: MoveWithPieces| {
            legal_moves.push(move_with_pieces.mv);
        },
    );
    legal_moves
}

pub fn all_legal_quiescent_moves(turn: Turn, board: &mut Board, depth_left: i32) -> MoveVector {
    let mut legal_moves = Vec::new();
    for_all_legal_quiescent_moves(turn, board, depth_left, &mut |mv: Move| {
        legal_moves.push(mv);
    });
    legal_moves
}

pub fn count_legal_moves_and_captures(board: &mut Board, state: &SearchState) -> usize {
    let mut count = 0;
    let state_clone = state.clone();
    let board_clone = board.clone();
    let mut count_move = |_piece: Piece, mv: Move| {
        let mut temp_board = board_clone.clone();
        if does_not_check(&mut temp_board, &state_clone, mv) {
            count += 1;
        }
    };

    find_non_pawn_captures(&board_clone, &mut state_clone.clone(), &mut count_move);
    find_non_pawn_moves(&board_clone, &mut state_clone.clone(), &mut count_move);
    find_pawn_captures(&mut state_clone.clone(), &mut count_move);
    find_en_passant(&board_clone, state.turn, &mut count_move);
    find_pawn_pushes(&mut state_clone.clone(), &mut count_move);
    find_castles(&state_clone, &mut count_move);

    count
}

pub fn for_all_legal_quiescent_moves<F>(
    turn: Turn,
    board: &mut Board,
    depth_left: i32,
    action: &mut F,
) where
    F: FnMut(Move),
{
    let state = SearchState::new(board, turn);
    let board_clone = board.clone();
    let mut do_move = |_piece: Piece, mv: Move| {
        let change = make_move(board, mv);
        if does_not_check(board, &state, mv) {
            action(mv);
        }
        moves::unmake_move_board(board, change);
    };

    find_captures(&board_clone, &mut state.clone(), &mut do_move);

    // Check if opponent may promote
    let other_may_promote = depth_left > options::PROMOTION_MIN_DEPTH_LEFT
        && may_have_promo_move(!turn.active_color(), &board_clone, &(!state.occupancy));

    // Avoid horizon effect: don't promote in the last plies
    if depth_left >= options::PROMOTION_MIN_DEPTH_LEFT {
        find_promotion_moves(&mut state.clone(), &mut do_move);
    }

    find_en_passant(&board_clone, turn, &mut do_move);
    if state.in_check || other_may_promote {
        find_moves(&board_clone, &mut state.clone(), &mut do_move);
    }
}

pub fn for_all_legal_moves_and_captures<F>(board: &Board, state: &SearchState, action: &mut F)
where
    F: FnMut(&mut Board, MoveWithPieces),
{
    let mut board_mut = board.clone();
    let board_ref = board.clone();
    let mut do_move = |piece: Piece, mv: Move| {
        let change = make_move(&mut board_mut, mv);
        if does_not_check(&mut board_mut, state, mv) {
            action(
                &mut board_mut,
                MoveWithPieces {
                    mv,
                    piece,
                    captured: change.captured,
                },
            );
        }
        moves::unmake_move_board(&mut board_mut, change);
    };

    find_captures(&board_ref, &mut state.clone(), &mut do_move);
    find_en_passant(&board_ref, state.turn, &mut do_move);
    find_moves(&board_ref, &mut state.clone(), &mut do_move);
    find_castles(state, &mut do_move);
}

pub fn for_all_legal_moves_and_captures_simple<F>(turn: Turn, board: &Board, action: &mut F)
where
    F: FnMut(&mut Board, MoveWithPieces),
{
    let state = SearchState::new(board, turn);
    for_all_legal_moves_and_captures(board, &state, action);
}

// Helper function to check if opponent may have promotion moves
fn may_have_promo_move(color: Color, board: &Board, _occupancy: &Occupancy) -> bool {
    // Simplified implementation - check if there are pawns near promotion
    let pawn = Piece::from_type_and_color(PieceType::Pawn, color);
    let near_promo_rank = match color {
        Color::White => SquareSet::rank(6), // 7th rank
        Color::Black => SquareSet::rank(1), // 2nd rank
    };

    for square in near_promo_rank.iter() {
        if board[square] == pawn {
            return true;
        }
    }
    false
}

pub fn is_attacked_debug(board: &Board, square: Square, occupancy: &Occupancy) -> bool {
    is_attacked(board, square, occupancy)
}
