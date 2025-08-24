use moves_table::{MovesTable, initialize_moves_table, moves_table, clear_path};
use fen::{Square, Piece};
use square_set::SquareSet;

fn main() {
    println!("Testing moves table implementation...");
    
    // Initialize the global moves table at startup
    println!("Initializing moves table...");
    let init_result = initialize_moves_table();
    println!("Initialization {}", if init_result { "successful" } else { "already done" });
    
    let table = moves_table();
    
    // Test piece moves
    test_possible_moves(table);
    
    // Test piece captures
    test_possible_captures(table);
    
    // Test path calculations
    test_paths(table);
    
    // Test attackers
    test_attackers(table);
    
    // Test utility functions
    test_utilities(table);
    
    println!("All moves table tests passed!");
}

fn test_possible_moves(table: &MovesTable) {
    println!("Testing possible moves...");
    
    // Test rook moves
    let rook_moves = table.possible_moves(Piece::R, Square::A1);
    assert_eq!(rook_moves.len(), 14, "Rook on A1 should have 14 moves");
    assert!(rook_moves.contains(Square::H1), "Rook should reach H1");
    assert!(rook_moves.contains(Square::A8), "Rook should reach A8");
    
    // Test bishop moves
    let bishop_moves = table.possible_moves(Piece::b, Square::A1);
    assert_eq!(bishop_moves.len(), 7, "Bishop on A1 should have 7 moves");
    assert!(bishop_moves.contains(Square::H8), "Bishop should reach H8");
    
    // Test knight moves
    let knight_moves = table.possible_moves(Piece::N, Square::A1);
    assert_eq!(knight_moves.len(), 2, "Knight on A1 should have 2 moves");
    assert!(knight_moves.contains(Square::C2), "Knight should reach C2");
    assert!(knight_moves.contains(Square::B3), "Knight should reach B3");
    
    // Test king moves
    let king_moves = table.possible_moves(Piece::k, Square::A1);
    assert_eq!(king_moves.len(), 3, "King on A1 should have 3 moves");
    
    // Test queen moves
    let queen_moves = table.possible_moves(Piece::Q, Square::A1);
    assert_eq!(queen_moves.len(), 21, "Queen on A1 should have 21 moves");
    
    // Test white pawn moves
    let white_pawn_moves = table.possible_moves(Piece::P, Square::A2);
    assert_eq!(white_pawn_moves.len(), 2, "White pawn on A2 should have 2 moves");
    assert!(white_pawn_moves.contains(Square::A3), "Pawn should reach A3");
    assert!(white_pawn_moves.contains(Square::A4), "Pawn should reach A4");
    
    // Test black pawn moves
    let black_pawn_moves = table.possible_moves(Piece::p, Square::A7);
    assert_eq!(black_pawn_moves.len(), 2, "Black pawn on A7 should have 2 moves");
    assert!(black_pawn_moves.contains(Square::A6), "Pawn should reach A6");
    assert!(black_pawn_moves.contains(Square::A5), "Pawn should reach A5");
    
    // Test pawn edge cases
    let white_pawn_edge = table.possible_moves(Piece::P, Square::A8);
    assert_eq!(white_pawn_edge.len(), 0, "White pawn on A8 should have no moves");
    
    let black_pawn_edge = table.possible_moves(Piece::p, Square::A1);
    assert_eq!(black_pawn_edge.len(), 0, "Black pawn on A1 should have no moves");
    
    println!("Possible moves tests passed!");
}

