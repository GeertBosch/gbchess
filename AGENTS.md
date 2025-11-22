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
  - Use standard library methods and algorithms if this results in more concise code with same perf
  -
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

## File Organization
```
src/
├── core/              - Core chess types and utilities
├── move/              - Move generation and representation
│   └── magic/         - Magic bitboard generation
├── eval/              - Position evaluation
│   └── nnue/          - Neural network evaluation
├── search/            - Search algorithms
├── hash/              - Transposition tables
├── fen/               - FEN position parsing
├── square_set/        - Bitboard operations
├── uci/               - UCI protocol interface
└── perft/             - Performance testing
```

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
