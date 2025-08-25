use crate::Score;
use fen::{Board, Color, Piece, PieceType, Square, NUM_SQUARES};

pub type SquareTable = [Score; NUM_SQUARES];
pub type PieceValueTable = [Score; 5]; // Pawn, Knight, Bishop, Rook, Queen (no King)
pub type PieceSquareTable = [SquareTable; 13]; // One for each piece type including Empty

/// Reverse the ranks in the table and negate the scores
/// This allows tables made from white's perspective to be used for black
pub fn flip_table(table: &mut SquareTable) {
    // Flip the table vertically (king side remains king side, queen side remains queen side)
    for rank in 0..4 {
        for file in 0..8 {
            let sq1 = rank * 8 + file;
            let sq2 = (7 - rank) * 8 + file;
            table.swap(sq1, sq2);
        }
    }
    // Invert the values
    for score in table.iter_mut() {
        *score = -*score;
    }
}

/// Add a score to all elements in a square table
pub fn add_score_to_table(table: &mut SquareTable, score: Score) {
    for square_score in table.iter_mut() {
        *square_score += score;
    }
}

/// Add two square tables element-wise
pub fn add_tables(lhs: &SquareTable, rhs: &SquareTable) -> SquareTable {
    let mut result = *lhs;
    for (i, score) in result.iter_mut().enumerate() {
        *score += rhs[i];
    }
    result
}

/// Multiply a square table by a score
pub fn multiply_table(table: &SquareTable, score: Score) -> SquareTable {
    let mut result = *table;
    for square_score in result.iter_mut() {
        *square_score *= score;
    }
    result
}

/// Game phase computation
pub struct GamePhase {
    pub phase: u8, // ranges from 7 (opening) down to 0 (endgame)
}

impl GamePhase {
    const OPENING: u8 = 7;
    #[allow(dead_code)]
    const ENDGAME: u8 = 0;
    
    const WEIGHTS: [Score; 8] = [
        Score::from_cp(0),    // Endgame
        Score::from_cp(14),   // 1
        Score::from_cp(28),   // 2
        Score::from_cp(42),   // 3
        Score::from_cp(58),   // 4
        Score::from_cp(72),   // 5
        Score::from_cp(86),   // 6
        Score::from_cp(100),  // Opening
    ];
    
    pub fn new(board: &Board) -> Self {
        let piece_values = Self::get_piece_values();
        let mut material = [0, 0]; // per color, in pawns
        
        for square in 0..NUM_SQUARES {
            let piece = board[Square::from_int(square)];
            if piece == Piece::Empty {
                continue;
            }
            
            let piece_type = piece.piece_type();
            if piece_type == PieceType::Empty || piece_type == PieceType::King {
                continue;
            }
            
            let val = piece_values[piece_type as usize].pawns();
            match piece.color() {
                Color::White => material[1] += val,
                Color::Black => material[0] += val,
            }
        }
        
        let phase = ((material[0].max(material[1]) - 10) / 2).clamp(0, Self::OPENING as i16) as u8;
        Self { phase }
    }
    
    /// Interpolate between opening and endgame tables based on game phase
    pub fn interpolate(&self, opening: &SquareTable, endgame: &SquareTable) -> SquareTable {
        let opening_weight = Self::WEIGHTS[self.phase as usize];
        let endgame_weight = Score::from_cp(100) - opening_weight;
        
        let mut result = multiply_table(opening, opening_weight);
        let endgame_scaled = multiply_table(endgame, endgame_weight);
        
        for (i, score) in result.iter_mut().enumerate() {
            *score += endgame_scaled[i];
        }
        
        result
    }
    
    fn get_piece_values() -> PieceValueTable {
        [
            Score::from_cp(100), // Pawn
            Score::from_cp(300), // Knight
            Score::from_cp(300), // Bishop
            Score::from_cp(500), // Rook
            Score::from_cp(900), // Queen
        ]
    }
}

/// Compute the game phase for a given board
pub fn compute_phase(board: &Board) -> u8 {
    GamePhase::new(board).phase
}

/// Bill Jordan's piece-square tables
pub mod bill_jordan {
    use super::*;
    
    pub const PIECE_VALUES: PieceValueTable = [
        Score::from_cp(100), // Pawn
        Score::from_cp(300), // Knight  
        Score::from_cp(300), // Bishop
        Score::from_cp(500), // Rook
        Score::from_cp(900), // Queen
    ];
    
