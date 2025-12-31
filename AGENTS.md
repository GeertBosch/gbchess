# AI Agent Instructions for gbchess

## Coding Style & Conventions
- Use C++17 features
- Prefer early returns over deep nesting
- Keep functions small and focused
- If you see dead code or unused code, remove it
- Use `/** */` documentation for APIs, `//` for clarifying implementation
- Avoid unnecessary braces/parentheses
- Follow existing naming conventions in the codebase
- Be mindful of performance - this is a chess engine where speed matters

## Project Description
gbchess is a high-performance C++ chess engine focusing on:
- Implement commonly used chess programming algorithms
- Keep the code base as modular as possible
- Allow individual chess algorithms and optimizations to be turned on and off
- Efficient move generation using table-based methods for simplicity and speed
- Strong position evaluation using NNUE
- Fast search algorithms
- Correctness validation through extensive testing

## Development Guidelines

### Code Quality
- **Language**:
  - Idiomatic C++17 with careful attention to performance
  - Minimize dependence on external libraries to keep the code self-contained
- **Style**:
  - Minimize nesting, prefer early returns, keep functions focused
  - Use standard library methods and algorithms where more concise with same perf
  - Prefer "\n" over `std::endl`;
- **Conventions**:
  - Follow existing naming conventions in the code, avoid unnecessary braces/parentheses
- **Documentation**:
  - Use `/** */` for to document APIs, such as function, struct, class and method declarations
  - `//` for clarifying the implementation
- **Error Handling**: Prefer compile-time checks and assertions

### Build System Knowledge
- **Build Tool**: GNU Make with custom functions like `calc_objs`.
- **CRITICAL**: Always use `make -j` for parallel builds to minimize wait time and see all failures
  at once. Avoid incremental testing that requires multiple LLM/human interaction rounds. Only omit
  `-j` when narrowing down specific failures after seeing the complete failure picture.
- **Structure**:
  - `src/` - Source files
  - `build/opt/` - Optimized builds
  - `build/dbg/` - Debug builds with sanitizers
- **Tests**: Unit tests named `xxx_test.cpp` unit test the `xxx.{h,cpp}` files.
- **Key Functions**:
  - `calc_objs(BUILD_TYPE, SOURCES)` - converts source to object files
  - `test_dir` - determines build directory based on target

### Chess Engine Specifics
- **Move Generation**: Critical performance path, uses magic bitboards called `SquareSet` here
- **Evaluation**: Hybrid classical + NNUE neural network
- **Search**: Alpha-beta with various optimizations
- **Testing**: Perft for move generation validation, tactical puzzles for search
- **Key Components**: Move generation (`move/`), Position evaluation (`eval/`, `nnue/`),
  Search algorithms (`search/`), Hash tables (`hash/`), FEN parsing (`fen/`)

### Testing Protocol
**ALWAYS start with parallel builds**: Use `make -j` to see the complete failure picture immediately.
This is far more efficient than incremental testing requiring multiple LLM/human interaction rounds.

**Primary testing commands** (use these first):
- Test all changes: `make test -j`
- Build specific targets: `make -j <target1> <target2>...`
- Clean rebuild: `make clean && make -j`

**Specific debugging** (only after seeing full failure picture):
- Run unit tests: `make test-cpp`
- Validate move generation: `make perft-test`
- Check search correctness: `make mate123 mate45`
- Performance regression: `make perft-bench`

### Performance Considerations
- Hot paths: move generation, evaluation, search
- Memory usage: minimize allocations in search
- SIMD: Use well-known x86 SSE2 APIs, but implement these in standard C++ to keep the code base target
  independent. Rely on compiler optimization generate into efficient code.
  Check performance of the SIMD methods against native SSE2 libraries on x86 targets.
- Compiler optimizations: separate debug/release builds, by default build both

### Common Tasks
- **Adding features**: Update relevant tests first, reference well-known sites such as Wikipedia or
  chessprogramming.org.
- **Adding tests**:
  - Prefer testing interfaces (APIs) instead of implementation details
  - Prefer unit tests over integration tests
- **Bug fixes**:
  - Find the earliest commit with the problem, using `git bisect run` when appropriate
  - If the cause is unknown, bisect the set of changes introducing the problem when feasible
  - Add a test reproducing the issue before fixing to avoid future regressions.
  - For incorrect move generation, see `USING_PERFT.md` to debug.
  - For errors in solving puzzles, do the following:
    - From the output of the failed `search-test` or `search-debug` run, find the correct solution
    - Create a new FEN by applying
  - After fixing a bug, consider the more general and wider class of similar bugs and update our
    testing approach to avoid them. A bug is a symptom of insufficient testing, so don't just fix
    the symptom but also the root cause.
