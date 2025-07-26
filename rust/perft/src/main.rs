/**
 * Simplified perft implementation for chess position analysis.
 *
 * This is a streamlined version of perft.cpp that:
 * - Uses 64-bit node counts instead of 128-bit
 * - Doesn't use hash table caching  
 * - Uses make_move/unmake_move instead of incremental updates
 * - Takes exactly 2 arguments: FEN position and depth
 * - Outputs moves with counts and total nodes searched
 *
 * Usage: perft-test <fen|startpos> <depth>
 * Example: perft-test startpos 3
 */
use fen::{parse_position, INITIAL_POSITION};
use perft::perft_with_divide;
use std::env;
use std::process;

fn error(message: &str) -> ! {
    eprintln!("Error: {}", message);
    process::exit(1);
}

fn usage() -> ! {
    eprintln!("Usage: perft-test <fen|startpos> <depth>");
    eprintln!("Example: perft-test startpos 3");
    process::exit(1);
}

fn run(fen: &str, depth: i32) {
    let actual_fen = if fen == "startpos" {
        INITIAL_POSITION
    } else {
        fen
    };

    match parse_position(actual_fen) {
        Ok(position) => {
            perft_with_divide(position, depth);
        }
        Err(e) => error(&e.to_string()),
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() != 3 {
        usage();
    }

    let fen = &args[1];
    let depth = match args[2].parse::<i32>() {
        Ok(d) => d,
        Err(_) => error("depth must be a valid integer"),
    };

    if depth < 0 {
        error("depth must be non-negative");
    }

    run(fen, depth);
}
