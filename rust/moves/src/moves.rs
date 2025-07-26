use crate::{
    BoardChange, FromTo, Move, MoveKind, MoveWithPieces, Occupancy,
    UndoPosition,
};
use fen::{Board, Color, Piece, PieceType, Square, Turn, Position, CastlingMask, NO_EN_PASSANT_TARGET};
use moves_table::MovesTable;
use square_set::SquareSet;

struct PinData {
    captures: SquareSet,
    pinning_pieces: Vec<PieceType>,
}

/// Returns the set of pieces that would result in the king         (Square::E8, CastlingMask::kq),   // Black Kingeing checked,
/// if the piece were to be removed from the board.
pub fn pinned_pieces(board: &Board, occupancy: Occupancy, king_square: Square) -> SquareSet {
    let table = MovesTable::new(); // TODO: Use a global instance for performance

    let pin_data = [
        PinData {
            captures: table.possible_captures(Piece::R, king_square),
            pinning_pieces: vec![PieceType::Rook, PieceType::Queen],
        },
        PinData {
            captures: table.possible_captures(Piece::B, king_square),
            pinning_pieces: vec![PieceType::Bishop, PieceType::Queen],
        },
    ];

    let mut pinned = SquareSet::new();

    for data in &pin_data {
        for pinner in (data.captures & occupancy.theirs()).iter() {
            // Check if the pin is a valid sliding piece
            let piece_type = board[pinner].piece_type();
            if !data.pinning_pieces.contains(&piece_type) {
                continue;
            }

            let pieces = occupancy.all() & table.path(king_square, pinner);
            if pieces.len() == 1 {
                pinned = pinned | pieces;
            }
        }
    }

    pinned & occupancy.ours()
}

/// Returns true if the given square is attacked by a piece of the given opponent color.
pub fn is_attacked_square(board: &Board, square: Square, occupancy: Occupancy) -> bool {
    let table = MovesTable::new(); // TODO: Use a global instance for performance

    for from in (occupancy.theirs() & table.attackers(square)).iter() {
        if clear_path(occupancy.all(), from, square)
            && table.possible_captures(board[from], from).contains(square)
        {
            return true;
        }
    }
    false
}

/// Returns true if any of the given squares is attacked by a piece of the opponent color.
pub fn is_attacked_squares(board: &Board, squares: SquareSet, occupancy: Occupancy) -> bool {
    for square in squares.iter() {
        if is_attacked_square(board, square, occupancy) {
            return true;
        }
    }
    false
}

/// Returns true if any of the given squares is attacked by a piece of the given opponent color.
pub fn is_attacked_by_color(board: &Board, squares: SquareSet, opponent_color: Color) -> bool {
    let occupancy = Occupancy::from_board(board, opponent_color);
    is_attacked_squares(board, squares, occupancy)
}

/// Returns whether the piece on 'from' can attack the square 'to', ignoring occupancy or en passant.
pub fn attacks(board: &Board, from: Square, to: Square) -> bool {
    let table = MovesTable::new(); // TODO: Use a global instance for performance
    table.possible_captures(board[from], from).contains(to)
}

/// Returns the set of squares that is attacking the given square, including pieces of both sides.
pub fn attackers(board: &Board, target: Square, occupancy: SquareSet) -> SquareSet {
    let table = MovesTable::new(); // TODO: Use a global instance for performance
    let mut result = SquareSet::new();

    // Knight attackers
    let knight_attackers = table.possible_captures(Piece::N, target) & occupancy;
    for from in knight_attackers.iter() {
        if board[from].piece_type() == PieceType::Knight {
            result = result | SquareSet::from_square(from);
        }
    }

    // Rook attackers (including queens)
    let rook_attackers = magic::targets(target, false, occupancy);
    for from in rook_attackers.iter() {
        let piece_type = board[from].piece_type();
        if matches!(piece_type, PieceType::Rook | PieceType::Queen) {
            result = result | SquareSet::from_square(from);
        }
    }

    // Bishop attackers (including queens)
    let bishop_attackers = magic::targets(target, true, occupancy);
    for from in bishop_attackers.iter() {
        let piece_type = board[from].piece_type();
        if matches!(piece_type, PieceType::Bishop | PieceType::Queen) {
            result = result | SquareSet::from_square(from);
        }
    }

    // Pawn and king attackers
    let other_attackers = (occupancy - result) & table.possible_captures(Piece::K, target);
    for from in other_attackers.iter() {
        if attacks(board, from, target) {
            result = result | SquareSet::from_square(from);
        }
    }

    result
}

