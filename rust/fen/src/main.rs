mod board;
mod fen;
mod types;

use board::CastlingMask;
use fen::{EMPTY_PIECE_PLACEMENT, INITIAL_PIECE_PLACEMENT, INITIAL_POSITION};
use types::{Color, Piece, Square};

fn test_piece() {
    // Test from_char
    assert_eq!(Piece::from_char('P'), Some(Piece::P));
    assert_eq!(Piece::from_char('N'), Some(Piece::N));
    assert_eq!(Piece::from_char('B'), Some(Piece::B));
    assert_eq!(Piece::from_char('R'), Some(Piece::R));
    assert_eq!(Piece::from_char('Q'), Some(Piece::Q));
    assert_eq!(Piece::from_char('K'), Some(Piece::K));
    assert_eq!(Piece::from_char('p'), Some(Piece::p));
    assert_eq!(Piece::from_char('n'), Some(Piece::n));
    assert_eq!(Piece::from_char('b'), Some(Piece::b));
    assert_eq!(Piece::from_char('r'), Some(Piece::r));
    assert_eq!(Piece::from_char('q'), Some(Piece::q));
    assert_eq!(Piece::from_char('k'), Some(Piece::k));
    assert_eq!(Piece::from_char(' '), Some(Piece::Empty));

    // Test to_char
    assert_eq!(Piece::P.to_char(), 'P');
    assert_eq!(Piece::N.to_char(), 'N');
    assert_eq!(Piece::B.to_char(), 'B');
    assert_eq!(Piece::R.to_char(), 'R');
    assert_eq!(Piece::Q.to_char(), 'Q');
    assert_eq!(Piece::K.to_char(), 'K');
    assert_eq!(Piece::p.to_char(), 'p');
    assert_eq!(Piece::n.to_char(), 'n');
    assert_eq!(Piece::b.to_char(), 'b');
    assert_eq!(Piece::r.to_char(), 'r');
    assert_eq!(Piece::q.to_char(), 'q');
    assert_eq!(Piece::k.to_char(), 'k');
    assert_eq!(Piece::Empty.to_char(), '.');

    println!("All Piece tests passed!");
}

fn test_parse() -> Result<(), Box<dyn std::error::Error>> {
    let position = fen::parse_position(INITIAL_POSITION)?;
    let turn = &position.turn;

    println!("Piece Placement: {}", fen::board_to_string(&position.board));
    println!("Active Color: {}", turn.active_color());
    println!("Castling Availability: {}", turn.castling().value());
    println!(
        "En Passant Target: {}",
        turn.en_passant()
            .map_or("-".to_string(), |sq| sq.to_string())
    );
    println!("Halfmove Clock: {}", turn.halfmove());
    println!("Fullmove Number: {}", turn.fullmove());

    Ok(())
}

fn test_initial_position() -> Result<(), Box<dyn std::error::Error>> {
    let board = fen::parse_piece_placement(INITIAL_PIECE_PLACEMENT)?;
    assert_eq!(board[Square::E1], Piece::K);
    assert_eq!(board[Square::H1], Piece::R);
    assert_eq!(board[Square::A1], Piece::R);
    assert_eq!(board[Square::E8], Piece::k);
    assert_eq!(board[Square::H8], Piece::r);
    assert_eq!(board[Square::A8], Piece::r);

    let position = fen::parse_position(INITIAL_POSITION)?;
    assert_eq!(position.board, board);
    let turn = &position.turn;
    assert_eq!(turn.active_color(), Color::White);
    assert_eq!(turn.castling().value(), CastlingMask::KQ_kq.value());
    assert_eq!(turn.en_passant(), None);
    assert_eq!(turn.halfmove(), 0);
    assert_eq!(turn.fullmove(), 1);

    Ok(())
}

fn test_fen_piece_placement() -> Result<(), Box<dyn std::error::Error>> {
    let test_strings = vec![
        EMPTY_PIECE_PLACEMENT,
        INITIAL_PIECE_PLACEMENT,
        "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R",
        "4k3/8/8/8/8/8/8/4K3",
        "4k3/8/8/3Q4/8/8/8/4K3",
        "4k3/8/8/3q4/8/8/8/4K3",
    ];

    for fen_str in test_strings {
        let board = fen::parse_piece_placement(fen_str)?;
        let round_tripped = fen::board_to_string(&board);
        assert_eq!(fen_str, round_tripped);
    }

    Ok(())
}

fn test_fen_position() -> Result<(), Box<dyn std::error::Error>> {
    let test_strings = vec![
        INITIAL_POSITION,
        "4k3/8/8/2q5/5Pp1/8/7P/4K2R b Kkq f3 0 42",
        "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R w KQkq - 0 1",
        "4k3/8/8/3Q4/8/8/8/4K3 w - - 0 1",
    ];

    for fen_str in test_strings {
        let position = fen::parse_position(fen_str)?;
        let round_tripped = fen::position_to_string(&position);
        assert_eq!(fen_str, round_tripped);
    }

    Ok(())
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    test_piece();
    test_parse()?;
    test_initial_position()?;
    test_fen_piece_placement()?;
    test_fen_position()?;
    println!("All FEN tests passed!");
    Ok(())
}