    // Piece-square tables from Bill Jordan
    pub const PAWN: SquareTable = [
        // Rank 1
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        // Rank 2
        Score::from_cp(0), Score::from_cp(2), Score::from_cp(4), Score::from_cp(-12),
        Score::from_cp(-12), Score::from_cp(4), Score::from_cp(2), Score::from_cp(0),
        // Rank 3
        Score::from_cp(0), Score::from_cp(2), Score::from_cp(4), Score::from_cp(4),
        Score::from_cp(4), Score::from_cp(4), Score::from_cp(2), Score::from_cp(0),
        // Rank 4
        Score::from_cp(0), Score::from_cp(2), Score::from_cp(4), Score::from_cp(8),
        Score::from_cp(8), Score::from_cp(4), Score::from_cp(2), Score::from_cp(0),
        // Rank 5
        Score::from_cp(0), Score::from_cp(2), Score::from_cp(4), Score::from_cp(8),
        Score::from_cp(8), Score::from_cp(4), Score::from_cp(2), Score::from_cp(0),
        // Rank 6
        Score::from_cp(4), Score::from_cp(8), Score::from_cp(10), Score::from_cp(16),
        Score::from_cp(16), Score::from_cp(10), Score::from_cp(8), Score::from_cp(4),
        // Rank 7
        Score::from_cp(100), Score::from_cp(100), Score::from_cp(100), Score::from_cp(100),
        Score::from_cp(100), Score::from_cp(100), Score::from_cp(100), Score::from_cp(100),
        // Rank 8
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
    ];
    
    pub const KNIGHT: SquareTable = [
        // Rank 1
        Score::from_cp(-30), Score::from_cp(-20), Score::from_cp(-10), Score::from_cp(-8),
        Score::from_cp(-8), Score::from_cp(-10), Score::from_cp(-20), Score::from_cp(-30),
        // Rank 2
        Score::from_cp(-16), Score::from_cp(-6), Score::from_cp(-2), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(-2), Score::from_cp(-6), Score::from_cp(-16),
        // Rank 3
        Score::from_cp(-8), Score::from_cp(-2), Score::from_cp(4), Score::from_cp(6),
        Score::from_cp(6), Score::from_cp(4), Score::from_cp(-2), Score::from_cp(-8),
        // Rank 4
        Score::from_cp(-5), Score::from_cp(0), Score::from_cp(6), Score::from_cp(8),
        Score::from_cp(8), Score::from_cp(6), Score::from_cp(0), Score::from_cp(-5),
        // Rank 5
        Score::from_cp(-5), Score::from_cp(0), Score::from_cp(6), Score::from_cp(8),
        Score::from_cp(8), Score::from_cp(6), Score::from_cp(0), Score::from_cp(-5),
        // Rank 6
        Score::from_cp(-10), Score::from_cp(-2), Score::from_cp(4), Score::from_cp(6),
        Score::from_cp(6), Score::from_cp(4), Score::from_cp(-2), Score::from_cp(-10),
        // Rank 7
        Score::from_cp(-20), Score::from_cp(-10), Score::from_cp(-2), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(-2), Score::from_cp(-10), Score::from_cp(-20),
        // Rank 8
        Score::from_cp(-150), Score::from_cp(-20), Score::from_cp(-10), Score::from_cp(-5),
        Score::from_cp(-5), Score::from_cp(-10), Score::from_cp(-20), Score::from_cp(-150),
    ];
    
    pub const BISHOP: SquareTable = [
        // Rank 1
        Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(-12), Score::from_cp(-10),
        Score::from_cp(-10), Score::from_cp(-12), Score::from_cp(-10), Score::from_cp(-10),
        // Rank 2
        Score::from_cp(0), Score::from_cp(4), Score::from_cp(4), Score::from_cp(4),
        Score::from_cp(4), Score::from_cp(4), Score::from_cp(4), Score::from_cp(0),
        // Rank 3
        Score::from_cp(2), Score::from_cp(4), Score::from_cp(6), Score::from_cp(6),
        Score::from_cp(6), Score::from_cp(6), Score::from_cp(4), Score::from_cp(2),
        // Rank 4
        Score::from_cp(2), Score::from_cp(4), Score::from_cp(6), Score::from_cp(8),
        Score::from_cp(8), Score::from_cp(6), Score::from_cp(4), Score::from_cp(2),
        // Rank 5
        Score::from_cp(2), Score::from_cp(4), Score::from_cp(6), Score::from_cp(8),
        Score::from_cp(8), Score::from_cp(6), Score::from_cp(4), Score::from_cp(2),
        // Rank 6
        Score::from_cp(2), Score::from_cp(4), Score::from_cp(6), Score::from_cp(6),
        Score::from_cp(6), Score::from_cp(6), Score::from_cp(4), Score::from_cp(2),
        // Rank 7
        Score::from_cp(-10), Score::from_cp(4), Score::from_cp(4), Score::from_cp(4),
        Score::from_cp(4), Score::from_cp(4), Score::from_cp(4), Score::from_cp(-10),
        // Rank 8
        Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(-10),
        Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(-10),
    ];
    
