# gbchess Engine

`gbchess` is a UCI-compatible chess engine. It reads commands from standard input
and writes responses to standard output, following the
[Universal Chess Interface (UCI) protocol](https://www.shredderchess.com/chess-features/uci-universal-chess-interface.html).

## Invocation

```
gbchess                          # Read UCI commands from stdin
gbchess <script.in> [...]        # Read UCI commands from one or more files
gbchess --cmd <cmd1> [<cmd2> ...]  # Execute commands given directly as arguments
```

A log of all UCI traffic is written to `$TMPDIR/engine-<pid>.log`.

---

## Standard UCI Commands

### `uci`
Identify the engine and list options.

**Response:**
```
id name gbchess
id author Geert Bosch
option name OwnBook type check default true
uciok
```

---

### `isready`
Check that the engine is ready to receive commands.

**Response:** `readyok`

---

### `ucinewgame`
Signal the start of a new game. Clears the transposition table, resets the
position to the starting position, resets the time control, and reseeds the
opening book's random move selector.

---

### `position (startpos | fen <fen>) [moves <move1> <move2> ...]`
Set the current position.

| Token | Description |
|---|---|
| `startpos` | Start from the standard initial position |
| `fen <fen>` | Start from the given FEN string (6 space-separated fields) |
| `moves <move1> ...` | Apply the listed moves (in UCI long-algebraic notation, e.g. `e2e4`) after the position |

**Examples:**
```
position startpos
position startpos moves e2e4 e7e5 g1f3
position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 moves d5e6
```

---

### `go [<option> ...]`
Start searching the current position. The engine runs the search in a background
thread and eventually responds with `bestmove <move> [ponder <move>]`.

Options can be combined freely. With no time-related options the search runs
until `stop` is received.

| Option | Description |
|---|---|
| `depth <n>` | Search to at most `<n>` half-moves (plies). Default: 15. |
| `movetime <ms>` | Use exactly `<ms>` milliseconds per move (fixed, ignores clock). |
| `wtime <ms>` | White's remaining time on the clock in milliseconds. |
| `btime <ms>` | Black's remaining time on the clock in milliseconds. |
| `winc <ms>` | White's increment per move in milliseconds. |
| `binc <ms>` | Black's increment per move in milliseconds. |
| `movestogo <n>` | Number of moves remaining until the next time control. |
| `nodes <n>` | Limit the search to `<n>` nodes (0 = no limit). |
| `perft <n>` | Run a perft (performance test / move-count) to depth `<n>` instead of searching. Responds with `Nodes searched: <count>`. |
| `wait` | Block until the search is complete before returning (non-standard; useful for scripting). |

**Time management:** When `wtime`/`btime` are given, the engine allocates time
using the formula `time / movesToGo + 80% × increment`. If `movestogo` is not
provided, the engine estimates moves remaining based on game length (assuming a
50-move game, with a minimum of 20 moves to go).

**Examples:**
```
go depth 10
go movetime 1000
go wtime 60000 btime 60000 winc 1000 binc 1000
go wtime 60000 btime 60000 movestogo 20
go nodes 100000
go depth 5 wait
go perft 5
```

---

### `stop`
Stop the search as soon as possible. The engine will respond with `bestmove`.

---

### `setoption name <name> value <value>`
Set an engine option.

| Option | Type | Default | Description |
|---|---|---|---|
| `OwnBook` | `check` | `true` | Use the built-in opening book (`book.csv`). When `true` the engine may reply immediately with a book move without searching. |

**Example:**
```
setoption name OwnBook value false
```

---

### `quit`
Exit the engine. Prints search statistics to stdout before exiting.

---

## Non-Standard / Debug Commands

These commands are not part of the UCI specification but are useful for
development and debugging.

### `d`
Display the current position as a FEN string, after applying all moves set by
`position`.

**Example:**
```
d
```
**Response** (example): `rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1`

---

### `debug [<move1> <move2> ...]`
Print detailed search-tree diagnostics to stderr for the current position after
optionally applying additional moves. The output format is controlled by the
`traceSearchTree` and `traceSearchMaxPly` compile-time options.

**Example:**
```
debug e2e4
```

---

### `lookup [<move1> <move2> ...]`
Look up the transposition-table entry for the current position after optionally
applying additional moves. Prints the entry (or `(none)`) to stderr.

**Example:**
```
lookup
lookup e2e4 e7e5
```

---

### `invalidate [<move1> <move2> ...]`
Evict the transposition-table entry for the current position after optionally
applying additional moves.

---

### `save [<filename>]`
Persist the full engine state (position, moves, options, and transposition table)
to a binary file. Defaults to `save-state.bin`.

**Response:** `info string saved engine state to <filename>`

---

### `restore [<filename>]`
Restore engine state from a previously saved file. Defaults to `save-state.bin`.
Useful for resuming a specific position in the debugger.

**Response:** `info string restored engine state from <filename>`

---

### `# <text>` / `expect <pattern>` / `expect-count <n> <pattern>`
Scripting helpers used in UCI test files (`.in` files under `test/`).
These are silently echoed to stderr and ignored by the engine's command
dispatch. They are interpreted by the test harness (`test/check-uci.sh`).

---

## `info` Output

During search the engine emits `info` lines in standard UCI format:

```
info depth <d> seldepth <sd> score cp <cp> nodes <n> nps <nps> time <ms> pv <move> ...
info depth <d> seldepth <sd> score mate <m> nodes <n> nps <nps> time <ms> pv <move> ...
```

Additional non-standard `info string` messages are produced for debugging:
- `info string max nodes <n> time <ms>` — node budget computed from clock time
- `info string time <ms> exceeded <limit>` — time limit reached; search stopping
- `info string nodes <n> exceeded <limit>` — node limit reached; search stopping
- `info string book reseeded with <seed>` — opening book reseeded on `ucinewgame`
- `info string saved/restored engine state to/from <filename>`

## Search Statistics

On `quit`, a detailed statistics block is printed to stdout summarising
transposition-table behaviour, null-move and aspiration-window attempts,
PVS/LMR reduction counts, beta-cutoff move ordering quality, and more.
