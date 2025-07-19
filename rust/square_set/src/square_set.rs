use std::fmt;

// Re-export from fen crate for convenience
pub use fen::board::*;
pub use fen::types::*;

/// Represents a set of squares on a chess board using a 64-bit bitset.
/// This is equivalent to std::set<Square> but more efficient for chess operations.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SquareSet {
    bits: u64,
}

impl SquareSet {
    /// Create an empty square set
    pub const fn new() -> Self {
        Self { bits: 0 }
    }

    /// Create a square set from a raw u64 bitset
    pub const fn from_bits(bits: u64) -> Self {
        Self { bits }
    }

    /// Create a square set containing a single square
    pub const fn from_square(square: Square) -> Self {
        Self {
            bits: 1u64 << (square as usize),
        }
    }

    /// Create a square set representing a rank (0-7)
    pub fn rank(rank: usize) -> Self {
        debug_assert!(rank < 8);
        let shift = rank * 8;
        let mask = 0xffu64 << shift;
        Self { bits: mask }
    }

    /// Create a square set representing a file (0-7)
    pub fn file(file: usize) -> Self {
        debug_assert!(file < 8);
        Self {
            bits: 0x0101010101010101u64 << file,
        }
    }

    /// Create a square set for a valid square, or empty if out of bounds
    pub fn valid(rank: i32, file: i32) -> Self {
        if rank >= 0 && rank < 8 && file >= 0 && file < 8 {
            let square = Square::make_square(file as usize, rank as usize);
            Self::from_square(square)
        } else {
            Self::new()
        }
    }

    /// Create a path between two squares (excluding the starting square)
    /// Returns empty set if the move is not horizontal, vertical, or diagonal
    pub fn make_path(from: Square, to: Square) -> Self {
        let from_rank = from.rank() as i32;
        let from_file = from.file() as i32;
        let to_rank = to.rank() as i32;
        let to_file = to.file() as i32;

        let rank_diff = to_rank - from_rank;
        let file_diff = to_file - from_file;

        // Check if this is a valid line move
        if rank_diff != 0 && file_diff != 0 && rank_diff.abs() != file_diff.abs() {
            return Self::new();
        }

        let step_rank = if rank_diff > 0 {
            1
        } else if rank_diff < 0 {
            -1
        } else {
            0
        };

        let step_file = if file_diff > 0 {
            1
        } else if file_diff < 0 {
            -1
        } else {
            0
        };

        let mut path = Self::new();
        let mut current_rank = from_rank + step_rank;
        let mut current_file = from_file + step_file;

        while current_rank != to_rank || current_file != to_file {
            if current_rank >= 0 && current_rank < 8 && current_file >= 0 && current_file < 8 {
                let square = Square::make_square(current_file as usize, current_rank as usize);
                path = path.with_square(square);
            }
            current_rank += step_rank;
            current_file += step_file;
        }

        path
    }

    /// Create a square set with all squares
    pub const fn all() -> Self {
        Self {
            bits: 0xffffffffffffffffu64,
        }
    }

    /// Get the raw bits
    pub const fn bits(self) -> u64 {
        self.bits
    }

    /// Check if the set is empty
    pub const fn is_empty(self) -> bool {
        self.bits == 0
    }

    /// Count the number of squares in the set
    pub const fn len(self) -> u32 {
        self.bits.count_ones()
    }

    /// Check if a square is in the set
    pub const fn contains(self, square: Square) -> bool {
        (self.bits & (1u64 << (square as usize))) != 0
    }

    /// Add a square to the set
    pub const fn with_square(self, square: Square) -> Self {
        Self {
            bits: self.bits | (1u64 << (square as usize)),
        }
    }

    /// Remove a square from the set
    pub const fn without_square(self, square: Square) -> Self {
        Self {
            bits: self.bits & !(1u64 << (square as usize)),
        }
    }

    /// Add another square set to this one
    pub const fn with_set(self, other: SquareSet) -> Self {
        Self {
            bits: self.bits | other.bits,
        }
    }

