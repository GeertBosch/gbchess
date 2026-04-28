# Search Optimization Analysis

## Test Position
```
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
Depth: 9
```

Both engines find `c2h7` as the best move at depth 9.

## Node Count Comparison

| Engine | Nodes | Seldepth | Time | Best Move | Score |
|--------|-------|----------|------|-----------|-------|
| **Stockfish 12** | 9,503 | 12 | 8ms | c2h7 | +3.76 |
| **gbchess** | 1,090,423 | 10 | 3,076ms | c2h7 | +1.52 |
| **Ratio** | **115x** | 0.83x | 385x | — | — |

Note: Both engines report "seldepth" (maximum depth reached in main search including extensions,
excluding quiescence).

## Detailed Statistics Comparison (depth 9)

| Metric | Stockfish 12 | gbchess | Ratio |
|--------|--------------|---------|-------|
| **Total Nodes** | 9,503 | 1,090,423 | **115x** |
| **Null Move Attempts** | 1,467 (15.4% of nodes) | 515 (0.05% of nodes) | **300x fewer** |
| **Null Move Cutoffs** | 1,075 (73% of attempts) | 315 (61% of attempts) | Similar |
| **Beta Cutoffs** | 1,575 (16.6% of nodes) | 50,141 (4.6% of nodes) | Lower % |
| **First-Move Cutoffs** | 1,373 (87% of beta) | 41,287 (82% of beta) | Similar ordering |

Node counts at other depths:

| Depth | Stockfish 12 | gbchess | Ratio |
|-------|-------------|---------|-------|
| 5 | 472 | 13,049 | 28x |
| 7 | 2,155 | 217,183 | 101x |
| 9 | 9,503 | 1,090,423 | 115x |

## Root Cause: Null Move Pruning Barely Used

The dominant issue is that gbchess attempts null move pruning at **0.05% of nodes** vs
Stockfish's **15.4%** — a 300x difference. At depth 9, gbchess encounters ~45,000 positions
eligible for NMP but attempts it at only 515 of them.

Breaking down why NMP is skipped (depth 9):

| Skip Reason | Count | % of all skips |
|-------------|-------|----------------|
| Null-window node (`beta == alpha + 1`) | 44,337 | **98.9%** |
| Position behind (static eval < beta) | 371 | 0.8% |
| In check | 108 | 0.2% |
| Mate score in beta | 3 | ~0% |
| Endgame (no non-pawn material) | 0 | 0% |

The null-window skip condition in `tryNullMovePruning` is:
```cpp
// Skip null-move in null-window (PVS probe) nodes.
if (beta.cp() - 1 <= alpha.cp()) {   // true when beta == alpha + 1
    ++nullMoveSkippedPV;
    return false;
}
```

This condition fires when `beta == alpha + 1`, which identifies **null-window (non-PV) nodes** —
but these are precisely the nodes where NMP *should* be applied. Standard practice (and what
Stockfish does) is to skip NMP in **PV nodes** (full-window, `beta > alpha + 1`) and allow it
in null-window nodes. The condition is inverted, causing NMP to be suppressed in ~99% of
eligible positions.

## Implemented Optimizations

### ✅ Already in gbchess
1. **Iterative Deepening** — Yes
2. **Aspiration Windows** — Yes (windows: 30cp, 125cp)
3. **Transposition Table** — Yes (16 MB)
4. **Null Move Pruning** — Yes (R=3, min_depth=2) — but effectively disabled by inverted PV check
5. **Late Move Reductions (LMR)** — Yes
6. **Killer Move Heuristic** — Yes (2 killers per depth)
7. **History Heuristic** — Yes
8. **Countermove Heuristic** — Yes
9. **MVV/LVA Move Ordering** — Yes
10. **Static Exchange Evaluation (SEE)** — Yes (in quiescence)
11. **Quiescence Search** — Yes (depth=5, includes checks)
12. **Reverse Futility Pruning** — Yes (max depth=3, margin=100cp×depth+100cp)
13. **Principal Variation Search (PVS)** — Yes

### ❌ Missing Optimizations

#### High Impact
1. **Futility Pruning (Move Level)** — Skip quiet moves when static eval + margin ≤ alpha
2. **Move Count Pruning** — After N moves searched, skip remaining quiet moves
3. **SEE Pruning in Main Search** — Skip moves with negative SEE at shallow depth
   (gbchess uses SEE only in quiescence)

#### Medium Impact
4. **Dynamic Null Move Reduction** — Stockfish uses `R = f(depth, eval-beta)` rather than fixed R=3
5. **Razoring** — At depth 1, if eval << alpha, go directly to quiescence
6. **ProbCut** — Beta cutoff after a shallow reduced-depth search

### 🔧 Known Bug: NMP Condition Inverted

**Current code** (in `tryNullMovePruning`):
```cpp
// Skips NMP when beta == alpha + 1 (null-window = non-PV nodes)
if (beta.cp() - 1 <= alpha.cp()) { ++nullMoveSkippedPV; return false; }
```

**Should be:**
```cpp
// Skip NMP in PV nodes (full-window: beta > alpha + 1)
if (beta.cp() - alpha.cp() > 1) { ++nullMoveSkippedPV; return false; }
```

This single fix would bring NMP attempts from 515 to ~32,000 at depth 9 (matching Stockfish's
~15% rate), and is expected to be the dominant remaining performance issue.

## Estimated Impact

**Current state:** 1,090,423 nodes, 3,076ms at depth 9 (115x vs Stockfish)

Fixing the NMP condition inversion alone is expected to reduce nodes by ~4-6x based on the
null move skip breakdown (98.9% of skips are the inverted condition). This would bring gbchess
to roughly 180K–275K nodes at depth 9 (~20-30x vs Stockfish).

The remaining ~20-30x gap after fixing NMP would be addressable through:
- **Move Count Pruning**: 1.5–2x reduction
- **Futility Pruning (Move Level)**: 1.3–1.5x reduction
- **SEE Pruning in Main Search**: 1.2–1.4x reduction
- **Dynamic NMP Reduction**: ~1.2x reduction

## Recommended Fix Order

1. **Fix NMP PV condition** (inverted check, single line) — expected ~4-6x node reduction
2. **Move Count Pruning** — prune late quiet moves
3. **SEE Pruning in Main Search** — extend existing SEE to main search
4. **Futility Pruning (Move Level)** — prune moves that can't improve alpha
5. **Dynamic Null Move Reduction** — scale R with depth and margin

## Code Locations

### gbchess `src/search/search.cpp`
- `tryNullMovePruning()`: null move pruning (includes inverted PV check)
- `alphaBeta()`: main search loop, LMR, PVS, reverse futility pruning
- `quiesce()`: quiescence search with SEE

### `src/core/options.h`
- All tunable parameters: `nullMoveReduction`, `nullMoveMinDepth`, `reverseFutilityMaxDepth`, etc.
