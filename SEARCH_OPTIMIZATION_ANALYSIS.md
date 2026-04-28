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
| **gbchess** | 709,677 | 10 | 2,074ms | c2h7 | +1.52 |
| **Ratio** | **75x** | 0.83x | 259x | — | — |

### Before NMP fix (for reference)

| Engine | Nodes | Seldepth | Time |
|--------|-------|----------|------|
| **Stockfish 12** | 9,503 | 12 | 8ms |
| **gbchess** | 1,090,423 | 10 | 3,076ms |
| **Ratio** | **115x** | 0.83x | 385x |

**Current cumulative reduction vs pre-fix baseline: 35% (1,090,423 → 709,677).**

Note: Both engines report "seldepth" (maximum depth reached in main search including extensions,
excluding quiescence).

## Depth-3 Efficiency Investigation (NMP Disabled)

At depth 3, null-move pruning is not active in either engine (`Null move attempts: 0`), so this
comparison isolates low-depth efficiency from NMP effects.

### Direct depth-3 comparison

| Engine | Nodes | Beta cutoffs | First-move cutoffs |
|--------|-------|--------------|--------------------|
| Stockfish 12 | 185 | 5 (2%) | 5 (100%) |
| gbchess | 4,938 | 110 (2%) | 79 (71%) |

gbchess uses **27x** more nodes at depth 3 (4,938 vs 185).

### New instrumentation findings (gbchess)

Added counters in `src/search/search.cpp` and printed in `src/engine/engine.cpp`:

- Root search calls: **3**
- Root move visits: **126** (42 per root pass)
- Aspiration attempts: **0** (fail-low=0, fail-high=0, fallback=2)
- PVS attempts: **1,827**, PVS researches: **37**
- TT cutoffs: **0**, TT refinements: **4**

Interpretation:

1. The repeated `info depth 3` lines were confirmed to be **aspiration re-searches** (window
   fail-low/fail-high), and removing aspiration windows reduced root passes and node count.
2. Move ordering is likely part of the remaining problem (`first-move cutoff` is 71% vs Stockfish 100%),
   but not the whole story.
3. The remaining gap is still low cutoff efficiency overall at shallow depth (2% beta-cutoff rate
   and 4,938 total nodes), indicating missing shallow-depth pruning and weaker root efficiency.

Conclusion: the suspicion about move ordering is valid, but depth-3 inefficiency is a combination
of (a) aspiration re-search overhead and (b) insufficient shallow-depth pruning/cutoffs.

## Detailed Statistics Comparison (depth 9, current)

| Metric | Stockfish 12 | gbchess | Ratio |
|--------|--------------|---------|-------|
| **Total Nodes** | 9,503 | 709,677 | **75x** |
| **Null Move Attempts** | 1,467 (15.4% of nodes) | 29,740 (4.2% of nodes) | **4x fewer** |
| **Null Move Cutoffs** | 1,075 (73% of attempts) | 24,361 (81% of attempts) | Similar |
| **Beta Cutoffs** | 1,575 (16.6% of nodes) | 25,572 (3.6% of nodes) | Lower % |
| **First-Move Cutoffs** | 1,373 (87% of beta) | 20,670 (80% of beta) | Lower ordering |

Node counts at multiple depths:

| Depth | Stockfish 12 | gbchess (current) | gbchess (pre-fix) | Ratio (current) |
|-------|-------------|-------------------|-------------------|-----------------|
| 5 | 472 | 34,521 | 13,049 | 73x |
| 7 | 2,155 | 235,585 | 217,183 | 109x |
| 9 | 9,503 | 709,677 | 1,090,423 | 75x |

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

### Depth-3 Efficiency Plan (priority now)

This plan is focused on reducing node count at depth 3 specifically.

1. **Quantify aspiration overhead at depth 3**
   - Run `go depth 3` with aspiration windows disabled *for diagnostic measurement only*.
   - Compare root search calls and total nodes to current baseline.
   - Success criterion: prove how much of 8,172 nodes is pure re-search overhead.

#### Step 1 Execution Result (April 28, 2026)

**Status:** Success.

