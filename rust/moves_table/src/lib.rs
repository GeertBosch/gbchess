use fen::{Piece, Square};
use square_set::SquareSet;

/// The MovesTable holds precomputed move data for all pieces
pub struct MovesTable {
    // Precomputed possible moves for each piece type on each square
    moves: [[SquareSet; 64]; 13], // [piece][square]

    // Precomputed possible captures for each piece type on each square
    captures: [[SquareSet; 64]; 13], // [piece][square]

    // Precomputed possible squares that each square can be attacked from
    attackers: [SquareSet; 64], // [square]

    // Precomputed paths between squares
    paths: [[SquareSet; 64]; 64], // [from][to]
}

impl MovesTable {
    /** Initialize all precomputed move tables. */
    pub fn new() -> Self {
        let mut table = MovesTable {
            moves: [[SquareSet::new(); 64]; 13],
            captures: [[SquareSet::new(); 64]; 13],
            attackers: [SquareSet::new(); 64],
            paths: [[SquareSet::new(); 64]; 64],
        };

        table.initialize_piece_moves_and_captures();
        table.initialize_attackers();
        table.initialize_paths();

        table
    }

    pub fn possible_moves(&self, piece: Piece, square: Square) -> SquareSet {
        self.moves[piece.index()][square as usize]
    }

    pub fn possible_captures(&self, piece: Piece, square: Square) -> SquareSet {
        self.captures[piece.index()][square as usize]
    }

    pub fn attackers(&self, square: Square) -> SquareSet {
        self.attackers[square as usize]
    }

    pub fn path(&self, from: Square, to: Square) -> SquareSet {
        self.paths[from as usize][to as usize]
    }

    fn initialize_piece_moves_and_captures(&mut self) {
        for piece_idx in 0..13 {
            let piece = unsafe { std::mem::transmute(piece_idx as u8) };
            for square_idx in 0..64 {
                let square = unsafe { std::mem::transmute(square_idx as u8) };
                self.moves[piece_idx][square_idx] = Self::compute_possible_moves(piece, square);
                self.captures[piece_idx][square_idx] =
                    Self::compute_possible_captures(piece, square);
            }
        }
    }

    fn initialize_attackers(&mut self) {
        for from_idx in 0..64 {
            let from = unsafe { std::mem::transmute(from_idx as u8) };
            let mut all_targets = SquareSet::new();

            // Gather all possible squares that can be attacked by any piece from this square
            for piece_idx in 0..13 {
                all_targets = all_targets | self.captures[piece_idx][from_idx];
            }

            // Distribute attackers over the target squares
            for to in all_targets.iter() {
                self.attackers[to as usize] =
                    self.attackers[to as usize] | SquareSet::from_square(from);
            }
        }
    }

    fn initialize_paths(&mut self) {
        for from_idx in 0..64 {
            for to_idx in 0..64 {
                let from = unsafe { std::mem::transmute(from_idx as u8) };
                let to = unsafe { std::mem::transmute(to_idx as u8) };
                self.paths[from_idx][to_idx] = SquareSet::make_path(from, to);
            }
        }
    }

    fn compute_possible_moves(piece: Piece, from: Square) -> SquareSet {
        match piece {
            Piece::P => Self::white_pawn_moves(from),
            Piece::p => Self::black_pawn_moves(from),
            Piece::N | Piece::n => Self::knight_moves(from),
            Piece::B | Piece::b => Self::bishop_moves(from),
            Piece::R | Piece::r => Self::rook_moves(from),
            Piece::Q | Piece::q => Self::queen_moves(from),
            Piece::K | Piece::k => Self::king_moves(from),
            Piece::Empty => SquareSet::new(),
        }
    }

    fn compute_possible_captures(piece: Piece, from: Square) -> SquareSet {
        match piece {
            Piece::P => Self::white_pawn_attacks(from),
            Piece::p => Self::black_pawn_attacks(from),
            Piece::N | Piece::n => Self::knight_moves(from),
            Piece::B | Piece::b => Self::bishop_moves(from),
            Piece::R | Piece::r => Self::rook_moves(from),
            Piece::Q | Piece::q => Self::queen_moves(from),
            Piece::K | Piece::k => Self::king_moves(from),
            Piece::Empty => SquareSet::new(),
        }
    }

