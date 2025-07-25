use crate::types::{Color, Piece, Square, NUM_SQUARES};
use std::ops::{Index, IndexMut};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Board {
    squares: [Piece; NUM_SQUARES],
}

impl Board {
    pub fn new() -> Self {
        Self {
            squares: [Piece::Empty; NUM_SQUARES],
        }
    }
}

impl Default for Board {
    fn default() -> Self {
        Self::new()
    }
}

impl Index<Square> for Board {
    type Output = Piece;

    fn index(&self, square: Square) -> &Self::Output {
        &self.squares[square as usize]
    }
}

impl IndexMut<Square> for Board {
    fn index_mut(&mut self, square: Square) -> &mut Self::Output {
        &mut self.squares[square as usize]
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CastlingMask(u8);

#[allow(non_upper_case_globals)] // FEN notation uses K, Q, k, q which is more readable
impl CastlingMask {
    pub const EMPTY: Self = CastlingMask(0);
    pub const K: Self = CastlingMask(1); // White king-side
    pub const Q: Self = CastlingMask(2); // White queen-side
    pub const k: Self = CastlingMask(4); // Black king-side
    pub const q: Self = CastlingMask(8); // Black queen-side
    
    pub const KQ_kq: Self = CastlingMask(Self::K.0 | Self::Q.0 | Self::k.0 | Self::q.0); // All castling rights

    pub fn value(self) -> u8 {
        self.0
    }

    pub fn has_white_kingside(self) -> bool {
        self.0 & Self::K.0 != 0
    }

    pub fn has_white_queenside(self) -> bool {
        self.0 & Self::Q.0 != 0
    }

    pub fn has_black_kingside(self) -> bool {
        self.0 & Self::k.0 != 0
    }

    pub fn has_black_queenside(self) -> bool {
        self.0 & Self::q.0 != 0
    }
}

impl std::ops::BitAnd for CastlingMask {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self::Output {
        Self(self.0 & rhs.0)
    }
}

impl std::ops::BitOr for CastlingMask {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

impl std::ops::BitAndAssign for CastlingMask {
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0;
    }
}

impl std::ops::BitOrAssign for CastlingMask {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl std::ops::Not for CastlingMask {
    type Output = Self;

    fn not(self) -> Self::Output {
        Self(!self.0 & 0b1111) // Only invert lower 4 bits
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Turn {
    active_color: Color,
    castling_mask: CastlingMask,
    en_passant_target: Option<Square>,
    halfmove_clock: u8,
    fullmove_number: u16,
}

impl Turn {
    pub fn new(
        active_color: Color,
        castling_mask: CastlingMask,
        en_passant_target: Option<Square>,
        halfmove_clock: u8,
        fullmove_number: u16,
    ) -> Self {
        Self {
            active_color,
            castling_mask,
            en_passant_target,
            halfmove_clock,
            fullmove_number,
        }
    }

    pub fn initial() -> Self {
        Self::new(Color::White, CastlingMask::KQ_kq, None, 0, 1)
    }

    pub fn active_color(&self) -> Color {
        self.active_color
    }

    pub fn castling(&self) -> CastlingMask {
        self.castling_mask
    }

    pub fn en_passant(&self) -> Option<Square> {
        self.en_passant_target
    }
    pub fn halfmove(&self) -> u8 {
        self.halfmove_clock
    }

    pub fn fullmove(&self) -> u16 {
        self.fullmove_number
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Position {
    pub board: Board,
    pub turn: Turn,
}

impl Position {
    pub fn new() -> Self {
        Self {
            board: Board::new(),
            turn: Turn::initial(),
        }
    }

    /// Returns the active color for the current position.
    /// This method is part of the public API for other crates (e.g., hash crate).
    #[allow(dead_code)]
    pub fn active(&self) -> Color {
        self.turn.active_color
    }
}

impl Default for Position {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{Color, Square};

    #[test]
    fn test_position_active_color() {
        let position = Position::new();
        assert_eq!(position.active(), Color::White);
        
        // Test with a position where black is active
        let mut turn = Turn::initial();
        turn.active_color = Color::Black;
        let position = Position {
            board: Board::new(),
            turn,
        };
        assert_eq!(position.active(), Color::Black);
    }

    #[test]
    fn test_board_indexing() {
        let mut board = Board::new();
        
        // Test reading from empty board
        assert_eq!(board[Square::A1], crate::types::Piece::Empty);
        assert_eq!(board[Square::E4], crate::types::Piece::Empty);
        
        // Test writing to board
        board[Square::A1] = crate::types::Piece::K;
        assert_eq!(board[Square::A1], crate::types::Piece::K);
        
        board[Square::E4] = crate::types::Piece::p;
        assert_eq!(board[Square::E4], crate::types::Piece::p);
    }

    #[test]
    fn test_castling_mask_operations() {
        let mask = CastlingMask::K | CastlingMask::Q;
        assert!(mask.has_white_kingside());
        assert!(mask.has_white_queenside());
        assert!(!mask.has_black_kingside());
        assert!(!mask.has_black_queenside());
        
        let full_mask = CastlingMask::KQ_kq;
        assert!(full_mask.has_white_kingside());
        assert!(full_mask.has_white_queenside());
        assert!(full_mask.has_black_kingside());
        assert!(full_mask.has_black_queenside());
    }
}
