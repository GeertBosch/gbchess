#!/bin/bash
usage() {
    echo "usage: $0 [-m[X][-[Y]]] [--game N] <w|b|white|black> pgnfile [goopts...]\n" >&2
    echo "  -mX-Y: limit replay to moves X through Y (inclusive)" >&2
    echo "  --game N: replay the Nth game (1-based) in a PGN file with multiple games" >&2
    echo "            (default: 1)" >&2
    echo "goopts: additional options to pass to the engine, such as:" >&2
    echo "        depth <n>: set the search depth to <n>" >&2
    echo "        movetime <n>: set the maximum time to search to <n> ms per move" >&2
    echo
    echo "Generate UCI commands to replay a PGN file for testing. This is especially useful for"
    echo "scenarios where caches affect the engine's behavior, such as when an aborted search leads"
    echo "to storing an incorrect evaluation in the transposition table. A final save command"
    echo "will cause the gbchess engine to generate a dump of its caches, allowing the engine to"
    echo "resume from that state through a quick restore instead of a complete replay."

    exit 1
}

# Parse options
start_move=""
end_move=""
game=1

# getopts doesn't handle long options, so pull --game out of the argument list first.
args=()
while [ $# -gt 0 ]; do
    case "$1" in
        --game)
            game=$2
            shift 2
            ;;
        --game=*)
            game=${1#--game=}
            shift
            ;;
        *)
            args+=("$1")
            shift
            ;;
    esac
done
set -- "${args[@]}"

if ! [[ $game =~ ^[0-9]+$ ]] || [ "$game" -lt 1 ]; then
    echo "Error: --game must be a positive integer" >&2
    usage
fi

while getopts "m:" opt; do
    case $opt in
        m)
            if [[ $OPTARG =~ ^([0-9]*)-([0-9]*)$ ]]; then
                if [ -n "${BASH_REMATCH[1]}" ]; then
                    start_move=${BASH_REMATCH[1]}
                fi
                if [ -n "${BASH_REMATCH[2]}" ]; then
                    end_move=${BASH_REMATCH[2]}
                fi
                if [ -n "$start_move" ] && [ -n "$end_move" ] && [ $start_move -gt $end_move ]; then
                    echo "Error: start move must be <= end move" >&2
                    usage
                fi
            else
                echo "Error: -m option must be in format [X]-[Y]" >&2
                usage
            fi
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

# Set 'side' to 1 for white or 2 for black
case "$1" in
    w|white) side=1 ;;
    b|black) side=2 ;;
    *) usage ;;
esac
shift

pgnfile=$1
shift

goopt=$*
shift

if [ ! -f "$pgnfile" ] ; then
    echo "PGN file not found: $pgnfile" >&2
    exit 2
fi

# Extract just the requested game (1-based) from a possibly multi-game PGN file: each game
# starts with an [Event tag, so lines belong to game N once the Nth such tag has been seen
# and until the next one appears.
gametext=$(awk -v g="$game" '/^\[Event / { n++ } n == g { print }' "$pgnfile")
if [ -z "$gametext" ] ; then
    echo "Error: game $game not found in $pgnfile" >&2
    exit 2
fi

# Include the original PGN as comments, omitting empty and zero tags
echo "$gametext" | egrep -v '"[0]?"' | while read line ; do
    echo "# $line"
done
moves=$(echo "$gametext" | build/pgn-test -v - | awk '/^Moves: / { sub(/^Moves: /, ""); print $0 }')

# A [FEN] tag sets up the starting position; without one the game starts from startpos. The FEN
# also gives the side to move (black may move first) and the fullmove number to count moves from.
fen=$(echo "$gametext" | sed -n 's/^\[FEN "\(.*\)"\].*/\1/p' | head -1)
first_black=0
first_num=1
if [ -n "$fen" ] ; then
    setup="fen $fen"
    [ "$(echo "$fen" | cut -d' ' -f2)" = "b" ] && first_black=1
    fen_num=$(echo "$fen" | cut -d' ' -f6)
    if [[ $fen_num =~ ^[0-9]+$ ]] && [ "$fen_num" -gt 0 ] ; then
        first_num=$fen_num
    fi
else
    setup="startpos"
fi

# Now output UCI commands for recreating the positions needing to be played by the side given:
# For white:
# position startpos moves e2e4
# position startpos moves e2e4 e7e5 g1f3
# etc
(
echo "ucinewgame"
n=$(echo $moves | wc -w)

# Ply index i counts the moves of the game as listed in the PGN, so i=0 is the first move played
# from the setup position, which is black's move when first_black is 1. The move number of ply i
# is first_num + (i + first_black) / 2, and ply i is white's move iff (i + first_black) is even.
if [ -z "$start_move" ]; then
    start_move=$first_num
fi
if [ -z "$end_move" ]; then
    end_move=$((first_num + (n - 1 + first_black) / 2))
fi

# For white, i is the ply index of the first white move; for black that of the first black move.
for i in $(seq $(( ((side - 1) - first_black + 2) % 2 )) 2 $((n - 1))) ; do
    num=$((first_num + (i + first_black) / 2))

    # Skip if outside requested range
    if [ $num -lt $start_move ]; then
        continue
    fi
    if [ $num -gt $end_move ]; then
        continue
    fi

    # Show move number and UCI moves for white and black. When black moves first, the first move
    # number has no white move, so only black's move is shown.
    white_ply=$((2 * (num - first_num) - first_black))
    black_ply=$((white_ply + 1))
    if [ $white_ply -lt 0 ] ; then
        move_uci=$(echo $moves | cut -d' ' -f$((black_ply + 1)))
    else
        move_uci=$(echo $moves | cut -d' ' -f$((white_ply + 1))-$((black_ply + 1)))
    fi
    echo "# $num. $move_uci"

    if [ $i -eq 0 ] ; then
        echo "position $setup"
    else
        echo "position $setup moves $(echo $moves | cut -d' ' -f1-$i)"
    fi
    # During the last iteration, save the search state
    if [ $num -eq $end_move ] ; then
        echo "save"
    fi
    echo "go $goopt"
done)
