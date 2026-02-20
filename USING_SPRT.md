# SPRT testing with fast-chess

This project includes a helper script for Sequential Probability Ratio Test (SPRT) runs:

- `test/sprt.sh`

It supports:

- gbchess vs previous gbchess binaries
- gbchess vs a known baseline such as `stockfish-12`
- configurable time controls, SPRT boundaries, and per-engine UCI options

## Prerequisites

1. Install `fast-chess` (or make `fastchess` available in your `PATH`).
2. Build the engine:

```bash
make -j build/engine
```

## Quick start

### 1) gbchess vs previous gbchess build

```bash
make sprt-self SPRT_BASE=build/engine-prev
```

This runs:

- new engine: `build/engine` (or `SPRT_NEW=...`)
- base engine: `build/engine-prev`

### 2) gbchess vs stockfish-12

```bash
make sprt-sf12 SPRT_STOCKFISH12=stockfish-12
```

If `stockfish-12` is not on `PATH`, pass an absolute path instead.

No UCI options are required for gbchess. Engine options are optional and only needed if the
engine actually supports them.

## Common tuning examples

Pass extra arguments through `SPRT_ARGS`.

### Faster smoke SPRT

```bash
make sprt-self SPRT_BASE=build/engine-prev \
  SPRT_ARGS='--tc 5+0.05 --games 4000 --elo0 0 --elo1 5'
```

### More conservative confirmation run

```bash
make sprt-self SPRT_BASE=build/engine-prev \
  SPRT_ARGS='--tc 10+0.1 --games 40000 --elo0 0 --elo1 3 --alpha 0.05 --beta 0.05'
```

### Explicit gbchess options (optional)

```bash
make sprt-self SPRT_BASE=build/engine-prev \
  SPRT_ARGS='--new-option OwnBook=false --base-option OwnBook=false'
```

## Openings behavior

By default the script tries to use `build/sprt-openings.epd`.

- If it does not exist and `lichess/fixed_puzzles.csv` exists, the script auto-generates `build/sprt-openings.epd` from puzzle FENs.
- If no opening file can be found/generated, it runs from start position only.

Override openings explicitly:

```bash
test/sprt.sh --base-cmd build/engine-prev --openings-file your.epd --openings-format epd
```

Disable openings:

```bash
test/sprt.sh --base-cmd build/engine-prev --no-openings
```

## Output

The script writes PGN output to:

- `build/sprt-YYYYmmdd-HHMMSS.pgn`

Override with:

```bash
test/sprt.sh --base-cmd build/engine-prev --pgnout build/my-sprt-run.pgn
```

## Full script help

```bash
test/sprt.sh --help
```