    pub const ROOK: SquareTable = [
        // Rank 1
        Score::from_cp(4), Score::from_cp(4), Score::from_cp(4), Score::from_cp(6),
        Score::from_cp(6), Score::from_cp(4), Score::from_cp(4), Score::from_cp(4),
        // Rank 2
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        // Rank 3
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        // Rank 4
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        // Rank 5
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        // Rank 6
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        // Rank 7
        Score::from_cp(20), Score::from_cp(20), Score::from_cp(20), Score::from_cp(20),
        Score::from_cp(20), Score::from_cp(20), Score::from_cp(20), Score::from_cp(20),
        // Rank 8
        Score::from_cp(10), Score::from_cp(10), Score::from_cp(10), Score::from_cp(10),
        Score::from_cp(10), Score::from_cp(10), Score::from_cp(10), Score::from_cp(10),
    ];
    
    pub const QUEEN: SquareTable = [
        // Rank 1
        Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(-6), Score::from_cp(-4),
        Score::from_cp(-4), Score::from_cp(-6), Score::from_cp(-10), Score::from_cp(-10),
        // Rank 2
        Score::from_cp(-10), Score::from_cp(2), Score::from_cp(2), Score::from_cp(2),
        Score::from_cp(2), Score::from_cp(2), Score::from_cp(2), Score::from_cp(-10),
        // Rank 3
        Score::from_cp(2), Score::from_cp(2), Score::from_cp(2), Score::from_cp(3),
        Score::from_cp(3), Score::from_cp(2), Score::from_cp(2), Score::from_cp(2),
        // Rank 4
        Score::from_cp(2), Score::from_cp(2), Score::from_cp(3), Score::from_cp(4),
        Score::from_cp(4), Score::from_cp(3), Score::from_cp(2), Score::from_cp(2),
        // Rank 5
        Score::from_cp(2), Score::from_cp(2), Score::from_cp(2), Score::from_cp(3),
        Score::from_cp(3), Score::from_cp(2), Score::from_cp(2), Score::from_cp(2),
        // Rank 6
        Score::from_cp(-10), Score::from_cp(2), Score::from_cp(2), Score::from_cp(2),
        Score::from_cp(2), Score::from_cp(2), Score::from_cp(2), Score::from_cp(-10),
        // Rank 7
        Score::from_cp(-10), Score::from_cp(-10), Score::from_cp(2), Score::from_cp(2),
        Score::from_cp(2), Score::from_cp(2), Score::from_cp(-10), Score::from_cp(-10),
        // Rank 8
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
        Score::from_cp(0), Score::from_cp(0), Score::from_cp(0), Score::from_cp(0),
    ];
    
    pub const KING_MIDDLEGAME: SquareTable = [
        // Rank 1
        Score::from_cp(20), Score::from_cp(20), Score::from_cp(20), Score::from_cp(-40),
        Score::from_cp(10), Score::from_cp(-60), Score::from_cp(20), Score::from_cp(20),
        // Rank 2
        Score::from_cp(15), Score::from_cp(2), Score::from_cp(-25), Score::from_cp(-30),
        Score::from_cp(-30), Score::from_cp(-45), Score::from_cp(20), Score::from_cp(15),
        // Rank 3
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        // Rank 4
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        // Rank 5
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        // Rank 6
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        // Rank 7
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        // Rank 8
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
        Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48), Score::from_cp(-48),
    ];
    
    pub const KING_ENDGAME: SquareTable = [
        // Rank 1
        Score::from_cp(0), Score::from_cp(8), Score::from_cp(16), Score::from_cp(18),
        Score::from_cp(18), Score::from_cp(16), Score::from_cp(8), Score::from_cp(0),
        // Rank 2
        Score::from_cp(8), Score::from_cp(16), Score::from_cp(24), Score::from_cp(32),
        Score::from_cp(32), Score::from_cp(24), Score::from_cp(16), Score::from_cp(8),
        // Rank 3
        Score::from_cp(16), Score::from_cp(24), Score::from_cp(32), Score::from_cp(40),
        Score::from_cp(40), Score::from_cp(32), Score::from_cp(24), Score::from_cp(16),
        // Rank 4
        Score::from_cp(25), Score::from_cp(32), Score::from_cp(40), Score::from_cp(48),
        Score::from_cp(48), Score::from_cp(40), Score::from_cp(32), Score::from_cp(25),
        // Rank 5
        Score::from_cp(25), Score::from_cp(32), Score::from_cp(40), Score::from_cp(48),
        Score::from_cp(48), Score::from_cp(40), Score::from_cp(32), Score::from_cp(25),
        // Rank 6
        Score::from_cp(16), Score::from_cp(24), Score::from_cp(32), Score::from_cp(40),
        Score::from_cp(40), Score::from_cp(32), Score::from_cp(24), Score::from_cp(16),
        // Rank 7
        Score::from_cp(8), Score::from_cp(16), Score::from_cp(24), Score::from_cp(32),
        Score::from_cp(32), Score::from_cp(24), Score::from_cp(16), Score::from_cp(8),
        // Rank 8
        Score::from_cp(0), Score::from_cp(8), Score::from_cp(16), Score::from_cp(18),
        Score::from_cp(18), Score::from_cp(16), Score::from_cp(8), Score::from_cp(0),
    ];
}

