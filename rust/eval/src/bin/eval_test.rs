use eval::{compute_phase, evaluate_board, evaluate_board_simple, Score};
use fen::{
    parse_piece_placement, parse_position, position_to_string, Color, Position,
    INITIAL_PIECE_PLACEMENT,
};
use moves::{make_move_position, unmake_move_position, MoveKind};
use moves_gen::{all_legal_moves_and_captures, all_legal_quiescent_moves, SearchState};
use std::env;

fn test_score() {
    let zero = Score::from_cp(0);
    let q = Score::from_cp(-900);
    let q_cap = Score::from_cp(900);

    assert_eq!(q_cap, -q);
    assert_eq!(q_cap + q, zero);
    assert!(q < q_cap);
    assert_eq!(format!("{}", q), "-9.00");
    assert_eq!(format!("{}", q_cap), "9.00");

    let f = Score::from_cp(-123);
    let f_cap = Score::from_cp(123);

    assert_eq!(f_cap, -f);
    assert_eq!(f_cap + f, zero);
    assert_eq!(format!("{}", f), "-1.23");
    assert_eq!(format!("{}", f_cap), "1.23");

    println!("Score tests passed");
}

fn test_mate_score() {
    let m1 = Score::max();
    assert_eq!(format!("{}", m1), "M1");
    assert_eq!(m1.mate(), 1);

    let mated1 = -m1;
    assert_eq!(mated1.mate(), -1);
    assert_eq!(mated1, Score::min());
    assert_eq!(format!("{}", mated1), "-M1");

    println!("Mate score tests passed");
}

fn test_evaluate_board() {
    // Test position: "8/8/8/8/4p3/5pNN/4p3/2K1k3"
    // 2 knights vs 3 pawns
    let piece_placement = "8/8/8/8/4p3/5pNN/4p3/2K1k3";
    let board = parse_piece_placement(piece_placement).expect("Failed to parse test position");
    let score = evaluate_board_simple(&board);

    // 2 knights (600) vs 3 pawns (300) = +300 for white
    assert_eq!(score, Score::from_cp(300));

    // Test initial position
    let initial_board =
        parse_piece_placement(INITIAL_PIECE_PLACEMENT).expect("Failed to parse initial position");
    let initial_score = evaluate_board(&initial_board);
    assert_eq!(initial_score, Score::from_cp(0));

    println!("evaluateBoard tests passed");
}

/// Simplistic quiescence search for evaluation test purposes.
fn quiesce(position: &mut Position, alpha: Score, beta: Score, depth_left: i32) -> Score {
    let mut alpha = alpha;

    // Stand pat evaluation (adjust for perspective like C++)
    let mut stand_pat = evaluate_board(&position.board);
    if position.turn.active_color() == Color::Black {
        stand_pat = -stand_pat;
    }

    // Return stand_pat if depth is exhausted
    if depth_left == 0 {
        return stand_pat;
    }

    // Check if in check
    let state = SearchState::new(&position.board, position.turn);
    let in_check = state.in_check;

    // Beta cutoff only if not in check
    if stand_pat >= beta && !in_check {
        return beta;
    }

    if alpha < stand_pat {
        alpha = stand_pat;
    }

    let mut board_copy = position.board.clone();
    let move_list = all_legal_quiescent_moves(position.turn, &mut board_copy, depth_left);

    // Check for mate: empty move list while in check
    if move_list.is_empty() {
        return if in_check { Score::min() } else { stand_pat };
    }

    for mv in move_list {
        let undo = make_move_position(position, mv);

        let score = -quiesce(position, -beta, -alpha, depth_left - 1);

        unmake_move_position(position, undo);

        if score >= beta {
            return beta;
        }
        if score > alpha {
            alpha = score;
        }
    }

    alpha
}

fn print_available_moves_and_captures(position: &Position) {
    let moves = all_legal_moves_and_captures(position.turn, &position.board);

    // Separate captures from quiet moves
    let mut captures = Vec::new();
    let mut quiet_moves = Vec::new();

    for mv in moves {
        if is_capture(&mv.kind) {
            captures.push(mv);
        } else {
            quiet_moves.push(mv);
        }
    }

    // Print captures
    print!("Captures: [ ");
    for mv in captures {
        print!("{} ", mv);
    }
    println!("]");

    // Print quiet moves
    print!("Moves: [ ");
    for mv in quiet_moves {
        print!("{} ", mv);
    }
    println!("]");
}

/// Check if a move kind represents a capture
fn is_capture(kind: &MoveKind) -> bool {
    matches!(
        kind,
        MoveKind::Capture
            | MoveKind::EnPassant
            | MoveKind::KnightPromotionCapture
            | MoveKind::BishopPromotionCapture
            | MoveKind::RookPromotionCapture
            | MoveKind::QueenPromotionCapture
    )
}

fn main() {
    let args: Vec<String> = env::args().collect();

    // Run tests
    test_score();
    test_mate_score();
    test_evaluate_board();

    println!("\n*** Skipping NNUE evaluation as it is not implemented in Rust version. ***\n");

    // Default position to analyze
    let mut position = parse_position("6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1")
        .expect("Failed to parse default position");

    // Parse FEN and moves from command line (simplified version)
    if args.len() > 1 {
        // Try to parse as FEN
        if let Ok(parsed_position) = parse_position(&args[1]) {
            position = parsed_position;
            println!("Position: {}", position_to_string(&position));
        } else {
            println!("Position: {}", position_to_string(&position));
        }
    } else {
        println!("Position: {}", position_to_string(&position));
    }

    // Evaluate the board
    let simple_eval = evaluate_board_simple(&position.board);
    println!("Simple Board Evaluation: {}", simple_eval);

    let piece_square_eval = evaluate_board(&position.board);
    println!("Piece-Square Board Evaluation: {}", piece_square_eval);

    // Quiescence search evaluation
    let mut position_copy = position.clone();
    let mut quiesce_eval = quiesce(&mut position_copy, Score::min(), Score::max(), 4);
    if position.turn.active_color() == fen::Color::Black {
        quiesce_eval = -quiesce_eval;
    }
    println!("Quiescence Evaluation: {}", quiesce_eval);

    // NNUE evaluation not implemented
    println!("NNUE Evaluation: (not implemented)");

    // Print game phase
    let phase = compute_phase(&position.board);
    println!("Game phase: {}", phase);

    print_available_moves_and_captures(&position);
}
