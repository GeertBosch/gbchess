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
make -j build/gbchess
```

## Quick start

### 1) gbchess vs previous gbchess build

```bash
make sprt-self SPRT_BASE=build/gbchess-prev
```

This runs:

- new engine: `build/gbchess` (or `SPRT_NEW=...`)
- base engine: `build/gbchess-prev`

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
make sprt-self SPRT_BASE=build/gbchess-prev \
  SPRT_ARGS='--tc 5+0.05 --games 4000 --elo0 0 --elo1 5'
```

### More conservative confirmation run

```bash
make sprt-self SPRT_BASE=build/gbchess-prev \
  SPRT_ARGS='--tc 10+0.1 --games 40000 --elo0 0 --elo1 3 --alpha 0.05 --beta 0.05'
```

### Explicit gbchess options (optional)

```bash
make sprt-self SPRT_BASE=build/gbchess-prev \
  SPRT_ARGS='--new-option OwnBook=false --base-option OwnBook=false'
```

## Openings behavior

By default the script tries to use `build/sprt-openings.epd`.

- If it does not exist and `lichess/fixed_puzzles.csv` exists, the script auto-generates `build/sprt-openings.epd` from puzzle FENs.
- If no opening file can be found/generated, it runs from start position only.

Override openings explicitly:

```bash
test/sprt.sh --base-cmd build/gbchess-prev --openings-file your.epd --openings-format epd
```

Disable openings:

```bash
test/sprt.sh --base-cmd build/gbchess-prev --no-openings
```

## Openings-focused runs

`make sprt-openings` is a variant aimed at improving the opening book rather than at
measuring overall Elo. Every game starts from the initial position (`--no-openings`),
so the engines' own book — not a puzzle FEN — picks the opening.

```bash
make generate-book          # required: without book.csv every game is identical
make sprt-openings
```

It writes `build/sprt-openings-<timestamp>.pgn` plus a matching `.md` report produced by
`test/opening_summary.py`, which shows:

- how many book moves each engine got before its first real search
- per named opening (fast-chess supplies `ECO` / `Opening` tags), worst-scoring first
- White's score and median engine eval at book exit, bucketed by exit ply
- an opening tree (score % and exit eval per node) and the worst/best full lines

Score percentages are from White's point of view, since an opening line is a property of
the position rather than of an engine. Pass `--by-engine` to also split by engine.

Knobs:

```bash
make sprt-openings SPRT_OPENINGS_BASE=build/gbchess-prev
make sprt-openings SPRT_ARGS='--tc 10+0.1 --games 20000'
make sprt-openings SPRT_OPENINGS_REPORT_ARGS='--plies 10 --min-games 50 --by-engine'

# Measure the book itself: same binary both sides, book only on one.
make sprt-openings SPRT_OPENINGS_BASE=build/gbchess \
  SPRT_OPENINGS_ARGS='--base-option OwnBook=false'
```

Rerun the report on an existing PGN at any time:

```bash
test/opening_summary.py build/sprt-openings-20260725-163721.pgn
```

## Time forfeits

`test/sprt_summary.py` reports causes split both by engine and by first mover. Time forfeits
belong in the second table: the per-move budget is a fraction of the engine's own remaining
clock, so the side that moves first spends before its opponent in every cycle and reaches the
flag threshold first. A run from the start position therefore puts every forfeit on white,
which looks like a colour bug and is not one.

`MoveOverhead` (default 10ms) reserves the per-move cost the clock is charged but the search
does not measure. Raise it if forfeits show up in a real-time run — under high `--concurrency`
the arbiter charges scheduling delays that the engine never sees. With `nodestime` enabled the
virtual clock cannot flag on its own, but fast-chess still enforces the real clock, so a
nodestime run needs a real time control with enough headroom that the virtual clock is what
binds; otherwise the run loses the reproducibility it was set up for.

## Engine names

By default the engines are named after what they actually are: the binary's basename plus
a tag per UCI option. `build/gbchess` with `--base-option OwnBook=false` is reported as
`gbchess-OwnBook-false`, not as "base". Only when both sides end up identical does the
script fall back to `-new`/`-base` suffixes (and it says so, since that is an A/A test).
Override with `--new-name` / `--base-name`.

Because the names no longer say which side is "new", the script writes a sidecar
`build/<run>.engines` holding `new=` and `base=`; `test/sprt_summary.py` reads it so its
"from the new engine's perspective" numbers stay correct. Neither PGN order nor the
`Round` tag can be used for this — fast-chess writes games as they finish.

## Output

The script writes PGN output to:

- `build/sprt-YYYYmmdd-HHMMSS.pgn`

Override with:

```bash
test/sprt.sh --base-cmd build/gbchess-prev --pgnout build/my-sprt-run.pgn
```

## Full script help

```bash
test/sprt.sh --help
```
