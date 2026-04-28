# Search Optimization Analysis

## Test Position
```
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
Depth: 9
```

Both engines find `c2h7` as the best move at depth 9.

## Node Count Comparison

### After NMP PV-check fix (current)

| Engine | Nodes | Seldepth | Time | Best Move | Score |
|--------|-------|----------|------|-----------|-------|
| **Stockfish 12** | 9,503 | 12 | 8ms | c2h7 | +3.76 |
| **gbchess** | 822,718 | 11 | 2,454ms | c2h7 | +1.52 |
| **Ratio** | **87x** | 0.92x | 307x | — | — |

### Before NMP fix (for reference)

| Engine | Nodes | Seldepth | Time |
|--------|-------|----------|------|
| **Stockfish 12** | 9,503 | 12 | 8ms |
| **gbchess** | 1,090,423 | 10 | 3,076ms |
| **Ratio** | **115x** | 0.83x | 385x |

**The NMP fix reduced depth-9 nodes by 25% (1,090,423 → 822,718).**

Note: Both engines report "seldepth" (maximum depth reached in main search including extensions,
excluding quiescence).

## Detailed Statistics Comparison (depth 9, current)

| Metric | Stockfish 12 | gbchess | Ratio |
|--------|--------------|---------|-------|
| **Total Nodes** | 9,503 | 822,718 | **87x** |
| **Null Move Attempts** | 1,467 (15.4% of nodes) | 31,964 (3.9% of nodes) | **4x fewer** |
| **Null Move Cutoffs** | 1,075 (73% of attempts) | 25,655 (80% of attempts) | Similar |
| **Beta Cutoffs** | 1,575 (16.6% of nodes) | 33,447 (4.1% of nodes) | Lower % |
| **First-Move Cutoffs** | 1,373 (87% of beta) | 28,000 (83% of beta) | Similar ordering |

Node counts at multiple depths:

| Depth | Stockfish 12 | gbchess (current) | gbchess (pre-fix) | Ratio (current) |
|-------|-------------|-------------------|-------------------|-----------------|
| 5 | 472 | 34,521 | 13,049 | 73x |
| 7 | 2,155 | 235,585 | 217,183 | 109x |
| 9 | 9,503 | 822,718 | 1,090,423 | 87x |

Note that the NMP fix improved depth 9 significantly but slightly regressed depths 5 and 7.
This is expected: NMP with R=3 and min_depth=2 now fires aggressively at shallow depths where
the null-move search itself adds overhead that isn't yet recovered by pruning. NMP parameter
re-tuning is the first recommended next step.

## What the NMP Fix Did

The bug was an inverted PV-node check in `tryNullMovePruning`. The old condition
(`beta - 1 <= alpha`, i.e. `beta == alpha + 1`) fired in null-window (non-PV) nodes, which are
exactly where NMP *should* be applied. The fix flips this to skip NMP in PV (full-window) nodes.

**Before fix** — NMP skip breakdown at depth 9:

| Skip Reason | Count | % of skips |
|-------------|-------|------------|
| Null-window node (inverted bug) | 44,337 | **98.9%** |
| Position behind (eval < beta) | 371 | 0.8% |
| In check | 108 | 0.2% |

**After fix** — NMP attempts jumped from 515 to 31,964 (62x more), with 80% cutoff rate.

