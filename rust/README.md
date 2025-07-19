# Rust Components of GB Chess Engine

This directory contains Rust implementations of the GB Chess Engine components, developed alongside the existing C++ codebase as part of a gradual migration.

## Structure

```
rust/
├── elo/              # ELO rating system (first migrated component)
│   ├── src/
│   │   ├── main.rs   # Integration tests matching C++ behavior
│   │   └── elo.rs    # ELO rating calculation module
│   └── Cargo.toml
├── fen/              # FEN parsing and chess types (second migrated component)
│   ├── src/
│   │   ├── main.rs   # Integration tests and examples
│   │   ├── types.rs  # Chess types (Square, Piece, Color, etc.)
│   │   ├── board.rs  # Board representation and game state
│   │   └── fen.rs    # FEN string parsing and generation
│   └── Cargo.toml
├── hash/             # Zobrist hashing for positions (third migrated component)
│   ├── src/
│   │   ├── main.rs   # Integration tests and examples
│   │   └── hash.rs   # Hash implementation with move types
│   └── Cargo.toml
└── (future components)
```

## Building

The Rust components are integrated with the main Makefile:

```bash
# Build all Rust components
make rust-build

# Run Rust unit tests
make rust-test

# Run all tests (C++ and Rust)
make test

# Clean everything including Rust artifacts
make clean
```

You can also use Cargo directly:

```bash
# Build all workspace components
cargo build --release

# Run all tests
cargo test

# Run a specific binary
cargo run --bin elo-test
```

## Migration Status

- ✅ **elo**: Complete - ELO rating calculations with unit tests
- ✅ **fen**: Complete - FEN string parsing, board representation, and chess types
- ✅ **hash**: Complete - Zobrist hashing for chess positions with incremental updates
- ✅ **square_set**: Complete - Bitboard operations and square set manipulation
- ⏳ **magic**: Planned - Magic bitboard generation for sliding piece attacks
- ⏳ **moves**: Next priority - Move generation and validation
- ⏳ **perft**: Planned - Performance testing for comprehensive move generation validation
- ⏳ **eval**: Planned - Position evaluation
- ⏳ **nnue**: Planned - Neural network evaluation
- ⏳ **search**: Planned - Alpha-beta search algorithm
- ⏳ **uci**: Planned - UCI protocol implementation

## Code Style

The Rust code follows these principles carried over from the C++ codebase:

- **Early returns**: Use `?` operator and guard clauses instead of deep nesting
- **Small functions**: Keep functions focused on a single responsibility
- **Minimal nesting**: Use pattern matching and early returns
- **Documentation**: Use `///` for public APIs, avoid over-commenting trivial code

Additional Rust-specific guidelines:

- Use `snake_case` for functions and variables
- Use `PascalCase` for types and traits
- Prefer `match` over nested `if` statements
- Leverage the type system for compile-time guarantees
- Use `Result<T, E>` and `Option<T>` for error handling

## Testing

Each component includes:

1. **Unit tests**: Using Rust's built-in `#[test]` framework
2. **Integration tests**: Matching C++ behavior exactly
3. **Property-based tests**: Where applicable (future work)

Tests can be run with `cargo test` or through the Makefile with `make rust-test`.

## Performance

The Rust implementations aim to match or exceed the performance of the C++ versions while providing memory safety guarantees. Performance comparisons will be documented as components are migrated.

## Dependencies

The workspace uses minimal external dependencies to maintain build simplicity:

- Standard library only for core components
- `criterion` for benchmarking (development dependency)
- `clap` for command-line parsing where needed
- `anyhow` for error handling convenience

See `Cargo.toml` in the root directory for the complete dependency specification.