- **Optimization**: Measure before/after with perft-bench for move gen or make puzzles for search.
  If an attempted optimization didn't result in expected performance gain, consider removing it to
  simplify the code.
- **Refactoring**: Ensure the code is appropriately covered by tests before refactoring. After any
  refactoring follow the testing protocol to ensure all tests still pass and performance has not
  regressed.

## File Organization and Modular Architecture

See `src/README.md`.

## Integration Notes
When working with this codebase:
1. Understand chess concepts involved
2. Check existing similar implementations
3. Consider both correctness AND performance
4. Validate changes with appropriate test suite
5. Document any algorithmic changes

## Quick Development Workflow
When making changes:
1. Understand the chess domain context
2. Maintain backward compatibility with existing tests
3. Consider performance implications
4. Update relevant tests if behavior changes
5. **ALWAYS validate with `make -j` first** - see all issues at once instead of incremental testing

### Efficient Testing Workflow
1. **Make changes** to source files
2. **Test comprehensively**: `make clean && make -j` or `make test -j`
3. **Review ALL failures** at once - don't fix incrementally
4. **Only then** use specific tests like `make test-cpp` to debug individual issues
5. **Performance check**: `make perft-bench` after algorithmic changes

## Data-Driven Optimization Protocol

**CRITICAL**: When optimizing search or evaluation, follow this scientific methodology to avoid
regressions. Past experience shows that making multiple simultaneous changes without proper
validation leads to correctness bugs and performance regressions that are hard to diagnose.

### Phase 1: Establish Baseline (No Code Changes)

Before making ANY optimization changes, record comprehensive baseline metrics:

1. **Performance Metrics**:
   ```bash
   # Record node counts at multiple depths
   ./search.sh gbchess "test-position-fen" 2
   ./search.sh gbchess "test-position-fen" 4
   ./search.sh gbchess "test-position-fen" 8

   # Compare with Stockfish
   ./search.sh stockfish "test-position-fen" 8
   ```

2. **Quality Metrics**:
   ```bash
   make test -j          # All tests must pass
   make mate123 mate45   # Mate-finding ability
   make puzzles          # maintain 93/100 minimum
   ```

3. **Document Current State**:
   - Node counts at depth 2, 4, 8 for standard test positions
   - Time to depth for real game positions
   - Puzzle success rate
   - All UCI test results
   - Current vs Stockfish node ratio at each depth

4. **Identify Specific Problems**:
   - Which puzzles fail and why?
   - Which positions show biggest node explosion vs Stockfish?
   - Where does search spend most time? (main search vs quiescence)

### Phase 2: Gather Diagnostic Data

Before implementing "fixes", understand the root cause:

1. **Instrument Both Engines**:
   - Add verbose logging to trace specific problem positions
   - Log quiescence search decisions: moves tried, pruning reasons, depth reached
   - Compare gbchess vs Stockfish behavior on identical positions
   - Track statistics: TT hit rates, move ordering effectiveness, pruning frequency

2. **Profile Resource Usage**:
   - What % of nodes are in quiescence vs main search?
   - Which pruning techniques fire and how often?
   - What's the first-move cutoff rate? (should be >85%)
   - What's the TT hit rate? (should be >80%)

3. **Analyze Specific Cases**:
   - Pick one problem position and understand it completely
   - Why does Stockfish use fewer nodes on this position?
   - What decisions differ between the engines?

**Do NOT proceed to Phase 3 until you have concrete data explaining the performance difference.**

### Phase 3: One Change at a Time

For each potential optimization:

1. **State Clear Hypothesis**:
   - "Delta pruning will reduce QS nodes by ~X% without hurting quality"
   - "Improving TT hit rate from Y% to Z% will reduce nodes by W%"
   - Base hypothesis on Phase 2 diagnostic data, not assumptions

2. **Implement ONLY One Change**:
   - Change one thing: one pruning technique, one parameter, one algorithm
   - Do not combine multiple optimizations
   - Keep changes small and reversible

