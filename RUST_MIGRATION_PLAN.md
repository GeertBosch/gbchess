# Rust Migration Plan for GB Chess Engine

## Overview
This document outlines the step-by-step plan to migrate the GB Chess Engine from C++ to Rust while
maintaining the same functionality, organization, and coding style principles.

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
9. **perft** (move generation validation and testing) ✅ COMPLETE
10. **eval** (evaluation functions) ✅ **COMPLETE**
11. **search** (main search algorithm) � **NEXT - HIGH PRIORITY**
12. **uci** (UCI protocol implementation) 📋 PLANNED
13. **nnue** (neural network evaluation) 📋 PLANNED (optional)

### 3.2 Migration Strategy per Component
- Create separate Rust crate for each major component
- Maintain C++ and Rust versions side-by-side during transition
- Use integration tests to verify equivalent behavior
- Gradually replace C++ dependencies with Rust equivalents

**Special Note on Perft**: The perft component is critical for validating move generation
correctness. It performs exhaustive tree searches to count all possible moves at various depths,
making it an excellent debugging tool for finding edge cases in move generation algorithms. ✅
**COMPLETED** - Perft implementation is working correctly and fully validated:
- ✅ Starting position: depths 1-6 (20, 400, 8,902, 197,281, 4,865,609, 119,060,324 nodes)
- ✅ Kiwipete (castling/en passant): 4,085,603 nodes at depth 4
- ✅ All 6 well-known test positions from chessprogramming.org
- ✅ Comprehensive test suite integrated into `cargo test`
- 🔧 **Key Bug Fixed**: Castling path calculation was corrected to match C++ implementation

**Next Step**: With move generation and evaluation fully validated, we can now proceed to **search**
(search algorithms) with confidence that our move generation and position evaluation are 100% correct.

## Phase 4: Build System Integration

### 4.1 Cargo Workspace Configuration
```toml
[workspace]
members = [
    "rust/elo",
    "rust/eval", 
    "rust/fen",
    "rust/hash",
    "rust/magic",
    "rust/moves",
    "rust/moves_table",
    "rust/square_set",
    "rust/moves_gen",
    "rust/perft",
    # Future components:
    # "rust/search",
    # "rust/uci",
    # "rust/nnue", 
]
```

### 4.2 Makefile Integration
- Update Makefile to build both C++ and Rust components
- Configure output to same `build/` directory
- Maintain existing test targets and workflows

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

## Migration Status

### Completed Components (11/14) - 79% Complete 🚀
- ✅ **Phase 1**: Environment Setup and Hello World - COMPLETE
- ✅ **elo**: ELO rating calculations - COMPLETE
- ✅ **fen**: FEN parsing and board representation - COMPLETE  
- ✅ **hash**: Hash table and transposition table - COMPLETE
- ✅ **square_set**: Bitboard operations and square sets - COMPLETE
- ✅ **magic**: Magic bitboard generation - COMPLETE
- ✅ **moves_table**: Precomputed move lookup tables - COMPLETE
- ✅ **moves**: Core move data structures and operations - COMPLETE
- ✅ **moves_gen**: Move generation algorithms - COMPLETE
- ✅ **perft**: Move generation validation and testing - **COMPLETE** ✅
- ✅ **eval**: Position evaluation functions - **COMPLETE** ✅

#### eval Migration Details (COMPLETED) ✅  
**Duration**: 1 day (+ quiescence search completion)
**Status**: **FULLY COMPLETE** - All tests passing, 100% identical results to C++

**Key Features Implemented**:
- ✅ Score type with centipawn arithmetic and mate score encoding
- ✅ Bill Jordan's piece-square tables with tapered evaluation 
- ✅ Game phase computation and opening/endgame interpolation
- ✅ Simple evaluation (piece values) and full PST evaluation
- ✅ **Quiescence search** with alpha-beta pruning for tactical analysis
- ✅ **100% identical results** to C++ version across all evaluation methods
- ✅ Complete test program matching C++ eval_test functionality

**API Functions**:
- `Score::from_cp()` - Create scores with centipawn values
- `evaluate_board()` - Full piece-square table evaluation
- `evaluate_board_simple()` - Basic piece value evaluation
- `evaluate_board_for_player()` - Player-relative scoring
- `quiesce()` - Quiescence search for tactical position analysis
- Command-line tool: `eval_test <fen>` with identical C++ interface

