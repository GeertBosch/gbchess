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

#[allow(dead_code)] // Methods will be used as migration progresses
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

    pub fn set_active(&mut self, color: Color) {
        self.active_color = color;
    }

    pub fn castling(&self) -> CastlingMask {
        self.castling_mask
    }

    pub fn set_castling(&mut self, castling: CastlingMask) {
        self.castling_mask = castling;
    }

    pub fn en_passant(&self) -> Option<Square> {
        self.en_passant_target
    }

    pub fn set_en_passant(&mut self, square: Option<Square>) {
        self.en_passant_target = square;
    }

    pub fn halfmove(&self) -> u8 {
        self.halfmove_clock
    }

    pub fn reset_halfmove(&mut self) {
        self.halfmove_clock = 0;
    }

    pub fn fullmove(&self) -> u16 {
        self.fullmove_number
    }

    pub fn tick(&mut self) {
        self.halfmove_clock += 1;
        self.active_color = !self.active_color;
        if self.active_color == Color::White {
            self.fullmove_number += 1;
        }
    }

    pub fn make_null_move(&mut self) {
        self.active_color = !self.active_color;
        self.en_passant_target = None;
        self.halfmove_clock += 1;
        if self.active_color == Color::White {
            self.fullmove_number += 1;
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Position {
    pub board: Board,
    pub turn: Turn,
}

#[allow(dead_code)] // Methods will be used as migration progresses
impl Position {
    pub fn new() -> Self {
        Self {
            board: Board::new(),
            turn: Turn::initial(),
        }
    }

    pub fn active(&self) -> Color {
        self.turn.active_color()
    }
}

impl Default for Position {
    fn default() -> Self {
        Self::new()
    }
}
