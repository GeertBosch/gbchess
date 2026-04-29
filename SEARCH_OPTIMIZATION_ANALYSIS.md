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
| gbchess | 4,897 | 110 (2%) | 79 (71%) |

gbchess uses **26x** more nodes at depth 3 (4,897 vs 185).

### New instrumentation findings (gbchess)

Added counters in `src/search/search.cpp` and printed in `src/engine/engine.cpp`:

- Root search calls: **3**
- Root move visits: **126** (42 per root pass)
- Aspiration attempts: **0** (fail-low=0, fail-high=0, fallback=2)
- PVS attempts: **1,815**, PVS researches: **37**
- TT cutoffs: **0**, TT refinements: **4**

Interpretation:

1. The repeated `info depth 3` lines were confirmed to be **aspiration re-searches** (window
   fail-low/fail-high), and removing aspiration windows reduced root passes and node count.
2. Move ordering is likely part of the remaining problem (`first-move cutoff` is 71% vs Stockfish 100%),
   but not the whole story.
3. The remaining gap is still low cutoff efficiency overall at shallow depth (2% beta-cutoff rate
   and 4,897 total nodes), indicating missing shallow-depth pruning and weaker root efficiency.

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
2. **Aspiration Windows** — Implemented, currently disabled in config
   (`aspirationWindows = {}` in `src/core/options.h`; previously 30cp, 125cp)
3. **Transposition Table** — Yes (16 MB)
4. **Null Move Pruning** — Yes (R=3, min_depth=2, PV-check bug fixed)
5. **Late Move Reductions (LMR)** — Yes, but only fires at `depth.left > 2` (not at depth 2)
6. **Killer Move Heuristic** — Yes (2 killers per depth)
7. **History Heuristic** — Yes
8. **Countermove Heuristic** — Yes
9. **MVV/LVA Move Ordering** — Yes
10. **Static Exchange Evaluation (SEE)** — Yes (in quiescence)
11. **Quiescence Search** — Yes (depth=5, includes checks)
12. **Reverse Futility Pruning** — Yes (max depth=3, margin=200cp×depth+100cp = 500cp at depth 2)
13. **Principal Variation Search (PVS)** — Yes

### ❌ Missing or Miscalibrated Optimizations

#### High Impact (confirmed by Stockfish trace)
1. **Futility Margin Still Below Stockfish** — gbchess uses `200×depth+100cp` (500cp at depth 2)
   vs Stockfish's `223×(depth-improving)` (446cp at depth 2 non-improving, larger at depth 3+).
   The original 146cp gap at depth 2 was reduced, but formula differences still affect pruning shape.
   positions with static eval between -34 and +112 (relative to beta) recurse in gbchess but
   are pruned immediately in Stockfish. This is the single largest confirmed contributor to the
   depth-3 node gap. **Status: partially mitigated; further tuning remains possible.**
2. **Contextual Move Ordering Gap (Primary Next Target)** — gbchess uses TT move + history +
   killer/countermove, but lacks Stockfish-style continuation history and capture-history terms.
   This weakens late-move quality signals used by LMR. **Fix: add continuation-history scoring
   and updates for quiet moves.**
3. **Move Count Pruning (Late Move Pruning)** — After N moves searched at shallow depth, skip
   remaining quiet moves. Stockfish: `moveCount >= futility_move_count(improving, depth)`.
   Previously attempted (Step 4) but gate was too aggressive; needs re-try with tighter margins.
4. **Futility Pruning (Move Level)** — Skip quiet moves when `staticEval + margin ≤ alpha`.
   This fires per-move (vs node-level RFP which fires before the loop). Stockfish's Step 13
   combines both forms.

#### Medium Impact
5. **Dynamic Null Move Reduction** — Scale R with depth and eval margin.
   Stockfish: `R = (817 + 71*depth)/213 + min((eval-beta)/192, 3)`. Currently fixed at R=3.
6. **Razoring** — At depth 1, if eval << alpha, go directly to quiescence.
7. **ProbCut** — Try a reduced-depth search with a widened beta; cut if it fails high.
8. **SEE Pruning Extension** — Extend main-search SEE pruning (currently captures-only, SEE<-100cp)
   to quiet moves or looser thresholds only if quality remains stable.

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

