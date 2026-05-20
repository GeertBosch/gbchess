# Continuous Integration Puzzle Tests

The `lichess/` directory has the following puzzle files:

     ci_mate123_4000.csv
     ci_mate45_100.csv
     ci_nonmate_100.csv

They contain mate-in-N and non-mate puzzles as implied by the file name. The final number in the
name indicates the number of puzzles in the file. These have been derived from
https://database.lichess.org/lichess_db_puzzle.csv.zst. The reason they've been copied here rather
than download them as part of the build is to both ensure stability (no puzzle regressions due to
the introduction of new puzzles) and not needlessly put load on lichess servers downloading a very
large archive of which we need just a small part.

Puzzles are a great way to get started with testing a chess engine. The tables below generally list
the results for chess engines in a passed / ELO format. For example, the first result of Stockfish
12 below lists 3858 / 2017 indicating 3,858 out of 4,000 puzzles passed, and that the "puzzle ELO"
is 2017. This puzzle ELO results in losing more rating points when failing lower rated puzzles and
gaining more rating points when solving higher rated ones. It's a crude measure for puzzle solving ability.

## 4,000 Mate-in-123 Puzzles

The mate-in-N puzzles do not depend on strength of evaluation at all and should be solvable even by
rudimentary engines without any optimizations, fancy evaluation, or even quiescence search. However,
it is illustrative that Stockfish needs high depth to solve all of them, clearly showing the tension
between more complete tactical exploration of the search tree vs. more strategic positional play
with sophisticated evaluation and aggressive pruning. In actual game play it may be better to spend
time on the latter rather than focus on the former.

| Depth |    Stockfish 12 |  Stockfish 16.1 |       GNU Chess |        GB Chess |
| ----: | --------------: | --------------: | --------------: | --------------: |
|     2 |     3858 / 2017 |     3561 / 1702 | **4000** / 2717 |     3756 / 1891 |
|     3 |     3869 / 2058 |     3584 / 1746 |                 |     3949 / 2380 |
|     4 |     3874 / 2041 |     3628 / 1786 |                 |     3983 / 2528 |
|     5 |     3896 / 2070 |     3692 / 1862 |                 |     3999 / 2702 |
|     6 |     3915 / 2101 |     3824 / 1989 |                 | **4000** / 2717 |
|     7 |     3950 / 2220 |     3913 / 2155 |                 |                 |
|     8 |     3969 / 2343 |     3944 / 2275 |                 |                 |
|     9 |     3982 / 2448 |     3969 / 2349 |                 |                 |
|    10 |     3992 / 2560 |     3978 / 2413 |                 |                 |
|    11 |     3998 / 2663 |     3987 / 2474 |                 |                 |
|    12 | **4000** / 2717 |     3992 / 2544 |                 |                 |
|    13 |                 |     3995 / 2599 |                 |                 |
|    14 |                 |     3998 / 2665 |                 |                 |
|    15 |                 |     3999 / 2687 |                 |                 |
|    16 |                 | **4000** / 2717 |                 |                 |

## 100 Mate-in-45 Puzzles

As these problems require significantly deeper search, they rely on reasonable efficiency of the
engine to complete in an acceptable amount of time. Efficiency here means limit the number of nodes
to evaluate to reach a certain depth in the search. The more tactical search of the Stockfish
engine is paying of.

Let's consider the last problem solved for each engine and the depth needed to solve that problem,
as well as the number of nodes evaluated.

|          Engine | Problem | Depth |      Nodes | Relative |
| :-------------- | :------ | ----: | ---------: | -------: |
|    Stockfish 12 |   0K45A |    18 |    159,435 |     3.1x |
|  Stockfish 16.1 |   065sJ |    16 | **51,172** | **1.0x** |
| GNU Chess 6.2.9 |   0G1P5 | **7** |    886,044 |    17.3x |
|        GB Chess |   0G1P5 |    14 |    876,458 |    17.1x |

| Depth |   Stockfish 12 | Stockfish 16.1 |      GNU Chess |       GB Chess |
| ----: | -------------: | -------------: | -------------: | -------------: |
|     2 |      40 / 1736 |      28 / 1530 |      93 / 2449 |      45 / 1781 |
|     3 |      40 / 1717 |      32 / 1583 |      95 / 2479 |      65 / 2021 |
|     4 |      43 / 1761 |      36 / 1596 |      98 / 2511 |      81 / 2246 |
|     5 |      47 / 1794 |      37 / 1637 |      98 / 2511 |      93 / 2423 |
|     6 |      56 / 1921 |      51 / 1835 |      98 / 2511 |      97 / 2508 |
|     7 |      66 / 2073 |      70 / 2068 | **100** / 2562 |      98 / 2531 |
|     8 |      73 / 2169 |      80 / 2215 |                |      98 / 2531 |
|     9 |      85 / 2346 |      85 / 2287 |                |      98 / 2531 |
|    10 |      90 / 2422 |      93 / 2449 |                |      98 / 2531 |
|    11 |      94 / 2467 |      95 / 2479 |                |      99 / 2542 |
|    12 |      96 / 2491 |      98 / 2530 |                |      99 / 2542 |
|    13 |      97 / 2507 |      98 / 2530 |                |      99 / 2542 |
|    14 |      98 / 2518 |      98 / 2530 |                | **100** / 2562 |
|    15 |      99 / 2532 |      99 / 2552 |                |                |
|    16 |      99 / 2532 | **100** / 2562 |                |                |
|    17 |      99 / 2532 |                |                |                |
|    18 | **100** / 2562 |                |                |                |

