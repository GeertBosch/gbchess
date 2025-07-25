# Rust Migration Plan for GB Chess Engine

## Overview
This document outlines the step-by-step plan to migrate the GB Chess Engine from C++ to Rust while maintaining the same functionality, organization, and coding style principles.

## Phase 1: Environment Setup and Hello World

### 1.1 Install Rust Toolchain
- Install `rustup` (Rust installer and version management tool)
- Install the latest stable Rust compiler
- Install `cargo` (Rust's build system and package manager)
- Set up VS Code extensions for Rust development

### 1.2 Project Structure Setup
- Create `Cargo.toml` workspace file in the root directory
- Create `rust/` directory to house Rust code alongside existing `src/` (C++)
- Set up workspace with multiple crates for different components
- Configure build system to produce executables in `build/` directory

### 1.3 Hello World Verification
- Create a simple "Hello, World!" Rust program
- Verify compilation and execution
- Test cargo build system integration

## Phase 2: First Migration - ELO Test

### 2.1 Analyze elo_test.cpp
- Review current C++ implementation
- Identify dependencies and data structures
- Map C++ concepts to Rust equivalents

### 2.2 Create Rust ELO Test Crate
- Set up `elo-test` crate in `rust/elo-test/`
- Implement basic structure following C++ organization
- Apply coding style: early returns, minimal nesting, small functions

### 2.3 Port Core Functionality
- Convert main ELO calculation logic
- Implement equivalent data structures using Rust idioms
- Maintain same command-line interface and output format

## Phase 3: Incremental Component Migration

### 3.1 Priority Order for Migration
1. **elo_test** (standalone, good starter) ✅ COMPLETE
2. **fen** (fundamental, used by many components) ✅ COMPLETE
3. **hash** (utility component with clear interface) ✅ COMPLETE
4. **square_set** (bitboard operations) ✅ COMPLETE
5. **magic** (magic bitboard generation) ✅ COMPLETE
6. **moves_table** (precomputed move tables) ✅ COMPLETE
7. **moves** (core move logic) ✅ COMPLETE
8. **moves_gen** (move generation algorithms) ⏳ NEXT
9. **perft** (move generation validation and testing) ⏳ HIGH PRIORITY
10. **eval** (evaluation functions) 📋 PLANNED
11. **nnue** (neural network evaluation) 📋 PLANNED
12. **search** (main search algorithm) 📋 PLANNED
13. **uci** (UCI protocol implementation) 📋 PLANNED

### 3.2 Migration Strategy per Component
- Create separate Rust crate for each major component
- Maintain C++ and Rust versions side-by-side during transition
- Use integration tests to verify equivalent behavior
- Gradually replace C++ dependencies with Rust equivalents

**Special Note on Perft**: The perft component is critical for validating move generation correctness. It performs exhaustive tree searches to count all possible moves at various depths, making it an excellent debugging tool for finding edge cases in move generation algorithms. This should be implemented immediately after moves_gen to catch any bugs early.

## Phase 4: Build System Integration

### 4.1 Cargo Workspace Configuration
```toml
[workspace]
members = [
    "rust/elo",
    "rust/fen",
    "rust/hash",
    "rust/magic",
    "rust/moves",
    "rust/moves_table",
    "rust/square_set",
    # Future components:
    # "rust/moves_gen",
    # "rust/perft",
    # "rust/eval",
    # "rust/nnue", 
    # "rust/search",
    # "rust/uci",
    # "rust/chess-engine"
]
```

### 4.2 Makefile Integration
- Update Makefile to build both C++ and Rust components
- Configure output to same `build/` directory
- Maintain existing test targets and workflows

### 4.3 Cross-Language Integration
- Use C FFI when needed for gradual migration
- Create wrapper functions for interfacing between C++ and Rust
- Plan for eventual complete replacement

## Phase 5: Rust-Specific Optimizations

### 5.1 Memory Management
- Leverage Rust's ownership system for safe memory management
- Replace manual memory management with RAII patterns
- Use `Box`, `Rc`, `Arc` appropriately for different ownership needs

### 5.2 Error Handling
- Use `Result<T, E>` and `Option<T>` instead of error codes
- Implement proper error propagation with `?` operator
- Create custom error types for domain-specific errors

### 5.3 Concurrency and Performance
- Use Rust's safe concurrency primitives where applicable
- Leverage zero-cost abstractions
- Apply SIMD optimizations using safe Rust libraries

### 5.4 Idiomatic Rust Patterns
- Use iterators and functional programming patterns
- Implement traits for common operations
- Use pattern matching for control flow
- Apply borrowing and lifetimes for efficient memory usage

## Phase 6: Testing and Validation

### 6.1 Unit Testing
- Port existing C++ unit tests to Rust using `cargo test`
- Maintain test coverage during migration
- Add Rust-specific tests for new functionality

### 6.2 Integration Testing
- Verify identical behavior between C++ and Rust versions
- Use existing puzzle tests as integration benchmarks
- Performance comparison between implementations

### 6.3 Benchmarking
- Use `criterion` crate for Rust benchmarking
- Compare performance with C++ implementation
- Identify and address performance regressions

## Phase 7: Documentation and Cleanup

### 7.1 Documentation
- Write comprehensive rustdoc documentation
- Update README with Rust build instructions
- Document migration lessons learned

### 7.2 Final Cleanup
- Remove C++ code once Rust equivalents are verified
- Consolidate build system to pure Rust
- Optimize final project structure

## Coding Style Guidelines for Rust Migration

### From C++ Principles to Rust
- **Early returns**: Use `?` operator and guard clauses
- **Small functions**: Maintain same function decomposition
- **Minimal nesting**: Use pattern matching and early returns
- **Documentation**: Use `///` for public APIs, avoid over-commenting trivial code

### Rust-Specific Guidelines
- Use `snake_case` for functions and variables
- Use `PascalCase` for types and traits
- Prefer `match` over nested `if` statements
- Use `impl` blocks to organize related functionality
- Leverage type system for compile-time guarantees

## Timeline Estimate

- **Phase 1**: 1-2 days (setup and hello world)
- **Phase 2**: 3-5 days (elo-test migration)
- **Phase 3**: 2-3 weeks (component-by-component migration)
- **Phase 4**: 3-5 days (build system integration)
- **Phase 5**: 1 week (Rust optimizations)
- **Phase 6**: 1 week (testing and validation)
- **Phase 7**: 2-3 days (documentation and cleanup)

**Total Estimated Time**: 6-8 weeks for complete migration

## Success Criteria

1. All existing functionality preserved
2. Same command-line interfaces maintained
3. Performance equal to or better than C++ version
4. All tests passing
5. Clean, idiomatic Rust code
6. Comprehensive documentation
7. Streamlined build process

## Next Steps

1. Begin with Phase 1: Install Rust toolchain and set up development environment
2. Create initial project structure
3. Implement "Hello, World!" to verify setup
4. Start with elo-test migration as proof of concept

## Migration Status

### Completed Components
- ✅ **Phase 1**: Environment Setup and Hello World - COMPLETE
- ✅ **elo_test**: ELO calculation system with probability functions, K-factor adjustments, and comprehensive testing
- ✅ **fen**: FEN string parsing and generation with position validation, board representation, and move parsing
- ✅ **hash**: Zobrist hashing implementation with piece-square tables, castling, en passant, and turn hashing
- ✅ **square_set**: Efficient bitboard operations for square sets with rank/file/diagonal operations
- ✅ **magic**: Magic bitboard generation for sliding piece attack lookups (rooks and bishops)
- ✅ **moves_table**: Precomputed move/capture tables for all pieces with path finding and attacker detection
- ✅ **moves**: Core move representation, make/unmake operations, position updates, and attack detection

### Component Details

#### moves_table (Sixth Migration)
- **Status**: COMPLETE ✅
- **Location**: `/rust/moves_table/`
- **Features**:
  - Precomputed move tables for all piece types (pawns, knights, bishops, rooks, queens, kings)
  - Separate capture tables for pieces with different move/capture patterns (pawns)
  - Path finding between squares for sliding pieces
  - Attacker detection for given squares
  - Full integration with fen and square_set crates
  - Comprehensive unit and integration tests
- **Tests**: All passing (5 unit tests + integration test binary)
- **API**: Compatible with chess engine requirements, supports both white and black pieces

#### moves (Seventh Migration - Latest)
- **Status**: COMPLETE ✅  
- **Location**: `/rust/moves/`
- **Features**:
  - Complete Move type with from/to squares and move kinds (quiet, capture, castling, en passant, promotions)
  - Null move representation (from == to == A1) matching C++ behavior
  - BoardChange and compound move system for complex moves (castling, en passant, promotions)
  - make_move/unmake_move functions for board state updates
  - Position updates with turn state (active color, castling rights, en passant target, clocks)
  - Attack detection and pinned piece calculation
  - Castling mask calculations for rights management
  - **Critical Fix**: En passant capture logic corrected to properly remove captured pawns
- **Tests**: All passing, including complex en passant scenarios
- **API**: Faithful port of C++ moves namespace with identical behavior
- **Display**: Move formatting with UCI notation and null move ("0000") support

#### Current Workspace Structure
```
rust/
├── elo/           # ELO calculation system
├── fen/           # FEN parsing and board representation  
├── hash/          # Zobrist hashing for positions
├── magic/         # Magic bitboard generation
├── moves/         # Core move operations and position updates
├── moves_table/   # Move/capture table generation
└── square_set/    # Bitboard square set operations
```

### In Progress
- None currently

### Remaining Components
- **moves_gen**: Move generation using moves_table and magic bitboards (next priority)
- **perft**: Move generation validation and correctness testing (critical for move_gen validation)
- **eval**: Position evaluation functions  
- **nnue**: Neural network evaluation
- **search**: Main search algorithm (minimax, alpha-beta, etc.)
- **uci**: UCI protocol implementation
- **Integration**: Final chess engine binary

### Recent Achievements
- **En Passant Fix**: Resolved critical bug in Rust moves implementation where en passant captures weren't properly removing the captured pawn. The compound move system now correctly handles the complex two-step process of en passant.
- **API Completeness**: The moves crate now provides full parity with the C++ implementation, including all move types, position updates, and edge cases.
- **Test Coverage**: All move-related tests pass, including complex scenarios like castling, en passant, and promotions.
- **Quiet Mode**: Updated magic test generator to match C++ behavior with quiet default mode and `--verbose` flag for statistics.

---

*This plan will be updated as we progress through the migration and learn more about the specific challenges and opportunities in converting this chess engine to Rust.*
