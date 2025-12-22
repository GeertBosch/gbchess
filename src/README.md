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
    book([**book**
    Opening Book
    ])

    engine --> search
    book --> move
    engine --> book
    search --> move
    engine ~~~ core
    search --> eval
    move --> core
    eval --> core

    %% Styling
    style engine fill:#fda,stroke:#333,stroke-width:2px
    style book fill:#ffc,stroke:#333,stroke-width:2px
    style move fill:#cec,stroke:#333,stroke-width:2px
    style search fill:#eba,stroke:#333,stroke-width:2px
    style eval fill:#edf,stroke:#333,stroke-width:2px
    style core fill:#cef,stroke:#333,stroke-width:2px
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
    Core[core.h]
        CastlingInfo[castling_info.h]
        PieceSet[piece_set.h]
        Options[options.h]
        Uint128[uint128_str.h]
        SquareSet[square_set.h]
        Hash[hash.h]

    %% UCI Engine
    Engine[engine.cpp]
        FEN[fen.h]
        PerftCore[perft_core.h]
        Book[book.h]
        PGN[pgn.h]

    %% Eval module - position evaluation
    Eval[eval.h]
        NNUE[nnue.h]

    %% Move module - Move generation
    Move[move.h]
        %% Move Magic module - Sliding move generation
        MoveMagic[magic.h]
        Occupancy[occupancy.h]


    %% Search module
    PV[pv.h]
    Search[search.h]

    Time[time.h]

    %% The following are include dependencies between the above modules.
    %% The order is chosen to be able to render a planar graph, without crossing edges.
    %% Transitive dependencies are generally omitted for the graph.
    Engine --> Time
    Time ----> Core

    Book --> Move
    Engine --> Book
    Engine --> FEN
    Engine --> Search
    Engine --> PerftCore
    FEN -----> Core
    Search --> Move
    PerftCore -----> Core
    Engine ----> Uint128
    Uint128 ---> Core
    Engine --> Options
    Options ~~~~ Core
    PGN ----> Core
    Move --> Occupancy
    Book --> PGN
    Occupancy --> SquareSet
    Move --> MoveMagic
    SquareSet --> Core
    Search --> NNUE
    Search --> Hash
    Search --> PV
    PV ---> Eval
    NNUE --> Eval
    Move --> CastlingInfo
    Move ---> PieceSet
    MoveMagic --> SquareSet
    CastlingInfo --> Core
    PieceSet ---> Core
    Hash --> Core
    Eval ---> Core

    %% Styling
    classDef engine fill:#fda,stroke:#333,stroke-width:2px,font-family:monospace
    classDef book fill:#ffc,stroke:#333,stroke-width:2px,font-family:monospace
    classDef move fill:#cec,stroke:#333,stroke-width:2px,font-family:monospace
    classDef search fill:#eba,stroke:#333,stroke-width:2px,font-family:monospace
    classDef eval fill:#edf,stroke:#333,stroke-width:2px,font-family:monospace
    classDef core fill:#cef,stroke:#333,stroke-width:2px,font-family:monospace

    class FEN,Engine,PerftCore,Time engine
    class Book,PGN book
    class MoveMagic,Occupancy,Move move
    class Search,PV search
    class Eval,NNUE eval
    class Core,PieceSet,CastlingInfo,SquareSet,Hash,Options,Uint128 core
```