    /// Remove another square set from this one
    pub const fn without_set(self, other: SquareSet) -> Self {
        Self {
            bits: self.bits & !other.bits,
        }
    }

    /// Mutable version: insert a square
    pub fn insert(&mut self, square: Square) {
        self.bits |= 1u64 << (square as usize);
    }

    /// Mutable version: remove a square
    pub fn remove(&mut self, square: Square) {
        self.bits &= !(1u64 << (square as usize));
    }

    /// Mutable version: insert another square set
    pub fn insert_set(&mut self, other: SquareSet) {
        self.bits |= other.bits;
    }

    /// Get an iterator over the squares in the set
    pub const fn iter(self) -> SquareSetIter {
        SquareSetIter { bits: self.bits }
    }

    /// Get the first (lowest) square in the set, if any
    pub const fn first(self) -> Option<Square> {
        if self.bits == 0 {
            None
        } else {
            Some(unsafe { std::mem::transmute(self.bits.trailing_zeros() as u8) })
        }
    }

    /// Remove and return the first square from the set
    pub fn pop_first(&mut self) -> Option<Square> {
        if let Some(square) = self.first() {
            self.remove(square);
            Some(square)
        } else {
            None
        }
    }
}

/// Bitwise operations
impl std::ops::BitAnd for SquareSet {
    type Output = Self;
    fn bitand(self, rhs: Self) -> Self::Output {
        Self {
            bits: self.bits & rhs.bits,
        }
    }
}

impl std::ops::BitOr for SquareSet {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self::Output {
        Self {
            bits: self.bits | rhs.bits,
        }
    }
}

impl std::ops::BitXor for SquareSet {
    type Output = Self;
    fn bitxor(self, rhs: Self) -> Self::Output {
        Self {
            bits: self.bits ^ rhs.bits,
        }
    }
}

impl std::ops::Not for SquareSet {
    type Output = Self;
    fn not(self) -> Self::Output {
        Self { bits: !self.bits }
    }
}

impl std::ops::Sub for SquareSet {
    type Output = Self;
    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            bits: self.bits & !rhs.bits,
        }
    }
}

impl std::ops::Shl<u32> for SquareSet {
    type Output = Self;
    fn shl(self, rhs: u32) -> Self::Output {
        Self {
            bits: self.bits << rhs,
        }
    }
}

impl std::ops::Shr<u32> for SquareSet {
    type Output = Self;
    fn shr(self, rhs: u32) -> Self::Output {
        Self {
            bits: self.bits >> rhs,
        }
    }
}

/// Assignment operators
impl std::ops::BitAndAssign for SquareSet {
    fn bitand_assign(&mut self, rhs: Self) {
        self.bits &= rhs.bits;
    }
}

impl std::ops::BitOrAssign for SquareSet {
    fn bitor_assign(&mut self, rhs: Self) {
        self.bits |= rhs.bits;
    }
}

impl std::ops::BitXorAssign for SquareSet {
    fn bitxor_assign(&mut self, rhs: Self) {
        self.bits ^= rhs.bits;
    }
}

impl std::ops::SubAssign for SquareSet {
    fn sub_assign(&mut self, rhs: Self) {
        self.bits &= !rhs.bits;
    }
}

impl std::ops::ShlAssign<u32> for SquareSet {
    fn shl_assign(&mut self, rhs: u32) {
        self.bits <<= rhs;
    }
}

impl std::ops::ShrAssign<u32> for SquareSet {
    fn shr_assign(&mut self, rhs: u32) {
        self.bits >>= rhs;
    }
}

impl Default for SquareSet {
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Display for SquareSet {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "  a b c d e f g h")?;
        for rank in (0..8).rev() {
            write!(f, "{} ", rank + 1)?;
            for file in 0..8 {
                let square = Square::make_square(file, rank);
                if self.contains(square) {
                    write!(f, "X ")?;
                } else {
                    write!(f, ". ")?;
                }
            }
            writeln!(f)?;
        }
        Ok(())
    }
}

/// Iterator over squares in a SquareSet
#[derive(Debug, Clone, Copy)]
pub struct SquareSetIter {
    bits: u64,
}

