/**
 * Integration tests for magic bitboard functionality.
 *
 * This file contains tests that verify the magic attack lookup produces
 * the same results as brute force attack generation.
 */

use crate::*;

fn brute_force_rook_attacks(square: Square, occupancy: SquareSet) -> SquareSet {
    compute_rook_targets(square, occupancy)
}

fn brute_force_bishop_attacks(square: Square, occupancy: SquareSet) -> SquareSet {
    compute_bishop_targets(square, occupancy)
}

#[test]
fn test_magic_vs_brute_force() {
    println!("Testing magic bitboard lookups against brute force calculation...");
    
    let test_squares = [
        Square::A1, Square::H1, Square::A8, Square::H8, // Corners
        Square::E4, Square::D5, Square::F3, Square::C6, // Center squares
        Square::A4, Square::H5, Square::D1, Square::E8, // Edges
    ];

    let test_occupancies = [
        SquareSet::new(), // Empty board
        SquareSet::all(),  // Full board
        SquareSet::rank(3), // Middle rank
        SquareSet::file(4), // Middle file
        SquareSet::from_square(Square::E4) | SquareSet::from_square(Square::E6), // Scattered pieces
    ];

    let mut tests_run = 0;
    let mut rook_matches = 0;
    let mut bishop_matches = 0;

    for &square in &test_squares {
        for &occupancy in &test_occupancies {
            // Test rook attacks
            let magic_rook = targets(square, false, occupancy);
            let brute_rook = brute_force_rook_attacks(square, occupancy);
            
            if magic_rook == brute_rook {
                rook_matches += 1;
            } else {
                println!("MISMATCH: Rook on {:?} with occupancy bits {:016x}", square, occupancy.bits());
                println!("  Magic:  {:016x}", magic_rook.bits());
                println!("  Brute:  {:016x}", brute_rook.bits());
            }

            // Test bishop attacks
            let magic_bishop = targets(square, true, occupancy);
            let brute_bishop = brute_force_bishop_attacks(square, occupancy);
            
            if magic_bishop == brute_bishop {
                bishop_matches += 1;
            } else {
                println!("MISMATCH: Bishop on {:?} with occupancy bits {:016x}", square, occupancy.bits());
                println!("  Magic:  {:016x}", magic_bishop.bits());
                println!("  Brute:  {:016x}", brute_bishop.bits());
            }

            tests_run += 1;
        }
    }

    assert_eq!(rook_matches, tests_run, "All rook magic lookups should match brute force");
    assert_eq!(bishop_matches, tests_run, "All bishop magic lookups should match brute force");

    println!("✓ All {} test cases passed for both rooks and bishops!", tests_run);
}

#[test]
fn test_magic_properties() {
    println!("Testing magic bitboard properties...");
    
    // Test that attacks are symmetric for empty board
    let empty = SquareSet::new();
    
    // Rook on E4 should attack same squares as attacks to E4
    let e4_attacks = targets(Square::E4, false, empty);
    for square in e4_attacks {
        let reverse_attacks = targets(square, false, empty);
        assert!(reverse_attacks.contains(Square::E4), 
               "Rook attacks should be symmetric on empty board");
    }

    // Test that adding blockers reduces attack scope
    let e4_empty_attacks = targets(Square::E4, false, empty);
    let blocker = SquareSet::from_square(Square::E6);
    let e4_blocked_attacks = targets(Square::E4, false, blocker);
    
    // Should have fewer or equal attacks with blocker
    assert!(e4_blocked_attacks.len() <= e4_empty_attacks.len(), 
           "Adding blockers should not increase attack scope");
    
    // Should still include the blocker square
    assert!(e4_blocked_attacks.contains(Square::E6), 
           "Attack should include blocker square");
    
    // Should not include squares beyond blocker in that direction
    assert!(!e4_blocked_attacks.contains(Square::E7), 
           "Attack should not include squares beyond blocker");

    println!("✓ Magic bitboard properties verified!");
}

#[test]
fn test_performance_characteristics() {
    println!("Testing performance characteristics...");
    
    let start = std::time::Instant::now();
    let mut total_attacks = 0u32;
    
    // Run many lookups to test performance
    for square_idx in 0..64 {
        let square = unsafe { std::mem::transmute(square_idx as u8) };
        for occupancy_bits in 0..256u64 {
            let occupancy = SquareSet::from_bits(occupancy_bits);
            let rook_attacks = targets(square, false, occupancy);
            let bishop_attacks = targets(square, true, occupancy);
            total_attacks += rook_attacks.len() + bishop_attacks.len();
        }
    }
    
    let duration = start.elapsed();
    println!("✓ Completed {} lookups in {:?} ({:.0} lookups/sec)", 
             64 * 256 * 2, duration, 
             (64.0 * 256.0 * 2.0) / duration.as_secs_f64());
    
    assert!(duration.as_millis() < 1000, "Magic lookups should be very fast");
    assert!(total_attacks > 0, "Should have generated some attacks");
}