fn test_possible_captures(table: &MovesTable) {
    println!("Testing possible captures...");
    
    // Test knight captures (center position)
    let knight_captures = table.possible_captures(Piece::N, Square::E4);
    assert_eq!(knight_captures.len(), 8, "Knight on E4 should have 8 capture squares");
    assert!(knight_captures.contains(Square::D2), "Knight should capture D2");
    assert!(knight_captures.contains(Square::F2), "Knight should capture F2");
    assert!(knight_captures.contains(Square::C3), "Knight should capture C3");
    assert!(knight_captures.contains(Square::G3), "Knight should capture G3");
    assert!(knight_captures.contains(Square::C5), "Knight should capture C5");
    assert!(knight_captures.contains(Square::G5), "Knight should capture G5");
    assert!(knight_captures.contains(Square::D6), "Knight should capture D6");
    assert!(knight_captures.contains(Square::F6), "Knight should capture F6");
    
    // Test knight captures (edge case)
    let knight_edge = table.possible_captures(Piece::n, Square::H8);
    assert_eq!(knight_edge.len(), 2, "Knight on H8 should have 2 capture squares");
    assert!(knight_edge.contains(Square::G6), "Knight should capture G6");
    assert!(knight_edge.contains(Square::F7), "Knight should capture F7");
    
    // Test bishop captures
    let bishop_captures = table.possible_captures(Piece::B, Square::E4);
    assert!(bishop_captures.contains(Square::D3), "Bishop should capture D3");
    assert!(bishop_captures.contains(Square::C2), "Bishop should capture C2");
    assert!(bishop_captures.contains(Square::B1), "Bishop should capture B1");
    assert!(bishop_captures.contains(Square::F5), "Bishop should capture F5");
    assert!(bishop_captures.contains(Square::G6), "Bishop should capture G6");
    assert!(bishop_captures.contains(Square::H7), "Bishop should capture H7");
    
    // Test rook captures
    let rook_captures = table.possible_captures(Piece::r, Square::E4);
    assert!(rook_captures.contains(Square::A4), "Rook should capture A4");
    assert!(rook_captures.contains(Square::B4), "Rook should capture B4");
    assert!(rook_captures.contains(Square::C4), "Rook should capture C4");
    assert!(rook_captures.contains(Square::D4), "Rook should capture D4");
    assert!(rook_captures.contains(Square::F4), "Rook should capture F4");
    assert!(rook_captures.contains(Square::G4), "Rook should capture G4");
    assert!(rook_captures.contains(Square::H4), "Rook should capture H4");
    assert!(rook_captures.contains(Square::E1), "Rook should capture E1");
    assert!(rook_captures.contains(Square::E8), "Rook should capture E8");
    
    // Test queen captures (combination of rook and bishop)
    let queen_captures = table.possible_captures(Piece::Q, Square::E4);
    assert!(queen_captures.contains(Square::D3), "Queen should capture D3");
    assert!(queen_captures.contains(Square::A4), "Queen should capture A4");
    assert!(queen_captures.len() > 20, "Queen should have many capture squares");
    
    // Test king captures
    let king_captures = table.possible_captures(Piece::k, Square::E4);
    assert_eq!(king_captures.len(), 8, "King on E4 should have 8 capture squares");
    assert!(king_captures.contains(Square::D3), "King should capture D3");
    assert!(king_captures.contains(Square::E3), "King should capture E3");
    assert!(king_captures.contains(Square::F3), "King should capture F3");
    assert!(king_captures.contains(Square::D4), "King should capture D4");
    assert!(king_captures.contains(Square::F4), "King should capture F4");
    assert!(king_captures.contains(Square::D5), "King should capture D5");
    assert!(king_captures.contains(Square::E5), "King should capture E5");
    assert!(king_captures.contains(Square::F5), "King should capture F5");
    
    // Test white pawn captures
    let white_pawn_captures = table.possible_captures(Piece::P, Square::E4);
    assert_eq!(white_pawn_captures.len(), 2, "White pawn on E4 should have 2 capture squares");
    assert!(white_pawn_captures.contains(Square::D5), "White pawn should capture D5");
    assert!(white_pawn_captures.contains(Square::F5), "White pawn should capture F5");
    
    // Test black pawn captures
    let black_pawn_captures = table.possible_captures(Piece::p, Square::E4);
    assert_eq!(black_pawn_captures.len(), 2, "Black pawn on E4 should have 2 capture squares");
    assert!(black_pawn_captures.contains(Square::D3), "Black pawn should capture D3");
    assert!(black_pawn_captures.contains(Square::F3), "Black pawn should capture F3");
    
    // Test pawn captures on edge
    let white_pawn_edge = table.possible_captures(Piece::P, Square::A4);
    assert_eq!(white_pawn_edge.len(), 1, "White pawn on A4 should have 1 capture square");
    assert!(white_pawn_edge.contains(Square::B5), "White pawn should capture B5");
    
    println!("Possible captures tests passed!");
}

