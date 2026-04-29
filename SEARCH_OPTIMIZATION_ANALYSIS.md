# Search Optimization Analysis

## Goal

Bring gbchess to within roughly 2-3x of Stockfish's node efficiency.

Priority order:
- First close the gap at depth 3 on the standard diagnostic FEN.
- Then carry the same improvements to deeper searches, with depth 9 as the first meaningful target.

## Test Positions

### Depth-3 diagnostic position

```text
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
Depth: 3
```

### Depth-9 comparison position

```text
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
Depth: 9
```

Both engines find `c2h7` on this position.

## Current Baselines

### Production baseline (accepted search changes, no diagnostic oracle)

| Depth | Stockfish 12 | gbchess | Ratio |
|-------|--------------|---------|-------|
| 3 | 185 | 4,798 | 25.9x |
| 9 | 9,503 | 672,996 | 70.8x |

### Diagnostic oracle results (not production candidates)

These runs use an isolated move-ordering oracle to estimate the best node count reachable from
ordering alone.

| Oracle mode | Depth-3 nodes | First-move cutoffs | Runtime |
|-------------|---------------|--------------------|---------|
| None | 4,798 | 68% | ~12 ms |
| Root-only oracle | 4,862 | 69% | small |
| All-node oracle, depth 4 | 3,998 | 95% | ~48 s |
| All-node oracle, depth 3 | 3,934 | 100% | ~6.4 s |
| All-node oracle, depth 2 | 3,862 | 99% | ~1.1 s |

Key conclusion: even near-perfect ordering only reduces depth-3 nodes by about 20%. The
remaining gap to Stockfish is overwhelmingly a pruning and search-structure gap, not just an
ordering gap.

## What Worked

### Accepted changes

1. **Null-move PV-check fix**
   - Fixed an inverted PV/null-window guard in null-move pruning.
   - Large depth-9 improvement versus the pre-fix baseline.
   - This remains one of the highest-value fixes so far.

2. **Disable aspiration windows**
   - Reduced unnecessary re-search overhead on the diagnostic workload.
   - Improved both shallow and deeper measurements in the current setup.

3. **Increase reverse futility margin**
   - Raising the margin to `200 * depth + 100` gave a small but real improvement.
   - Helped depth 3 slightly and depth 9 more meaningfully.

4. **Conservative shallow SEE pruning in main search**
   - Capture-only, shallow, non-PV SEE pruning is tactically safe in the current configuration.
   - Benefit is small, but it is a sound building block.

5. **1-ply continuation history**
   - Safe and integrated cleanly.
   - Immediate node-count gain is minimal, but the data path is now present for stronger future
     contextual ordering work.

6. **Fix continuation-history save/restore state**
   - Correctness fix, not a speed optimization.
   - Prevents search state corruption when saving/restoring engine state.

## What Did Not Work

1. **Aggressive move-count pruning**
   - Reduced nodes but failed tactical validation and puzzle quality.
   - Current implementation approach is too unsafe.

2. **TT-driven learning updates on early return**
   - Rewarding TT moves or adding fail-low penalties increased depth-9 nodes.
   - The TT feedback signal is too noisy in the current search.

3. **Low-ply history**
   - No measurable gain on the diagnostic workload.
   - Safe, but not useful enough to keep.

4. **Depth-2 LMR**
   - Consistently improved depth 3.
   - Consistently regressed depth 9 and/or puzzle quality.
   - Conclusion: current late-move ranking is not reliable enough to safely reduce depth-2 nodes.

5. **Root-only ordering oracle**
   - No meaningful gain.
   - Confirms root move ordering is not the main issue.

## Main Learnings

### 1. Ordering is not the main bottleneck

The all-node oracle pushes first-move cutoffs to 99-100%, yet depth-3 nodes only fall from 4,798
to 3,862-3,934. That is still about 21x above Stockfish. Better ordering matters, but it is not
the dominant reason for the gap.

### 2. The biggest remaining gap is shallow pruning coverage

Stockfish is cutting far more branches before they become full subtrees. The strongest remaining
evidence points to:
- more effective shallow pruning,
- better reduction policy at low depths,
- and better conversion of search information into immediate cutoffs.

### 3. Depth-2 LMR is attractive but currently unsafe

When enabled, it gives the kind of depth-3 node reduction we want. But it harms deeper search.
That means the idea is probably directionally right, but gbchess does not yet have the move-quality
signals needed to use it safely.

### 4. TT write volume is not the problem

gbchess writes enough TT entries. The weakness is reuse quality, not lack of writes.

### 5. Small, validated pruning changes are still worthwhile

The reverse futility margin increase and shallow SEE pruning both helped or at least stayed safe.
The right path is still isolated, measurable pruning changes with full validation.

## Current Search State

Relevant current characteristics:
- Aspiration windows are disabled.
- Null move pruning is enabled with the PV-check fix.
- Reverse futility pruning is enabled with the larger accepted margin.
- LMR still does **not** safely operate at depth 2.
- 1-ply continuation history exists, but has not yet translated into major node savings.
- The move-ordering oracle is a diagnostic tool only and should not be treated as a production
  solution.

## Next Steps

### Immediate depth-3 priority

Target: get substantially closer to Stockfish at depth 3 without harming correctness.

1. **Focus on pruning, not more oracle work**
   - The oracle answered the main question already.
   - Further oracle refinement is unlikely to change the conclusion.

2. **Improve shallow pruning coverage with one change at a time**
   - Best candidates:
   - safer move-level futility pruning,
   - tighter late-move pruning variants,
   - or more selective SEE-based pruning beyond the current minimal gate.

3. **Improve TT reuse quality**
   - Investigate why TT hits convert poorly into cutoffs, especially at shallow depth.
   - Prioritize reuse behavior over increasing TT insert count.

4. **Revisit depth-2 LMR only after better move-quality signals exist**
   - Depth-2 LMR is still the most promising lever for depth-3 node count.
   - But it must be retried only after ordering and pruning signals are reliable enough.

### Depth-9 follow-through

Any depth-3 improvement is only acceptable if it stays neutral or positive at depth 9.

Validation standard for every experiment:
- `make test -j` passes.
- `make puzzles` stays at or above the current acceptable quality bar.
- Depth-3 nodes improve materially.
- Depth-9 nodes do not regress.

### Practical success criteria

Near-term targets:
- Depth 3: move from 4,798 toward the low thousands first, then toward a few hundred.
- Depth 9: move from 672,996 toward the tens of thousands.

Strategic target:
- Finish within roughly 2-3x of Stockfish at both shallow and medium depth.

## Code Areas Most Likely to Matter Next

### Search core
- `src/search/search.cpp`
  - `alphaBeta()`
  - `tryNullMovePruning()`
  - `quiesce()`
  - LMR and futility gates

### Options and tuning constants
- `src/core/options.h`

### Diagnostic reference only
- move-ordering oracle code in `src/search/search.cpp`