**Testing**: **COMPREHENSIVE VALIDATION COMPLETE** ✅
- ✅ **Score arithmetic**: All operations match C++ exactly
- ✅ **Mate scores**: M1, M2, etc. formatting identical to C++
- ✅ **Test positions**: 100% identical evaluation results across all methods
  - Knights vs Pawns: Simple=3.00, PST=1.70, Quiesce=2.88 (identical C++ and Rust)
  - Initial position: Simple=0.00, PST=0.00, Quiesce=0.00 (identical C++ and Rust)
  - Starting position: Simple=0.00, PST=0.00, Quiesce=0.00 (identical C++ and Rust)
  - All reference positions match exactly
- ✅ **Quiescence search**: Proper alpha-beta pruning, depth limiting, check handling
- ✅ **Build integration**: Part of `make test-rust` suite
- ✅ **Unit tests**: All score and evaluation tests passing

**Result**: Position evaluation including tactical analysis is **100% validated** and ready for search algorithms

#### perft Migration Details (COMPLETED) ✅
**Duration**: 2 days (including comprehensive debugging)  
**Status**: **FULLY COMPLETE** - All tests passing, move generation 100% validated

**Key Features Implemented**:
- ✅ Complete perft (performance test) implementation for move generation validation
- ✅ Depth-based exhaustive move counting from any position
- ✅ Perft with divide functionality showing node counts per root move
- ✅ Support for FEN input and "startpos" shorthand
- ✅ **All test positions validated** - comprehensive test suite complete
- ✅ Integration with all move generation and position management components

**API Functions**:
- `perft()` - Count total nodes at given depth
- `perft_with_divide()` - Show breakdown by root moves
- Command-line tool: `perft-test <fen|startpos> <depth>`

**Testing**: **COMPREHENSIVE VALIDATION COMPLETE** ✅
- ✅ **Starting position**: depths 1-6 (20, 400, 8,902, 197,281, 4,865,609, 119,060,324 nodes)
- ✅ **Kiwipete** (castling, en passant): depth 4, 4,085,603 nodes
- ✅ **Position 3** (pawn promotion): depth 5, 674,624 nodes  
- ✅ **Position 4** (promotions to all pieces): depth 4, 422,333 nodes
- ✅ **Position 5** (tactical position): depth 4, 2,103,487 nodes
- ✅ **Position 6** (middlegame): depth 4, 3,894,594 nodes
- ✅ **All known reference positions** from chessprogramming.org validated
- 🔧 **Critical castling bug fixed** using systematic perft debugging methodology

**Result**: Move generation pipeline is **100% validated** and we're ready for evaluation

### Current Workspace Structure
```
rust/
├── elo/           # ELO calculation system
├── eval/          # Position evaluation functions (COMPLETE)
├── fen/           # FEN parsing and board representation  
├── hash/          # Zobrist hashing for positions
├── magic/         # Magic bitboard generation
├── moves/         # Core move operations and position updates
├── moves_gen/     # Complete move generation algorithms
├── moves_table/   # Move/capture table generation
├── perft/         # Move generation validation and testing (COMPLETE)
└── square_set/    # Bitboard square set operations
```

### Remaining Components (3/14)
- 🔄 **search**: Main search algorithm (**NEXT PRIORITY** - HIGH)
- 📋 **uci**: UCI protocol implementation
- 📋 **nnue**: Neural network evaluation (optional advanced feature)

### Recent Major Achievement: Move Generation + Evaluation Foundation Complete 🏆
- **Perft Comprehensive Testing**: ✅ **COMPLETED** - All 6 well-known perft positions validated with exact node counts
- **Position Evaluation**: ✅ **COMPLETED** - 100% identical results to C++ across all test positions
- **Critical Bug Resolution**: Castling path calculation fixed using systematic debugging approach documented in USING_PERFT.md
- **100% Move Generation + Evaluation Validation**: Ready to build search algorithms on solid, tested foundation
- **Build System Integration**: All Rust components seamlessly integrated with existing Makefile system

### Next Priority: Search Algorithm (`search`) 🎯

**Why search is critical now:**
- **Foundation Ready**: Move generation and evaluation are 100% validated and correct
- **Engine Completion**: Final major component needed for functional chess engine
- **Playing Capability**: Search provides the "intelligence" that finds best moves
- **Critical Path**: search → uci completes the engine

**Implementation Approach:**
1. **Analyze C++ `search.cpp`** - understand current search framework and algorithms
2. **Port alpha-beta algorithm** - core minimax search with pruning
3. **Implement quiescence search** - tactical position analysis for captures
4. **Add move ordering** - optimize search efficiency using evaluation
5. **Validate with test positions** - ensure search finds same moves as C++ version

---

*This plan will be updated as we progress through the migration and learn more about the specific challenges and opportunities in converting this chess engine to Rust.*