/// Evaluation table that combines piece values and piece-square tables
pub struct EvalTable {
    piece_square_table: PieceSquareTable,
}

impl EvalTable {
    /// Create a new evaluation table with simple piece values (no PST)
    pub fn new() -> Self {
        let mut table = Self {
            piece_square_table: [[Score::from_cp(0); NUM_SQUARES]; 13],
        };
        
        let piece_values = [
            Score::from_cp(100), // Pawn
            Score::from_cp(300), // Knight
            Score::from_cp(300), // Bishop
            Score::from_cp(500), // Rook
            Score::from_cp(900), // Queen
            Score::from_cp(0),   // King (no material value)
        ];
        
        // Set piece values for all squares for each piece type
        for piece in 0..13 {
            if piece == Piece::Empty as usize {
                continue;
            }
            
            let piece_enum = Piece::from_index(piece);
            let piece_type = piece_enum.piece_type();
            if piece_type == PieceType::Empty {
                continue;
            }
            
            let base_value = if piece_type == PieceType::King {
                Score::from_cp(0) // King has no material value
            } else {
                piece_values[piece_type as usize]
            };
            let sign = if piece_enum.color() == Color::White { 1 } else { -1 };
            
            for square in 0..NUM_SQUARES {
                table.piece_square_table[piece][square] = Score::from_cp(base_value.cp() * sign);
            }
        }
        
        table
    }
    
    /// Create evaluation table with piece-square tables
    pub fn with_piece_square_tables(board: &Board) -> Self {
        let phase = GamePhase::new(board);
        let mut table = Self {
            piece_square_table: [[Score::from_cp(0); NUM_SQUARES]; 13],
        };
        
        // Process each piece type
        for piece in 0..13 {
            if piece == Piece::Empty as usize {
                continue;
            }
            
            let piece_enum = Piece::from_index(piece);
            let piece_type = piece_enum.piece_type();
            if piece_type == PieceType::Empty {
                continue;
            }
            
            let mut square_table = match piece_type {
                PieceType::Pawn => bill_jordan::PAWN,
                PieceType::Knight => bill_jordan::KNIGHT,
                PieceType::Bishop => bill_jordan::BISHOP,
                PieceType::Rook => bill_jordan::ROOK,
                PieceType::Queen => bill_jordan::QUEEN,
                PieceType::King => {
                    phase.interpolate(&bill_jordan::KING_MIDDLEGAME, &bill_jordan::KING_ENDGAME)
                }
                PieceType::Empty => continue,
            };
            
            // Add piece value to the table
            let piece_value = if piece_type == PieceType::King {
                Score::from_cp(0) // King has no material value
            } else {
                bill_jordan::PIECE_VALUES[piece_type as usize]
            };
            add_score_to_table(&mut square_table, piece_value);
            
            // Flip table for black pieces
            if piece_enum.color() == Color::Black {
                flip_table(&mut square_table);
            }
            
            table.piece_square_table[piece] = square_table;
        }
        
        table
    }
    
    /// Get the score for a piece on a given square
    pub fn get_score(&self, piece: Piece, square: Square) -> Score {
        self.piece_square_table[piece.index()][square as usize]
    }
}

impl Default for EvalTable {
    fn default() -> Self {
        Self::new()
    }
}
