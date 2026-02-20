#!/bin/bash
set -euo pipefail

usage() {
    cat <<'EOF'
Run SPRT matches with fast-chess.

Usage:
  test/sprt.sh --base-cmd <cmd> [options]

Required:
  --base-cmd <cmd>           Baseline engine command, e.g. build/engine-prev or stockfish

Options:
  --new-cmd <cmd>            New engine command (default: build/engine)
  --base-name <name>         Baseline engine name (default: baseline)
  --new-name <name>          New engine name (default: gbchess-new)
  --base-option <K=V>        UCI option for baseline engine (repeatable)
  --new-option <K=V>         UCI option for new engine (repeatable)
  --tc <time>                Time control (default: 8+0.08)
  --concurrency <n>          Parallel games (default: CPU count)
  --games <n>                Max games (default: 10000)
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
  test/sprt.sh --base-cmd build/engine-prev
    test/sprt.sh --base-cmd stockfish-12 --base-name stockfish-12
    test/sprt.sh --base-cmd build/engine-prev --new-option OwnBook=false --base-option OwnBook=false
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

ensure_default_openings() {
    local file=$1
    [ -f "$file" ] && return

    local src=lichess/fixed_puzzles.csv
    [ -f "$src" ] || return

    mkdir -p "$(dirname "$file")"
    awk -F, 'NR>1 && NF>=2 { print $2 " ;" }' "$src" > "$file"
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
BASE_NAME=baseline
NEW_CMD=build/engine
NEW_NAME=gbchess-new
TC=8+0.08
CONCURRENCY=$(cpu_count)
GAMES=10000
ELO0=0
ELO1=5
ALPHA=0.05
BETA=0.05
OPENINGS_FILE=build/sprt-openings.epd
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
        --new-cmd) NEW_CMD=${2:-}; shift 2 ;;
        --new-name) NEW_NAME=${2:-}; shift 2 ;;
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

NEW_ENGINE=(-engine "name=$NEW_NAME" "cmd=$NEW_CMD")
for opt in "${NEW_OPTIONS[@]+"${NEW_OPTIONS[@]}"}"; do
    NEW_ENGINE+=("option.$opt")
done

BASE_ENGINE=(-engine "name=$BASE_NAME" "cmd=$BASE_CMD")
for opt in "${BASE_OPTIONS[@]+"${BASE_OPTIONS[@]}"}"; do
    BASE_ENGINE+=("option.$opt")
done

CMD=(
    "$FAST_CHESS_BIN"
    -repeat
    -recover
    -concurrency "$CONCURRENCY"
    -games "$GAMES"
    -each "proto=uci" "tc=$TC" "depth=10" "nodes=250000"
    -sprt "elo0=$ELO0" "elo1=$ELO1" "alpha=$ALPHA" "beta=$BETA"
    "${NEW_ENGINE[@]}"
    "${BASE_ENGINE[@]}"
    -pgnout "file=$PGNOUT" notation=uci
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
fi

echo "Running SPRT with:"
printf '  %q' "${CMD[@]}"
printf '\n\n'

exec "${CMD[@]}"
