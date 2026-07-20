#!/bin/bash
set -euo pipefail

find_stockfish12() {
        if [[ -n "${STOCKFISH12:-}" ]]; then
                echo "${STOCKFISH12}"
                return 0
        fi
        if command -v stockfish-12 >/dev/null 2>&1; then
                echo "stockfish-12"
                return 0
        fi
        if command -v stockfish >/dev/null 2>&1; then
                echo "stockfish"
                return 0
        fi
        if [[ -x ./sf12 ]]; then
                echo "./sf12"
                return 0
        fi
        return 1
}

check_stockfish12() {
        local sf
        if ! sf="$(find_stockfish12)"; then
                echo "$0: Need Stockfish 12 (set STOCKFISH12=/path/to/stockfish-12, or install stockfish-12/stockfish on PATH)" >&2
                [[ -L ./sf12 && ! -x ./sf12 ]] && echo "$0: Note: ./sf12 exists but is not executable (broken symlink or missing binary)" >&2
                return 1
        fi

        local banner
        banner="$({ printf 'quit\n'; } | "$sf" 2>/dev/null | head -n 1 || true)"
        if [[ ! "$banner" =~ [Ss]tockfish[[:space:]._-]*12 ]]; then
                echo "$0: Need Stockfish 12, but '$sf' reports: ${banner:-<no banner>}" >&2
                echo "$0: Set STOCKFISH12 to a Stockfish 12 binary path" >&2
                return 1
        fi
        echo "$sf"
}

if [[ "${1:-}" == "--check" ]]; then
        check_stockfish12 >/dev/null
        exit 0
fi

STOCKFISH_CMD="$(check_stockfish12)"

phase=${1:-middlegame}

# Use end positions of from puzzles, as they should be relatively quiet for evaluation
echo "cp,fen"
awk -F, -v phase="$phase" '$0 ~ phase { print $2 " " $3; if (++count >= 1000) exit }' \
        lichess/lichess_db_puzzle.csv |
while read fen1 fen2 fen3 fen4 fen5 fen6 moves ; do 
        echo -e "uci\nposition fen \"$fen1 $fen2 $fen3 $fen4 $fen5 $fen6\" moves $moves\neval\nd"
done | 
"$STOCKFISH_CMD" |
awk '
/^NNUE evaluation/ { cp=$3 }
/^Fen/ && cp { $1=cp ; sub(/ /, ",");  print ; cp = "" } 
'
