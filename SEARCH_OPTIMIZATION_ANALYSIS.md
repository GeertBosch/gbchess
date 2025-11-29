# Search Optimization Analysis

## Test Position
```
FEN: rn3br1/1pk1pNpp/p7/q1pP4/2P2PP1/P4p1N/1PQ4P/4RK2 w - - 3 23
Depth: 9
```

## Node Count Comparison

| Engine | Nodes | Seldepth | Time |
|--------|-------|----------|------|
| **Stockfish 12** | 7,028 | 12 | 9ms |
| **gbchess** | 598,390 | 10 | 2,497ms |
| **Ratio** | **85x** | 0.83x | 277x |

**Recent Optimizations:**
- Changed `nullMoveReduction` from 3 to 4 (more aggressive)
- Changed `nullMoveMinDepth` from 3 to 2 (allows null move at more depths)
- Removed `max(1, ...)` constraint on null move search depth
- **Selective quiescence extension** for checking moves (fixes missed checkmates without regression)
- Result: **22% faster** than previous best (768K nodes, 3.2s) with correct checkmate detection

Note: Both engines report "seldepth" (selective depth = maximum depth in main search including extensions, excluding quiescence).

## Detailed Statistics Comparison (depth 9)

| Metric | Stockfish 12 | gbchess | Ratio |
|--------|--------------|---------|-------|
| **Total Nodes** | 7,028 | 598,390 | **85x** |
| **Null Move Attempts** | 1,467 (20.87% of nodes) | 21,896 (3.66% of nodes) | **6x fewer attempts** |
| **Null Move Cutoffs** | 1,075 (73% of attempts) | 17,748 (81% of attempts) | Better effectiveness! |
| **Beta Cutoffs** | 1,575 (22% of nodes) | 100,009 (16.7% of nodes) | Lower % but 63x more |
| **First-Move Cutoffs** | 1,373 (87% of beta) | 85,883 (85% of beta) | Similar ordering |


**Current Null Move Statistics:**
- Stockfish 12: **20.87%** of nodes attempt null move
- gbchess: **3.66%** of nodes attempt null move
- Gap: **6x fewer attempts** (improved from previous 24x)

**Success Rates (when attempted):**
- Stockfish 12: 73% of attempts result in cutoff
- gbchess: **81%** of attempts result in cutoff (even better!)

This demonstrates that **our null move implementation is effective when used**, but we're severely underutilizing it by attempting it too rarely.

### Analysis: Null Move Conditions

**gbchess null move conditions:**
```cpp
if (options::nullMovePruning && depth.left >= options::nullMoveMinDepth &&  // depth >= 2 ✓
    !isInCheck(position) && beta > Score::min() + 100_cp &&
    hasNonPawnMaterial(position))
```

**Stockfish null move conditions:**
```cpp
if (   !PvNode
    && (ss-1)->currentMove != MOVE_NULL  // No consecutive null moves
    && (ss-1)->statScore < 22977         // History-based condition
    &&  eval >= beta                     // Only when ahead ✅
    &&  eval >= ss->staticEval
    &&  ss->staticEval >= beta - 30*depth - 28*improving + 84*ss->ttPv + 182  // Depth-scaled margin
    && !excludedMove
    &&  pos.non_pawn_material(us)
    && (ss->ply >= thisThread->nmpMinPly || us != thisThread->nmpColor))
```

**Experimental Findings:**
- `nullMoveMinDepth = 3`: 1.55M nodes, 6.1s (baseline)
- `nullMoveMinDepth = 2`: 768K nodes, 3.2s (initial improvement)
- `nullMoveMinDepth = 2` + selective extension: **598K nodes, 2.5s** (current, best) ✓
- `nullMoveMinDepth = 1`: More nodes and slower (negative ROI)

**Remaining Gap:**
- gbchess still attempts null move 6x less often (3.41% vs 20.87%)
- However, our 79% cutoff rate shows the attempts we make are highly effective
- The gap is likely due to Stockfish's more sophisticated conditions (history scores, improving positions, etc.)

## Implemented Optimizations

