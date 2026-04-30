# Search Optimization Analysis

## Diagnostic Position

```
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
```

Both engines agree this position is a win for White. gbchess plays `c2h7`, SF plays `h3g5`.

## Node Counting: Critical Finding

**The 9.7x reported gap is largely a measurement artifact** -- the engines do not count nodes the
same way.

| What is counted | Stockfish | gbchess |
|-----------------|-----------|---------|
| `search()` at depth > 0 | yes | yes (`alphaBeta`, depth.left > 0) |
| Leaf call at depth = 0 (goes to QS) | **no** (returns before `++totalNodes`) | yes (`alphaBeta` at depth.left=0 still increments) |
| QS recursion calls | **no** (`qsearch` has no node increment) | yes (inner `quiesce()` increments `nodeCount`) |

Stockfish: `depth <= 0 -> return qsearch(...)` fires at line 651, **before** `++totalNodes`
at line 674 in `Stockfish/src/search.cpp`. `qsearch()` itself never increments. So SF `nodes`
= main search calls with `depth > 0` only.

gbchess: `alphaBeta` increments `nodeCount` at every depth including depth.left=0, and the inner
recursive `quiesce()` also increments `nodeCount`. So gbchess `nodes` = main + all QS recursion.

### Depth-3 node breakdown (diagnostic FEN, from statistics output)

```
Category                        gbchess    Stockfish    Notes
Main search (depth.left > 0)       122        185        1.5x gap
Leaf alphaBeta (depth.left = 0)    621          0        counted by gbchess, not SF
Inner QS recursion               1,057          0        counted by gbchess, not SF
----------------------------------------------------------------------
Total reported nodes             1,800        185        9.7x (misleading)
```

**The real main-search tree gap is only ~1.5x.** Every other reported gap is an artifact of
how QS work is counted.

## Current Measured State

| Metric | Stockfish 12 | gbchess | Raw ratio |
|--------|--------------|---------|-----------|
| Depth 3 nodes | 185 | 1,800 | 9.7x |
| Depth 9 nodes | 9,503 | ~514,000 | ~54x |
| Puzzles | -- | 96/100 | target met |

These ratios are inflated by the counting difference (see Step 1 below). The depth-3 baseline
improved from 4,798 nodes in initial work -- the 1,800 figure represents real gains.

### Key diagnostic stats (depth 3, gbchess)

```
Total nodes:            1,800
  Main (d.left > 0):      122
  Leaf -> QS:             621  (quiescenceCount; each enters quiesce once, then may recurse)
  Inner QS recursion:   1,057  (nodeCount - alphaBeta calls)
Null move:              39 attempts, 27 cutoffs (69%)
First-move cutoffs:     37 / 62 total (59%)   <- SF is 5/5 = 100%
TT main depth-miss:     38 cases where d=2 was needed, d=1 was found (in depth-3 search)
QS avg recursion depth: 1,057 / 621 = 1.7 inner calls per QS entry
```

### Stockfish depth-3 stats (for comparison)

```
Total nodes:   185  (main-search only, depth > 0)
First-move:    5/5 = 100%
TT main hit:   90/93 = 97%
TT qs probes:  104  (no recursion counted)
```

## What Drove Progress

1. **Root-level PVS** -- largest single gain. Non-first root moves now get null-window probes,
   matching SF's root shape. gbchess had been searching too many root children with wide PV
   windows; fixing that was the biggest structural win.
2. **Reverse futility pruning and null-move PV fix** -- foundational; still in the profile.
3. **Move-level futility pruning** -- conservative tuning required; aggressive variants hurt
   puzzles. Currently enabled at `moveFutilityMaxDepthLeft = 3`.
4. **IID at `depth.left >= 4`** -- ~7% depth-9 reduction by seeding move ordering with a
   reduced-depth best move when TT has no hint.
5. **History bonus scaled by `depthLeft^2`** -- matches SF weighting (~4% depth-9 reduction).
6. **SPRT validation** -- the combined profile confirmed +16.86 Elo in self-play vs `b6caa84`.

## Mistakes to Avoid Repeating

- **Depth-2 LMR**: always reduced node counts but reliably caused quality regressions. Do not
  re-enable until move ordering quality improves substantially.
- **Aggressive move-count pruning**: nodes dropped but puzzles fell below 96/100.
- **TT learning from early returns**: increased noise, no strength gain.
- **Multiple simultaneous changes**: after these failures, strict one-change-at-a-time discipline
  was adopted. It works.
- **Trusting raw node counts without verifying counting semantics**: the 9.7x gap was taken at
  face value for too long. Always verify what "nodes" means in each engine before comparing.
- **Oracle ordering as a fix**: proved that even perfect ordering closes only a minority of the
  gap. Structure and pruning matter more than ordering at this stage.

## Next Steps

### Step 1 -- Fix node counting to match Stockfish (reporting change only, no search impact)

Move `++nodeCount` out of inner `quiesce()` into a dedicated `qsNodeCount` counter. Do not
count `alphaBeta` calls at `depth.left = 0` in the main `nodes` reporter (matching SF semantics:
those are leaf transitions to QS, not main-search nodes).

After this change, depth-3 reported `nodes` drops from 1,800 to 122 -- directly comparable to
SF's 185. Add to the stats block:

```
nodes(main/leaf/qs) = 122 / 621 / 1,057
```

Validate: `make test -j` output unchanged. Pure instrumentation, no search behavior change.

### Step 2 -- Diagnose and close the first-move cutoff gap

gbchess 59% vs SF 100% first-move cutoffs. The 41% late-cutoff rate wastes nodes finding
refutations that better ordering would surface first.

Add per-depth first-move cutoff rate to the stats printout. Profile which move categories
(quiet, capture, killer, counter) are causing late cutoffs before attempting fixes. Measure first.

### Step 3 -- Reduce QS work (after Step 1, so gains are measurable)

QS over-expansion has several separable causes; test each in isolation:

1. **Reduce check generation depth in QS** (`checksMinDepthLeft`): currently 5 (all QS levels).
   SF generates checks only at the first QS level. Try `checksMinDepthLeft = 1`. Low risk.

2. **Delta pruning in QS**: after computing `standPat`, set `futilityBase = standPat + 145cp`.
   Before each capture, if `futilityBase + capturedPieceValue(move) <= alpha`, skip it.
   Directly limits recursion depth.

3. **Tighten QS SEE threshold to >= 0**: currently `< -100cp`; SF uses `see_ge(move, 0)`.
   Skips all losing captures. May affect endgame tactics -- test with puzzles.

4. **QS move count hard cap**: after `moveCount > 2` (no check, no advanced pawn push), skip
   remaining captures. Matches SF's hard cap.

Test each independently. Puzzle floor is 96/100. Commit only if: all tests pass AND QS
entry/recursion counts decrease with no puzzle regression.

### Step 4 -- Address TT depth misses

38 depth-2 TT misses at depth 3 because QsFrontier entries are stored at depth 0, insufficient
for depth-2 re-use. These 38 positions expand full subtrees instead of cutting. Investigate
raising the stored depth of QsFrontier entries from 0 to 1.

### Step 5 -- LMR and deeper pruning

Revisit depth-2 LMR and deeper null-move tuning only after Steps 1-3 are complete and node
counts are on a comparable footing with SF. The remaining gap at that point may be much smaller
than the current raw ratios suggest.
