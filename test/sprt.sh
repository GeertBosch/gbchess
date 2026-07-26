#!/bin/bash
set -euo pipefail

usage() {
    cat <<'EOF'
Run SPRT matches with fast-chess.

Usage:
  test/sprt.sh --base-cmd <cmd> [options]

Required:
  --base-cmd <cmd>           Baseline engine command, e.g. build/gbchess-base or stockfish

Options:
  --new-cmd <cmd>            New engine command (default: build/gbchess)
  --base-name <name>         Baseline engine name (default: derived from cmd + options)
  --new-name <name>          New engine name (default: derived from cmd + options)
  --base-option <K=V>        UCI option for baseline engine (repeatable)
  --new-option <K=V>         UCI option for new engine (repeatable)
  --base-args <ARGS>         Extra command-line arguments for baseline engine
  --new-args <ARGS>          Extra command-line arguments for new engine
  --tc <time>                Time control (default: 8+0.08)
  --concurrency <n>          Parallel games (default: CPU count)
  --games <n>                Max games in total, rounded up to a pair (default: 10000)
  --elo0 <n>                 SPRT H0 Elo (default: 0)
  --elo1 <n>                 SPRT H1 Elo (default: 5)
  --alpha <x>                SPRT alpha (default: 0.05)
  --beta <x>                 SPRT beta (default: 0.05)
  --openings-file <path>     Opening file path (EPD/PGN)
  --openings-format <fmt>    Opening format (default: epd)
  --openings-plies <n>       Opening plies (default: 8)
  --no-openings              Disable openings file
  --pgnout <path>            Write PGN to path (default: build/sprt-YYYYmmdd-HHMMSS.pgn)
  --fast-chess <cmd>         fast-chess executable (default: auto-detect)
  -h, --help                 Show this help

Examples:
  test/sprt.sh --base-cmd build/gbchess-base
    test/sprt.sh --base-cmd stockfish-12 --base-name stockfish-12
    test/sprt.sh --base-cmd build/gbchess-base --new-option OwnBook=false --base-option OwnBook=false
EOF
}

find_fast_chess() {
    if [ -n "${FAST_CHESS:-}" ]; then
        command -v "$FAST_CHESS" >/dev/null 2>&1 || {
            echo "FAST_CHESS is set but not executable: $FAST_CHESS" >&2
            exit 2
        }
        echo "$FAST_CHESS"
        return
    fi

    if command -v fast-chess >/dev/null 2>&1; then
        echo fast-chess
        return
    fi

    if command -v fastchess >/dev/null 2>&1; then
        echo fastchess
        return
    fi

    echo "fast-chess not found in PATH. Set FAST_CHESS or pass --fast-chess." >&2
    exit 2
}

cpu_count() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
        return
    fi

    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu
        return
    fi

    echo 4
}

# Say "start position" explicitly rather than letting fast-chess fall back to it: with no
# -openings block at all it warns about a missing book and an unknown opening format, which
# reads like an error in a run that wants the start position on purpose.
ensure_startpos_openings() {
    local file=$1
    mkdir -p "$(dirname "$file")"
    echo 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ;' > "$file"
}

# Default suite: positions at book.csv's frontier (see book-gen --sprt-suite), a diverse,
# roughly balanced set of start positions rather than the ten-position fixed-puzzles fixture.
ensure_default_openings() {
    local file=$1
    [ -f "$file" ] && return

    local book_gen=build/book-gen
    local book_csv=book.csv
    [ -x "$book_gen" ] && [ -f "$book_csv" ] || return

    mkdir -p "$(dirname "$file")"
    "$book_gen" --sprt-suite "$book_csv" "$file" >/dev/null
}

# Derive an engine name from what is actually being run: the binary's basename plus
# a tag per non-default UCI option. Keeps labels honest when both sides are the same
# binary with different options (e.g. gbchess vs gbchess-OwnBook-false).
derive_name() {
    local cmd=$1
    shift
    local name=${cmd##*/}
    name=${name%% *}
    local opt
    for opt in "$@"; do
        name+="-$(printf '%s' "$opt" | tr -c 'A-Za-z0-9' '-')"
        name=${name%-}
    done
    printf '%s' "$name"
}

require_cmd() {
    local value=$1
    local arg_name=$2
    if [ -n "$value" ]; then
        return
    fi

    echo "Missing required argument: $arg_name" >&2
    usage
    exit 2
}

BASE_CMD=
BASE_NAME=
BASE_ARGS=
NEW_CMD=build/gbchess
NEW_NAME=
NEW_ARGS=
TC=8+0.08
CONCURRENCY=$(($(cpu_count) - 2))
GAMES=10000
ELO0=0
ELO1=5
ALPHA=0.05
BETA=0.05
OPENINGS_FILE=build/book-openings.epd
OPENINGS_FORMAT=epd
OPENINGS_PLIES=8
USE_OPENINGS=1
PGNOUT=build/sprt-$(date +%Y%m%d-%H%M%S).pgn
FAST_CHESS_CMD=
NEW_OPTIONS=()
BASE_OPTIONS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --base-cmd) BASE_CMD=${2:-}; shift 2 ;;
        --base-name) BASE_NAME=${2:-}; shift 2 ;;
        --base-args) BASE_ARGS=${2:-}; shift 2 ;;
        --new-cmd) NEW_CMD=${2:-}; shift 2 ;;
        --new-name) NEW_NAME=${2:-}; shift 2 ;;
        --new-args) NEW_ARGS=${2:-}; shift 2 ;;
        --base-option) BASE_OPTIONS+=("${2:-}"); shift 2 ;;
        --new-option) NEW_OPTIONS+=("${2:-}"); shift 2 ;;
        --tc) TC=${2:-}; shift 2 ;;
        --concurrency) CONCURRENCY=${2:-}; shift 2 ;;
        --games) GAMES=${2:-}; shift 2 ;;
        --elo0) ELO0=${2:-}; shift 2 ;;
        --elo1) ELO1=${2:-}; shift 2 ;;
        --alpha) ALPHA=${2:-}; shift 2 ;;
        --beta) BETA=${2:-}; shift 2 ;;
        --openings-file) OPENINGS_FILE=${2:-}; shift 2 ;;
        --openings-format) OPENINGS_FORMAT=${2:-}; shift 2 ;;
        --openings-plies) OPENINGS_PLIES=${2:-}; shift 2 ;;
        --no-openings) USE_OPENINGS=0; shift ;;
        --pgnout) PGNOUT=${2:-}; shift 2 ;;
        --fast-chess) FAST_CHESS_CMD=${2:-}; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage; exit 2 ;;
    esac