### ✅ Already Implemented in gbchess
1. **Iterative Deepening** - Yes
2. **Aspiration Windows** - Yes
3. **Transposition Table** - Yes (1M entries)
4. **Null Move Pruning** - Yes (reduction=4, min_depth=2) ✓ Recently optimized
5. **Late Move Reductions (LMR)** - Yes
6. **Killer Move Heuristic** - Yes
7. **History Heuristic** - Yes
8. **MVV/LVA Move Ordering** - Yes
9. **Static Exchange Evaluation (SEE)** - Yes
10. **Quiescence Search** - Yes (depth=5)

### ❌ Missing Optimizations (Found in Stockfish 12)

#### High Impact (~50+ Elo)
1. **Futility Pruning (Node Level)** - **~50 Elo**
   - If eval >> beta at shallow depth, return without searching moves
   - Stockfish: `if (eval - futility_margin(depth) >= beta) return eval;`
   - Missing in gbchess

2. **Move Count Pruning** - **~20 Elo**
   - After searching N moves, skip remaining quiet moves
   - Stockfish: `moveCount >= futility_move_count(improving, depth)`
   - Missing in gbchess

3. **SEE Pruning for Quiet Moves** - **~20 Elo**
   - Skip moves with negative SEE at shallow depths
   - Stockfish: `!pos.see_ge(move, threshold)`
   - gbchess only uses SEE in quiescence

#### Medium Impact (~5-20 Elo)
4. **Futility Pruning (Move Level)** - **~5 Elo**
   - Skip quiet moves that can't improve alpha
   - Stockfish: `ss->staticEval + margin <= alpha`
   - Missing in gbchess

5. **Razoring** - **~1 Elo**
   - At depth 1, if eval << alpha, go to quiescence
   - Stockfish: `if (depth == 1 && eval <= alpha - 510) return qsearch()`
   - Missing in gbchess

6. **Countermove Pruning** - **~20 Elo**
   - Skip moves with poor history scores
   - Uses continuation history tables
   - Missing in gbchess

7. **More Aggressive Null Move** - **~10 Elo**
   - Dynamic reduction: `R = f(depth, eval-beta)`
   - Stockfish: `R = (817 + 71*depth)/213 + min((eval-beta)/192, 3)`
   - gbchess: Fixed `R = 3`

### 🔧 Tuning Opportunities

1. **Null Move Conditions**
   - gbchess: `depth >= 5 && !inCheck && beta > -infinity+100 && hasNonPawnMaterial`
   - Stockfish: Much more complex with eval-based conditions

2. **LMR Conditions**
   - gbchess: `depth > 2 && moveCount > 2 && isQuiet`
   - Stockfish: Uses reduction table based on depth and move number

3. **Quiescence Depth**
   - gbchess: Fixed at 5
   - Stockfish: Adaptive based on position

## Estimated Impact

**Current Status:**
- After selective extension: 598K nodes, 2.5s (correct and fast)
- 85x node gap vs Stockfish (down from original 221x)
- **61% improvement** from baseline (1.55M → 598K nodes)

Adding the missing optimizations would likely improve node count by:
- **Futility Pruning (Node)**: 2-3x reduction
- **Move Count Pruning**: 1.5-2x reduction
- **SEE Pruning**: 1.3-1.5x reduction
- **Combined**: **4-9x total reduction** (conservative estimate)

This would bring gbchess from 598K nodes to **66K-150K nodes** at depth 9.

To match Stockfish's 7K nodes (~85x remaining gap) would require additional optimizations:
- More aggressive pruning margins
- Better move ordering (reducing researches)
- Singular extensions
- Check extensions
- ProbCut
- Multi-cut

## Recommended Implementation Order

1. **Futility Pruning (Node Level)** - Easiest, highest impact
2. **Move Count Pruning** - Simple to add
3. **SEE Pruning in Main Search** - Extend existing SEE usage
4. **Dynamic Null Move Reduction** - Tune existing null move
5. **Futility Pruning (Move Level)** - More complex margin tuning
6. **Razoring** - Low impact but easy

## Code References

### Stockfish search.cpp
- Line 819: Futility pruning (node level)
- Line 807: Razoring
- Line 1022: Move count pruning
- Line 1036: Futility pruning (move level)
- Line 1046: SEE pruning
- Line 837: Dynamic null move reduction

### gbchess search.cpp
- Line 480: Null move pruning (fixed R=3)
- Line 537: LMR (simple conditions)
- Line 395: Quiescence search (fixed depth=5)
