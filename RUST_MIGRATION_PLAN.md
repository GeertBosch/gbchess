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
1. **elo_test** (standalone, good starter)
2. **fen** (fundamental, used by many components)
3. **hash** (utility component with clear interface)
4. **moves** (core engine logic, complex but well-defined)
5. **eval** (evaluation functions)
6. **nnue** (neural network evaluation)
7. **search** (main search algorithm)
8. **uci** (UCI protocol implementation)

### 3.2 Migration Strategy per Component
- Create separate Rust crate for each major component
- Maintain C++ and Rust versions side-by-side during transition
- Use integration tests to verify equivalent behavior
- Gradually replace C++ dependencies with Rust equivalents

## Phase 4: Build System Integration

### 4.1 Cargo Workspace Configuration
```toml
[workspace]
members = [
    "rust/elo-test",
    "rust/fen",
    "rust/hash",
    "rust/moves",
    "rust/eval",
    "rust/nnue", 
    "rust/search",
    "rust/uci",
    "rust/chess-engine"
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

---

*This plan will be updated as we progress through the migration and learn more about the specific challenges and opportunities in converting this chess engine to Rust.*
