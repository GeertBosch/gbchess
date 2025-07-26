use fen::{Board, Color, Piece, Square};
use moves::{make_move, unmake_move_board, Occupancy, Move, MoveKind, Position, Turn, CastlingMask, NO_EN_PASSANT_TARGET, apply_move, is_attacked_square, castling_mask};
use square_set::SquareSet;

#[allow(dead_code)]
fn to_string_squares(squares: SquareSet) -> String {
    let mut result = String::new();
    for square in squares.iter() {
        if !result.is_empty() {
            result.push(' ');
        }
        result.push_str(&square.to_string());
    }
    result
}

fn print_board(board: &Board) {
    for rank in (0..8).rev() {
        print!("{}  ", rank + 1);
        for file in 0..8 {
            let square = Square::make_square(file, rank);
            let piece = board[square];
            print!(" {}", piece.to_char());
        }
        println!();
    }
    print!("   ");
    for file in 0..8 {
        print!(" {}", (b'a' + file as u8) as char);
    }
    println!();
}

fn test_make_and_unmake_move(board: &mut Board, mv: Move) {
    let original_board = board.clone();
    let active_color = board[mv.from].color();
    let _occupancy = Occupancy::from_board(board, active_color);

    // Note: We don't have occupancyDelta in our implementation yet
    let undo = make_move(board, mv);

    // Verify the move was applied correctly
    unmake_move_board(board, undo);

    if *board != original_board {
        println!("Original board:");
        print_board(&original_board);
        println!("Move: {}", mv);
        println!("Board after makeMove and unmakeMove:");
        print_board(board);
        panic!("unmakeMove did not restore original board");
    }

    // Reapply the move for the test
    make_move(board, mv);
}

fn test_make_and_unmake_move_tests() {
    println!("Testing makeMove and unmakeMove...");

    // Test pawn move
    {
        let mut board = Board::new();
        board[Square::A2] = Piece::P;
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A2, Square::A3, MoveKind::QuietMove),
        );
        assert_eq!(board[Square::A3], Piece::P);
        assert_eq!(board[Square::A2], Piece::Empty);
    }

    // Test pawn capture
    {
        let mut board = Board::new();
        board[Square::A2] = Piece::P;
        board[Square::B3] = Piece::r; // White pawn captures black rook
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A2, Square::B3, MoveKind::Capture),
        );
        assert_eq!(board[Square::B3], Piece::P);
        assert_eq!(board[Square::A2], Piece::Empty);
    }

    // Test pawn promotion move
    {
        // White pawn promotion
        let mut board = Board::new();
        board[Square::A7] = Piece::P;
        let mv = Move::new(Square::A7, Square::A8, MoveKind::QueenPromotion);
        test_make_and_unmake_move(&mut board, mv);
        assert_eq!(board[Square::A8], Piece::Q);
        assert_eq!(board[Square::A7], Piece::Empty);

        // Black pawn promotion
        board[Square::A2] = Piece::p;
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A2, Square::A1, MoveKind::RookPromotion),
        );
        assert_eq!(board[Square::A1], Piece::r);
        assert_eq!(board[Square::A2], Piece::Empty);
    }

    // Test pawn promotion capture
    {
        // White pawn promotion
        let mut board = Board::new();
        board[Square::A7] = Piece::P;
        board[Square::B8] = Piece::r; // White pawn captures black rook
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A7, Square::B8, MoveKind::BishopPromotionCapture),
        );
        assert_eq!(board[Square::B8], Piece::B);
        assert_eq!(board[Square::A7], Piece::Empty);

        // Black pawn promotion
        board[Square::A2] = Piece::p;
        board[Square::B1] = Piece::R; // Black pawn captures white rook
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A2, Square::B1, MoveKind::KnightPromotionCapture),
        );
        assert_eq!(board[Square::B1], Piece::n);
        assert_eq!(board[Square::A2], Piece::Empty);
    }

    // Test rook move
    {
        let mut board = Board::new();
        board[Square::A8] = Piece::R;
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A8, Square::H8, MoveKind::QuietMove),
        );
        assert_eq!(board[Square::H8], Piece::R);
        assert_eq!(board[Square::A8], Piece::Empty);
    }

    // Test rook capture
    {
        let mut board = Board::new();
        board[Square::A8] = Piece::r;
        board[Square::A1] = Piece::R; // Black rook captures white rook
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A8, Square::A1, MoveKind::Capture),
        );
        assert_eq!(board[Square::A1], Piece::r);
        assert_eq!(board[Square::A8], Piece::Empty);
    }

    // Test knight move
    {
        let mut board = Board::new();
        board[Square::B1] = Piece::N;
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::B1, Square::C3, MoveKind::QuietMove),
        );
        assert_eq!(board[Square::C3], Piece::N);
        assert_eq!(board[Square::B1], Piece::Empty);
    }

    // Test knight capture
    {
        let mut board = Board::new();
        board[Square::B1] = Piece::N;
        board[Square::C3] = Piece::r; // White knight captures black rook
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::B1, Square::C3, MoveKind::Capture),
        );
        assert_eq!(board[Square::C3], Piece::N);
        assert_eq!(board[Square::B1], Piece::Empty);
    }

    // Test en passant capture
    {
        let mut board = Board::new();
        board[Square::A5] = Piece::P;
        board[Square::B5] = Piece::p; // Black pawn moved two squares
        let mv = Move::new(Square::A5, Square::B6, MoveKind::EnPassant);
        test_make_and_unmake_move(&mut board, mv);
        assert_eq!(board[Square::B6], Piece::P);
        assert_eq!(board[Square::B5], Piece::Empty);
        assert_eq!(board[Square::A5], Piece::Empty);
    }

    // Test king castling move
    {
        let mut board = Board::new();
        board[Square::E1] = Piece::K;
        board[Square::H1] = Piece::R;
        test_make_and_unmake_move(&mut board, Move::new(Square::E1, Square::G1, MoveKind::OO));
        assert_eq!(board[Square::G1], Piece::K);
        assert_eq!(board[Square::F1], Piece::R);
        assert_eq!(board[Square::E1], Piece::Empty);
        assert_eq!(board[Square::H1], Piece::Empty);
    }

    // Test queen castling move
    {
        let mut board = Board::new();
        board[Square::E1] = Piece::K;
        board[Square::A1] = Piece::R;
        test_make_and_unmake_move(&mut board, Move::new(Square::E1, Square::C1, MoveKind::OOO));
        assert_eq!(board[Square::C1], Piece::K);
        assert_eq!(board[Square::D1], Piece::R);
        assert_eq!(board[Square::E1], Piece::Empty);
        assert_eq!(board[Square::A1], Piece::Empty);
    }

    println!("All makeMove and unmakeMove tests passed!");
}

