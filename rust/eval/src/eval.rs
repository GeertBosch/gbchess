use crate::{Score, EvalTable};
use fen::{Board, Color, Piece, Square, NUM_SQUARES};

/// Evaluate the board using simple piece values (no piece-square tables)
pub fn evaluate_board_simple(board: &Board) -> Score {
    let table = EvalTable::new();
    evaluate_board_with_table(board, &table)
}

/// Evaluate the board using piece-square tables
pub fn evaluate_board(board: &Board) -> Score {
    let table = EvalTable::with_piece_square_tables(board);
    evaluate_board_with_table(board, &table)
}

/// Evaluate the board using the provided evaluation table
pub fn evaluate_board_with_table(board: &Board, table: &EvalTable) -> Score {
    let mut value = Score::from_cp(0);
    
    for square in 0..NUM_SQUARES {
        let square_enum = Square::from_int(square);
        let piece = board[square_enum];
        if piece != Piece::Empty {
            value += table.get_score(piece, square_enum);
        }
    }
    
    value
}

/// Evaluate the board from the perspective of the given player
/// Returns positive scores for advantage to the given player
pub fn evaluate_board_for_player(board: &Board, player: Color) -> Score {
    let score = evaluate_board(board);
    match player {
        Color::White => score,
        Color::Black => -score,
    }
}

/// Evaluate the board (simple) from the perspective of the given player
pub fn evaluate_board_simple_for_player(board: &Board, player: Color) -> Score {
    let score = evaluate_board_simple(board);
    match player {
        Color::White => score,
        Color::Black => -score,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use fen::{parse_piece_placement, INITIAL_PIECE_PLACEMENT};
    
    #[test]
    fn test_initial_position_evaluation() {
        let board = parse_piece_placement(INITIAL_PIECE_PLACEMENT).unwrap();
        let score = evaluate_board(&board);
        // Initial position should be roughly equal
        assert_eq!(score.cp(), 0);
    }
    
    #[test]
    fn test_simple_evaluation() {
        // Test position: "8/8/8/8/4p3/5pNN/4p3/2K1k3"
        // 2 knights vs 3 pawns = 600 vs 300 = +300 for white
        let mut board = Board::new();
        
        // Place pieces
        board[Square::C1] = Piece::K; // White king
        board[Square::E1] = Piece::k; // Black king
        board[Square::G3] = Piece::N; // White knight
        board[Square::H3] = Piece::N; // White knight
        board[Square::E4] = Piece::p; // Black pawn
        board[Square::F3] = Piece::p; // Black pawn
        board[Square::E2] = Piece::p; // Black pawn
        
        let score = evaluate_board_simple(&board);
        assert_eq!(score.cp(), 300); // 2 knights (600) vs 3 pawns (300)
    }
}