/// Returns true if the given side has a pawn that may promote on the next move.
/// Does not check for legality of the promotion move.
pub fn may_have_promo_move(side: Color, board: &Board, occupancy: Occupancy) -> bool {
    let (from_mask, to_mask, pawn, shift) = match side {
        Color::White => (
            SquareSet::from_bits(0x00ff_0000_0000_0000u64),
            SquareSet::from_bits(0xff00_0000_0000_0000u64),
            Piece::P,
            8,
        ),
        Color::Black => (
            SquareSet::from_bits(0x0000_0000_0000_ff00u64),
            SquareSet::from_bits(0x0000_0000_0000_00ffu64),
            Piece::p,
            8i32 as u32, // Actually should be left shift for black
        ),
    };

    let from = from_mask & occupancy.ours();
    let to = to_mask - occupancy.all();
    let from = match side {
        Color::White => from & (to >> shift),
        Color::Black => from & (to << shift),
    };

    for square in from.iter() {
        if board[square] == pawn {
            return true;
        }
    }
    false
}

/// Decompose a possibly complex move (castling, promotion, en passant) into two simpler moves
/// that allow making and unmaking the change to the board without complex conditional logic.
pub fn prepare_move(board: &Board, mv: Move) -> BoardChange {
    let compound = compound_move(mv);
    let captured = board[compound.to];
    BoardChange {
        captured,
        promo: compound.promo,
        first: FromTo {
            from: mv.from,
            to: compound.to,
        },
        second: compound.second,
    }
}

/// Compound move information for complex moves
#[derive(Debug, Clone, Copy)]
struct CompoundMove {
    to: Square,
    promo: u8,
    second: FromTo,
}

/// Get the compound move information for a given move
fn compound_move(mv: Move) -> CompoundMove {
    match mv.kind {
        MoveKind::OO => {
            // King-side castling
            match mv.from {
                Square::E1 => CompoundMove {
                    to: mv.to,
                    promo: 0,
                    second: FromTo {
                        from: Square::H1,
                        to: Square::F1,
                    },
                },
                Square::E8 => CompoundMove {
                    to: mv.to,
                    promo: 0,
                    second: FromTo {
                        from: Square::H8,
                        to: Square::F8,
                    },
                },
                _ => unreachable!("Invalid king-side castling from square"),
            }
        }
        MoveKind::OOO => {
            // Queen-side castling
            match mv.from {
                Square::E1 => CompoundMove {
                    to: mv.to,
                    promo: 0,
                    second: FromTo {
                        from: Square::A1,
                        to: Square::D1,
                    },
                },
                Square::E8 => CompoundMove {
                    to: mv.to,
                    promo: 0,
                    second: FromTo {
                        from: Square::A8,
                        to: Square::D8,
                    },
                },
                _ => unreachable!("Invalid queen-side castling from square"),
            }
        }
        MoveKind::EnPassant => {
            // En passant capture
            let captured_square = if mv.to.rank() > mv.from.rank() {
                // White capturing
                Square::make_square(mv.to.file(), mv.to.rank() - 1)
            } else {
                // Black capturing
                Square::make_square(mv.to.file(), mv.to.rank() + 1)
            };
            CompoundMove {
                to: captured_square,
                promo: 0,
                second: FromTo {
                    from: captured_square,
                    to: mv.to,
                },
            }
        }
        MoveKind::KnightPromotion
        | MoveKind::BishopPromotion
        | MoveKind::RookPromotion
        | MoveKind::QueenPromotion
        | MoveKind::KnightPromotionCapture
        | MoveKind::BishopPromotionCapture
        | MoveKind::RookPromotionCapture
        | MoveKind::QueenPromotionCapture => {
            // Promotion
            let promo_value = (mv.kind.index() & 3) + 1; // 1=Knight, 2=Bishop, 3=Rook, 4=Queen
            CompoundMove {
                to: mv.to,
                promo: promo_value,
                second: FromTo {
                    from: mv.to,
                    to: mv.to,
                },
            }
        }
        _ => {
            // Simple move or capture
            CompoundMove {
                to: mv.to,
                promo: 0,
                second: FromTo::default(),
            }
        }
    }
}

/// Updates the board with the given move, which may be a capture.
/// Does not perform any legality checks. Any captured piece is returned.
pub fn make_move_board(board: &mut Board, change: BoardChange) -> BoardChange {
    // First move: move piece from 'from' to 'to'
    let mut first = Piece::Empty;
    std::mem::swap(&mut first, &mut board[change.first.from]);
    board[change.first.to] = first;

    // Second move: handle promotions and complex moves like castling
    let mut second = Piece::Empty;
    std::mem::swap(&mut second, &mut board[change.second.from]);
    if change.promo > 0 {
        let promoted = Piece::from_index(second.index() + change.promo as usize);
        second = promoted;
    }
    board[change.second.to] = second;

    change
}

