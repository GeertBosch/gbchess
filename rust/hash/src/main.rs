mod hash;

pub use hash::*;

fn main() {
    println!("Testing Hash implementation...");

    // Test basic hash creation
    let hash = Hash::new();
    println!("Empty hash: {}", hash);

    // Test hash from starting position
    use ::fen::*;
    let position = parse_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        .expect("Failed to parse starting position");
    let start_hash = Hash::from_position(&position);
    println!("Starting position hash: {}", start_hash);

    // Test different position
    let position2 = parse_position("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1")
        .expect("Failed to parse after e4");
    let hash2 = Hash::from_position(&position2);
    println!("After 1.e4 hash: {}", hash2);

    // Verify hashes are different
    assert_ne!(start_hash, hash2);
    println!("✓ Different positions have different hashes");

    // Test hash toggle functionality
    let mut test_hash = Hash::new();
    let original = test_hash.value();

    test_hash.toggle_piece(Piece::P, Square::E4);
    assert_ne!(test_hash.value(), original);

    test_hash.toggle_piece(Piece::P, Square::E4);
    assert_eq!(test_hash.value(), original);
    println!("✓ Hash toggle works correctly");

    // Test castling hash
    let mut castle_hash = Hash::new();
    castle_hash.toggle_castling(CastlingMask::K);
    println!("Hash with white kingside castling: {}", castle_hash);

    // Test en passant hash
    let mut ep_hash = Hash::new();
    ep_hash.toggle_vector(Hash::EN_PASSANT_E);
    println!("Hash with e-file en passant: {}", ep_hash);

    // Test reproducibility
    let pos_copy = parse_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        .expect("Failed to parse starting position copy");
    let hash_copy = Hash::from_position(&pos_copy);
    assert_eq!(start_hash, hash_copy);
    println!("✓ Hash is reproducible");

    // Test move kinds
    let move_quiet = Move::new(Square::E2, Square::E4, MoveKind::DoublePush);
    let move_capture = Move::new(Square::E5, Square::F6, MoveKind::Capture);
    let move_promotion = Move::new(Square::E7, Square::E8, MoveKind::QueenPromotion);

    println!(
        "Move types: quiet={:?}, capture={:?}, promotion={:?}",
        move_quiet.kind, move_capture.kind, move_promotion.kind
    );

    assert!(move_capture.kind.is_capture());
    assert!(move_promotion.kind.is_promotion());
    assert_eq!(
        move_promotion.kind.promotion_piece_type(),
        Some(PieceType::Queen)
    );
    println!("✓ Move kind properties work correctly");

    println!("All Hash tests passed!");
}

#[cfg(test)]
mod integration_tests {
    use super::*;
    use ::fen::*;

    #[test]
    fn test_starting_position_hash() {
        let position = parse_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            .expect("Failed to parse starting position");
        let hash = Hash::from_position(&position);

        // Hash should be non-zero and reproducible
        assert_ne!(hash.value(), 0);

        let hash2 = Hash::from_position(&position);
        assert_eq!(hash, hash2);
    }

    #[test]
    fn test_different_positions_different_hashes() {
        let pos1 = parse_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            .expect("Failed to parse starting position");
        let pos2 = parse_position("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1")
            .expect("Failed to parse after e4");

        let hash1 = Hash::from_position(&pos1);
        let hash2 = Hash::from_position(&pos2);

        assert_ne!(hash1, hash2);
    }

    #[test]
    fn test_castling_rights_affect_hash() {
        let pos_with_castling = parse_position("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1")
            .expect("Failed to parse position with castling");
        let pos_no_castling = parse_position("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1")
            .expect("Failed to parse position without castling");

        let hash1 = Hash::from_position(&pos_with_castling);
        let hash2 = Hash::from_position(&pos_no_castling);

        assert_ne!(hash1, hash2);
    }

    #[test]
    fn test_en_passant_affects_hash() {
        let pos_with_ep =
            parse_position("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1")
                .expect("Failed to parse position with en passant");
        let pos_no_ep =
            parse_position("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1")
                .expect("Failed to parse position without en passant");

        let hash1 = Hash::from_position(&pos_with_ep);
        let hash2 = Hash::from_position(&pos_no_ep);

        assert_ne!(hash1, hash2);
    }

    #[test]
    fn test_active_color_affects_hash() {
        let pos_white = parse_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            .expect("Failed to parse position with white to move");
        let pos_black = parse_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1")
            .expect("Failed to parse position with black to move");

        let hash1 = Hash::from_position(&pos_white);
        let hash2 = Hash::from_position(&pos_black);

        assert_ne!(hash1, hash2);
    }
}