done

require_cmd "$BASE_CMD" --base-cmd

if [ -n "$FAST_CHESS_CMD" ]; then
    FAST_CHESS=$FAST_CHESS_CMD
fi
FAST_CHESS_BIN=$(find_fast_chess)

for cmd in "$BASE_CMD" "$NEW_CMD"; do
    if [[ "$cmd" == *" "* ]]; then
        continue
    fi

    if command -v "$cmd" >/dev/null 2>&1; then
        continue
    fi

    if [ -x "$cmd" ]; then
        continue
    fi

    echo "Engine command not found or not executable: $cmd" >&2
    exit 2
done

mkdir -p build

# Name the engines after what they actually are, unless the caller said otherwise.
[ -n "$NEW_NAME" ] || NEW_NAME=$(derive_name "$NEW_CMD" "${NEW_OPTIONS[@]+"${NEW_OPTIONS[@]}"}")
[ -n "$BASE_NAME" ] || BASE_NAME=$(derive_name "$BASE_CMD" "${BASE_OPTIONS[@]+"${BASE_OPTIONS[@]}"}")

if [ "$NEW_NAME" = "$BASE_NAME" ]; then
    if [ "$NEW_CMD" = "$BASE_CMD" ] && \
       [ "${NEW_OPTIONS[*]+${NEW_OPTIONS[*]}}" = "${BASE_OPTIONS[*]+${BASE_OPTIONS[*]}}" ]; then
        echo "Note: both sides are the same engine with the same options (A/A test)." >&2
    fi
    # fast-chess needs distinct names; only now is a new/base suffix truthful.
    NEW_NAME+=-new
    BASE_NAME+=-base
fi

# Record which name is the new engine: neither PGN order nor the Round tag identifies
# it (fastchess writes games in completion order and picks colors per pairing), so the
# summary tools read this sidecar instead of guessing from the names.
# The SPRT bounds go in too, so the summary can report the same LLR fast-chess does.
{
    echo "new=$NEW_NAME"
    echo "base=$BASE_NAME"
    echo "elo0=$ELO0"
    echo "elo1=$ELO1"
    echo "alpha=$ALPHA"
    echo "beta=$BETA"
    echo "tc=$TC"
} > "${PGNOUT%.pgn}.engines"

NEW_ENGINE=(-engine "name=$NEW_NAME" "cmd=$NEW_CMD")
[ -n "$NEW_ARGS" ] && NEW_ENGINE+=("args=$NEW_ARGS")
for opt in "${NEW_OPTIONS[@]+"${NEW_OPTIONS[@]}"}"; do
    NEW_ENGINE+=("option.$opt")
done

BASE_ENGINE=(-engine "name=$BASE_NAME" "cmd=$BASE_CMD")
[ -n "$BASE_ARGS" ] && BASE_ENGINE+=("args=$BASE_ARGS")
for opt in "${BASE_OPTIONS[@]+"${BASE_OPTIONS[@]}"}"; do
    BASE_ENGINE+=("option.$opt")
done

# fast-chess counts -games per round and defaults to 2 rounds, so passing the game budget
# as -games plays twice as many games as asked. Ask for rounds of 2 instead: that is the
# pairing -repeat means, one round per opening with the colors swapped.
if [ "$GAMES" -lt 2 ]; then
    echo "--games must be at least 2 (one color-swapped pair)" >&2
    exit 2
fi
ROUNDS=$(((GAMES + 1) / 2))

CMD=(
    "$FAST_CHESS_BIN"
    -recover
    -concurrency "$CONCURRENCY"
    -rounds "$ROUNDS"
    -games 2
    -each "proto=uci" "tc=$TC" 
    -sprt "elo0=$ELO0" "elo1=$ELO1" "alpha=$ALPHA" "beta=$BETA"
    "${NEW_ENGINE[@]}"
    "${BASE_ENGINE[@]}"
    -pgnout "file=$PGNOUT" notation=san
)

if [ "$USE_OPENINGS" -eq 1 ]; then
    ensure_default_openings "$OPENINGS_FILE"
    if [ -f "$OPENINGS_FILE" ]; then
        CMD+=(
            -openings
            "file=$OPENINGS_FILE"
            "format=$OPENINGS_FORMAT"
            order=random
            "plies=$OPENINGS_PLIES"
        )
    else
        echo "No openings file found. Running from start position only." >&2
    fi
else
    STARTPOS_FILE=build/startpos.epd
    ensure_startpos_openings "$STARTPOS_FILE"
    CMD+=(-openings "file=$STARTPOS_FILE" format=epd order=random)
fi

echo "Running SPRT with:"
printf '  %q' "${CMD[@]}"
printf '\n\n'

exec "${CMD[@]}"
