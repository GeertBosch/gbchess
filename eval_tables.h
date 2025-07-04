#pragma once

#include <array>

#include "common.h"
#include "eval.h"

// Values of pieces, in centipawns (using _cp suffix to indicate centipawns).
// The values are in order from the lowest rank to the highest, from file a to h.
// See https://chessprogramming.wikispaces.com/Simplified+evaluation+function#Piece-SquareTables


namespace Bill_Jordan {
constexpr PieceValueTable pieceValues = {100_cp, 300_cp, 300_cp, 500_cp, 900_cp};

// Bill Jordan's piece-square value tables
constexpr SquareTable pawn = {
    0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,    // rank 1
    0_cp,   2_cp,   4_cp,   -12_cp, -12_cp, 4_cp,   2_cp,   0_cp,    // rank 2
    0_cp,   2_cp,   4_cp,   4_cp,   4_cp,   4_cp,   2_cp,   0_cp,    // rank 3
    0_cp,   2_cp,   4_cp,   8_cp,   8_cp,   4_cp,   2_cp,   0_cp,    // rank 4
    0_cp,   2_cp,   4_cp,   8_cp,   8_cp,   4_cp,   2_cp,   0_cp,    // rank 5
    4_cp,   8_cp,   10_cp,  16_cp,  16_cp,  10_cp,  8_cp,   4_cp,    // rank 6
    100_cp, 100_cp, 100_cp, 100_cp, 100_cp, 100_cp, 100_cp, 100_cp,  // rank 7
    0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,    // rank 8
};

constexpr SquareTable knight = {
    -30_cp,  -20_cp, -10_cp, -8_cp, -8_cp, -10_cp, -20_cp, -30_cp,   // rank 1
    -16_cp,  -6_cp,  -2_cp,  0_cp,  0_cp,  -2_cp,  -6_cp,  -16_cp,   // rank 2
    -8_cp,   -2_cp,  4_cp,   6_cp,  6_cp,  4_cp,   -2_cp,  -8_cp,    // rank 3
    -5_cp,   0_cp,   6_cp,   8_cp,  8_cp,  6_cp,   0_cp,   -5_cp,    // rank 4
    -5_cp,   0_cp,   6_cp,   8_cp,  8_cp,  6_cp,   0_cp,   -5_cp,    // rank 5
    -10_cp,  -2_cp,  4_cp,   6_cp,  6_cp,  4_cp,   -2_cp,  -10_cp,   // rank 6
    -20_cp,  -10_cp, -2_cp,  0_cp,  0_cp,  -2_cp,  -10_cp, -20_cp,   // rank 7
    -150_cp, -20_cp, -10_cp, -5_cp, -5_cp, -10_cp, -20_cp, -150_cp,  // rank 8
};

constexpr SquareTable bishop = {
    -10_cp, -10_cp, -12_cp, -10_cp, -10_cp, -12_cp, -10_cp, -10_cp,  // rank 1
    0_cp,   4_cp,   4_cp,   4_cp,   4_cp,   4_cp,   4_cp,   0_cp,    // rank 2
    2_cp,   4_cp,   6_cp,   6_cp,   6_cp,   6_cp,   4_cp,   2_cp,    // rank 3
    2_cp,   4_cp,   6_cp,   8_cp,   8_cp,   6_cp,   4_cp,   2_cp,    // rank 4
    2_cp,   4_cp,   6_cp,   8_cp,   8_cp,   6_cp,   4_cp,   2_cp,    // rank 5
    2_cp,   4_cp,   6_cp,   6_cp,   6_cp,   6_cp,   4_cp,   2_cp,    // rank 6
    -10_cp, 4_cp,   4_cp,   4_cp,   4_cp,   4_cp,   4_cp,   -10_cp,  // rank 7
    -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp,  // rank 8
};

constexpr SquareTable rookScores = {
    4_cp,  4_cp,  4_cp,  6_cp,  6_cp,  4_cp,  4_cp,  4_cp,   // rank 1
    0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,   // rank 2
    0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,   // rank 3
    0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,   // rank 4
    0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,   // rank 5
    0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,   // rank 6
    20_cp, 20_cp, 20_cp, 20_cp, 20_cp, 20_cp, 20_cp, 20_cp,  // rank 7
    10_cp, 10_cp, 10_cp, 10_cp, 10_cp, 10_cp, 10_cp, 10_cp,  // rank 8
};

constexpr SquareTable queen = {
    -10_cp, -10_cp, -6_cp, -4_cp, -4_cp, -6_cp, -10_cp, -10_cp,  // rank 1
    -10_cp, 2_cp,   2_cp,  2_cp,  2_cp,  2_cp,  2_cp,   -10_cp,  // rank 2
    2_cp,   2_cp,   2_cp,  3_cp,  3_cp,  2_cp,  2_cp,   2_cp,    // rank 3
    2_cp,   2_cp,   3_cp,  4_cp,  4_cp,  3_cp,  2_cp,   2_cp,    // rank 4
    2_cp,   2_cp,   2_cp,  3_cp,  3_cp,  2_cp,  2_cp,   2_cp,    // rank 5
    -10_cp, 2_cp,   2_cp,  2_cp,  2_cp,  2_cp,  2_cp,   -10_cp,  // rank 6
    -10_cp, -10_cp, 2_cp,  2_cp,  2_cp,  2_cp,  -10_cp, -10_cp,  // rank 7
};

constexpr SquareTable kingMiddlegame = {
    20_cp,  20_cp,  20_cp,  -40_cp, 10_cp,  -60_cp, 20_cp,  20_cp,   // rank 1
    15_cp,  2_cp,   -25_cp, -30_cp, -30_cp, -45_cp, 20_cp,  15_cp,   // rank 2
    -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp,  // rank 3
    -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp,  // rank 4
    -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp,  // rank 5
    -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp,  // rank 6
    -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp,  // rank 7
    -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp, -48_cp,  // rank 8
};

constexpr SquareTable kingEndgame = {
    0_cp,  8_cp,  16_cp, 18_cp, 18_cp, 16_cp, 8_cp,  0_cp,   // rank 1
    8_cp,  16_cp, 24_cp, 32_cp, 32_cp, 24_cp, 16_cp, 8_cp,   // rank 2
    16_cp, 24_cp, 32_cp, 40_cp, 40_cp, 32_cp, 24_cp, 16_cp,  // rank 3
    25_cp, 32_cp, 40_cp, 48_cp, 48_cp, 40_cp, 32_cp, 25_cp,  // rank 4
    25_cp, 32_cp, 40_cp, 48_cp, 48_cp, 40_cp, 32_cp, 25_cp,  // rank 5
    16_cp, 24_cp, 32_cp, 40_cp, 40_cp, 32_cp, 24_cp, 16_cp,  // rank 6
    8_cp,  16_cp, 24_cp, 32_cp, 32_cp, 24_cp, 16_cp, 8_cp,   // rank 7
    0_cp,  8_cp,  16_cp, 18_cp, 18_cp, 16_cp, 8_cp,  0_cp,   // rank 8
};

constexpr PieceSquareTable middlegame = {pawn, knight, bishop, rookScores, queen, kingMiddlegame};
constexpr PieceSquareTable endgame = {pawn, knight, bishop, rookScores, queen, kingEndgame};
constexpr TaperedPieceSquareTable tapered = {middlegame, endgame};
}  // namespace Bill_Jordan

