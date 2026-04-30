# Search Optimization Analysis

## Goal

Bring gbchess closer to Stockfish's node efficiency while preserving playing strength and test
quality.

Priority order:
- Improve depth-3 efficiency on the standard diagnostic FEN.
- Carry those gains to deeper searches, especially depth 9.
- Keep search behavior strong enough to pass the full test suite and at least 96/100 puzzles.

## Diagnostic Position

```text
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
```

Both engines find `c2h7` on this position.

## Current Tuned State

### Tuned settings currently enabled

- `useOracleMaxDepth = 0` (oracle disabled)
- `rootPVS = true`
- `rootPVSMaxDepthLeft = 3`
- `qsFrontierInMainMaxDepthLeft = 1`
- `moveLevelFutilityPruning = true`
- `moveFutilityMaxDepthLeft = 3`
- `moveFutilityMinMoveCount = 2`
- `moveFutilityMarginBase = 240`
- `internalIterativeDeepening = true`
- `iidMinDepthLeft = 4`
- `iidDepthReduction = 2`
- History bonus scaled by `depthLeft**2` (not `ply**2`)

### Current measured results

| Metric | Stockfish 12 | gbchess current | Gap |
|--------|--------------|-----------------|-----|
| Depth 3 nodes | 185 | ~1,800 | ~9.7x |
| Depth 9 nodes | 9,503 | ~514,000 | ~54x |
| Puzzles | — | 96/100 | target met |

Compared with the earlier baseline:
- Depth 3 improved from `4,798` to about `1,800` nodes.
- Depth 9 improved from `672,996` to ~`514,000` nodes.

Oracle diagnostic (for reference only):

| oracle depth | Depth 3 nodes | Depth 9 nodes |
| ------------ | ------------- | ------------- |
| 2            | 1,301         | 309,100       |
| 3            | 1,469         | 272,533       |

## Strength Results

Extensive SPRT self-play against [b6caa84](b6caa84) (`Raise RFP margin from 100x+100 to 200x+100`):

```text
Elo: 16.86 +/- 6.16
LOS: 100.00 %
Games: 1712
SPRT ([0.00, 5.00]) completed - H1 was accepted
```

Interpretation:
- The current tuned search is not only faster on the diagnostic position, it also improved actual
  playing strength in self-play.
- Earlier concern that the efficiency gains might just trade away tactical quality is no longer the
  best explanation for the current tuned profile.

## What Moved the Needle

### 1. Root-level PVS was the biggest structural improvement

The largest single gain came from changing root search shape so only the first root move gets a
full-window search and later root moves get null-window probes first.

Why it mattered:
- Stockfish already had this root-child shape.
- gbchess had been searching too many root children with wide PV windows.
- Fixing that reduced unnecessary expensive searches and made the tree shape much closer to
  Stockfish at depth 3.

### 2. Conservative shallow pruning helped once made configurable

The current tuned profile keeps a conservative move-level futility rule enabled. The exact tuning
matters; stronger or poorly scoped variants caused quality regressions.

### 3. Internal Iterative Deepening reduced depth-9 nodes ~7%

When no TT move is available at `depth.left >= 4`, a reduced-depth probe (`depth.left - 2`)
seeds `sortMoves` with a searched best move rather than relying on history/killers alone. This
directly addresses TT-miss nodes where move ordering had no first-move hint.

### 4. Scaling history bonus by $\tt{depthLeft}^2$ (not $\tt{ply}^2$)

Beta cutoffs at high remaining depth are stronger evidence than shallow-remaining cutoffs.
Scaling the quiet-history update by `depthLeft * depthLeft` (as Stockfish does) weights updates
by evidence quality rather than tree position, giving ~4% node reduction.

### 5. Shallow QsFrontier TT reuse is helpful, but secondary

Allowing limited main-search reuse of shallow frontier qsearch entries is useful, but its impact is
incremental compared with the root-PVS fix.

### 6. Reverse futility and the null-move PV fix remain foundational

These earlier accepted improvements still matter and remain part of the current good profile.

## Diagnostic Findings That Shaped the Tuned Search

### 1. Oracle move ordering (diagnostic-only by design)

The oracle was intentionally introduced as a diagnostic instrument, not as a deployable search
mechanism.

What it proved:
- Even near-perfect ordering alone only explained a minority of the depth-3 gap.
- Search structure and pruning policy mattered more than root ordering alone.

### 2. Depth-2 LMR

Depth-2 LMR repeatedly reduced depth-3 nodes but did not hold up on broader quality checks. It is
still considered unsafe with current ordering quality.

### 3. Aggressive late pruning variants

Several stronger pruning experiments reduced nodes but damaged puzzles or broader search quality.
The current lesson is to keep shallow pruning conservative and configurable.

### 4. TT learning updates from early returns

Those feedback-style updates increased noise and did not help overall strength.

## Main Learnings

### 1. Ordering alone was not the main bottleneck

Oracle diagnostics showed that even near-perfect ordering did not get gbchess close enough to
Stockfish by itself.

### 2. Root search shape mattered far more than expected

Changing non-first root moves to null-window probing was a major structural win and translated from
analysis mode into the tuned default search.

### 3. Configurability was necessary

The newer optimizations only became practical once they could be tuned independently in
[options.h](src/core/options.h). This made it possible to recover puzzle quality while keeping much
of the efficiency gain.

### 4. Full validation beats isolated node-count wins

The best profile is the one that improves both efficiency and playing strength. The current tuned
settings passed:
- `make test -j`
- `make ci`
- `make puzzles` with 96/100
- SPRT self-play improvement versus `b6caa84`

## Current Recommendation

Keep the current tuned profile as the new baseline.

Reason:
- It materially improves depth-3 and depth-9 efficiency.
- It meets the puzzle-quality floor.
- It passes the test suite.
- It improved in self-play, which is the strongest evidence that the search trade-offs are good.

## Next Steps

1. Improve move ordering further — oracle diagnostics show ~54% gap remaining at depth 9
   vs Stockfish; IID has begun closing it but ordering quality is still the main bottleneck.
2. Revisit deeper TT reuse and shallow pruning only through configurable, isolated changes.
3. Re-test depth-2 LMR only after move ordering quality improves further.
4. Continue using SPRT/self-play as the final gate for changes that meaningfully affect search.
