pub mod moves;

pub use moves::*;
// Re-export types from fen that are now canonical
pub use fen::{Turn, Position, CastlingMask, NO_EN_PASSANT_TARGET};
pub use moves_table::{MoveKind, FromTo};

use fen::{Piece, Square};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Move {
    pub from: Square,
    pub to: Square,
    pub kind: MoveKind,
}

impl Default for Move {
    fn default() -> Self {
        Self {
            from: Square::A1,
            to: Square::A1,
            kind: MoveKind::default(),
        }
    }
}

impl Move {
    pub fn new(from: Square, to: Square, kind: MoveKind) -> Self {
        Self { from, to, kind }
    }

    pub fn is_null(self) -> bool {
        self.from == self.to
    }

    /// Returns true if this is a valid move (not a null move)
    /// This matches the C++ operator bool() behavior
    pub fn is_valid(self) -> bool {
        !self.is_null()
    }
}

pub type MoveVector = Vec<Move>;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MoveWithPieces {
    pub mv: Move,
    pub piece: Piece,
    pub captured: Piece,
}

/// Representation of data needed to make or unmake a move
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct BoardChange {
    pub captured: Piece,
    pub promo: u8,
    pub first: FromTo,
    pub second: FromTo,
}

impl Default for BoardChange {
    fn default() -> Self {
        Self {
            captured: Piece::Empty,
            promo: 0,
            first: FromTo::default(),
            second: FromTo::default(),
        }
    }
}


/// Represents a move error
#[derive(Debug, Clone)]
pub struct MoveError {
    pub message: String,
}

impl std::fmt::Display for MoveError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for MoveError {}

impl MoveError {
    pub fn new(message: String) -> Self {
        Self { message }
    }
}

/// Information needed to undo a move
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct UndoPosition {
    pub board: BoardChange,
    pub turn: fen::Turn,
}

impl UndoPosition {
    pub fn new(board: BoardChange, turn: fen::Turn) -> Self {
        Self { board, turn }
    }
}

use std::fmt;

impl fmt::Display for Move {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_null() {
            write!(f, "0000")
        } else {
            let mut result = format!("{}{}", self.from, self.to);
            if self.kind.is_promotion() {
                let promo_piece = match self.kind.index() & 3 {
                    0 => 'n', // Knight
                    1 => 'b', // Bishop  
                    2 => 'r', // Rook
                    3 => 'q', // Queen
                    _ => unreachable!(),
                };
                result.push(promo_piece);
            }
            write!(f, "{}", result)
        }
    }
}
