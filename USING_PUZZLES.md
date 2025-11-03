# Using Stockfish to debug failing puzzle tests

Once move generation of our chess engine is correct, see `USING_PERFT.md`, the next step is using
mate-in-N puzzles to verify basic correctness of the search algorithm. Then other (non-mate) puzzles
allow verification of evaluation, including the quiescence search.

## Prerequisites

You'll need:
- A known-good chess engine like Stockfish for reference
- Chess puzzles
- The `search-test` program provided with this chess engine

## Chess puzzles

This project uses puzzles downloaded from https://database.lichess.org/lichess_db_puzzle.csv. The
`puzzles/lichess_db_puzzle.csv` file contains millions of chess puzzles in a specific CSV format.
For our testing purposes the `FEN` and `Moves` fields are most important. The start position for a
puzzle is the given `FEN` after applying the first half-move from `Moves`. The remaining moves form
the solution of the puzzle. If the solution has more half-moves than our search depth, the tests
should skip that puzzle as the search-depth is insufficient to find the solution.

### What is a correct solution for a mate-in-N puzzle?

For mate-in-N puzzles, the puzzle is solved correctly if a solution is found with the same number of
moves as the given solution, as sometimes there is more than one equivalent mate.

### What is a correct solution for other (non-mate) puzzles?

For non-mate puzzles, the puzzle is considered solved when the first half-move of the found solution
matches that of the given solution. The file `puzzles/puzzles.csv` contains a subset of 100
non-mate-in-N puzzles.


## The `search_test` program

The `search_test` program can be found in the `build` directory. If missing or out of date,
use `make -j build/search_test` to rebuild it. The program can be used in two ways:
 - CSV puzzles: `grep mateIn5 puzzles/lichess_db_puzzle.csv | build/search_test 9`, or
 - Single position: `build/search-test "1k6/p4ppp/1P4r1/1R6/6r1/3Nb3/2P3bP/1Q4RK w - - 0 3" 9`

In the first form the program compares the found solution as the given (presumably correct)
solution. At the end of the run it will give a count of the number of tests run and a summary of the
failures as well as an ELO score derived from the rating of the puzzles:

    100 puzzles, 86 correct 10 too deep, 2 eval errors, 2 search errors, 2296 rating

The "too deep" outcome is not an error, as the search depth given was less than the length of the
correct solution in plies.

### Evaluation errors

Here the engine consider its solution as better than the correct solution. This can be the result of
incorrect quiescence search or disagreements in the evaluation of the position.

    Puzzle 006eO, rating 2186: "8/8/2p5/1p1p1k2/3P4/1PP1pK2/8/8 w - - 4 65" [b3b4, ]
    Evaluation error: Got [f3e3, f5g4, e3e2, g4g3, e2d2, ], but expected [b3b4, ]
    Got: -0.03 (white side) "8/8/2p5/1p1p4/3P4/1PP3k1/3K4/8 b - - 4 67"
    Expected: -0.71 (white side) "8/8/2p5/1p1p1k2/1P1P4/2P1pK2/8/8 b - - 0 65"

The FEN position given is the actual start position. The move sequence between `[ ]` is the correct
solution.

### Search errors

    Puzzle 007eS, rating 1240: "6k1/p4p2/1p5p/4r3/P3B3/1P2KP2/2P3PP/8 b - - 1 29" [f7f5, g2g4, f5e4, ]
    Search error: Got [g8g7, g2g4, e5c5, e4d3, c5e5, ], but expected [f7f5, g2g4, f5e4, ]
    Got: -0.16 (black side) "8/p4pk1/1p5p/4r3/P5P1/1P1BKP2/2P4P/8 w - - 3 32"
    Expected: 7.03 (black side) "6k1/p7/1p5p/4r3/P3p1P1/1P2KP2/2P4P/8 w - - 0 31"

Here the engine agrees that the correct solution is better, but it didn't find it.

## Step-by-Step Debugging Process

### Step 1: Reproduce in single-shot mode

If the error is found during a multi-puzzle run (from a CSV file), use the error output to find the
start position in FEN notation. It's on the line starting with the word "Puzzle". Look in the
`Makefile` to find the search depth, or assume depth 5 for mate in 2 or 3, depth 9 for mate in 4 or
5, or depth 6 for other puzzles.

Example output for an error in a mate-in-4 problem:

    Puzzle 0HBeT, rating 2174: "1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32" [g4g2, ... ]
    Evaluation error: Got [b7g2, ], but expected [g4g2, b6a7, b8a8, b5b7, g2g1, b1g1, g6g1, ]
    Got: M1 (black side) "1k6/p4ppp/1P4r1/1R6/6r1/3Nb3/2P3bP/1Q4RK w - - 0 33"
    Expected: M4 (black side) "k7/PR3ppp/8/8/8/3Nb3/2P4P/6rK w - - 0 36"

Reproduce with:

    build/search-test "1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32" 9

The output will have statistics and performance numbers as well as the following result line:

    Best Move: mate 1 pv b7g2 for 1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32

We reproduced the problem as the found move b7g2 is not the expected move g4g2.

### Step 2: Verify move generation

Run `perft` to check the move generation for the start position:

    build/perft "1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32" 6
    echo -e "1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32\ngo perft 6" |
      stockfish | grep Nodes

The output for both commands should be the same:

    Nodes searched: 3029694883

If not, see `USING_PERFT.md` to further debug move generation.

### Step 3: Use the first half-move in the solution to simplify the reproducer

Determine the position after playing the first half-move:

    echo -e 'position fen 1k6/pb3ppp/1P4r1/1R6/6r1/3Nb3/2P3PP/1Q4RK b - - 0 32 moves g4g2\nd' |
      stockfish

The output has the line:

    Fen: 1k6/pb3ppp/1P4r1/1R6/8/3Nb3/2P3rP/1Q4RK w - - 0 33

Now have the `search-test` program try this new start position at one less depth:

    build/search-test "1k6/pb3ppp/1P4r1/1R6/8/3Nb3/2P3rP/1Q4RK w - - 0 33" 4

In this example, the bug still reproduces the bug at the lower depth:

    Best Move: cp -4 pv g8g7 g2g4 h6h5 f3f4 for 6k1/p4p2/1p5p/4r3/P3B3/1P2KP2/2P3PP/8 b - - 1 29

The proposed move g8g7 does not match the next move in the solution, g2g4. The new problem is
at less depth and that still reproduced the bug. So, go back to Step 1 until the minimal reproducer
has been found.

