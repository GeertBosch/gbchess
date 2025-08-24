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
8. **moves_gen** (move generation algorithms) ✅ COMPLETE
9. **perft** (move generation validation and testing) 🔄 **IN PROGRESS**
10. **eval** (evaluation functions) ⏳ NEXT - HIGH PRIORITY
11. **nnue** (neural network evaluation) 📋 PLANNED
12. **search** (main search algorithm) 📋 PLANNED
13. **uci** (UCI protocol implementation) 📋 PLANNED

### 3.2 Migration Strategy per Component
- Create separate Rust crate for each major component
- Maintain C++ and Rust versions side-by-side during transition
- Use integration tests to verify equivalent behavior
- Gradually replace C++ dependencies with Rust equivalents

**Special Note on Perft**: The perft component is critical for validating move generation correctness. It performs exhaustive tree searches to count all possible moves at various depths, making it an excellent debugging tool for finding edge cases in move generation algorithms. ✅ **COMPLETED** - Perft implementation is working correctly and validated through depth 3: startpos depth 1 (20 moves), depth 2 (400 moves), depth 3 (8,902 moves) - all matching expected reference values.

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
    "rust/moves_gen",
    "rust/perft",
    # Future components:
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
- ✅ **elo**: ELO rating calculations - COMPLETE
- ✅ **fen**: FEN parsing and board representation - COMPLETE  
- ✅ **hash**: Hash table and transposition table - COMPLETE
- ✅ **square_set**: Bitboard operations and square sets - COMPLETE
- ✅ **magic**: Magic bitboard generation - COMPLETE
- ✅ **moves_table**: Precomputed move lookup tables - COMPLETE
- ✅ **moves**: Core move data structures and operations - COMPLETE
- ✅ **moves_gen**: Move generation algorithms - COMPLETE
- 🔄 **perft**: Move generation validation and testing - **IN PROGRESS** ✨ **BASIC COMPLETE**

#### perft Migration Details (In Progress) 🔄 **BASIC COMPLETE**
**Duration**: 1 day (basic implementation)  
**Status**: Basic functionality complete, comprehensive testing needed
**Key Features Implemented**:
- ✅ Complete perft (performance test) implementation for move generation validation
- ✅ Depth-based exhaustive move counting from any position
- ✅ Perft with divide functionality showing node counts per root move
- ✅ Support for FEN input and "startpos" shorthand
- ✅ **Basic validation** - starting position tested through depth 3
- ⏳ **Comprehensive test suite needed** - well-known positions with edge cases
- ✅ Validation testing confirming correct move counts at multiple depths:
  - Depth 1: 20 moves ✅ 
  - Depth 2: 400 moves ✅
  - Depth 3: 8,902 moves ✅ 
  - All results match known perft reference values
- ✅ Integration with all move generation and position management components

**API Functions**:
- `perft()` - Count total nodes at given depth
- `perft_with_divide()` - Show breakdown by root moves
- Command-line tool: `perft-test <fen|startpos> <depth>`

**Testing**: Successfully validates move generation correctness with known perft values.
- ✅ **Starting position validated through depth 3**: 20, 400, 8,902 nodes
- ⏳ **Next: Comprehensive test suite needed** covering:
  - Kiwipete position (castling, en passant): depth 4, 4,085,603 nodes
  - Position 3 (pawn promotion, complex): depth 5, 674,624 nodes  
  - Position 4 (promotions to all pieces): depth 4, 422,333 nodes
  - Position 5 (tactical position): depth 4, 2,103,487 nodes
  - Position 6 (middlegame): depth 4, 3,894,594 nodes
  - All known reference positions from chessprogramming.org#### moves_gen Migration Details (Completed)
**Duration**: 1 day  
**Key Features Implemented**:
- ✅ Complete pawn move generation (single/double pushes, captures, en passant, promotions)
- ✅ Piece move generation for all piece types (sliding and non-sliding)
- ✅ Castling logic (king-side and queen-side)
- ✅ Legal move validation with check detection
- ✅ Move counting for search algorithms
- ✅ Integration with existing Rust chess infrastructure

**API Functions**:
- `all_legal_moves_and_captures()` - Generate all legal moves
- `all_legal_captures()` - Generate only capture moves  
- `all_legal_moves()` - Generate only non-capture moves
- `SearchState` - Maintain search context and game state
- `does_not_check()` - Validate moves don't leave king in check

**Testing**: Comprehensive test suite with multiple chess positions validates correctness.
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
├── moves_gen/     # Complete move generation algorithms
├── moves_table/   # Move/capture table generation
├── perft/         # Move generation validation and testing
└── square_set/    # Bitboard square set operations
```

### In Progress
- 🔄 **perft**: Comprehensive test suite needed for edge cases (castling, en passant, promotions)

### Remaining Components
- **eval**: Position evaluation functions (next priority)
- **nnue**: Neural network evaluation
- **search**: Main search algorithm (minimax, alpha-beta, etc.)
- **uci**: UCI protocol implementation
- **Integration**: Final chess engine binary

### Recent Achievements
- **Perft Basic Implementation**: ✨ **NEW** - Added perft testing capability with basic validation through depth 3 (20, 400, 8,902 nodes for starting position). **Next: comprehensive test suite needed** for edge cases.
- **Moves Generation Fixes**: Multiple critical fixes including out-of-bounds access prevention and API improvements.
- **Build System Integration**: All Rust components now build and test seamlessly with the existing Makefile system.
- **Singleton Pattern**: Refactored moves_table to use efficient global singleton pattern for better performance.
- **Enhanced Debugging**: Added Rust panic breakpoints for better debugging experience in VS Code.

### Next Immediate Steps (Recommended)
1. **Extended Perft Testing** 🎯 **HIGH PRIORITY** 
   - ✅ **Starting position validated** (depths 1-3: 20, 400, 8,902 nodes)
   - **Add comprehensive test suite** with well-known perft positions:
     - **Kiwipete**: `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -` (castling, en passant)
     - **Position 3**: `8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -` (pawn promotion)
     - **Position 4**: `r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -` (promotions to all pieces)
     - **Position 5**: `rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -` (tactical)
     - **Position 6**: `r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -` (middlegame)
   - Test depths up to 4-6 (avoiding 128-bit integer requirements)
   - Performance comparison with C++ implementation

2. **Begin eval Migration**
   - Port basic piece-square table evaluation
   - Implement material counting and basic positional evaluation
   - Set up evaluation testing framework

---

*This plan will be updated as we progress through the migration and learn more about the specific challenges and opportunities in converting this chess engine to Rust.*