/// Updates the board with the given move.
pub fn make_move(board: &mut Board, mv: Move) -> BoardChange {
    let change = prepare_move(board, mv);
    make_move_board(board, change)
}

/// Like make_move but also updates per turn state (active color, castling availability,
/// en passant target, halfmove clock, and fullmove number).
pub fn make_move_position_with_change(
    position: &mut Position,
    change: BoardChange,
    mv: Move,
) -> UndoPosition {
    let ours = position.board[change.first.from];
    let undo = UndoPosition::new(make_move_board(&mut position.board, change), position.turn);
    let mwp = MoveWithPieces {
        mv,
        piece: ours,
        captured: undo.board.captured,
    };
    position.turn = apply_move_turn(position.turn, mwp);
    undo
}

/// Like make_move but also updates per turn state.
pub fn make_move_position(position: &mut Position, mv: Move) -> UndoPosition {
    let change = prepare_move(&position.board, mv);
    make_move_position_with_change(position, change, mv)
}

/// Undoes the given move, restoring the captured piece to the captured square.
pub fn unmake_move_board(board: &mut Board, undo: BoardChange) {
    // Undo second move
    let mut ours = Piece::Empty;
    std::mem::swap(&mut board[undo.second.to], &mut ours);
    if undo.promo > 0 {
        ours = Piece::from_index(ours.index() - undo.promo as usize);
    }
    board[undo.second.from] = ours;

    // Undo first move and restore captured piece
    let mut piece = undo.captured;
    std::mem::swap(&mut piece, &mut board[undo.first.to]);
    board[undo.first.from] = piece;
}

/// Undoes the given move including turn state.
pub fn unmake_move_position(position: &mut Position, undo: UndoPosition) {
    unmake_move_board(&mut position.board, undo.board);
    position.turn = undo.turn;
}

/// Apply a move to a position and return the new position (functional style).
pub fn apply_move(mut position: Position, mv: Move) -> Position {
    // Remember the piece being moved, before applying the move to the board
    let piece = position.board[mv.from];

    // Apply the move to the board
    let undo = make_move(&mut position.board, mv);
    let mwp = MoveWithPieces {
        mv,
        piece,
        captured: undo.captured,
    };
    position.turn = apply_move_turn(position.turn, mwp);

    position
}

/// Apply a move to turn state.
pub fn apply_move_turn(mut turn: Turn, mwp: MoveWithPieces) -> Turn {
    // Update en passant target
    turn.set_en_passant(NO_EN_PASSANT_TARGET);
    if mwp.mv.kind == MoveKind::DoublePush {
        let from_rank = mwp.mv.from.rank();
        let to_rank = mwp.mv.to.rank();
        let from_file = mwp.mv.from.file();
        let target_rank = (from_rank + to_rank) / 2;
        let target_square = Square::make_square(from_file, target_rank);
        turn.set_en_passant(target_square);
    }

    // Update castling availability
    let mask = castling_mask(mwp.mv.from, mwp.mv.to);
    turn.set_castling(turn.castling() & !mask);

    // Update halfmove clock and fullmove number, and switch the active side
    turn.tick();

    // Reset halfmove clock on pawn advance or capture
    if mwp.piece.piece_type() == PieceType::Pawn || mwp.mv.kind.is_capture() {
        turn.reset_halfmove();
    }

    turn
}

/// Returns the castling mask for the castling rights cancelled by the given move.
pub fn castling_mask(from: Square, to: Square) -> CastlingMask {
    // Define the squares that affect castling rights
    let mask_table = [
        (Square::E1, CastlingMask::KQ), // White King
        (Square::H1, CastlingMask::K),  // White King Side Rook
        (Square::A1, CastlingMask::Q),  // White Queen Side Rook
        (Square::E8, CastlingMask::kq), // Black King
        (Square::H8, CastlingMask::k),  // Black King Side Rook
        (Square::A8, CastlingMask::q),  // Black Queen Side Rook
    ];

    let mut result = CastlingMask::None;

    for (square, mask) in &mask_table {
        if from == *square || to == *square {
            result = result | *mask;
        }
    }

    result
}

/// Helper function to check if there's a clear path between two squares
fn clear_path(occupancy: SquareSet, from: Square, to: Square) -> bool {
    let table = MovesTable::new(); // TODO: Use a global instance for performance
    let path = table.path(from, to);
    (path & occupancy).len() == 0
}
