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
- **Build Tool**: GNU Make with custom functions like `calc_objs`. Pass the `-j` option to make, as
  we generally expect success and want to minimize wait time. Only when narrowing down failures you
  can omit `-j`.
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
- **Key Components**: Move generation (`moves*.cpp/h`), Position evaluation (`eval.cpp/h`, NNUE),
  Search algorithms (`search.cpp/h`), Hash tables (`hash.cpp/h`), FEN parsing (`fen.cpp/h`)

### Testing Protocol
Generally use `make test -j` for compiling the code testing changes, as that is quicker than
multiple round trips between LLM and engineer doing narrower compiles and tests. When debugging
failures use the more specific tests:
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
├── moves*.{cpp,h}     - Move generation
├── eval.{cpp,h}       - Position evaluation  
├── search.{cpp,h}     - Search algorithms
├── nnue*.{cpp,h}      - Neural network evaluation
├── hash.{cpp,h}       - Transposition tables
├── fen.{cpp,h}        - FEN position parsing
└── xxx_test.cpp       - Unit tests for xxx.{cpp,h}
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
5. Use appropriate compiler flags for the component
