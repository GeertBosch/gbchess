#!/bin/bash
usage() {
    echo -e "usage:  $0 [-m[X][-[Y]]] <engine> pgnfile [goopts]\n" >&2
    echo -e "  -mX-Y: limit analysis to moves X through Y (inclusive)" >&2
    echo -e "  -n: no new game reset between evaluations (faster, but less consistent)" >&2

    echo -e "goopts: additional options to pass to the engine, such as:" >&2
    echo -e "        depth <n>: set the search depth to <n>" >&2
    echo -e "        movetime <n>: set the maximum time to search to <n> milliseconds per move" >&2
    exit 1
}

# Parse options
start_move=1
end_move=""
new_game=1
while getopts "nm:" opt; do
    case $opt in
        m)
            if [[ $OPTARG =~ ^([0-9]*)-([0-9]*)$ ]]; then
                if [ -n "${BASH_REMATCH[1]}" ]; then
                    start_move=${BASH_REMATCH[1]}
                fi
                if [ -n "${BASH_REMATCH[2]}" ]; then
                    end_move=${BASH_REMATCH[2]}
                fi
                if [ -n "$end_move" ] && [ $start_move -gt $end_move ]; then
                    echo "Error: start move must be <= end move" >&2
                    usage
                fi
            else
                echo "Error: -m option must be in format [X]-[Y]" >&2
                usage
            fi
            ;;
        n)
            new_game=""
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ $# -lt 2 ] ; then
    usage
fi
engine=$1
shift

pgnfile=$1
shift

extraopt=$*

egrep  "Black |White |Result "  "$pgnfile"
moves=$(build/pgn-test -v "$pgnfile" | awk '/^Moves: / { sub(/^Moves: /, ""); print $0 }')

# offset = number of plies before the start move (used to align awk move index, color, move number)
offset=$(( 2 * (start_move - 1) ))

# Build the position before the first analyzed move (used for the initial evaluation)
init_position="startpos moves"
init_ply=0
for move in $moves ; do
    init_ply=$(( init_ply + 1 ))
    init_num=$(( (init_ply + 1) / 2 ))
    if [ $init_num -lt $start_move ]; then
        init_position="$init_position $move"
    else
        break
    fi
done
# If no moves were prepended, use plain startpos (avoids trailing "moves" with nothing after it)
[ "$init_position" = "startpos moves" ] && init_position="startpos"

# Validate engine
if [ ! -x "$(which "$engine")" ]; then
    echo "cannot find engine: $engine" >&2
    exit 2
fi

# Helper: append a UCI move to a position string
append_move() {
    local pos="$1" mov="$2"
    [ "$pos" = "startpos" ] && echo "startpos moves $mov" || echo "$pos $mov"
}

# Query the engine for a position; sets _score_label, _score_numeric, _bm_line.
# sign: 1 = keep raw score (white to move), -1 = negate (black to move)
query_pos() {
    local pos="$1" sign="$2"
    local line score_type raw_score score_value
    echo -e "position $pos\ngo $extraopt" >&3
    while IFS= read -r line <&4; do
        if [[ "$line" =~ score\ (cp|mate)\ (-?[0-9]+) ]]; then
            score_type="${BASH_REMATCH[1]}"
            raw_score="${BASH_REMATCH[2]}"
            score_value=$(( sign * raw_score ))
            if [ "$score_type" = "mate" ]; then
                _score_label="mate $score_value"
                _score_numeric=$(( score_value > 0 ? 10000 - score_value : -10000 - score_value ))
            else
                _score_label="cp $score_value"
                _score_numeric=$score_value
            fi
        fi
        if [[ "$line" =~ ^bestmove ]]; then
            _bm_line="$line"
            break
        fi
    done
}

# Start engine using named pipes for bidirectional communication (bash 3.2 compatible)
_fifo_in=$(mktemp -u /tmp/analyze_in.XXXXXX)
_fifo_out=$(mktemp -u /tmp/analyze_out.XXXXXX)
mkfifo "$_fifo_in" "$_fifo_out"
trap 'rm -f "$_fifo_in" "$_fifo_out"' EXIT
"$engine" < "$_fifo_in" > "$_fifo_out" &
_engine_pid=$!
exec 3>"$_fifo_in" 4<"$_fifo_out"

# Initial evaluation: position before first analyzed move; always from white's perspective (sign=1)
echo "ucinewgame" >&3
query_pos "$init_position" 1
prev_bm_line="$_bm_line"

# Analyze each in-range ply: query position after best move, then after played move.
# diff = eval(played) - eval(best), both in white's perspective.
position_before="$init_position"
ply=0
for move in $moves; do
    ply=$(( ply + 1 ))
    num=$(( (ply + 1) / 2 ))
    if [ $num -lt $start_move ]; then
        continue
    fi
    if [ -n "$end_move" ] && [ $num -gt $end_move ]; then
        break
    fi

    # sign: after white plays it is black to move, so engine score is from black's view -> negate
    sign=$(( (ply + offset) % 2 ? -1 : 1 ))
    (( (ply + offset) % 2 )) && color="w" || color="b"
    movenum=$num
    best_move=$(awk '{print $2}' <<< "$prev_bm_line")

    # Optional new game before B-query; also before M-query with -n, for independent evaluations
    [ -n "$new_game" ] && echo "ucinewgame" >&3

    # Query position after best move
    query_pos "$(append_move "$position_before" "$best_move")" "$sign"
    eval_B="$_score_numeric"

    # Query position after played move; with -n each query is independent (no TT sharing)
    [ -n "$new_game" ] && echo "ucinewgame" >&3
    query_pos "$(append_move "$position_before" "$move")" "$sign"
    eval_M="$_score_numeric"
    next_bm="$_bm_line"

    diff=$(( eval_M - eval_B ))
    # Express diff from the perspective of the player who moved (negative = blunder for that player)
    (( (ply + offset) % 2 == 0 )) && diff=$(( -diff ))
    echo "$movenum $color $move $_score_label diff $diff $prev_bm_line"
    prev_bm_line="$next_bm"
    position_before="$(append_move "$position_before" "$move")"
done