    fn white_pawn_moves(from: Square) -> SquareSet {
        let mut moves = SquareSet::new();
        let file = from.file();
        let rank = from.rank();

        if rank < 7 {
            moves = moves | SquareSet::from_square(Square::make_square(file, rank + 1));
            if rank == 1 {
                moves = moves | SquareSet::from_square(Square::make_square(file, rank + 2));
            }
        }

        moves
    }

    fn black_pawn_moves(from: Square) -> SquareSet {
        let mut moves = SquareSet::new();
        let file = from.file();
        let rank = from.rank();

        if rank > 0 {
            moves = moves | SquareSet::from_square(Square::make_square(file, rank - 1));
            if rank == 6 {
                moves = moves | SquareSet::from_square(Square::make_square(file, rank - 2));
            }
        }

        moves
    }

    fn white_pawn_attacks(from: Square) -> SquareSet {
        let mut attacks = SquareSet::new();
        let file = from.file();
        let rank = from.rank();

        if rank < 7 {
            if file > 0 {
                attacks = attacks | SquareSet::from_square(Square::make_square(file - 1, rank + 1));
            }
            if file < 7 {
                attacks = attacks | SquareSet::from_square(Square::make_square(file + 1, rank + 1));
            }
        }

        attacks
    }

    fn black_pawn_attacks(from: Square) -> SquareSet {
        let mut attacks = SquareSet::new();
        let file = from.file();
        let rank = from.rank();

        if rank > 0 {
            if file > 0 {
                attacks = attacks | SquareSet::from_square(Square::make_square(file - 1, rank - 1));
            }
            if file < 7 {
                attacks = attacks | SquareSet::from_square(Square::make_square(file + 1, rank - 1));
            }
        }

        attacks
    }

    fn knight_moves(from: Square) -> SquareSet {
        let deltas = [
            (-2, -1),
            (-2, 1),
            (-1, -2),
            (-1, 2),
            (1, -2),
            (1, 2),
            (2, -1),
            (2, 1),
        ];

        let mut moves = SquareSet::new();
        let file = from.file() as i32;
        let rank = from.rank() as i32;

        for &(df, dr) in &deltas {
            let new_file = file + df;
            let new_rank = rank + dr;

            if new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8 {
                moves = moves
                    | SquareSet::from_square(Square::make_square(
                        new_file as usize,
                        new_rank as usize,
                    ));
            }
        }

        moves
    }

    fn king_moves(from: Square) -> SquareSet {
        let deltas = [
            (-1, -1),
            (-1, 0),
            (-1, 1),
            (0, -1),
            (0, 1),
            (1, -1),
            (1, 0),
            (1, 1),
        ];

        let mut moves = SquareSet::new();
        let file = from.file() as i32;
        let rank = from.rank() as i32;

        for &(df, dr) in &deltas {
            let new_file = file + df;
            let new_rank = rank + dr;

            if new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8 {
                moves = moves
                    | SquareSet::from_square(Square::make_square(
                        new_file as usize,
                        new_rank as usize,
                    ));
            }
        }

        moves
    }

    fn rook_moves(from: Square) -> SquareSet {
        let mut moves = SquareSet::new();
        let file = from.file();
        let rank = from.rank();

        // Horizontal moves
        for f in 0..8 {
            if f != file {
                moves = moves | SquareSet::from_square(Square::make_square(f, rank));
            }
        }

        // Vertical moves
        for r in 0..8 {
            if r != rank {
                moves = moves | SquareSet::from_square(Square::make_square(file, r));
            }
        }

        moves
    }

    fn bishop_moves(from: Square) -> SquareSet {
        let mut moves = SquareSet::new();
        let file = from.file() as i32;
        let rank = from.rank() as i32;

        let directions = [(1, 1), (1, -1), (-1, 1), (-1, -1)];

        for &(df, dr) in &directions {
            for i in 1..8 {
                let new_file = file + df * i;
                let new_rank = rank + dr * i;

                if new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8 {
                    moves = moves
                        | SquareSet::from_square(Square::make_square(
                            new_file as usize,
                            new_rank as usize,
                        ));
                } else {
                    break;
                }
            }
        }

        moves
    }

    fn queen_moves(from: Square) -> SquareSet {
        Self::rook_moves(from) | Self::bishop_moves(from)
    }
}

impl Default for MovesTable {
    fn default() -> Self {
        Self::new()
    }
}

