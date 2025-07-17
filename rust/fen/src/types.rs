use std::fmt;

pub const NUM_FILES: usize = 8;
pub const NUM_RANKS: usize = 8;
pub const NUM_SQUARES: usize = NUM_FILES * NUM_RANKS;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(u8)]
#[allow(dead_code)] // Many squares not used yet during migration
#[rustfmt::skip]    // Keep 8x8 grid layout
pub enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
}

impl Square {
    pub fn make_square(file: usize, rank: usize) -> Self {
        assert!(file < NUM_FILES && rank < NUM_RANKS);
        // Safe cast because we validate bounds above
        unsafe { std::mem::transmute((rank * NUM_FILES + file) as u8) }
    }

    pub fn rank(self) -> usize {
        (self as usize) / NUM_FILES
    }

    pub fn file(self) -> usize {
        (self as usize) % NUM_RANKS
    }
}

impl fmt::Display for Square {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let file_char = (b'a' + self.file() as u8) as char;
        let rank_char = (b'1' + self.rank() as u8) as char;
        write!(f, "{}{}", file_char, rank_char)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Color {
    White = 0,
    Black = 1,
}

impl std::ops::Not for Color {
    type Output = Self;

    fn not(self) -> Self {
        match self {
            Color::White => Color::Black,
            Color::Black => Color::White,
        }
    }
}

impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Color::White => write!(f, "w"),
            Color::Black => write!(f, "b"),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
#[allow(non_camel_case_types)]
pub enum Piece {
    P,
    N,
    B,
    R,
    Q,
    K,
    Empty, // White pieces + empty
    p,
    n,
    b,
    r,
    q,
    k, // Black pieces
}

impl Piece {
    pub fn from_char(c: char) -> Option<Self> {
        match c {
            'P' => Some(Piece::P),
            'N' => Some(Piece::N),
            'B' => Some(Piece::B),
            'R' => Some(Piece::R),
            'Q' => Some(Piece::Q),
            'K' => Some(Piece::K),
            'p' => Some(Piece::p),
            'n' => Some(Piece::n),
            'b' => Some(Piece::b),
            'r' => Some(Piece::r),
            'q' => Some(Piece::q),
            'k' => Some(Piece::k),
            ' ' | '.' => Some(Piece::Empty),
            _ => None,
        }
    }

    pub fn to_char(self) -> char {
        match self {
            Piece::P => 'P',
            Piece::N => 'N',
            Piece::B => 'B',
            Piece::R => 'R',
            Piece::Q => 'Q',
            Piece::K => 'K',
            Piece::p => 'p',
            Piece::n => 'n',
            Piece::b => 'b',
            Piece::r => 'r',
            Piece::q => 'q',
            Piece::k => 'k',
            Piece::Empty => '.',
        }
    }
}