## 100 Non-mate Puzzles

| Depth |   Stockfish 12 | Stockfish 16.1 | GNU Chess |       GB Chess |
| ----: | -------------: | -------------: | --------: | -------------: |
|     2 |      69 / 1823 |      80 / 2009 | 79 / 1972 |      79 / 2008 |
|     3 |      74 / 1906 |      79 / 1966 | 83 / 2049 |      86 / 2111 |
|     4 |      76 / 1931 |      79 / 1967 | 84 / 2083 |      89 / 2183 |
|     5 |      79 / 1964 |      81 / 2021 | 86 / 2112 |      89 / 2174 |
|     6 |      82 / 2046 |      86 / 2110 | 90 / 2186 |      94 / 2254 |
|     7 |      84 / 2064 |      95 / 2270 | 92 / 2210 |      95 / 2262 |
|     8 |      86 / 2089 |      98 / 2342 | 94 / 2243 |      98 / 2339 |
|     9 |      89 / 2181 |      96 / 2293 | 95 / 2269 |      95 / 2272 |
|    10 |      91 / 2206 |      97 / 2317 | 96 / 2280 |      97 / 2327 |
|    11 |      94 / 2254 |      98 / 2342 | 97 / 2308 |      97 / 2327 |
|    12 |      94 / 2254 |      99 / 2363 | (timeout) |      98 / 2352 |
|    13 |      96 / 2293 | **100** / 2395 |         — |      98 / 2340 |
|    14 |      96 / 2293 |                |         — |      98 / 2353 |
|    15 |      98 / 2340 |                |         — |      99 / 2372 |
|    16 |      98 / 2340 |                |         — | **100** / 2395 |
|    17 |      99 / 2363 |                |         — |                |
|    18 |      99 / 2363 |                |         — |                |
|    19 |      99 / 2363 |                |         — |                |
|    20 |      99 / 2363 |                |         — |                |
|    21 |      99 / 2363 |                |         — |                |
|    22 |      99 / 2363 |                |         — |                |
|    23 | **100** / 2395 |                |         — |                |

### Limiting by Nodes

When limiting nodes instead of depth, which is fixed at 23 here, the great efficiency of Stockfish
becomes very clear:

|      Nodes |   Stockfish 12 | Stockfish 16.1 |       GB Chess |
| :--------: | -------------: | -------------: | -------------: |
|     10,000 |      91 / 2197 |      97 / 2325 |      90 / 2194 |
|     20,000 |      94 / 2254 |      99 / 2372 |      92 / 2228 |
|     50,000 |      95 / 2271 | **100** / 2395 |      95 / 2261 |
|    100,000 |      97 / 2321 |                |      98 / 2340 |
|    200,000 |      98 / 2340 |                |      98 / 2320 |
|    500,000 | **100** / 2395 |                |      98 / 2352 |
|  1,000,000 |                |                |      98 / 2352 |
|  2,000,000 |                |                |      98 / 2340 |
|  5,000,000 |                |                |      98 / 2340 |
| 10,000,000 |                |                | **100** / 2395 |

Probably because of some incorrect pruning, GB Chess needs to visit a hundred times as many nodes to
solve the last two problems as opposed to the 98 problems solved with just 100,000 nodes. Note that
GNU Chess was omitted here as it doesn't seem to properly limit by node count.

### Limiting by Movetime

Looking at just nodes vs ELO above, GB chess keeps up with Stockfish 12 until about 100,000 nodes.
Let's see how that translates to actual move time. Here it's clear how far ahead Stockfish 16.1 is.
At just 20ms per move it solves all but one puzzles, a performance Stockfish 12 and GB Chess don't
even reach at 10 seconds per move.

| Depth | Movetime (ms) | Stockfish 12 | Stockfish 16.1 |  GB Chess |
| ----: | ------------: | -----------: | -------------: | --------: |
|    16 |            10 |    92 / 2230 |      95 / 2292 | 86 / 2123 |
|    16 |            20 |    93 / 2234 |      99 / 2374 | 91 / 2212 |
|    16 |            50 |    94 / 2254 |      97 / 2325 | 90 / 2194 |
|    16 |           100 |    95 / 2272 |      99 / 2372 | 93 / 2243 |
|    16 |           200 |    97 / 2321 |     100 / 2395 | 93 / 2232 |
|    16 |           500 |    98 / 2340 |                | 96 / 2291 |
|    16 |         1,000 |    98 / 2340 |                | 97 / 2320 |
|    16 |         2,000 |    98 / 2340 |                | 98 / 2352 |
|    16 |         5,000 |    98 / 2340 |                | 98 / 2352 |
|    16 |        10,000 |    98 / 2340 |                | 98 / 2340 |