/// Check if path between two squares is clear
pub fn clear_path(occupancy: SquareSet, from: Square, to: Square) -> bool {
    let path = SquareSet::make_path(from, to);
    (occupancy & path).is_empty()
}

#[cfg(test)]
mod tests {
    use super::*;
    use fen::{Piece, Square};

    #[test]
    fn test_piece_moves() {
        let table = MovesTable::new();

        // Test rook moves
        let rook_moves = table.possible_moves(Piece::R, Square::A1);
        assert_eq!(rook_moves.len(), 14);
        assert!(rook_moves.contains(Square::H1));
        assert!(rook_moves.contains(Square::A8));

        // Test knight moves
        let knight_moves = table.possible_moves(Piece::N, Square::A1);
        assert_eq!(knight_moves.len(), 2);
        assert!(knight_moves.contains(Square::B3));
        assert!(knight_moves.contains(Square::C2));

        // Test king moves
        let king_moves = table.possible_moves(Piece::K, Square::A1);
        assert_eq!(king_moves.len(), 3);

        // Test white pawn moves
        let pawn_moves = table.possible_moves(Piece::P, Square::A2);
        assert_eq!(pawn_moves.len(), 2);
        assert!(pawn_moves.contains(Square::A3));
        assert!(pawn_moves.contains(Square::A4));

        // Test black pawn moves
        let black_pawn_moves = table.possible_moves(Piece::p, Square::A7);
        assert_eq!(black_pawn_moves.len(), 2);
        assert!(black_pawn_moves.contains(Square::A6));
        assert!(black_pawn_moves.contains(Square::A5));
    }

    #[test]
    fn test_piece_captures() {
        let table = MovesTable::new();

        // Test white pawn captures
        let white_pawn_attacks = table.possible_captures(Piece::P, Square::E4);
        assert_eq!(white_pawn_attacks.len(), 2);
        assert!(white_pawn_attacks.contains(Square::D5));
        assert!(white_pawn_attacks.contains(Square::F5));

        // Test black pawn captures
        let black_pawn_attacks = table.possible_captures(Piece::p, Square::E4);
        assert_eq!(black_pawn_attacks.len(), 2);
        assert!(black_pawn_attacks.contains(Square::D3));
        assert!(black_pawn_attacks.contains(Square::F3));

        // Test knight captures (same as moves)
        let knight_attacks = table.possible_captures(Piece::N, Square::E4);
        assert_eq!(knight_attacks.len(), 8);
    }

    #[test]
    fn test_paths() {
        let table = MovesTable::new();

        // Test horizontal path
        let path = table.path(Square::A1, Square::H1);
        assert_eq!(path.len(), 6); // B1, C1, D1, E1, F1, G1
        assert!(path.contains(Square::B1));
        assert!(path.contains(Square::G1));
        assert!(!path.contains(Square::A1));
        assert!(!path.contains(Square::H1));

        // Test vertical path
        let path = table.path(Square::A1, Square::A8);
        assert_eq!(path.len(), 6); // A2, A3, A4, A5, A6, A7

        // Test diagonal path
        let path = table.path(Square::A1, Square::H8);
        assert_eq!(path.len(), 6); // B2, C3, D4, E5, F6, G7
    }

    #[test]
    fn test_clear_path() {
        let empty = SquareSet::new();
        let occupied = SquareSet::from_square(Square::D1);

        // Clear path
        assert!(clear_path(empty, Square::A1, Square::H1));

        // Blocked path
        assert!(!clear_path(occupied, Square::A1, Square::H1));

        // Path that doesn't go through occupied square
        assert!(clear_path(occupied, Square::A1, Square::C1));
    }

    #[test]
    fn test_attackers() {
        let table = MovesTable::new();

        // Test that a square has the expected attackers
        let attackers = table.attackers(Square::E4);

        // Should include knight squares that can reach E4
        assert!(attackers.contains(Square::D2));
        assert!(attackers.contains(Square::F2));
        assert!(attackers.contains(Square::C3));
        assert!(attackers.contains(Square::G3));

        // Should include diagonal squares for bishops/queens
        assert!(attackers.contains(Square::A8));
        assert!(attackers.contains(Square::H1));

        // Should include horizontal/vertical squares for rooks/queens
        assert!(attackers.contains(Square::A4));
        assert!(attackers.contains(Square::H4));
        assert!(attackers.contains(Square::E1));
        assert!(attackers.contains(Square::E8));
    }
}
