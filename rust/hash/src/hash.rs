use std::fmt;

pub const NUM_EXTRA_VECTORS: usize = 24;

// Re-export from fen crate for convenience
pub use fen::board::*;
pub use fen::types::*;

pub const NUM_BOARD_VECTORS: usize = NUM_PIECES * NUM_SQUARES;
pub const NUM_HASH_VECTORS: usize = NUM_BOARD_VECTORS + NUM_EXTRA_VECTORS;

// Using 64-bit hash values to match the C++ default (could be made configurable later)
pub type HashValue = u64;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
#[allow(dead_code)] // Will be used as moves module is migrated
pub enum MoveKind {
    // Moves that don't capture or promote
    QuietMove = 0,
    DoublePush = 1,
    CastleKingside = 2,
    CastleQueenside = 3,

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

impl MoveKind {
    pub const fn index(self) -> u8 {
        self as u8
    }

    pub const fn is_capture(self) -> bool {
        (self.index() & 4) != 0
    }

    pub const fn is_promotion(self) -> bool {
        (self.index() & 8) != 0
    }

    pub const fn is_castling(self) -> bool {
        matches!(self, MoveKind::CastleKingside | MoveKind::CastleQueenside)
    }

    pub fn promotion_piece_type(self) -> Option<PieceType> {
        if !self.is_promotion() {
            return None;
        }
        match self.index() & 3 {
            0 => Some(PieceType::Knight),
            1 => Some(PieceType::Bishop),
            2 => Some(PieceType::Rook),
            3 => Some(PieceType::Queen),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Move {
    pub from: Square,
    pub to: Square,
    pub kind: MoveKind,
}

impl Move {
    pub fn new(from: Square, to: Square, kind: MoveKind) -> Self {
        Self { from, to, kind }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct MoveWithPieces {
    pub mv: Move,
    pub piece: Piece,
    pub target: Piece,
}

impl MoveWithPieces {
    pub fn new(mv: Move, piece: Piece, target: Piece) -> Self {
        Self { mv, piece, target }
    }
}

/// Simple random number generator for hash vector initialization
/// Using xorshift algorithm for deterministic, fast random numbers
#[derive(Debug, Clone)]
pub struct XorShift {
    state: u64,
}

impl XorShift {
    pub fn new(seed: u64) -> Self {
        Self {
            state: if seed == 0 { 1 } else { seed },
        }
    }

    pub fn next(&mut self) -> u64 {
        self.state ^= self.state << 13;
        self.state ^= self.state >> 7;
        self.state ^= self.state << 17;
        self.state
    }
}

impl Default for XorShift {
    fn default() -> Self {
        Self::new(0x1234567890abcdef)
    }
}

/// Lazily initialized hash vectors using a deterministic seed
static HASH_VECTORS: std::sync::OnceLock<[HashValue; NUM_HASH_VECTORS]> =
    std::sync::OnceLock::new();

pub fn hash_vectors() -> &'static [HashValue; NUM_HASH_VECTORS] {
    HASH_VECTORS.get_or_init(|| {
        let mut vectors = [0u64; NUM_HASH_VECTORS];
        let mut rng = XorShift::default();

        for vector in &mut vectors {
            *vector = rng.next();
        }

        vectors
    })
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Hash {
    value: HashValue,
}

impl Hash {
    /// Extra vector indices for non-piece hash components
    pub const BLACK_TO_MOVE: usize = NUM_BOARD_VECTORS + 0;
    pub const CASTLING_0: usize = NUM_BOARD_VECTORS + 1; // White kingside
    pub const CASTLING_1: usize = NUM_BOARD_VECTORS + 2; // White queenside
    pub const CASTLING_2: usize = NUM_BOARD_VECTORS + 3; // Black kingside
    pub const CASTLING_3: usize = NUM_BOARD_VECTORS + 4; // Black queenside
    pub const EN_PASSANT_A: usize = NUM_BOARD_VECTORS + 16;
    pub const EN_PASSANT_B: usize = NUM_BOARD_VECTORS + 17;
    pub const EN_PASSANT_C: usize = NUM_BOARD_VECTORS + 18;
    pub const EN_PASSANT_D: usize = NUM_BOARD_VECTORS + 19;
    pub const EN_PASSANT_E: usize = NUM_BOARD_VECTORS + 20;
    pub const EN_PASSANT_F: usize = NUM_BOARD_VECTORS + 21;
    pub const EN_PASSANT_G: usize = NUM_BOARD_VECTORS + 22;
    pub const EN_PASSANT_H: usize = NUM_BOARD_VECTORS + 23;

    pub fn new() -> Self {
        Self { value: 0 }
    }

    pub fn from_position(position: &Position) -> Self {
        let mut hash = Self::new();

        // Hash all pieces on the board
        for square_idx in 0..NUM_SQUARES {
            let square = unsafe { std::mem::transmute(square_idx as u8) };
            let piece = position.board[square];
            if piece != Piece::Empty {
                hash.toggle_piece(piece, square);
            }
        }

        // Hash active color
        if position.active() == Color::Black {
            hash.toggle_vector(Self::BLACK_TO_MOVE);
        }

        // Hash castling rights
        hash.toggle_castling(position.turn.castling());

        // Hash en passant target
        let en_passant_square = position.turn.en_passant();
        if en_passant_square != NO_EN_PASSANT_TARGET {
            let file_idx = en_passant_square.file();
            hash.toggle_vector(Self::EN_PASSANT_A + file_idx);
        }

        hash
    }

    pub fn value(self) -> HashValue {
        self.value
    }

    pub fn toggle_piece(&mut self, piece: Piece, square: Square) {
        debug_assert!(piece != Piece::Empty);
        let vector_idx = piece.index() * NUM_SQUARES + (square as usize);
        self.toggle_vector(vector_idx);
    }

    pub fn toggle_vector(&mut self, vector_idx: usize) {
        debug_assert!(vector_idx < NUM_HASH_VECTORS);
        self.value ^= hash_vectors()[vector_idx];
    }

    pub fn toggle_castling(&mut self, mask: CastlingMask) {
        if mask.has_white_kingside() {
            self.toggle_vector(Self::CASTLING_0);
        }
        if mask.has_white_queenside() {
            self.toggle_vector(Self::CASTLING_1);
        }
        if mask.has_black_kingside() {
            self.toggle_vector(Self::CASTLING_2);
        }
        if mask.has_black_queenside() {
            self.toggle_vector(Self::CASTLING_3);
        }
    }

    pub fn move_piece(&mut self, piece: Piece, from: Square, to: Square) {
        self.toggle_piece(piece, from);
        self.toggle_piece(piece, to);
    }

    pub fn capture_piece(&mut self, piece: Piece, target: Piece, from: Square, to: Square) {
        self.toggle_piece(piece, from);
        self.toggle_piece(target, to);
        self.toggle_piece(piece, to);
    }

    /// Create a hash for a null move (flip side to move and clear en passant)
    pub fn make_null_move(&self, turn: &Turn) -> Self {
        let mut result = *self;
        result.toggle_vector(Self::BLACK_TO_MOVE);

        // Clear en passant hash if it was set
        let en_passant_square = turn.en_passant();
        if en_passant_square != NO_EN_PASSANT_TARGET {
            let file_idx = en_passant_square.file();
            result.toggle_vector(Self::EN_PASSANT_A + file_idx);
        }

        result
    }
}

impl Default for Hash {
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Display for Hash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:016x}", self.value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hash_creation() {
        let hash = Hash::new();
        assert_eq!(hash.value(), 0);
    }

    #[test]
    fn test_hash_position() {
        let position = Position::new(); // Starting position
        let hash = Hash::from_position(&position);

        // Hash should be non-zero for starting position
        assert_ne!(hash.value(), 0);
    }

    #[test]
    fn test_hash_toggle() {
        let mut hash = Hash::new();
        let original_value = hash.value();

        // Toggle twice should return to original
        hash.toggle_piece(Piece::P, Square::E4);
        assert_ne!(hash.value(), original_value);

        hash.toggle_piece(Piece::P, Square::E4);
        assert_eq!(hash.value(), original_value);
    }

    #[test]
    fn test_hash_reproducible() {
        let position1 = Position::new();
        let position2 = Position::new();

        let hash1 = Hash::from_position(&position1);
        let hash2 = Hash::from_position(&position2);

        assert_eq!(hash1, hash2);
    }

    #[test]
    fn test_move_kind_properties() {
        assert!(MoveKind::Capture.is_capture());
        assert!(!MoveKind::QuietMove.is_capture());

        assert!(MoveKind::QueenPromotion.is_promotion());
        assert!(!MoveKind::QuietMove.is_promotion());

        assert!(MoveKind::CastleKingside.is_castling());
        assert!(!MoveKind::QuietMove.is_castling());

        assert_eq!(
            MoveKind::QueenPromotion.promotion_piece_type(),
            Some(PieceType::Queen)
        );
        assert_eq!(MoveKind::QuietMove.promotion_piece_type(), None);
    }
}
