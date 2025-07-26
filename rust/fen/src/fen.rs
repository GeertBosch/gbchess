use crate::board::{Board, CastlingMask, Position, Turn, NO_EN_PASSANT_TARGET};
use crate::types::{Color, Piece, Square};
use std::fmt;

pub const EMPTY_PIECE_PLACEMENT: &str = "8/8/8/8/8/8/8/8";
pub const INITIAL_PIECE_PLACEMENT: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
pub const INITIAL_POSITION: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

#[derive(Debug, Clone)]
pub struct ParseError {
    pub message: String,
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "FEN parse error: {}", self.message)
    }
}

impl std::error::Error for ParseError {}

impl ParseError {
    pub fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

/// Convert a Board to FEN piece placement string
pub fn board_to_string(board: &Board) -> String {
    let mut result = String::new();

    for rank in (0..8).rev() {
        // Start from rank 8 (index 7) down to rank 1 (index 0)
        let mut empty_count = 0;

        for file in 0..8 {
            let square = Square::make_square(file, rank);
            let piece = board[square];

            if piece == Piece::Empty {
                empty_count += 1;
            } else {
                if empty_count > 0 {
                    result.push((b'0' + empty_count) as char);
                    empty_count = 0;
                }
                result.push(piece.to_char());
            }
        }

        if empty_count > 0 {
            result.push((b'0' + empty_count) as char);
        }

        if rank > 0 {
            result.push('/');
        }
    }

    result
}

/// Convert a Position to full FEN string
pub fn position_to_string(position: &Position) -> String {
    let mut result = board_to_string(&position.board);

    // Active color
    result.push(' ');
    result.push_str(&position.turn.active_color().to_string());

    // Castling availability
    result.push(' ');
    let castling = position.turn.castling();
    if castling.value() == 0 {
        result.push('-');
    } else {
        if castling.has_white_kingside() {
            result.push('K');
        }
        if castling.has_white_queenside() {
            result.push('Q');
        }
        if castling.has_black_kingside() {
            result.push('k');
        }
        if castling.has_black_queenside() {
            result.push('q');
        }
    }

    // En passant target
    result.push(' ');
    let en_passant_square = position.turn.en_passant();
    if en_passant_square == NO_EN_PASSANT_TARGET {
        result.push('-');
    } else {
        result.push_str(&en_passant_square.to_string());
    }

    // Halfmove clock
    result.push(' ');
    result.push_str(&position.turn.halfmove().to_string());

    // Fullmove number
    result.push(' ');
    result.push_str(&position.turn.fullmove().to_string());

    result
}

/// Parse piece placement part of FEN string
pub fn parse_piece_placement(piece_placement: &str) -> Result<Board, ParseError> {
    let mut board = Board::new();
    let ranks: Vec<&str> = piece_placement.split('/').collect();

    if ranks.len() != 8 {
        return Err(ParseError::new(format!(
            "Expected 8 ranks, got {}",
            ranks.len()
        )));
    }

    for (rank_index, rank_str) in ranks.iter().enumerate() {
        let rank = 7 - rank_index; // FEN starts with rank 8 (index 7)
        let mut file = 0;

        for c in rank_str.chars() {
            if file >= 8 {
                return Err(ParseError::new(format!(
                    "Too many pieces/numbers in rank {}",
                    rank + 1
                )));
            }

            if c.is_ascii_digit() {
                let empty_squares = c.to_digit(10).unwrap() as usize;
                if empty_squares == 0 || empty_squares > 8 {
                    return Err(ParseError::new(format!(
                        "Invalid empty square count: {}",
                        c
                    )));
                }
                file += empty_squares;
            } else {
                if let Some(piece) = Piece::from_char(c) {
                    if piece == Piece::Empty {
                        return Err(ParseError::new(format!(
                            "Unexpected empty piece character: {}",
                            c
                        )));
                    }
                    let square = Square::make_square(file, rank);
                    board[square] = piece;
                    file += 1;
                } else {
                    return Err(ParseError::new(format!("Invalid piece character: {}", c)));
                }
            }
        }

        if file != 8 {
            return Err(ParseError::new(format!(
                "Incomplete rank {}: only {} squares",
                rank + 1,
                file
            )));
        }
    }

    Ok(board)
}

fn parse_square(square_str: &str) -> Result<Square, ParseError> {
    if square_str.len() != 2 {
        return Err(ParseError::new(format!(
            "Invalid square notation: {}",
            square_str
        )));
    }

    let chars: Vec<char> = square_str.chars().collect();
    let file_char = chars[0];
    let rank_char = chars[1];

    if !('a'..='h').contains(&file_char) {
        return Err(ParseError::new(format!("Invalid file: {}", file_char)));
    }

    if !('1'..='8').contains(&rank_char) {
        return Err(ParseError::new(format!("Invalid rank: {}", rank_char)));
    }

    let file = (file_char as u8 - b'a') as usize;
    let rank = (rank_char as u8 - b'1') as usize;

    Ok(Square::make_square(file, rank))
}

/// Parse full FEN string to Position
pub fn parse_position(fen: &str) -> Result<Position, ParseError> {
    let parts: Vec<&str> = fen.split_whitespace().collect();

    if parts.len() != 6 {
        return Err(ParseError::new(format!(
            "Expected 6 FEN parts, got {}",
            parts.len()
        )));
    }

    // Parse piece placement
    let board = parse_piece_placement(parts[0])?;

    // Parse active color
    let active_color = match parts[1] {
        "w" => Color::White,
        "b" => Color::Black,
        _ => {
            return Err(ParseError::new(format!(
                "Invalid active color: {}",
                parts[1]
            )))
        }
    };

    // Parse castling availability
    let mut castling_mask = CastlingMask::EMPTY;
    if parts[2] != "-" {
        for c in parts[2].chars() {
            match c {
                'K' => castling_mask |= CastlingMask::K,
                'Q' => castling_mask |= CastlingMask::Q,
                'k' => castling_mask |= CastlingMask::k,
                'q' => castling_mask |= CastlingMask::q,
                _ => {
                    return Err(ParseError::new(format!(
                        "Invalid castling character: {}",
                        c
                    )))
                }
            }
        }
    }

    // Parse en passant target
    let en_passant_target = if parts[3] == "-" {
        NO_EN_PASSANT_TARGET
    } else {
        parse_square(parts[3])?
    };

    // Parse halfmove clock
    let halfmove_clock = parts[4]
        .parse::<u8>()
        .map_err(|_| ParseError::new(format!("Invalid halfmove clock: {}", parts[4])))?;

    // Parse fullmove number
    let fullmove_number = parts[5]
        .parse::<u16>()
        .map_err(|_| ParseError::new(format!("Invalid fullmove number: {}", parts[5])))?;

    let turn = Turn::new(
        active_color,
        castling_mask,
        en_passant_target,
        halfmove_clock,
        fullmove_number,
    );

    Ok(Position { board, turn })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_empty_board_round_trip() {
        let board = Board::new();
        let fen_string = board_to_string(&board);
        assert_eq!(fen_string, EMPTY_PIECE_PLACEMENT);

        let parsed_board = parse_piece_placement(&fen_string).unwrap();
        assert_eq!(board, parsed_board);
    }

    #[test]
    fn test_initial_position_round_trip() {
        let position = parse_position(INITIAL_POSITION).unwrap();
        let fen_string = position_to_string(&position);
        assert_eq!(fen_string, INITIAL_POSITION);
    }

    #[test]
    fn test_piece_placement_parsing() {
        let test_fens = vec![
            EMPTY_PIECE_PLACEMENT,
            INITIAL_PIECE_PLACEMENT,
            "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R",
            "4k3/8/8/8/8/8/8/4K3",
            "4k3/8/8/3Q4/8/8/8/4K3",
            "4k3/8/8/3q4/8/8/8/4K3",
        ];

        for fen in test_fens {
            let board = parse_piece_placement(fen).unwrap();
            let round_trip = board_to_string(&board);
            assert_eq!(fen, round_trip, "Round trip failed for: {}", fen);
        }
    }

    #[test]
    fn test_position_parsing() {
        let test_fens = vec![
            INITIAL_POSITION,
            "4k3/8/8/2q5/5Pp1/8/7P/4K2R b Kkq f3 0 42",
            "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R w KQkq - 0 1",
            "4k3/8/8/3Q4/8/8/8/4K3 w - - 0 1",
        ];

        for fen in test_fens {
            let position = parse_position(fen).unwrap();
            let round_trip = position_to_string(&position);
            assert_eq!(fen, round_trip, "Round trip failed for: {}", fen);
        }
    }
}
