# Using Perft for Move Generation Debugging

Perft (performance test) is a crucial tool for debugging chess move generation. It counts the number of positions reachable at a given depth and provides a deterministic way to verify that your move generator produces exactly the correct set of legal moves.

## What is Perft?

Perft recursively generates all legal moves from a position to a specified depth and counts the total number of leaf nodes (positions). It's deterministic - the same position and depth will always produce the same count if the move generation is correct.

**Perft Divide** shows the breakdown by root moves, displaying each first move and how many positions it leads to at the given depth.

## Prerequisites

You'll need:
- A known-good chess engine like Stockfish for reference
- Your perft implementation (C++ or Rust version)
- Test positions with known correct perft counts

## Step-by-Step Debugging Process

### Step 1: Get Reference Results

Use Stockfish to get the correct perft divide for your test case:

```bash
# For starting position at depth 3
echo -e "position startpos\ngo perft 3" | stockfish

# For custom FEN position
echo -e 'position fen "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"\ngo perft 4' | stockfish
```

Example Stockfish output:
```
a2a3: 380
b2b3: 420
c2c3: 420
...
Nodes searched: 8902
```

### Step 2: Run Your Perft Implementation

Run your perft test with the same position and depth:

```bash
# C++ version
cd /Users/bosch/gbchess
./build/perft "fen_string" depth

# Rust version  
cd /Users/bosch/gbchess/rust/perft
cargo run -- "fen_string" depth
```

Compare the output format and total node count.

## Common Issues and Solutions

### Too Many Moves (Higher node count)
- **Invalid castling**: Through check, with pieces blocking, or after king/rook moved
- **Moving into check**: Leaving own king in check
- **Invalid en passant**: Wrong pawn positions or illegal captures
- **Invalid pawn moves**: Double moves from non-starting ranks

### Too Few Moves (Lower node count)  
- **Missing move types**: Castling, en passant, promotions not generated
- **Overly restrictive validation**: Incorrectly rejecting valid moves
- **Incomplete piece movement**: Missing directions or knight moves
- **Promotion handling**: Not generating all options (Q, R, B, N)

### Same Count, Different Moves
Apply both above approaches - you have offsetting missing and extra moves.

### Same Moves, Different Counts
Create new test case: take the move with largest difference, use resulting position with `depth - 1`.

## Recommended Testing Sequence

### 1. Start Simple
Begin with basic positions and shallow depths:

```bash
# Depth 1: Should be exactly 20 moves
echo -e "position startpos\ngo perft 1" | stockfish

# Depth 2: Should be exactly 400 positions  
echo -e "position startpos\ngo perft 2" | stockfish

# Depth 3: Should be exactly 8902 positions
echo -e "position startpos\ngo perft 3" | stockfish
```

### 2. Test Specific Features

Once basic moves work, test positions that exercise specific rules:

**Kiwipete (castling, en passant):**
```bash
echo -e 'position fen "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"\ngo perft 4' | stockfish
```

**Position 3 (pawn promotion):**
```bash
echo -e 'position fen "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"\ngo perft 5' | stockfish
```

**Position 4 (complex promotions):**
```bash
echo -e 'position fen "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"\ngo perft 4' | stockfish
```

## Troubleshooting Tips

- **Start simple**: Begin with startpos at depths 1-3 before complex positions
- **Choose smallest failing cases**: Pick moves with lowest node counts for easier debugging  
- **Focus on percentage differences**: 10% difference is easier to debug than 1%
- **Use divide output**: Compare move-by-move to isolate issues
- **Test incrementally**: If depth 4 fails, try depth 3, 2, 1 for same position
- **Remember**: Perft is deterministic - different results mean bugs exist

## Tools and Commands

### C++ Implementation
```bash
cd /Users/bosch/gbchess
make perft-test        # Run comprehensive test suite
./build/perft "fen" depth  # Manual testing
```

### Rust Implementation
```bash
cd /Users/bosch/gbchess/rust/perft
cargo test             # Run built-in test suite
cargo run -- "fen" depth  # Manual testing
```

### Well-Known Test Positions

From [https://www.chessprogramming.org/Perft_Results](https://www.chessprogramming.org/Perft_Results):

| Position          | FEN                                                                        | Depth | Expected Nodes | Focus                 |
| ----------------- | -------------------------------------------------------------------------- | ----- | -------------- | --------------------- |
| Starting position | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`                 | 6     | 119,060,324    | Basic moves           |
| Kiwipete          | `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1`     | 4     | 4,085,603      | Castling, en passant  |
| Position 3        | `8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1`                                | 5     | 674,624        | Pawn promotion        |
| Position 4        | `r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1`         | 4     | 422,333        | Complex promotions    |
| Position 4 Mirror | `r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1`         | 4     | 422,333        | Complex promotions    |
| Position 5        | `rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8`                | 4     | 2,103,487      | Tactical positions    |
| Position 6        | `r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10` | 4     | 3,894,594      | Middlegame complexity |

## Troubleshooting Tips

### Performance Issues
- Use optimized builds for large depth tests
- Consider caching/memoization for repeated positions
- Profile to find bottlenecks in move generation

### Debugging Output
- Add verbose logging to see which moves are generated
- Print board state before/after moves
- Use divide output to isolate problematic branches

### Common Pitfalls
- **Off-by-one errors**: Depth 0 should return 1, not 0
- **Move format differences**: Ensure consistent move notation (e.g., "e2e4" vs "e2-e4")
- **Position parsing**: Verify FEN parsing handles all cases correctly
- **Turn management**: Make sure the correct side to move is used

Remember: Perft is deterministic. If your implementation gives different results than a known-good engine, your move generation has bugs. The systematic approach above will help you find and fix them efficiently.
