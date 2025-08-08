use std::fmt;
use num_enum::TryFromPrimitive;

pub const NUM_FILES: usize = 8;
pub const NUM_RANKS: usize = 8;
pub const NUM_SQUARES: usize = NUM_FILES * NUM_RANKS;
/// Number of different piece types (including Empty).
/// This constant is part of the public API for other crates (e.g., hash crate).
#[allow(dead_code)]
pub const NUM_PIECES: usize = 13; // P, N, B, R, Q, K, Empty, p, n, b, r, q, k

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, TryFromPrimitive)]
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

#[allow(dead_code)] // from_int used elsewhere
impl Square {
    pub fn make_square(file: usize, rank: usize) -> Self {
        assert!(file < NUM_FILES && rank < NUM_RANKS);
        let index = rank * NUM_FILES + file;
        Self::try_from(index as u8).expect("Square index out of bounds")
    }

    pub fn from_int(int: usize) -> Self {
        assert!(int < NUM_SQUARES);
        Self::try_from(int as u8).expect("Square index out of bounds")
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
#[allow(dead_code)] // Some piece types not used yet during migration
pub enum PieceType {
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    Empty,
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

    /// Returns the numeric index of this piece type.
    /// This method is part of the public API for other crates (e.g., hash crate).
    #[allow(dead_code)]
    pub fn index(self) -> usize {
        self as usize
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

    /// Returns the color of this piece.
    /// This method is part of the public API for other crates (e.g., moves crate).
    #[allow(dead_code)]
    pub fn color(self) -> Color {
        match self {
            Piece::P | Piece::N | Piece::B | Piece::R | Piece::Q | Piece::K => Color::White,
            Piece::p | Piece::n | Piece::b | Piece::r | Piece::q | Piece::k => Color::Black,
            Piece::Empty => unreachable!("Empty piece has no color"),
        }
    }

    /// Returns the piece type of this piece.
    /// This method is part of the public API for other crates (e.g., moves crate).
    #[allow(dead_code)]
    pub fn piece_type(self) -> PieceType {
        match self {
            Piece::P | Piece::p => PieceType::Pawn,
            Piece::N | Piece::n => PieceType::Knight,
            Piece::B | Piece::b => PieceType::Bishop,
            Piece::R | Piece::r => PieceType::Rook,
            Piece::Q | Piece::q => PieceType::Queen,
            Piece::K | Piece::k => PieceType::King,
            Piece::Empty => PieceType::Empty,
        }
    }

    /// Creates a piece from its numeric index.
    /// This method is part of the public API for other crates (e.g., moves crate).
    #[allow(dead_code)]
    pub fn from_index(index: usize) -> Self {
        match index {
            0 => Piece::P,
            1 => Piece::N,
            2 => Piece::B,
            3 => Piece::R,
            4 => Piece::Q,
            5 => Piece::K,
            6 => Piece::Empty,
            7 => Piece::p,
            8 => Piece::n,
            9 => Piece::b,
            10 => Piece::r,
            11 => Piece::q,
            12 => Piece::k,
            _ => panic!("Invalid piece index: {}", index),
        }
    }

    /// Creates a piece from piece type and color.
    /// This method is part of the public API for other crates (e.g., moves crate).
    #[allow(dead_code)]
    pub fn from_type_and_color(piece_type: PieceType, color: Color) -> Self {
        match (piece_type, color) {
            (PieceType::Pawn, Color::White) => Piece::P,
            (PieceType::Knight, Color::White) => Piece::N,
            (PieceType::Bishop, Color::White) => Piece::B,
            (PieceType::Rook, Color::White) => Piece::R,
            (PieceType::Queen, Color::White) => Piece::Q,
            (PieceType::King, Color::White) => Piece::K,
            (PieceType::Pawn, Color::Black) => Piece::p,
            (PieceType::Knight, Color::Black) => Piece::n,
            (PieceType::Bishop, Color::Black) => Piece::b,
            (PieceType::Rook, Color::Black) => Piece::r,
            (PieceType::Queen, Color::Black) => Piece::q,
            (PieceType::King, Color::Black) => Piece::k,
            (PieceType::Empty, _) => Piece::Empty,
        }
    }

}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_piece_index() {
        // Test that each piece has a unique index
        assert_eq!(Piece::P.index(), 0);
        assert_eq!(Piece::N.index(), 1);
        assert_eq!(Piece::B.index(), 2);
        assert_eq!(Piece::R.index(), 3);
        assert_eq!(Piece::Q.index(), 4);
        assert_eq!(Piece::K.index(), 5);
        assert_eq!(Piece::Empty.index(), 6);
        assert_eq!(Piece::p.index(), 7);
        assert_eq!(Piece::n.index(), 8);
        assert_eq!(Piece::b.index(), 9);
        assert_eq!(Piece::r.index(), 10);
        assert_eq!(Piece::q.index(), 11);
        assert_eq!(Piece::k.index(), 12);
        
        // Verify that index is consistent with enum ordering
        assert_eq!(Piece::P.index(), Piece::P as usize);
        assert_eq!(Piece::k.index(), Piece::k as usize);
    }

    #[test]
    fn test_square_make_and_properties() {
        let square = Square::make_square(0, 0);
        assert_eq!(square, Square::A1);
        assert_eq!(square.file(), 0);
        assert_eq!(square.rank(), 0);
        
        let square = Square::make_square(7, 7);
        assert_eq!(square, Square::H8);
        assert_eq!(square.file(), 7);
        assert_eq!(square.rank(), 7);
        
        let square = Square::make_square(4, 3);
        assert_eq!(square, Square::E4);
        assert_eq!(square.file(), 4);
        assert_eq!(square.rank(), 3);
    }

    #[test]
    fn test_color_operations() {
        assert_eq!(!Color::White, Color::Black);
        assert_eq!(!Color::Black, Color::White);
        
        assert_eq!(Color::White.to_string(), "w");
        assert_eq!(Color::Black.to_string(), "b");
    }

    #[test]
    fn test_piece_char_conversion() {
        // Test round-trip conversion
        for &piece in &[Piece::P, Piece::N, Piece::B, Piece::R, Piece::Q, Piece::K,
                       Piece::p, Piece::n, Piece::b, Piece::r, Piece::q, Piece::k, Piece::Empty] {
            let ch = piece.to_char();
            assert_eq!(Piece::from_char(ch), Some(piece));
        }
        
        // Test invalid characters
        assert_eq!(Piece::from_char('x'), None);
        assert_eq!(Piece::from_char('1'), None);
    }

    #[test]
    fn test_constants() {
        assert_eq!(NUM_FILES, 8);
        assert_eq!(NUM_RANKS, 8);
        assert_eq!(NUM_SQUARES, 64);
        assert_eq!(NUM_PIECES, 13);
    }
}