#### Step 4 Execution Result (April 28, 2026)

**Status:** Failed (change reverted).

Attempted change:
- Added shallow non-PV move-count pruning for late quiet moves.

Observed performance impact:
- Depth-3 nodes: `4938 -> 4425` (improved)
- Depth-9 nodes: `709677 -> 300823` (large reduction)

Validation failures:
- `make test -j`: failed (`build/uci-tactical-position.out` wrong best move)
- `make puzzles`: `97/100 -> 92/100` with 1 search error and 5 eval errors

Conclusion:
- The implemented move-count pruning gate was too aggressive and removed tactically relevant
   quiet moves.
- The code change was reverted; no behavioral change was accepted for Step 4.

Revised next-step options (pick one before implementation):
1. Retry move-count pruning with much tighter gating (depth<=2 only, much later threshold,
    and skip pruning when score margin is small).
2. Switch to SEE pruning in main search first, since it is more tactical-safety aware.
3. Switch to move-level futility pruning first with conservative margins.

#### Step 5 Execution Result (April 28, 2026)

**Status:** Success (accepted).

Applied change (single isolated behavior change):
- Added conservative SEE pruning in `alphaBeta()` for shallow non-PV nodes.
- Gate: `depth.left <= 3`, not in check, move index `> 1`, capture only (`MoveKind::Capture`),
   prune only when `staticExchangeEvaluation(...) < -100cp`.

Measured impact on the test FEN:

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Depth 3 nodes | 4,938 | 4,897 | **-0.8%** |
| Main SEE pruned | 0 | 12 | +12 |

Validation:
- `make test -j`: pass
- `make puzzles`: unchanged quality (`97/100`, 2 too deep, 1 eval error, 0 search/mate errors)

Conclusion:
- This conservative SEE gate is tactically safe under current checks and provides a small but
   measurable node reduction.
- It is accepted as a foundation for future, still-conservative pruning work.

#### Step 6 Execution Result (April 28, 2026)

**Status:** Success (instrumentation complete, validated with tests).

Applied change (instrumentation only):
- Added TT insert/write counters in gbchess, including bound split (exact/lower/upper), and
   write-to-hit conversion counters.
- Added corresponding Stockfish instrumentation based on **actual TT writes** in `TTEntry::save`
   (not only callsite counts), including a small `other` bucket for non standard bound values.

Validation:
- `make test -j`: pass

Depth-3 comparison on the same FEN (`uci-comp.in`):

| Metric | Stockfish 12 | gbchess |
|--------|--------------|---------|
| Nodes | 185 | 4,897 |
| TT main probes/hits | 93 / 90 | 164 / 4 |
| TT qs probes/hits | 104 / 18 | 2,756 / 0 |
| TT writes | 118 | 166 |
| TT write split | exact/lower/upper/other = 25/62/28/3 | exact/lower/upper = 10/110/46 |
| TT hit-per-write | 91% | 27% |

Interpretation:
- gbchess is **not under-writing** TT entries at depth 3; it writes more entries than Stockfish on
   this position.
- The dominant issue is **reuse quality**, not write volume:
   - very low bounded TT hits in main search (`4/164`),
   - zero bounded TT hits in quiescence (`0/2756`),
   - much lower hit-per-write conversion (`27%` vs `91%`).

Conclusion:
- For depth-3 efficiency, the highest expected return is to improve TT reuse/conversion behavior,
   not to increase raw TT insert count.

#### Step 7 Execution Result (April 28, 2026)

**Status:** Diagnostic complete. Revised root cause identified.

Applied change:
- Instrumented Stockfish's `search()` to print every main-search node entry (ply, depth, alpha,
  beta, move), TT cutoffs, and beta cutoffs to stderr.

Full depth-3 trace captured and analyzed for the test FEN. Key findings:

**The complete depth-3 pass (185 nodes) breaks down as follows at ply 1 (42 root moves):**

