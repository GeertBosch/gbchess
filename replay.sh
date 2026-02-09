#!/bin/bash
# Generate UCI commands to replay a PGN file for testing. This is especially useful for scenarios
# where caches affect the engine's behavior, such as when an aborted search leads to storing an
# incorrect evaluation in the transposition table.
usage() {
    echo "usage: $0 [-m[X][-[Y]]] [white|black] pgnfile depth" >&2
    echo "  -mX-Y: limit replay to moves X through Y (inclusive)" >&2
    exit 1
}

# Parse options
start_move=1
end_move=""
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
                if [ -n "$end_move" ] && [ $start_move -gt $end_move ]; then
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

if [ $# -lt 3 ] ; then
    usage
fi

# Set 'side' to 1 for white or 2 for black
case "$1" in
    white) side=1 ;;
    black) side=2 ;;
    *) usage ;;
esac
shift

pgnfile=$1
shift

depth=$1
shift

# Include the origin PGN as comments, omitting empty and zero tags
egrep -v '"[0]?"' "$pgnfile" | while read line ; do
    echo "# $line"
done
moves=$(build/pgn-test -v "$pgnfile" | awk '/^Moves: / { sub(/^Moves: /, ""); print $0 }')

# Now output UCI commands for recreating the positions needing to be played by the side given:
# For white:
# position startpos moves e2e4
# position startpos moves e2e4 e7e5 g1f3
# etc
position=startpos
(
echo "ucinewgame"
# For white, j is 0,2,4,... ; for black, j is 1,3,5,...
n=$(echo $moves | wc -w)
for j in $(seq $((side - 1)) 2 $n) ; do
    # Show move number and UCI moves for white and black
    num=$((j / 2 + 1))

    # Skip if outside requested range
    if [ $num -lt $start_move ]; then
        continue
    fi
    if [ -n "$end_move" ] && [ $num -gt $end_move ]; then
        continue
    fi

    move_uci=$(echo $moves | cut -d' ' -f$((num * 2 - 1))-$((num * 2)))
    echo "# $num. $move_uci"

    if [ $j -eq 0 ] ; then
        position="startpos"
    else
        position="startpos moves $(echo $moves | cut -d' ' -f1-$j)"
    fi
    echo "position $position"
    echo "d"
    # During the last iteration, save the search state
    if [ $num -eq $end_move ] ; then
        echo "save"
    fi
    echo "go depth $depth"
done)