3. **Validate Completely Before Proceeding**:
   ```bash
   # All tests must pass
   make test -j

   # Quality must not degrade
   make puzzles  # Must maintain 93/100+
   make mate123 mate45  # Must pass

   # Record new performance
   ./search.sh gbchess "test-position-fen" 2
   ./search.sh gbchess "test-position-fen" 8

   # Compare to baseline
   ```

4. **Decision Criteria**:
   - **Keep**: If ALL tests pass AND (nodes decrease OR time decreases OR quality improves)
   - **Revert**: If ANY test fails OR quality drops OR no measurable improvement
   - **Commit**: Immediately after deciding to keep, before trying next change

5. **Document Results**:
   - What changed (code + rationale)
   - Baseline vs new metrics
   - Why it worked or didn't work
   - Commit message should reference the hypothesis and results

### Phase 4: Priority Order Based on Data

Only after Phase 2 diagnostics, prioritize optimizations. Example priority:

1. **If TT hit rate is low (<80%)**:
   - Investigate TT usage in quiescence
   - Check replacement policy
   - Verify depth calculations

2. **If move ordering is poor (<85% first-move cutoffs)**:
   - Improve move scoring
   - Try different heuristics (countermove, killer moves)
   - Compare ordering to Stockfish

3. **If many nodes in quiescence**:
   - Analyze quiescence depth strategy
   - Test pruning techniques (delta, SEE, futility)
   - Validate each independently

4. **If main search inefficient**:
   - Test null-move pruning parameters
   - Try late move reductions
   - Check extension criteria

**Process each item completely before moving to the next.**

### Anti-Patterns to Avoid

1. **Making Multiple Changes Simultaneously**:
   - ❌ "Let me add delta pruning, change QS depth, and modify SEE all at once"
   - ✅ Add delta pruning, test completely, commit. Then consider QS depth separately.

2. **Assuming Something is Broken**:
   - ❌ "This std::clamp looks buggy, let me fix it"
   - ✅ "Let me measure current behavior and verify it's actually a problem"

3. **Copying Stockfish Without Understanding**:
   - ❌ "Stockfish does X, so we should too"
   - ✅ "Stockfish does X. Let me understand why and test if it helps us"

4. **Optimizing Without Measuring**:
   - ❌ "This change should reduce nodes"
   - ✅ "Baseline is N nodes. After change, it's M nodes. Impact: (M-N)/N%"

5. **Prioritizing Nodes Over Correctness**:
   - ❌ "We reduced nodes by 50% but puzzles dropped from 93 to 92"
   - ✅ "Maintain 93/100+ puzzles. Only accept optimizations that preserve quality"

6. **Ignoring Test Failures**:
   - ❌ "These tests fail but the optimization is good overall"
   - ✅ "Any test failure means the change is rejected, no exceptions"

### Key Principles

1. **Measure Everything**: Before, during, and after. No assumptions.

2. **One Variable at a Time**: Change one thing, test completely, commit or revert, repeat.

3. **Validate Assumptions**: "This should help" → Test it. "Stockfish does X" → Understand why.

4. **Quality Over Speed**: Better to find the right move slowly than the wrong move quickly.

5. **Scientific Method**:
   - Hypothesis (based on data)
   - Experiment (isolated change)
   - Measure (objective metrics)
   - Conclude (based on results, not intentions)

6. **Reversibility**: Every change must be easily revertible. Commit working states frequently.

### Example: Good Optimization Process

```
1. Baseline: Record depth 8 = 72,603 nodes, puzzles = 93/100
2. Diagnose: Profile shows 60% of time in quiescence search
3. Analyze: Quiescence tries many losing captures
4. Hypothesis: "Delta pruning in QS will reduce nodes by 10-15%"
5. Implement: Add ONLY delta pruning to quiesce()
6. Test: make test -j → PASS, puzzles → 93/100, depth 8 → 61,756 nodes (15% reduction)
7. Commit: "Add delta pruning in quiescence: 15% node reduction at depth 8"
8. Next: Now consider next optimization (futility pruning)
```

### Example: Failed Optimization (What Not To Do)

```
❌ 1. Notice: "Stockfish uses std::clamp differently"
❌ 2. Change: Rewrite QS depth logic + add delta pruning + modify SEE + change node counting
❌ 3. Test: Multiple failures, unclear which change caused problems
❌ 4. Debug: Try to fix failures incrementally, making more changes
❌ 5. Result: Worse performance, broken tests, unclear how to recover
```

**If you find yourself in this situation, stop immediately and revert all changes. Start over with Phase 1.**