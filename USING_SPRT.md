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

Pass extra arguments through `SPRT_ARGS`. `--games N` is a budget of N games in total: the
script runs `N/2` fast-chess rounds of 2 games, so each opening is played once with each
colour. (Passing the budget to fast-chess as `-games` would play `N` games *per round*, i.e.
twice as many as asked.)

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

### Compare book-on play instead of search (sprt-self)

`sprt-self` defaults to `OwnBook=false` on both sides via `SPRT_SELF_ARGS`, since it starts every
game from a book-exit position (see below) and the point is to measure search strength, not the
book. To measure with the book playing on top of those positions instead, override it:

```bash
make sprt-self SPRT_BASE=build/gbchess-prev SPRT_SELF_ARGS=
```

## Openings behavior

By default the script tries to use `build/book-openings.epd`: a diverse, roughly balanced set of
positions at `book.csv`'s frontier (where the book runs out of well-supported continuations),
generated with `build/book-gen --sprt-suite book.csv build/book-openings.epd` — run explicitly via
`make book-openings`. `make sprt-self` builds it automatically as a prerequisite.

- If it does not exist and `book-gen`/`book.csv` are available, the script auto-generates it.
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

By default both sides are the *same* binary (`build/gbchess`) with `OwnBook=true` against
`OwnBook=false`, so the run measures the book and not the code. The engines appear as
`gbchess-OwnBook-true` and `gbchess-OwnBook-false`.

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
make sprt-openings SPRT_ARGS='--tc 10+0.1 --games 20000'
make sprt-openings SPRT_OPENINGS_REPORT_ARGS='--plies 10 --min-games 50 --by-engine'

# A book that works should be worth far more than 5 Elo, so wider bounds stop sooner.
make sprt-openings SPRT_ARGS='--elo0 0 --elo1 20'

# Compare two binaries from the start position instead of book vs no book.
make sprt-openings SPRT_OPENINGS_BASE=build/gbchess-base SPRT_OPENINGS_ARGS=
```

### Comparing two book files

`make sprt-book` A/Bs two book *files* on the same binary, so it measures book quality
rather than search code — needed to answer "is book B better than book A?", which
`sprt-openings`'s on/off comparison can't. `BookFile` is chosen via the `--book` CLI flag
(see `src/engine/engine.md`), not a UCI option, so it can't change mid-process; the
engines run as separate processes here just like `sprt-openings`' book on/off pair.

```bash
cp book.csv book-candidate.csv   # or generate a candidate some other way
make sprt-book SPRT_BOOK_B=book-candidate.csv
```

Engines are named after the book file basenames, e.g. `gbchess-book-book` vs
`gbchess-book-book-candidate`. Knobs: `SPRT_BOOK_A`/`SPRT_BOOK_B` (default `book.csv` /
`book-candidate.csv`), plus the usual `SPRT_ARGS`.

### Tuning book selection temperature

`BookTemperature` (a `setoption`-able UCI spin, unlike `BookFile`) controls how greedily
the book picks among scored moves; both sides can keep the *same* book file and differ
only in temperature:

```bash
make sprt-openings SPRT_OPENINGS_ARGS='--new-option BookTemperature=100 --base-option BookTemperature=140'
```

Rerun the report on an existing PGN at any time:

```bash
test/opening_summary.py build/sprt-openings-20260725-163721.pgn
```

## Reading the result

`test/sprt_summary.py` opens with a one-paragraph verdict, so a run can be judged without
interpreting the SPRT numbers by hand. It works on a partial PGN too — run it while the match
is still going:

```bash
test/sprt_summary.py build/sprt-20260725-220343.pgn
```

```
**Inconclusive so far** — +2.8 Elo (95% confidence interval -1.3 to +6.9), 91% likely
positive. After 3,532 games the test is 26% of the way to a decision (testing the null
hypothesis of +0 against +5 normalized Elo, i.e. is the gain at least +1.8 Elo); at the
current rate roughly 10,137 more games would accept it, about 3.0 hours of play at the
observed pace.
```

The verdict is one of: accepted, rejected, probably a gain / regression but unproven, likely no
meaningful change, or inconclusive — each with the Elo estimate, its 95% interval, and for an
unfinished run an extrapolation of how many more games a decision needs and how long that is in
wall-clock time. The pace comes from the `GameStartTime`/`GameEndTime` tags, so it already
reflects the `--concurrency` and machine load the run actually had.

The statistics come from the pentanomial pair counts (the same opening played with both
colours), which is what makes the interval honest, and reproduce fast-chess's own `Elo`, `nElo`,
`LOS` and `LLR` exactly. Note that fast-chess reads `elo0`/`elo1` as **normalized** Elo, so the
default `--elo1 5` asks "is this worth at least ~2 ordinary Elo", not 5; the verdict spells out
the logistic equivalent. Bounds are taken from the sidecar `test/sprt.sh` writes, or from
`--elo0/--elo1/--alpha/--beta` for a PGN produced elsewhere.

## Time forfeits

`test/sprt_summary.py` reports causes split both by engine and by first mover. The second table
is the one to read for time forfeits: an engine that budgets a fraction of its own remaining
clock with nothing held back spends before its opponent in every cycle, so the side that moves
first reaches the flag threshold first. Before `MoveOverhead` existed, a run from the start
position put *every* forfeit on white, which looks like a colour bug and is not one. A skew like
that now means the reserve is too small for the conditions.

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