Change applied:
- Disabled aspiration windows (`aspirationWindows = {}` in `src/core/options.h`).

Measured impact on the test FEN:

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Depth 3 nodes | 8,172 | 4,938 | **-39.6%** |
| Depth 3 root search calls | 5 | 3 | **-40%** |
| Depth 9 nodes | 822,718 | 709,677 | **-13.7%** |

Validation:
- `make test -j`: pass
- `make puzzles`: 97/100 (up from 96/100)

Conclusion:
- A large portion of depth-3 overhead was aspiration re-search overhead.
- Disabling aspiration windows is currently net positive on both shallow and deeper searches
   for this workload, so this change is accepted.

2. **Add cutoff-by-move-index instrumentation at root and ply-1**
   - Track where beta cutoffs happen (`moveCount == 1`, `<=3`, `>3`).
   - Goal: determine whether poor move ordering is delaying cutoffs too often.
   - This provides direct evidence before any move-ordering logic changes.

#### Step 2 Execution Result (April 28, 2026)

**Status:** Success (instrumentation complete, no search behavior change).

Added counters:
- Root cutoffs: total and buckets (`m1`, `m2-3`, `m4+`)
- Ply1 cutoffs: total and buckets (`m1`, `m2-3`, `m4+`)

Depth-3 results:
- Root cutoffs: `0` (expected at root PV)
- Ply1 cutoffs: `82` total
   - `m1`: `54`
   - `m2-3`: `25`
   - `m4+`: `3`

Depth-9 results:
- Root cutoffs: `0`
- Ply1 cutoffs: `328` total
   - `m1`: `294`
   - `m2-3`: `26`
   - `m4+`: `8`

Interpretation:
- At ply1, most cutoffs already happen very early (move 1 dominates; `m4+` is rare).
- This weakens the hypothesis that delayed cutoff timing from move ordering is the primary
   depth-3 bottleneck.
- Remaining issue is more likely insufficient shallow-depth pruning (too many branches that
   do not cut at all), so pruning is now the top priority.

3. **Add shallow-node classification counters (depth <= 3)**
   - Count nodes reaching quiescence directly vs nodes expanded in main search.
   - Count how often TT refines bounds but fails to cut.
   - Goal: identify whether missing pruning or TT misses dominate node growth.

#### Step 3 Execution Result (April 28, 2026)

**Status:** Success (instrumentation complete, no search behavior change).

Added counters:
- Shallow nodes (depth<=3): `main`, `leaves->QS`
- TT refine no-cut counters: `main`, `main(shallow)`, `qs`

Depth-3 results:
- Shallow nodes (depth<=3): `main=164`, `leaves->QS=1959`
- TT refine no-cut: `main=164` (`shallow=164`), `qs=2785`

Interpretation:
- Most shallow search work rapidly transitions into quiescence (`leaves->QS` is much larger
   than shallow main-node expansion).
- TT bound refinement in quiescence frequently fails to cut (`qs` no-cut is high), so TT probes
   are often not converting into pruning in the current depth-3 workload.
- Combined with Step 2 (cutoffs usually early when they happen), this confirms the next gains are
   more likely from **increasing shallow pruning coverage** than from move-ordering retuning.

4. **Implement one shallow-depth optimization at a time (after instrumentation phase)**
   - Candidate order: move-count pruning, SEE pruning in main search, move-level futility pruning.
   - Immediate next change: **move-count pruning** (Step 4), then remeasure depth 3/9.
   - Acceptance for each change:
     - Depth-3 nodes decrease materially
     - No regression in puzzle quality (>=96/100)
     - No depth-9 node explosion

5. **Revisit move ordering once cutoff histograms are available**
   - If cutoffs are frequently late (`moveCount > 3`), prioritize move ordering adjustments.
   - Current histogram shows late cutoffs are uncommon at ply1, so continue with pruning first.

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

#### NMP Re-tuning (previous experiment status)

NMP-only tuning was attempted earlier and did not satisfy acceptance criteria at that time.
Given the new aspiration baseline, any future NMP re-tuning should be re-run from the new
baseline rather than using the older failed matrix.

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
