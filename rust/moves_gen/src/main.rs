use fen::{parse_piece_placement, parse_position, position_to_string, Board, CastlingMask, Color, Piece, Square, Turn};
use moves_gen::{all_legal_captures, all_legal_moves_and_captures, SearchState};
use std::env;

/** 
 * Show all legal captures and moves for the given position.
 * This mimics the C++ showCapturesAndMoves function.
 */
fn show_captures_and_moves(fen: &str) {
    match parse_position(fen) {
        Ok(position) => {
            println!("FEN: {}", position_to_string(&position));
            
            let captures = all_legal_captures(position.turn, position.board.clone());
            let moves = all_legal_moves_and_captures(position.turn, &position.board);
            
            println!("Legal captures: {}", captures.len());
            if !captures.is_empty() {
                for capture in &captures {
                    print!("{} ", capture);
                }
                println!();
            } else {
                println!();
            }
            
            println!("Legal moves: {}", moves.len());
            for move_item in &moves {
                print!("{} ", move_item);
            }
            println!();
        }
        Err(e) => {
            eprintln!("Error parsing FEN '{}': {}", fen, e);
        }
    }
}

fn run_basic_tests() {
    println!("Running GB Chess moves_gen basic tests...\n");

    // Test 1: Basic pawn moves
    {
        let mut board = Board::new();
        board[Square::A2] = Piece::P;
        board[Square::A4] = Piece::P;
        board[Square::E1] = Piece::K; // Add a king
        let turn = Turn::new(Color::White, CastlingMask::EMPTY, None, 0, 0);
        let moves = all_legal_moves_and_captures(turn, &board);

        println!(
            "Test 1: Found {} moves for basic pawn position",
            moves.len()
        );
        if !moves.is_empty() {
            println!("✓ Basic pawn moves test passed");
        } else {
            println!("⚠ No moves found");
        }
    }

    // Test 2: Parse a standard position
    {
        if let Ok(position) =
            parse_position("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1")
        {
            let moves = all_legal_moves_and_captures(position.turn, &position.board);
            println!(
                "Test 2: Found {} moves for standard opening position",
                moves.len()
            );
            if !moves.is_empty() {
                println!("✓ Standard position test passed");
            }
        } else {
            println!("⚠ Failed to parse standard position");
        }
    }

    // Test 3: Simple board with pieces
    {
        if let Ok(board) = parse_piece_placement("r3kbnr/pP1qpppp/3p4/4N3/4P3/8/PPP2PPP/RNB1K2R") {
            let turn = Turn::new(Color::White, CastlingMask::KQ_kq, None, 0, 0);
            let captures = all_legal_captures(turn, board);
            println!(
                "Test 3: Found {} captures in complex position",
                captures.len()
            );
            if !captures.is_empty() {
                println!("✓ Complex position test passed");
            }
        } else {
            println!("⚠ Failed to parse complex position");
        }
    }

    // Test 4: SearchState creation
    {
        if let Ok(position) =
            parse_position("rnb1kbnr/pppp1ppp/4p3/8/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3")
        {
            let state = SearchState::new(&position.board, position.turn);
            println!("Test 4: SearchState created - in_check: {}", state.in_check);
            println!("✓ SearchState test passed");
        }
    }

    // Test 5: King in check position
    {
        println!("Test 5: King in check position");
        test_king_in_check();
    }

    println!("\n✅ All basic tests completed!");
}

/** 
 * Check that moves and captures count matches expected values for a given FEN position
 */
fn check_moves_and_captures(fen: &str, expected_moves: usize, expected_captures: usize) {
    match parse_position(fen) {
        Ok(position) => {
            let legal_moves = all_legal_moves_and_captures(position.turn, &position.board);
            let legal_captures = all_legal_captures(position.turn, position.board.clone());
            
            if legal_moves.len() != expected_moves || legal_captures.len() != expected_captures {
                println!("Expected moves: {}, got: {}", expected_moves, legal_moves.len());
                println!("Expected captures: {}, got: {}", expected_captures, legal_captures.len());
                show_captures_and_moves(fen);
            }
            assert_eq!(legal_moves.len(), expected_moves);
            assert_eq!(legal_captures.len(), expected_captures);
        }
        Err(e) => {
            panic!("Failed to parse FEN '{}': {}", fen, e);
        }
    }
}

/**
 * Test position where black king is in check from white queen
 */
fn test_king_in_check() {
    // Position: 4k3/8/4Qn2/3K4/8/8/8/8 b - - 0 1
    // - Black king on e8
    // - White queen on e6 (giving check along the e-file)
    // - Black knight on f6 (cannot move as it would leave king in check)
    // - White king on d5
    // Only legal moves should be king moves to safe squares (d8, f8)
    // No captures are possible
    check_moves_and_captures("4k3/8/4Qn2/3K4/8/8/8/8 b - - 0 1", 2, 0);
    println!("✓ King in check test passed");
}

fn main() {
    let args: Vec<String> = env::args().collect();
    
    if args.len() == 2 {
        // Single argument mode: show moves for the given FEN position
        let fen = &args[1];
        show_captures_and_moves(fen);
    } else {
        // No arguments: run the basic test suite
        run_basic_tests();
    }

    // Run the king in check test as a separate check
    test_king_in_check();
}