The remaining 4x gap in NMP attempt rate vs Stockfish (3.9% vs 15.4%) is mostly the `eval >= beta`
guard: we only try NMP when static eval ≥ beta (i.e. we're already ahead). Stockfish uses the
same guard but with a depth-scaled margin that allows more attempts when near-equal.

## Implemented Optimizations

### ✅ Already in gbchess
1. **Iterative Deepening** — Yes
2. **Aspiration Windows** — Yes (windows: 30cp, 125cp)
3. **Transposition Table** — Yes (16 MB)
4. **Null Move Pruning** — Yes (R=3, min_depth=2, PV-check bug fixed)
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
1. **Move Count Pruning (Late Move Pruning)** — After N moves searched at shallow depth, skip
   remaining quiet moves. Stockfish: `moveCount >= futility_move_count(improving, depth)`.
2. **SEE Pruning in Main Search** — Skip moves with negative SEE in the main search at shallow
   depth. Currently SEE is only used in quiescence.
3. **Futility Pruning (Move Level)** — Skip quiet moves when `staticEval + margin ≤ alpha`.

#### Medium Impact
4. **Dynamic Null Move Reduction** — Scale R with depth and eval margin.
   Stockfish: `R = (817 + 71*depth)/213 + min((eval-beta)/192, 3)`. Currently fixed at R=3.
5. **Razoring** — At depth 1, if eval << alpha, go directly to quiescence.
6. **ProbCut** — Try a reduced-depth search with a widened beta; cut if it fails high.

## Plan for Next Steps

Work through these in order, measuring before/after each change and reverting if it regresses
puzzles below 96/100 or increases nodes at depth 9.

### Step 1: Re-tune NMP Parameters

**Goal:** Recover the depth 5 and 7 regressions from the NMP fix without losing depth-9 gains.

The current `nullMoveMinDepth=2` and `R=3` combination now fires at many shallow nodes where
it adds overhead. Options to evaluate (one at a time):
- Raise `nullMoveMinDepth` from 2 to 3 — eliminates NMP at depth 2 nodes
- Switch to dynamic R: `R = 3 + depth/6` — more conservative at shallow depth, more aggressive
  at deep depth (like Stockfish's formula)
- Add `eval >= beta + margin` condition rather than strict `eval >= beta`

Measure at depths 5, 7, and 9 each time. Accept only if depth 9 improves or holds and
depths 5/7 also improve vs the post-fix baseline.

### Step 2: Move Count Pruning

**Goal:** Prune late quiet moves at shallow depths where they are unlikely to improve.

After searching the first few moves at a node (say, the first `3 + depth²` moves), skip
remaining quiet moves. This directly reduces the branching factor for moves ranked low by
the move ordering heuristics. Implement, then verify:
- `make test -j` passes
- Puzzles ≥ 96/100
- Node count at depth 9 decreases

### Step 3: SEE Pruning in Main Search

**Goal:** Skip clearly losing captures and quiet moves at shallow depth.

At depth ≤ 2–3, skip any move whose SEE score is below a threshold (e.g. −50cp for captures,
−10cp for quiet moves). SEE is already computed correctly in the codebase for quiescence; extend
its use to `alphaBeta()`. Validate with the same quality checks.

### Step 4: Futility Pruning (Move Level)

**Goal:** Skip quiet moves that cannot possibly improve alpha.

At shallow depth (≤ 2), skip quiet moves where `staticEval + futilityMargin(depth) ≤ alpha`.
The margin needs tuning; start with Stockfish's values (around 150cp at depth 1, 300cp at
depth 2) and adjust based on puzzle quality. This is more conservative than reverse futility
pruning (which already fires at the node level) and targets individual move decisions.

### Step 5: Dynamic Null Move Reduction

**Goal:** Make NMP more aggressive at deeper depths where savings are larger.

Replace fixed `R=3` with a depth-and-margin-scaled formula. After Steps 1–4 are complete and
baselines are re-established, try: `R = 3 + depth/6 + min((staticEval - beta) / 200, 1)`.
This reduces more aggressively when the position is clearly ahead and at greater depth.

## Code Locations

### `src/search/search.cpp`
- `tryNullMovePruning()`: null move pruning
- `alphaBeta()`: main search loop, LMR, PVS, reverse futility pruning
- `quiesce()`: quiescence search with SEE

### `src/core/options.h`
- All tunable parameters: `nullMoveReduction`, `nullMoveMinDepth`, `reverseFutilityMaxDepth`, etc.