namespace Tomasz_Michniewski {
// See https://www.chessprogramming.org/Simplified_Evaluation_Function
constexpr PieceValueTable pieceValues = {100_cp, 320_cp, 330_cp, 500_cp, 900_cp};
constexpr SquareTable pawn = {
    0_cp,  0_cp,  0_cp,   0_cp,   0_cp,   0_cp,   0_cp,  0_cp,   // rank 1
    5_cp,  10_cp, 10_cp,  -20_cp, -20_cp, 10_cp,  10_cp, 5_cp,   // rank 2
    5_cp,  -5_cp, -10_cp, 0_cp,   0_cp,   -10_cp, -5_cp, 5_cp,   // rank 3
    0_cp,  0_cp,  0_cp,   20_cp,  20_cp,  0_cp,   0_cp,  0_cp,   // rank 4
    5_cp,  5_cp,  10_cp,  25_cp,  25_cp,  10_cp,  5_cp,  5_cp,   // rank 5
    10_cp, 10_cp, 20_cp,  30_cp,  30_cp,  20_cp,  10_cp, 10_cp,  // rank 6
    50_cp, 50_cp, 50_cp,  50_cp,  50_cp,  50_cp,  50_cp, 50_cp,  // rank 7
    0_cp,  0_cp,  0_cp,   0_cp,   0_cp,   0_cp,   0_cp,  0_cp,   // rank 8
};
constexpr SquareTable knight = {
    -50_cp, -40_cp, -30_cp, -30_cp, -30_cp, -30_cp, -40_cp, -50_cp,  // rank 1
    -40_cp, -20_cp, 0_cp,   5_cp,   5_cp,   0_cp,   -20_cp, -40_cp,  // rank 2
    -30_cp, 5_cp,   10_cp,  15_cp,  15_cp,  10_cp,  5_cp,   -30_cp,  // rank 3
    -30_cp, 0_cp,   15_cp,  20_cp,  20_cp,  15_cp,  0_cp,   -30_cp,  // rank 4
    -30_cp, 5_cp,   15_cp,  20_cp,  20_cp,  15_cp,  5_cp,   -30_cp,  // rank 5
    -30_cp, 0_cp,   10_cp,  15_cp,  15_cp,  10_cp,  0_cp,   -30_cp,  // rank 6
    -40_cp, -20_cp, 0_cp,   0_cp,   0_cp,   0_cp,   -20_cp, -40_cp,  // rank 7
    -50_cp, -40_cp, -30_cp, -30_cp, -30_cp, -30_cp, -40_cp, -50_cp,  // rank 8
};
constexpr SquareTable bishop = {
    -20_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -20_cp,  // rank 8
    -10_cp, 5_cp,   0_cp,   0_cp,   0_cp,   0_cp,   5_cp,   -10_cp,  // rank 7
    -10_cp, 10_cp,  10_cp,  10_cp,  10_cp,  10_cp,  10_cp,  -10_cp,  // rank 6
    -10_cp, 0_cp,   10_cp,  10_cp,  10_cp,  10_cp,  0_cp,   -10_cp,  // rank 5
    -10_cp, 5_cp,   5_cp,   10_cp,  10_cp,  5_cp,   5_cp,   -10_cp,  // rank 4
    -10_cp, 0_cp,   5_cp,   10_cp,  10_cp,  5_cp,   0_cp,   -10_cp,  // rank 3
    -10_cp, 0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   0_cp,   -10_cp,  // rank 2
    -20_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -10_cp, -20_cp,  // rank 1
};
constexpr SquareTable rook = {
    0_cp,  0_cp,  0_cp,  5_cp,  5_cp,  0_cp,  0_cp,  0_cp,   // rank 8
    -5_cp, 0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  -5_cp,  // rank 7
    -5_cp, 0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  -5_cp,  // rank 6
    -5_cp, 0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  -5_cp,  // rank 5
    -5_cp, 0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  -5_cp,  // rank 4
    -5_cp, 0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  -5_cp,  // rank 3
    5_cp,  10_cp, 10_cp, 10_cp, 10_cp, 10_cp, 10_cp, 5_cp,   // rank 2
    0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,  0_cp,   // rank 1
};
constexpr SquareTable queen = {
    -20_cp, -10_cp, -10_cp, -5_cp, -5_cp, -10_cp, -10_cp, -20_cp,  // rank 1
    -10_cp, 0_cp,   5_cp,   0_cp,  0_cp,  0_cp,   0_cp,   -10_cp,  // rank 2
    -10_cp, 5_cp,   5_cp,   5_cp,  5_cp,  5_cp,   0_cp,   -10_cp,  // rank 3
    0_cp,   0_cp,   5_cp,   5_cp,  5_cp,  5_cp,   0_cp,   -5_cp,   // rank 4
    -5_cp,  0_cp,   5_cp,   5_cp,  5_cp,  5_cp,   0_cp,   -5_cp,   // rank 5
    -10_cp, 0_cp,   5_cp,   5_cp,  5_cp,  5_cp,   0_cp,   -10_cp,  // rank 6
    -10_cp, 0_cp,   0_cp,   0_cp,  0_cp,  0_cp,   0_cp,   -10_cp,  // rank 7
    -20_cp, -10_cp, -10_cp, -5_cp, -5_cp, -10_cp, -10_cp, -20_cp,  // rank 8
};
constexpr SquareTable kingMiddlegame = {
    20_cp,  30_cp,  10_cp,  0_cp,   0_cp,   10_cp,  30_cp,  20_cp,   // rank 1
    20_cp,  20_cp,  0_cp,   0_cp,   0_cp,   0_cp,   20_cp,  20_cp,   // rank 2
    -10_cp, -20_cp, -20_cp, -20_cp, -20_cp, -20_cp, -20_cp, -10_cp,  // rank 3
    -20_cp, -30_cp, -30_cp, -40_cp, -40_cp, -30_cp, -30_cp, -20_cp,  // rank 4
    -30_cp, -40_cp, -40_cp, -50_cp, -50_cp, -40_cp, -40_cp, -30_cp,  // rank 5
    -30_cp, -40_cp, -40_cp, -50_cp, -50_cp, -40_cp, -40_cp, -30_cp,  // rank 6
    -30_cp, -40_cp, -40_cp, -50_cp, -50_cp, -40_cp, -40_cp, -30_cp,  // rank 7
    -30_cp, -40_cp, -40_cp, -50_cp, -50_cp, -40_cp, -40_cp, -30_cp,  // rank 8
};
constexpr SquareTable kingEndgame = {
    -50_cp, -30_cp, -30_cp, -30_cp, -30_cp, -30_cp, -30_cp, -50_cp,  // rank 1
    -30_cp, -30_cp, 0_cp,   0_cp,   0_cp,   0_cp,   -30_cp, -30_cp,  // rank 2
    -30_cp, -10_cp, 20_cp,  30_cp,  30_cp,  20_cp,  -10_cp, -30_cp,  // rank 3
    -30_cp, -10_cp, 30_cp,  40_cp,  40_cp,  30_cp,  -10_cp, -30_cp,  // rank 4
    -30_cp, -10_cp, 30_cp,  40_cp,  40_cp,  30_cp,  -10_cp, -30_cp,  // rank 5
    -30_cp, -10_cp, 20_cp,  30_cp,  30_cp,  20_cp,  -10_cp, -30_cp,  // rank 6
    -30_cp, -20_cp, -10_cp, 0_cp,   0_cp,   -10_cp, -20_cp, -30_cp,  // rank 7
    -50_cp, -40_cp, -30_cp, -20_cp, -20_cp, -30_cp, -40_cp, -50_cp,  // rank 8
};
constexpr PieceSquareTable middlegame = {pawn, knight, bishop, rook, queen, kingMiddlegame};
constexpr PieceSquareTable endgame = {pawn, knight, bishop, rook, queen, kingEndgame};
constexpr TaperedPieceSquareTable tapered = {middlegame, endgame};

}  // namespace Tomasz_Michniewski