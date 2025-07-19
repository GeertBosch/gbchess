/**
 * Integration tests for SquareSet data structure.
 *
 * This file contains tests that match the C++ behavior exactly,
 * ensuring compatibility during the migration process.
 */
use square_set::square_set::*;

fn test_square_set() {
    let mut set = SquareSet::new();

    // Test empty set
    assert!(set.is_empty());
    assert_eq!(set.len(), 0);

    // Test insert and count
    set.insert(Square::A1);
    assert!(!set.is_empty());
    assert_eq!(set.len(), 1);
    assert!(set.contains(Square::A1));
    assert!(!set.contains(Square::B1));

    // Test insert and count with multiple squares
    set.insert(Square::B1);
    set.insert(Square::E1);
    assert_eq!(set.len(), 3);
    assert!(set.contains(Square::A1));
    assert!(set.contains(Square::B1));
    assert!(!set.contains(Square::C1));
    assert!(!set.contains(Square::D1));
    assert!(set.contains(Square::E1));

    // Test iterator
    set.insert(Square::B2);
    let mut iter = set.into_iter();
    assert_eq!(iter.next(), Some(Square::A1));
    assert_eq!(iter.next(), Some(Square::B1));
    assert_eq!(iter.next(), Some(Square::E1));
    assert_eq!(iter.next(), Some(Square::B2));
    assert_eq!(iter.next(), None);

    // Test copy and range insertion
    let mut set2 = SquareSet::new();
    set2.insert(Square::D1);
    set2.insert(Square::E1);
    set2.insert(Square::F1);

    for square in set2 {
        set.insert(square);
    }
    assert_eq!(set.len(), 6); // E1 was already in set, so only 2 new squares
    assert!(set.contains(Square::D1));
    assert!(set.contains(Square::E1));
    assert!(set.contains(Square::F1));

    println!("All SquareSet tests passed!");
}

fn test_occupancy() {
    let mut board = Board::new();
    board[Square::A3] = Piece::P;
    board[Square::B4] = Piece::P;
    board[Square::A1] = Piece::R;
    board[Square::D2] = Piece::N;
    board[Square::C2] = Piece::B;
    board[Square::D1] = Piece::Q;
    board[Square::E1] = Piece::K;
    board[Square::F2] = Piece::B;
    board[Square::G2] = Piece::N;
    board[Square::H5] = Piece::r;
    board[Square::D7] = Piece::p;
    board[Square::E5] = Piece::p;
    board[Square::F7] = Piece::p;
    board[Square::G7] = Piece::p;
    board[Square::H7] = Piece::p;
    board[Square::A8] = Piece::r;
    board[Square::B8] = Piece::n;
    board[Square::C8] = Piece::b;
    board[Square::D8] = Piece::q;
    board[Square::E8] = Piece::k;

    let squares = occupancy(&board);
    assert_eq!(squares.len(), 20);

    let white_squares = occupancy_by_color(&board, Color::White);
    assert_eq!(white_squares.len(), 9);

    let black_squares = occupancy_by_color(&board, Color::Black);
    assert_eq!(black_squares.len(), 11);

    println!("All occupancy tests passed!");
}

fn test_advanced_operations() {
    // Test rank and file operations
    let rank1 = SquareSet::rank(0);
    assert_eq!(rank1.len(), 8);
    for file in 0..8 {
        let square = Square::make_square(file, 0);
        assert!(rank1.contains(square));
    }

    let file_e = SquareSet::file(4); // E file
    assert_eq!(file_e.len(), 8);
    for rank in 0..8 {
        let square = Square::make_square(4, rank);
        assert!(file_e.contains(square));
    }

    // Test path generation
    let path = SquareSet::make_path(Square::A1, Square::A8);
    assert_eq!(path.len(), 6); // Excludes A1 and A8
    assert!(!path.contains(Square::A1));
    assert!(!path.contains(Square::A8));
    assert!(path.contains(Square::A2));
    assert!(path.contains(Square::A7));

    // Test diagonal path
    let diagonal = SquareSet::make_path(Square::A1, Square::H8);
    assert_eq!(diagonal.len(), 6);
    assert!(diagonal.contains(Square::B2));
    assert!(diagonal.contains(Square::G7));

    // Test bitwise operations
    let set1 = SquareSet::from_square(Square::A1) | SquareSet::from_square(Square::B1);
    let set2 = SquareSet::from_square(Square::A1) | SquareSet::from_square(Square::C1);

    let intersection = set1 & set2;
    assert_eq!(intersection.len(), 1);
    assert!(intersection.contains(Square::A1));

    let union = set1 | set2;
    assert_eq!(union.len(), 3);

    let difference = set1 - set2;
    assert_eq!(difference.len(), 1);
    assert!(difference.contains(Square::B1));

    println!("All advanced operation tests passed!");
}

fn test_edge_cases() {
    // Test all squares
    let all = SquareSet::all();
    assert_eq!(all.len(), 64);
    assert!(all.contains(Square::A1));
    assert!(all.contains(Square::H8));

    // Test complement
    let empty = SquareSet::new();
    let complement = !empty;
    assert_eq!(complement.len(), 64);

    // Test shifts
    let a1 = SquareSet::from_square(Square::A1);
    let shifted = a1 << 1;
    assert!(shifted.contains(Square::B1));
    assert!(!shifted.contains(Square::A1));

    println!("All edge case tests passed!");
}

fn main() {
    test_square_set();
    test_occupancy();
    test_advanced_operations();
    test_edge_cases();

    println!("All integration tests passed successfully!");
}
