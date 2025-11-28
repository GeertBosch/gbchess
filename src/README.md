# Modular Architecture

```mermaid
graph TD
    engine([**engine**
    Chess Engine
    ])
    move([**move**
    Move Generation
    ])
    eval([**eval**
    Position Evaluation
    ])
    search([**search**
    Search Algorithm
    ])
    core([**core**
    Foundational Types
    ])

    engine --> search
    engine --> move
    search --> move
    engine ~~~ core
    search --> eval
    move --> core
    eval --> core

    %% Styling
    style core fill:#e1f5fe,stroke:#333,stroke-width:2px
    style eval fill:#fff3e0,stroke:#333,stroke-width:2px
    style move fill:#e8f5e8,stroke:#333,stroke-width:2px
    style search fill:#e8eaf6,stroke:#333,stroke-width:2px
    style engine fill:#fce4ec,stroke:#333,stroke-width:2px
```

## File Organization

The file organization represents the module structure.
```
src/
├── core/              - Core chess types and utilities
│   ├── hash/              - Zobrist Hashing
│   └── square_set/        - Bitboard operations
├── engine/             - UCI Chess Engine
│   ├── fen/               - FEN position parsing
│   └── perft/             - Performance testing
├── eval/              - Position evaluation
│   └── nnue/              - Neural network evaluation
├── move/              - Move generation and representation
│   └── magic/             - Magic bitboard generation
└── search/            - Search algorithms
```

## Include Dependencies
The graph below shows dependencies of the public APIs of the chess engine. Transitive dependencies
are omited for clarity. The `check-arch.sh` script verifies that all actual dependencies are
included here, and is part of the `make build` target. Keep the dependency graph planar when making
changes.

```mermaid
graph TD

    %% Core module - foundational types
    C[core.h]
        CI[castling_info.h]
        PS[piece_set.h]
        O[options.h]
        CU[uint128_str.h]

    %% UCI Engine
    U[engine.cpp]

    %% Eval module - position evaluation
    E[eval.h]
        EN[nnue.h]

    %% FEN module - Forsyth-Edwards notation
    F[fen.h]

    %% Hash module - Zobrist hashing
    H[hash.h]

    %% Move module - Move generation
    M[move.h]
        %% Move Magic module - Sliding move generation
        MM[magic.h]
    MO[occupancy.h]

    %% Perft module - move generator testing
    PC[perft_core.h]

    %% Search module
    PV[pv.h]
    S[search.h]

    SS[square_set.h]

    T[time.h]

    %% The following are include dependencies between the above modules.
    %% The order is chosen to be able to render a planar graph, without crossing edges.
    %% Transitive dependencies are generally omitted for the graph.
    U --> T
    T ----> C
    U --> F
    U --> S
    U --> PC
    F -----> C
    S --> M
    U ----> CU
    CU ---> C
    PC -----> C
    M --> MO
    MO --> SS
    M --> MM
    SS --> C
    S --> EN
    S --> H
    S --> PV
    PV ---> E
    EN --> E
    M --> CI
    M ---> PS
    MM --> SS
    CI --> C
    PS ---> C
    H --> O
    O ~~~ C
    H --> C
    E ----> C

    %% Styling
    classDef core fill:#e1f5fe,stroke:#333,stroke-width:2px,font-family:monospace
    classDef move fill:#e8f5e8,stroke:#333,stroke-width:2px,font-family:monospace
    classDef eval fill:#fff3e0,stroke:#333,stroke-width:2px,font-family:monospace
    classDef engine fill:#fce4ec,stroke:#333,stroke-width:2px,font-family:monospace
    classDef search fill:#e8eaf6,stroke:#333,stroke-width:2px,font-family:monospace

    class C,PS,CI,SS,H,O,CU core
    class MM,MO,M move
    class E,EN eval
    class S,PV search
    class F,U,PC,T engine
```