impl Iterator for SquareSetIter {
    type Item = Square;

    fn next(&mut self) -> Option<Self::Item> {
        if self.bits == 0 {
            None
        } else {
            let square = unsafe { std::mem::transmute(self.bits.trailing_zeros() as u8) };
            self.bits &= self.bits - 1; // Clear the least significant bit
            Some(square)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.bits.count_ones() as usize;
        (len, Some(len))
    }
}

impl ExactSizeIterator for SquareSetIter {}

impl IntoIterator for SquareSet {
    type Item = Square;
    type IntoIter = SquareSetIter;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

/// Fast piece finding using SIMD-inspired techniques
/// Since we're assuming SSE2EMUL, we use portable implementations
/// that the compiler can optimize appropriately

/// Find all squares containing a specific piece using optimized comparison
pub fn equal_set(board: &Board, piece: Piece) -> SquareSet {
    let mut bits = 0u64;

    // Process board in chunks for better performance
    for chunk_start in (0..64).step_by(8) {
        let mut chunk_bits = 0u8;

        for i in 0..8 {
            let square_idx = chunk_start + i;
            if square_idx < 64 {
                let square = unsafe { std::mem::transmute(square_idx as u8) };
                if board[square] == piece {
                    chunk_bits |= 1u8 << i;
                }
            }
        }

        bits |= (chunk_bits as u64) << chunk_start;
    }

    SquareSet::from_bits(bits)
}

/// Find all non-empty squares on the board
pub fn occupancy(board: &Board) -> SquareSet {
    let mut bits = 0u64;

    for square_idx in 0..64 {
        let square = unsafe { std::mem::transmute(square_idx as u8) };
        if board[square] != Piece::Empty {
            bits |= 1u64 << square_idx;
        }
    }

    SquareSet::from_bits(bits)
}

/// Helper function to determine piece color
fn piece_color(piece: Piece) -> Option<Color> {
    match piece {
        Piece::P | Piece::N | Piece::B | Piece::R | Piece::Q | Piece::K => Some(Color::White),
        Piece::p | Piece::n | Piece::b | Piece::r | Piece::q | Piece::k => Some(Color::Black),
        Piece::Empty => None,
    }
}

/// Find all squares occupied by pieces of a specific color
pub fn occupancy_by_color(board: &Board, color: Color) -> SquareSet {
    let mut bits = 0u64;

    for square_idx in 0..64 {
        let square = unsafe { std::mem::transmute(square_idx as u8) };
        let piece = board[square];
        if let Some(piece_color) = piece_color(piece) {
            if piece_color == color {
                bits |= 1u64 << square_idx;
            }
        }
    }

    SquareSet::from_bits(bits)
}

/// Find all squares containing a specific piece
pub fn find_piece(board: &Board, piece: Piece) -> SquareSet {
    equal_set(board, piece)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_empty_set() {
        let set = SquareSet::new();
        assert!(set.is_empty());
        assert_eq!(set.len(), 0);
        assert!(!set.contains(Square::A1));
    }

    #[test]
    fn test_single_square() {
        let set = SquareSet::from_square(Square::E4);
        assert!(!set.is_empty());
        assert_eq!(set.len(), 1);
        assert!(set.contains(Square::E4));
        assert!(!set.contains(Square::E5));
    }

    #[test]
    fn test_insert_remove() {
        let mut set = SquareSet::new();

        set.insert(Square::A1);
        assert!(set.contains(Square::A1));
        assert_eq!(set.len(), 1);

        set.insert(Square::H8);
        assert!(set.contains(Square::A1));
        assert!(set.contains(Square::H8));
        assert_eq!(set.len(), 2);

        set.remove(Square::A1);
        assert!(!set.contains(Square::A1));
        assert!(set.contains(Square::H8));
        assert_eq!(set.len(), 1);
    }

    #[test]
    fn test_rank_file() {
        let rank1 = SquareSet::rank(0);
        assert_eq!(rank1.len(), 8);
        assert!(rank1.contains(Square::A1));
        assert!(rank1.contains(Square::H1));
        assert!(!rank1.contains(Square::A2));

        let file_a = SquareSet::file(0);
        assert_eq!(file_a.len(), 8);
        assert!(file_a.contains(Square::A1));
        assert!(file_a.contains(Square::A8));
        assert!(!file_a.contains(Square::B1));
    }

    #[test]
    fn test_bitwise_ops() {
        let set1 = SquareSet::from_square(Square::A1);
        let set2 = SquareSet::from_square(Square::B1);
        let set3 = SquareSet::from_square(Square::A1);

        let union = set1 | set2;
        assert_eq!(union.len(), 2);
        assert!(union.contains(Square::A1));
        assert!(union.contains(Square::B1));

        let intersection = set1 & set3;
        assert_eq!(intersection.len(), 1);
        assert!(intersection.contains(Square::A1));

        let difference = union - set1;
        assert_eq!(difference.len(), 1);
        assert!(!difference.contains(Square::A1));
        assert!(difference.contains(Square::B1));
    }

    #[test]
    fn test_iterator() {
        let mut set = SquareSet::new();
        set.insert(Square::A1);
        set.insert(Square::E4);
        set.insert(Square::H8);

        let squares: Vec<Square> = set.into_iter().collect();
        assert_eq!(squares.len(), 3);

        // Iterator returns squares in ascending order (due to trailing_zeros)
        assert_eq!(squares[0], Square::A1);
        assert_eq!(squares[1], Square::E4);
        assert_eq!(squares[2], Square::H8);
    }

    #[test]
    fn test_path() {
        // Horizontal path
        let path = SquareSet::make_path(Square::A1, Square::H1);
        assert_eq!(path.len(), 6); // Excludes starting square
        assert!(!path.contains(Square::A1));
        assert!(path.contains(Square::B1));
        assert!(path.contains(Square::G1));
        assert!(!path.contains(Square::H1));

        // Diagonal path
        let path = SquareSet::make_path(Square::A1, Square::H8);
        assert_eq!(path.len(), 6);
        assert!(!path.contains(Square::A1));
        assert!(path.contains(Square::B2));
        assert!(path.contains(Square::G7));
        assert!(!path.contains(Square::H8));

        // Invalid path (knight move)
        let path = SquareSet::make_path(Square::A1, Square::B3);
        assert!(path.is_empty());
    }

    #[test]
    fn test_occupancy() {
        let mut board = Board::new();
        board[Square::A1] = Piece::R;
        board[Square::E1] = Piece::K;
        board[Square::A8] = Piece::r;
        board[Square::E8] = Piece::k;

        let all_occupied = occupancy(&board);
        assert_eq!(all_occupied.len(), 4);
        assert!(all_occupied.contains(Square::A1));
        assert!(all_occupied.contains(Square::E1));
        assert!(all_occupied.contains(Square::A8));
        assert!(all_occupied.contains(Square::E8));

        let white_occupied = occupancy_by_color(&board, Color::White);
        assert_eq!(white_occupied.len(), 2);
        assert!(white_occupied.contains(Square::A1));
        assert!(white_occupied.contains(Square::E1));
        assert!(!white_occupied.contains(Square::A8));

        let black_occupied = occupancy_by_color(&board, Color::Black);
        assert_eq!(black_occupied.len(), 2);
        assert!(black_occupied.contains(Square::A8));
        assert!(black_occupied.contains(Square::E8));
        assert!(!black_occupied.contains(Square::A1));
    }

    #[test]
    fn test_find_piece() {
        let mut board = Board::new();
        board[Square::A1] = Piece::R;
        board[Square::H1] = Piece::R;
        board[Square::E1] = Piece::K;

        let rooks = find_piece(&board, Piece::R);
        assert_eq!(rooks.len(), 2);
        assert!(rooks.contains(Square::A1));
        assert!(rooks.contains(Square::H1));
        assert!(!rooks.contains(Square::E1));

        let kings = find_piece(&board, Piece::K);
        assert_eq!(kings.len(), 1);
        assert!(kings.contains(Square::E1));
    }
}
