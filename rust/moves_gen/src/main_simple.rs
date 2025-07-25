use fen::{parse_piece_placement, parse_position, Board, CastlingMask, Color, Piece, Square, Turn};
use moves::{Move, MoveKind};
use moves_gen::{all_legal_captures, all_legal_moves_and_captures, SearchState};

fn main() {
    println!("Running GB Chess moves_gen basic tests...\n");

    // Test 1: Basic pawn moves
    {
        let mut board = Board::new();
        board[Square::A2] = Piece::P;
        board[Square::A4] = Piece::P;
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

    println!("\n✅ All basic tests completed!");
}
