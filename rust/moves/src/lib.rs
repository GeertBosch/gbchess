pub mod moves;

pub use moves::*;

use fen::{Board, Color, Piece, Square};
use square_set::SquareSet;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
#[allow(non_camel_case_types)]
pub enum MoveKind {
    // Moves that don't capture or promote
    QuietMove = 0,
    DoublePush = 1,
    OO = 2,  // King-side castling
    OOO = 3, // Queen-side castling

    // Captures that don't promote
    Capture = 4,
    EnPassant = 5,

    // Promotions that don't capture
    KnightPromotion = 8,
    BishopPromotion = 9,
    RookPromotion = 10,
    QueenPromotion = 11,

    // Promotions that capture
    KnightPromotionCapture = 12,
    BishopPromotionCapture = 13,
    RookPromotionCapture = 14,
    QueenPromotionCapture = 15,
}

impl Default for MoveKind {
    fn default() -> Self {
        MoveKind::QuietMove
    }
}

impl MoveKind {
    pub const CAPTURE_MASK: u8 = 4;
    pub const PROMOTION_MASK: u8 = 8;

    pub fn index(self) -> u8 {
        self as u8
    }

    pub fn is_capture(self) -> bool {
        (self.index() & Self::CAPTURE_MASK) != 0
    }

    pub fn is_promotion(self) -> bool {
        (self.index() & Self::PROMOTION_MASK) != 0
    }

    pub fn is_castles(self) -> bool {
        matches!(self, MoveKind::OO | MoveKind::OOO)
    }
}

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
#[repr(u8)]
#[allow(non_camel_case_types)]
pub enum CastlingMask {
    // We follow the UCI standard naming for castling rights, even if it violates Rust naming
    // conventions, so we silence the linter here.
    None = 0,
    K = 1,     // White kingside
    Q = 2,     // White queenside
    k = 4,     // Black kingside
    q = 8,     // Black queenside
    KQ = 3,    // White both sides (K | Q)
    Kk = 5,    // White kingside + Black kingside (K | k)
    Qk = 6,    // White queenside + Black kingside (Q | k)
    KQk = 7,   // White both + Black kingside (K | Q | k)
    Kq = 9,    // White kingside + Black queenside (K | q)
    Qq = 10,   // White queenside + Black queenside (Q | q)
    KQq = 11,  // White both + Black queenside (K | Q | q)
    kq = 12,   // Black both sides (k | q)
    Kkq = 13,  // White kingside + Black both (K | k | q)
    Qkq = 14,  // White queenside + Black both (Q | k | q)
    KQkq = 15, // All castling rights (K | Q | k | q)
}

impl CastlingMask {
    pub fn all() -> Self {
        CastlingMask::KQkq
    }
}

impl std::ops::BitAnd for CastlingMask {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self {
        // Safe cast because we're only using the lower 4 bits
        unsafe { std::mem::transmute((self as u8) & (rhs as u8)) }
    }
}

impl std::ops::BitOr for CastlingMask {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self {
        // Safe cast because we're only using the lower 4 bits
        unsafe { std::mem::transmute((self as u8) | (rhs as u8)) }
    }
}

impl std::ops::Not for CastlingMask {
    type Output = Self;

    fn not(self) -> Self {
        // Safe cast because we're only using the lower 4 bits
        unsafe { std::mem::transmute(!(self as u8) & 0xF) }
    }
}

/// No en passant target
pub const NO_EN_PASSANT_TARGET: Square = Square::A1;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FromTo {
    pub from: Square,
    pub to: Square,
}

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

impl Default for FromTo {
    fn default() -> Self {
        Self {
            from: Square::A1,
            to: Square::A1,
        }
    }
}

/// Game turn state including castling, en passant, etc.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Turn {
    pub active_color: Color,
    pub castling_availability: CastlingMask,
    pub en_passant_target: Square,
    pub halfmove_clock: u8,
    pub fullmove_number: u16,
}

impl Turn {
    pub fn new(
        active_color: Color,
        castling_availability: CastlingMask,
        en_passant_target: Square,
        halfmove_clock: u8,
        fullmove_number: u16,
    ) -> Self {
        Self {
            active_color,
            castling_availability,
            en_passant_target,
            halfmove_clock,
            fullmove_number,
        }
    }

    pub fn new_starting_position() -> Self {
        Self {
            active_color: Color::White,
            castling_availability: CastlingMask::all(),
            en_passant_target: NO_EN_PASSANT_TARGET,
            halfmove_clock: 0,
            fullmove_number: 1,
        }
    }

    pub fn active_color(&self) -> Color {
        self.active_color
    }

    pub fn set_active_color(&mut self, color: Color) {
        self.active_color = color;
    }

    pub fn castling(&self) -> CastlingMask {
        self.castling_availability
    }

    pub fn set_castling(&mut self, castling: CastlingMask) {
        self.castling_availability = castling;
    }

    pub fn en_passant(&self) -> Square {
        self.en_passant_target
    }

    pub fn set_en_passant(&mut self, square: Square) {
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

    /// Advance the turn (switch sides, update clocks)
    pub fn tick(&mut self) {
        self.halfmove_clock += 1;
        self.active_color = !self.active_color;
        if self.active_color == Color::White {
            self.fullmove_number += 1;
        }
    }
}

impl Default for Turn {
    fn default() -> Self {
        Self::new_starting_position()
    }
}

/// Complete game position
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Position {
    pub board: Board,
    pub turn: Turn,
}

impl Position {
    pub fn new() -> Self {
        Self {
            board: Board::new(),
            turn: Turn::new_starting_position(),
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

/// Occupancy represents which squares are occupied by each side
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Occupancy {
    theirs: SquareSet,
    ours: SquareSet,
}

impl Occupancy {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn from_board(board: &Board, active_color: Color) -> Self {
        let mut ours = SquareSet::new();
        let mut theirs = SquareSet::new();

        for square_idx in 0..64 {
            let square = unsafe { std::mem::transmute(square_idx as u8) };
            let piece = board[square];
            if piece != Piece::Empty {
                let piece_color = piece.color();
                if piece_color == active_color {
                    ours = ours | SquareSet::from_square(square);
                } else {
                    theirs = theirs | SquareSet::from_square(square);
                }
            }
        }

        Self { theirs, ours }
    }

    pub fn delta(theirs: SquareSet, ours: SquareSet) -> Self {
        Self { theirs, ours }
    }

    pub fn theirs(&self) -> SquareSet {
        self.theirs
    }

    pub fn ours(&self) -> SquareSet {
        self.ours
    }

    pub fn all(&self) -> SquareSet {
        self.theirs | self.ours
    }

    pub fn flip(&self) -> Self {
        Self {
            theirs: self.ours,
            ours: self.theirs,
        }
    }
}

impl std::ops::BitXor for Occupancy {
    type Output = Self;

    fn bitxor(self, rhs: Self) -> Self {
        Self {
            theirs: self.theirs ^ rhs.theirs,
            ours: self.ours ^ rhs.ours,
        }
    }
}

impl std::ops::BitXorAssign for Occupancy {
    fn bitxor_assign(&mut self, rhs: Self) {
        self.theirs ^= rhs.theirs;
        self.ours ^= rhs.ours;
    }
}

impl std::ops::Not for Occupancy {
    type Output = Self;

    fn not(self) -> Self {
        self.flip()
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
    pub turn: Turn,
}

impl UndoPosition {
    pub fn new(board: BoardChange, turn: Turn) -> Self {
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