fn test_apply_move_tests() {
    println!("Testing applyMove...");

    // Test pawn move
    {
        let mut position = Position::new();
        position.board[Square::A2] = Piece::P;

        position = apply_move(
            position,
            Move::new(Square::A2, Square::A3, MoveKind::QuietMove),
        );
        assert_eq!(position.board[Square::A3], Piece::P);
        assert_eq!(position.board[Square::A2], Piece::Empty);
        assert_eq!(position.turn.active_color(), Color::Black);
        assert_eq!(position.turn.halfmove(), 0);
    }

    // Test pawn capture
    {
        let mut position = Position::new();
        position.board[Square::A2] = Piece::P;
        position.board[Square::B3] = Piece::r; // White pawn captures black rook
        position.turn = Turn::new(Color::White, CastlingMask::None, NO_EN_PASSANT_TARGET, 1, 1);

        position = apply_move(
            position,
            Move::new(Square::A2, Square::B3, MoveKind::Capture),
        );
        assert_eq!(position.board[Square::B3], Piece::P);
        assert_eq!(position.board[Square::A2], Piece::Empty);
        assert_eq!(position.turn.active_color(), Color::Black);
        assert_eq!(position.turn.halfmove(), 0);
    }

    // Test that the fullmove number is updated correctly on a black move
    {
        let mut position = Position::new();
        position.board[Square::A2] = Piece::p;
        position.turn = Turn::new(Color::Black, CastlingMask::None, NO_EN_PASSANT_TARGET, 1, 1);

        position = apply_move(
            position,
            Move::new(Square::A2, Square::A3, MoveKind::QuietMove),
        );
        assert_eq!(position.board[Square::A3], Piece::p);
        assert_eq!(position.board[Square::A2], Piece::Empty);
        assert_eq!(position.turn.active_color(), Color::White);
        assert_eq!(position.turn.halfmove(), 0);
        assert_eq!(position.turn.fullmove(), 2);
    }

    // Test that a move with a non-pawn piece does not reset the halfmoveClock
    {
        let mut position = Position::new();
        position.board[Square::B1] = Piece::N;
        position.turn = Turn::new(Color::White, CastlingMask::None, NO_EN_PASSANT_TARGET, 1, 1);

        position = apply_move(
            position,
            Move::new(Square::B1, Square::C3, MoveKind::QuietMove),
        );
        assert_eq!(position.board[Square::C3], Piece::N);
        assert_eq!(position.board[Square::B1], Piece::Empty);
        assert_eq!(position.turn.active_color(), Color::Black);
        assert_eq!(position.turn.halfmove(), 2);
    }

    println!("All applyMove tests passed!");
}

fn test_is_attacked_tests() {
    println!("Testing isAttacked...");

    let white_king_square = Square::A1;
    let black_king_square = Square::F6;
    let mut base = Board::new();
    base[white_king_square] = Piece::K;
    base[black_king_square] = Piece::k;

    // Test that a king is in check
    {
        let mut board = base.clone();
        board[Square::B1] = Piece::r;
        let occupancy = Occupancy::from_board(&board, Color::White);
        assert!(is_attacked_square(&board, white_king_square, occupancy));
        assert!(!is_attacked_square(&board, black_king_square, occupancy));
    }

    // Test that a king is in check after a move
    {
        let mut board = base.clone();
        board[Square::B2] = Piece::r;
        test_make_and_unmake_move(
            &mut board,
            Move::new(Square::A1, Square::A2, MoveKind::QuietMove),
        );
        let occupancy = Occupancy::from_board(&board, Color::White);
        assert!(is_attacked_square(&board, Square::A2, occupancy));
    }

    // Test that this method also works for an empty square
    {
        let mut board = Board::new();
        board[Square::E1] = Piece::K;
        board[Square::F2] = Piece::r;
        let occupancy = Occupancy::from_board(&board, Color::White);

        assert!(is_attacked_square(&board, Square::F1, occupancy));
    }

    println!("All isAttacked tests passed!");
}

fn test_castling_mask_tests() {
    println!("Testing castlingMask...");

    {
        let mask = castling_mask(Square::A1, Square::A8);
        assert_eq!(mask.as_u8(), (CastlingMask::Q | CastlingMask::q).as_u8());
    }

    println!("All castlingMask tests passed!");
}

fn main() {
    println!("Testing moves module...");

    test_make_and_unmake_move_tests();
    test_castling_mask_tests();
    test_apply_move_tests();
    test_is_attacked_tests();

    println!("All move tests passed!");
}