| Category | Count | Explanation |
|----------|-------|-------------|
| `[NP d2 p1]` with no children | 18 | **Node-level futility pruned** before move loop |
| `[NP d1 p1]` (LMR-reduced, depth 2→1) | 23 | **LMR fires** for late moves — recursed 1 ply, not 2 |
| `[PV d2 p1]` with full subtree | 1 | Only `h3g5` explored fully |
| TT cutoffs at ply 1 | 1 | `e1e7` → TT hit from prior iteration |

**How node-level futility pruning works in Stockfish (Step 8):**

```
if !PvNode && depth < 8 && eval - futility_margin(depth, improving) >= beta:
    return eval
```

With `futility_margin(2, false) = 223 × 2 = 446cp`, any ply-1 position where
`static_eval ≥ beta - 446 = 412 - 446 = -34` gets returned immediately without entering
the move loop. Since beta = 412cp (the already-found best), moves like `c2h7` (queen blunder,
static eval ≈ -3000cp) fail this check and are pruned in a single node, never recurse.

**How LMR works in Stockfish for depth=3:**

At ply 1, late moves (move 20+ approximately) have their depth reduced from 2 → 1. This means
they enter qsearch immediately, producing a single node instead of a full depth-2 subtree.

**gbchess comparison:**

| Mechanism | Stockfish | gbchess |
|-----------|-----------|---------|
| Node-level futility margin at depth 2 | `223 × 2 = 446cp` | `100 × 2 + 100 = 300cp` |
| Node-level futility max depth | 8 | 3 |
| LMR fires at depth | `depth > 0` (any) | `depth.left > 2` only |

**Root causes of the 26x node gap at depth 3:**

1. **Futility margin too small**: gbchess uses 300cp vs Stockfish's 446cp at depth 2.
   Positions with static eval between -34cp and +112cp (the gap between 300 and 446) fall
   through gbchess's futility gate but are caught by Stockfish's.
2. **LMR doesn't fire at depth 2**: gbchess requires `depth.left > 2` so LMR is disabled
   for the depth-2 children at ply 1. Stockfish reduces depth 2 → 1 for late moves, turning
   full subtrees into single qsearch calls.
3. **TT reuse** plays a secondary role (one extra cutoff from prior iteration), not the root cause.

**Previously hypothesized root causes that are now disproven:**
- *TT write volume* was not the bottleneck (gbchess writes more entries than Stockfish).
- *Purely delayed ply-1 cutoffs* were not the primary cause (ply-1 cutoffs already happen early).
- *Aspiration re-search overhead* was real but accounted for only ~40% of the original gap.

Updated interpretation:
- The remaining depth-3 gap at ply-1 is largely explained by futility/LMR behavior differences.
- However, a direct `depth.left >= 2` LMR change (and a conservative gated variant) regressed
   depth-9 nodes, indicating deeper-tree move ordering quality is still a limiting factor.
- Therefore, move ordering quality is now a primary implementation target, especially contextual
   quiet-move ordering (continuation history), before revisiting aggressive shallow LMR.

5. **Revisit move ordering once cutoff histograms are available**
   - If cutoffs are frequently late (`moveCount > 3`), prioritize move ordering adjustments.
    - Current histogram shows late cutoffs are uncommon at ply1, but this does not rule out deeper
       move-ordering weaknesses where LMR decisions are made.
    - Action: prioritize contextual quiet-move ordering (continuation history), then re-test LMR.

6. **Prioritize TT reuse quality over TT write volume (new priority)**
    - Stockfish/gbchess comparison shows write volume is already sufficient; conversion is weak.
    - Next implementation should be one isolated TT reuse change at a time, with full validation:
       1. Main-search TT: permit safe bound cutoffs without requiring full PV reconstruction.
       2. Main-search TT: keep TT move first ordering and measure first-move cutoff delta.
       3. Quiescence TT: evaluate conservative reuse improvements only after main-search TT change.
    - Acceptance criteria for each isolated change:
       - `make test -j` passes,
       - puzzle quality remains >= 96/100,
       - depth-3 nodes decrease,
       - TT hit-per-write and/or bounded TT hit rates improve.

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

### Step 2: Increase Reverse Futility Pruning Margin

**Goal:** Match Stockfish's futility margin to prune the same set of hopeless positions.

