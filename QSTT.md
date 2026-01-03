# Quiescence Search Transposition Table (QSTT) Implementation Plan

## Overview

Chess engines commonly use the transposition table (TT) in quiescence search (QS), storing results with depth=0 to distinguish them from main search entries. This prevents QS entries from being used in normal search while allowing QS to benefit from cached positions.

## Incremental Implementation Plan

### Step 1: Add QS TT Statistics Infrastructure

**Files to modify:** [src/search/search.h](src/search/search.h), [src/search/search.cpp](src/search/search.cpp)

**Changes:**
- Add counters `qsTTRefinements` and `qsTTCutoffs` (mirroring existing `ttRefinements`/`ttCutoffs`)
- Report them in `reportSearchStatistics()` around line 178
- Statistics should initially show zeros

**Test:** Run `make test -j` and `make puzzles` - should pass with zero QS TT stats.

### Step 2: Enable TT Probes in QS

**Files to modify:** [src/search/search.cpp](src/search/search.cpp)

**Changes:**
- Add `Hash hash(position)` at start of `quiesce()` around line 450
- Call `transpositionTable.refineAlphaBeta(hash, 0, alpha, beta)`
- Increment `qsTTRefinements` when refinement occurs
- Check for cutoff `if (alpha >= beta)` and increment `qsTTCutoffs`

**Test:** Run `make test -j` and `make puzzles` - statistics should now be non-zero.

### Step 3: Store QS Results in TT

**Files to modify:** [src/hash/hash.cpp](src/hash/hash.cpp), [src/search/search.cpp](src/search/search.cpp)

**Changes:**
- Modify `TranspositionTable::insert()` at line 160 to accept `depthleft=0`
  - Change `if (!move || depthleft < 1)` to `if (!move)`
- Add TT storage in `quiesce()` before returning
  - Store with correct entry type (EXACT/LOWERBOUND/UPPERBOUND)
  - Use depth=0 for all QS entries

**Test:** Run `make test -j` and `make puzzles` - verify correctness and measure node reduction.

## Design Decisions

### Move Tracking
Not implementing move tracking in QS for this change. QS doesn't currently track which move raised alpha, and adding that should be a separate change if needed.

### Hash Computation
Using `Hash hash(position)` at QS entry initially. While we have code for incremental hash updates, we'll measure impact first:
- If we see node reduction (TT helps) but increased CPU time, we can optimize hash computation separately
- This allows us to assess the improvement from each change individually

### Table Pressure
Not adding separate `qsTTStores` statistic initially. Existing TT statistics (`numCollisions`, `numOccupied`) will show if QS entries increase table pressure. We can add more detailed tracking if needed.

### Depth Semantics
Using `depth=0` for all QS entries ensures they:
- Won't be used in normal search (which requires `depthleft >= 1`)
- Are stored in the same TT structure (no separate QS table needed)
- Follow established chess engine practice

## Testing Protocol

After each step:
1. Run `make test -j` - all tests must pass
2. Run `make puzzles` - must maintain 93/100+ success rate
3. Record statistics and compare to baseline
4. Commit working state before proceeding to next step

## Expected Impact

**Baseline metrics** (to be recorded):
- Node counts at various depths
- Puzzle solve rate
- QS node percentage

**Expected improvements:**
- Reduced QS nodes due to TT cutoffs
- Better move ordering from TT hints
- Minimal table pressure increase (QS entries are depth=0, lowest priority)

## Rollback Plan

Each step is independently reversible via git. If any step:
- Fails tests
- Reduces puzzle quality below 93/100
- Shows no measurable benefit

Then revert that step and analyze before proceeding.
