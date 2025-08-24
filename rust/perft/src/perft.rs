/**
 * Perft implementation for chess position analysis.
 * 
 * This module provides functions for counting the number of possible
 * positions at a given depth (perft) and showing the breakdown by
 * root moves (perft with divide).
 */

use fen::Position;
use moves::apply_move;
use moves_gen::all_legal_moves_and_captures;

pub type NodeCount = u64;

/** 
 * Simple perft implementation without caching or incremental updates.
 * Uses apply_move with position copying for simplified state management.
 */
pub fn perft(position: Position, depth: i32) -> NodeCount {
    if depth == 0 {
        return 1;
    }

    let mut nodes = 0;
    let move_list = all_legal_moves_and_captures(position.turn, &position.board);

    for mv in move_list {
        let new_position = apply_move(position.clone(), mv);
        nodes += perft(new_position, depth - 1);
    }

    nodes
}

/**
 * Perft with divide - shows node count for each root move.
 * Returns the total number of nodes searched.
 */
pub fn perft_with_divide(position: Position, depth: i32) -> NodeCount {
    if depth == 0 {
        println!("Nodes searched: 1");
        return 1;
    }

    let mut total_nodes = 0;
    let move_list = all_legal_moves_and_captures(position.turn, &position.board);

    for mv in move_list {
        let new_position = apply_move(position.clone(), mv);
        let nodes = perft(new_position, depth - 1);

        println!("{}: {}", mv, nodes);
        total_nodes += nodes;
    }

    println!("Nodes searched: {}", total_nodes);
    total_nodes
}

#[cfg(test)]
mod tests {
    use super::*;
    use fen::{parse_position, INITIAL_POSITION};

    #[test]
    fn test_perft_depth_0() {
        let position = parse_position(INITIAL_POSITION).unwrap();
        assert_eq!(perft(position, 0), 1);
    }

    #[test]
    fn test_perft_starting_position() {
        let position = parse_position(INITIAL_POSITION).unwrap();
        
        // Test depth 1-3 (confirmed working)
        assert_eq!(perft(position.clone(), 1), 20);
        assert_eq!(perft(position.clone(), 2), 400);
        assert_eq!(perft(position.clone(), 3), 8902);
    }

    /// Test well-known perft positions from https://www.chessprogramming.org/Perft_Results
    #[test]
    fn test_known_perft_positions() {
        struct KnownPosition {
            name: &'static str,
            fen: &'static str,
            depth: i32,
            expected: u64,
        }

        let positions = [
            KnownPosition {
                name: "Kiwipete (castling, en passant)",
                fen: "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                depth: 4,
                expected: 4085603,
            },
            KnownPosition {
                name: "Position 3 (pawn promotion)",
                fen: "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                depth: 5,
                expected: 674624,
            },
            KnownPosition {
                name: "Position 4 (promotions to all pieces)",
                fen: "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                depth: 4,
                expected: 422333,
            },
            KnownPosition {
                name: "Position 4 mirrored",
                fen: "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
                depth: 4,
                expected: 422333,
            },
            KnownPosition {
                name: "Position 5 (tactical)",
                fen: "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                depth: 4,
                expected: 2103487,
            },
            KnownPosition {
                name: "Position 6 (middlegame)",
                fen: "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
                depth: 4,
                expected: 3894594,
            },
        ];

        for pos in &positions {
            let position = parse_position(pos.fen).unwrap_or_else(|e| {
                panic!("Failed to parse FEN '{}' for {}: {}", pos.fen, pos.name, e)
            });
            
            let result = perft(position, pos.depth);
            assert_eq!(
                result, pos.expected,
                "Perft mismatch for {} at depth {}: expected {}, got {}",
                pos.name, pos.depth, pos.expected, result
            );
            
            println!("✅ {} passed: {} nodes at depth {}", pos.name, result, pos.depth);
        }
    }

    #[test]
    fn test_perft_depth_1_startpos() {
        let position = parse_position(INITIAL_POSITION).unwrap();
        let result = perft(position, 1);
        assert_eq!(result, 20, "Starting position should have 20 moves at depth 1");
    }
}
