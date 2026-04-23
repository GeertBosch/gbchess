#!/bin/bash
usage() {
    echo -e "usage:  $0 [-m[X][-[Y]]] <engine> pgnfile [goopts]\n" >&2
    echo -e "  -mX-Y: limit analysis to moves X through Y (inclusive)" >&2
    echo -e "  -n: start a new game before analyzing each move" >&2

    echo -e "goopts: additional options to pass to the engine, such as:" >&2
    echo -e "        depth <n>: set the search depth to <n>" >&2
    echo -e "        movetime <n>: set the maximum time to search to <n> milliseconds per move" >&2
    exit 1
}

# Parse options
start_move=1
end_move=""
new_game=""
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
            new_game="ucinewgame\n"
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

{
    # Initial evaluation: position before first analyzed move, provides bestmove/ponder for move 1
    # Unconditional start a new game
    echo -e "ucinewgame\nposition $init_position\ngo $extraopt"

    position="startpos moves"
    ply=0
    for move in $moves ; do
        position="$position $move"
        ply=$(( ply + 1 ))
        num=$(( (ply + 1) / 2 ))
        if [ $num -lt $start_move ]; then
            continue
        fi
        if [ -n "$end_move" ] && [ $num -gt $end_move ]; then
            continue
        fi
        echo -e "${new_game}position $position\ngo $extraopt"
    done
} | (
    if [ ! -x $(which "$engine") ] ; then
        echo "cannot find engine: $engine" >&2
        exit 2
    fi
    $engine 2>/dev/null) |
# The output here is the raw UCI output from the engine, which we will post-process to extract
# the engines score for each move in the PGN, and output it in a format like:
#    e2e4 cp 20
#    e7e5 cp 15
# where the first column is the move and the second one the score from white's perspective.
awk -v "moves=$moves" -v "offset=$offset" '
# moves contains the list of UCI moves like "g1f3 d7d6 b1c3 "...
BEGIN {
    split(moves, move)
    # movenr=0 for the initial pre-start evaluation; incremented to 1 before processing move 1
    movenr = 0
    init_done = 0
}
/depth .*score / {
    sub(/^.+ score /, "")
    scoreType=$1
    rawScore=$2 + 0
    scoreValue=((movenr + offset) % 2 ? -rawScore : rawScore)
    if (scoreType == "mate") {
        scoreLabel="mate " scoreValue
        scoreNumeric=(scoreValue > 0 ? 10000 - scoreValue : -10000 - scoreValue)
    } else {
        scoreLabel="cp " scoreValue
        scoreNumeric=scoreValue
    }
}
/^bestmove / {
    if (!init_done) {
        # Store the initial evaluation result; bestmove/ponder will appear on move 1 line
        prev_bm_line = $0
        lastscore = scoreNumeric
        init_done = 1
        ++movenr
    } else {
        color=((movenr + offset) % 2 ? "w" : "b")
        diff = scoreNumeric - lastscore
        print int((movenr + offset + 1) / 2) " " color " " move[movenr + offset] " " scoreLabel " diff " diff " " prev_bm_line
        lastscore = scoreNumeric
        prev_bm_line = $0
        ++movenr
    }
}
'
