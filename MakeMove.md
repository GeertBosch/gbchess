# Anatomy of MakeMove

## Problem Statement
Given is a `Board` (array of 64 squares, each with a `Piece`) and a `Move` expressed as a triplet
`(MoveKind kind, Square from, Square to)`, where `Square` is an unsigned integer index into the
board with the 6 lowest bits used: the low 3 bits identifying the file and the next 3 bits the rank.

The `Piece` enum has the following values:

    WHITE_PAWN,
    WHITE_KNIGHT,
    WHITE_BISHOP,
    WHITE_ROOK,
    WHITE_QUEEN,
    WHITE_KING,
    NONE,
    BLACK_PAWN,
    BLACK_KNIGHT,
    BLACK_BISHOP,
    BLACK_ROOK,
    BLACK_QUEEN,
    BLACK_KING

Let's further consider a `MoveKind` with the following values:

    // Moves that don't capture or promote
    QUIET_MOVE = 0,
    DOUBLE_PAWN_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,

    // Captures that don't promote
    CAPTURE_MASK = 4,
    CAPTURE = 4,
    EN_PASSANT = 5,

    // Promotions that don't capture
    PROMOTION_MASK = 8,
    KNIGHT_PROMOTION = 8,
    BISHOP_PROMOTION = 9,
    ROOK_PROMOTION = 10,
    QUEEN_PROMOTION = 11,

    // Promotions that capture
    KNIGHT_PROMOTION_CAPTURE = 12,
    BISHOP_PROMOTION_CAPTURE = 13,
    ROOK_PROMOTION_CAPTURE = 14,
    QUEEN_PROMOTION_CAPTURE = 15,

So, a `Move` needs 16 bits to encode: 4 for the `kind` and 6 each for the `from` and `to` squares.

The question is how to efficiently update the board and return the captured piece? The goal is to
avoid conditional logic, as that helps instruction paralellism and may even open the door to SIMD
optimizations.

## Analysis
Mostly a standard simple move or capture consists of:
```
 1. board[from] -> piece
 2. Piece::NONE -> board[from]
 3. board[to] -> target
 4. piece -> board[to]
 5. return target
```
This would be easy to implement without conditional logic, and only relies on the `from` and `to`
values. So, the UCI notation like `e2e4` or `a1a8` is sufficient regardless of whether this is a
move or capture.

There are the three complications:
 * Castling moves
 * En Passant moves
 * Promotion moves (whether capturing or not)

If we look closer at these more complicated moves, they can also be expressed as a simple move,
followed by a "promo move":
```
 1. board[from2] -> piece
 2. Piece::NONE -> board[from2]
 3. piece += promo
 4. piece -> board[to2]
 ```
Let's delve in each of the three complications.

### Castling Moves

King side: `{ KING_CASTLE, "e1"_sq, "g1"_sq}` or `{ KING_CASTLE, "e8"_sq, "g8"_sq }`, and  
Queen side: `{ QUEEN_CASTLE, "e1"_sq, "c1"_sq}` or `{ QUEEN_CASTLE, "e8"_sq", "c8"_sq}`.

These become a combination of a king move and a rook move. Using UCI notiation this becomes:

King side: `e1g1` `h1f1` or `e8g8` `h8f8`, and  
Queen side: `e1c1` `a1d1` or `e8c8` `a8d8`.

Note that the low 4 bits of the `to` square suffice to identify both the destination file and the
side for castling. Even with chess960 this would be unambiguous. We don't even need the
`KING_CASTLE` vs `QUEEN_CASTLE` information from the `MoveKind` as it is redundant with the `to`
square file.

### En Passant Moves

White player: `{ EN_PASSANT, "e5"_sq, "f6"_sq}`,  
Black player: `{ EN_PASSANT, "d4"_sq, "c3"_sq}`.

These become a combination of a sideways capture followed by a regular pawn push:

White player: `e5f5` `f5f6`, and  
Black player: `d4c4` `c4c3`.

Again, the low 4 bits of the `to` square are sufficient to identify the changes in both the first
move (capture) as well as the second move.

## Promotion moves (whether capturing or not)

Same thing, just need `MoveKind` and `to` square.