fn test_paths(table: &MovesTable) {
    println!("Testing paths...");
    
    // Test horizontal path
    let path_horizontal = table.path(Square::A1, Square::H1);
    assert_eq!(path_horizontal.len(), 6, "Horizontal path A1-H1 should have 6 squares");
    assert!(path_horizontal.contains(Square::B1), "Path should contain B1");
    assert!(path_horizontal.contains(Square::C1), "Path should contain C1");
    assert!(path_horizontal.contains(Square::D1), "Path should contain D1");
    assert!(path_horizontal.contains(Square::E1), "Path should contain E1");
    assert!(path_horizontal.contains(Square::F1), "Path should contain F1");
    assert!(path_horizontal.contains(Square::G1), "Path should contain G1");
    assert!(!path_horizontal.contains(Square::A1), "Path should not contain start square");
    assert!(!path_horizontal.contains(Square::H1), "Path should not contain end square");
    
    // Test vertical path
    let path_vertical = table.path(Square::A1, Square::A8);
    assert_eq!(path_vertical.len(), 6, "Vertical path A1-A8 should have 6 squares");
    assert!(path_vertical.contains(Square::A2), "Path should contain A2");
    assert!(path_vertical.contains(Square::A7), "Path should contain A7");
    assert!(!path_vertical.contains(Square::A1), "Path should not contain start square");
    assert!(!path_vertical.contains(Square::A8), "Path should not contain end square");
    
    // Test diagonal path
    let path_diagonal = table.path(Square::A1, Square::H8);
    assert_eq!(path_diagonal.len(), 6, "Diagonal path A1-H8 should have 6 squares");
    assert!(path_diagonal.contains(Square::B2), "Path should contain B2");
    assert!(path_diagonal.contains(Square::C3), "Path should contain C3");
    assert!(path_diagonal.contains(Square::D4), "Path should contain D4");
    assert!(path_diagonal.contains(Square::E5), "Path should contain E5");
    assert!(path_diagonal.contains(Square::F6), "Path should contain F6");
    assert!(path_diagonal.contains(Square::G7), "Path should contain G7");
    
    // Test shorter path
    let path_short = table.path(Square::A1, Square::C1);
    assert_eq!(path_short.len(), 1, "Short path A1-C1 should have 1 square");
    assert!(path_short.contains(Square::B1), "Path should contain B1");
    
    // Test adjacent squares (no path)
    let path_adjacent = table.path(Square::A1, Square::A2);
    assert!(path_adjacent.is_empty(), "Adjacent squares should have empty path");
    
    // Test non-aligned squares (no path)
    let path_none = table.path(Square::A1, Square::B3);
    assert!(path_none.is_empty(), "Non-aligned squares should have empty path");
    
    println!("Paths tests passed!");
}

fn test_attackers(table: &MovesTable) {
    println!("Testing attackers...");
    
    // Test attackers of a central square
    let attackers = table.attackers(Square::E4);
    
    // Knight attackers
    assert!(attackers.contains(Square::D2), "E4 should be attacked by knight on D2");
    assert!(attackers.contains(Square::F2), "E4 should be attacked by knight on F2");
    assert!(attackers.contains(Square::C3), "E4 should be attacked by knight on C3");
    assert!(attackers.contains(Square::G3), "E4 should be attacked by knight on G3");
    assert!(attackers.contains(Square::C5), "E4 should be attacked by knight on C5");
    assert!(attackers.contains(Square::G5), "E4 should be attacked by knight on G5");
    assert!(attackers.contains(Square::D6), "E4 should be attacked by knight on D6");
    assert!(attackers.contains(Square::F6), "E4 should be attacked by knight on F6");
    
    // Diagonal attackers (bishops, queens)
    assert!(attackers.contains(Square::A8), "E4 should be attacked by piece on A8");
    assert!(attackers.contains(Square::H1), "E4 should be attacked by piece on H1");
    assert!(attackers.contains(Square::B1), "E4 should be attacked by piece on B1");
    assert!(attackers.contains(Square::H7), "E4 should be attacked by piece on H7");
    
    // Horizontal/vertical attackers (rooks, queens)
    assert!(attackers.contains(Square::A4), "E4 should be attacked by piece on A4");
    assert!(attackers.contains(Square::H4), "E4 should be attacked by piece on H4");
    assert!(attackers.contains(Square::E1), "E4 should be attacked by piece on E1");
    assert!(attackers.contains(Square::E8), "E4 should be attacked by piece on E8");
    
    // King attackers
    assert!(attackers.contains(Square::D3), "E4 should be attacked by king on D3");
    assert!(attackers.contains(Square::F5), "E4 should be attacked by king on F5");
    
    // Pawn attackers
    assert!(attackers.contains(Square::D3), "E4 should be attacked by black pawn on D3");
    assert!(attackers.contains(Square::F3), "E4 should be attacked by black pawn on F3");
    assert!(attackers.contains(Square::D5), "E4 should be attacked by white pawn on D5");
    assert!(attackers.contains(Square::F5), "E4 should be attacked by white pawn on F5");
    
    println!("Attackers tests passed!");
}

fn test_utilities(_table: &MovesTable) {
    println!("Testing utility functions...");
    
    // Test clear path
    let empty = SquareSet::new();
    let mut occupied = SquareSet::new();
    occupied.insert(Square::D1);
    
    // Clear path should return true for empty board
    assert!(clear_path(empty, Square::A1, Square::H1), "Path should be clear on empty board");
    
    // Blocked path should return false
    assert!(!clear_path(occupied, Square::A1, Square::H1), "Path should be blocked");
    
    // Path that doesn't go through occupied square should be clear
    assert!(clear_path(occupied, Square::A1, Square::C1), "Partial path should be clear");
    
    // Adjacent squares should always be clear (no path between them)
    assert!(clear_path(occupied, Square::A1, Square::A2), "Adjacent squares should be clear");
    
    println!("Utility functions tests passed!");
}