**Hypothesis**: Increasing the margin from `100×depth+100` to `200×depth+100` (or approaching
Stockfish's `223×depth`) will prune the ~18 ply-1 positions currently missed, reducing depth-3
node count by an estimated 10–20%.

**Implementation** (single isolated change):
- In `src/search/search.cpp`, change:
  ```cpp
  Score futilityMargin = Score::fromCP(100 * depth.left + 100);
  ```
  to:
  ```cpp
  Score futilityMargin = Score::fromCP(200 * depth.left + 100);
  ```
- No other changes.

**Validation**:
- `make test -j` passes
- `make puzzles` ≥ 96/100
- Depth-3 nodes decrease vs baseline of 4,897
- Depth-9 nodes do not increase

If successful, consider a second isolated step to further raise toward `223×depth`.
If quality drops, try intermediate value (150×depth+100).

#### Step 8 Execution Result (April 28, 2026)

**Status:** Success (accepted).

Applied change:
- Raised RFP margin: `100×depth+100` → `200×depth+100` (500cp at depth 2, 700cp at depth 3).

Measured impact on the test FEN:

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Depth 3 nodes | 4,897 | 4,798 | **-2.0%** |
| Depth 9 nodes | 709,677 | 673,815 | **-5.1%** |

Validation:
- `make test -j`: pass
- `make puzzles`: 96/100 (stable)

Conclusion:
- The larger margin provides a genuine improvement at depth 9 and a small improvement at depth 3.
- Change is accepted.
- The depth-3 impact is small because the 18 futility-pruned Stockfish nodes are pruned due to
  *structural* differences (Stockfish's formula is `223×depth`, firing without the `+100` offset,
  and Stockfish evaluates before entering the move loop unconditionally). With our new 500cp margin
  at depth 2, most of those 18 nodes would already have been pruned by the prior 300cp margin —
  the positions blunder material (static eval ≈ -3000cp) and were never the binding cases.
- The primary remaining depth-3 gap is LMR not firing at depth 2 (the 23 `[NP d1 p1]` nodes in
  Stockfish vs depth-2 subtrees in gbchess). That is the next isolated step.

### Step 3: Enable LMR at Depth 2

**Goal:** Allow LMR to reduce late depth-2 moves to depth-1, turning subtrees into qsearch calls.

**Hypothesis**: Removing the `depth.left > 2` floor and replacing with `depth.left >= 2` (or
equivalently `depth.left > 1`) will match Stockfish's behavior where ~23 of 42 ply-1 moves
are depth-reduced from 2 → 1 in the depth-3 pass.

**Implementation** (single isolated change after Step 2):
- In `src/search/search.cpp`, change the LMR condition from `depth.left > 2` to `depth.left >= 2`.

**Validation**: same as Step 2.

#### Step 9 Execution Result (April 28, 2026)

**Status:** Failed (both variants reverted).

Variant A (plain threshold):
- Change: `depth.left > 2` -> `depth.left >= 2`
- Depth 3 nodes: `4,798 -> 4,042` (**-15.7%**)
- Depth 9 nodes: `673,815 -> 745,188` (**+10.6%**, regression)
- Puzzles: `96/100 -> 95/100` (regression)

Variant B (conservative gate):
- Change: allow depth-2 LMR only when `!isPVNode && !inCheck`
- Depth 3 nodes: `4,798 -> 4,802` (flat)
- Depth 9 nodes: `673,815 -> 753,176` (**+11.8%**, regression)
- Puzzles: `96/100` (stable)

Conclusion:
- Directly enabling depth-2 LMR in gbchess is currently unsafe for depth-9 efficiency.
- The likely issue is that late-move rank is not yet reliable enough as a quality signal deeper in
  the tree. Improve move ordering quality first, then retry depth-2 LMR.

### Step 10: Implement Continuation History for Quiet Move Ordering

**Goal:** Improve contextual quiet-move ordering so "late move" is a stronger proxy for poor quality,
making LMR and late-move pruning safer and more effective.

**Rationale from Stockfish comparison:**
- TT move usage is already present in gbchess.
- The largest ordering gap is quiet scoring: Stockfish blends main history with continuation
  history terms from prior plies; gbchess currently uses only main history for quiet ordering.

**Implementation plan (isolated, ordered):**
1. Add a continuation-history table keyed by previous move context and current move destination.
   - Implemented shape: `continuationHistory[kNumPieces][kNumSquares][kNumPieces][kNumSquares]`.
2. Wire quiet-move scoring in `sortMoves(...)` to include continuation bonus from previous ply.
   - Implemented as: `history + 2 * continuationHistory(lastMoveCtx, moveCtx)`.
3. Update continuation-history on quiet beta cutoffs.
   - Implemented conservative positive reinforcement only (no fail-low penalty yet).
4. Keep capture ordering unchanged in this step (no capture-history changes yet).
5. Measure and report:
   - first-move cutoff ratio at ply1 and deeper plies,
   - LMR re-search rate,
   - depth-3 and depth-9 nodes,
   - puzzles / tactical tests.

**Acceptance criteria:**
- `make test -j` passes
- `make puzzles` remains >= 96/100
- Depth-9 nodes do not increase
- First-move cutoff rate and/or node count improves

#### Step 10 Execution Result (April 28, 2026)

**Status:** Success (accepted).

Applied change:
- Added 1-ply continuation history for quiet move ordering.
- Quiet score now combines main history with continuation signal from opponent's last move context.
- Continuation table is updated on quiet beta cutoffs.

Measured impact on the test FEN:

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Depth 3 nodes | 4,798 | 4,798 | 0.0% |
| Depth 9 nodes | 673,815 | 672,996 | **-0.1%** |

Validation:
- `make test -j`: pass
- `make puzzles`: 96/100 (stable)

Conclusion:
- The minimal continuation-history integration is safe and does not regress quality.
- Immediate node reduction is small, which is expected for a conservative first pass.
- This establishes the data path needed for stronger contextual ordering updates in follow-up steps.

### Step 11: Strengthen Continuation History Updates

**Goal:** Increase ordering signal quality now that continuation-history plumbing is in place.

Proposed isolated follow-up:
1. Add conservative fail-low penalties for quiet moves searched before cutoff.
2. Scale continuation bonus/penalty by depth similar to existing history update.
3. Add one extra continuation source (our previous move context) if step 1 remains safe.
4. Re-measure depth 3/9, first-move cutoffs, LMR re-search rate, and puzzle score.

#### Step 11 Execution Result (April 28, 2026)

**Status:** Failed (reverted).

Attempted change:
- Added TT early-return learning updates in main search:
   - reward quiet TT move on TT cutoff,
   - conservative fail-low penalty variant when TT hit did not cut.

Observed impact on the test FEN:

| Variant | Depth 3 nodes | Depth 9 nodes | Delta vs 672,996 |
|--------|----------------|---------------|------------------|
| TT cutoff reward + fail-low penalty | 4,798 | 702,200 | **+4.3%** |
| TT cutoff reward only | 4,798 | 710,653 | **+5.6%** |

Validation:
- `make test -j`: pass
- `make puzzles`: 96/100 (stable)

Conclusion:
- TT early-return ordering updates as implemented increased depth-9 node count and were reverted.
- This suggests the added feedback loop over-amplifies currently noisy TT move signals in this
   search configuration.

### Step 12: Add Low-Ply History for Quiet Ordering ← **Next Concrete Step**

**Goal:** Improve quiet move ordering near the root with a bounded, context-light signal that is
less likely to destabilize deeper search.

Proposed isolated implementation:
1. Add `lowPlyHistory[maxTrackedPly][kNumSquares][kNumSquares]`.
2. During quiet move scoring, add a small weighted term when `ply < maxTrackedPly`.
3. Update low-ply history only on quiet beta cutoffs at low ply (conservative bonus scale).
4. No TT early-return updates in this step.
5. Re-measure depth 3/9, first-move cutoffs, and puzzle score.

**Follow-up after Step 12:**
- Re-attempt depth-2 LMR with conservative guards once continuation history shows improved
  ordering quality.

### Step 4: Move Count Pruning (retry)

**Goal:** Prune late quiet moves at shallow depths where they are unlikely to improve.

After Steps 2–3, retry move-count pruning with much tighter gating:
- Only at `depth.left <= 2`
- Threshold much later than prior attempt
- Skip when static eval is close to alpha (small margin)

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
