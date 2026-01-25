#!/bin/bash
# Generate UCI commands to replay a PGN file for testing. This is especially useful for scenarios
# where caches affect the engine's behavior, such as when an aborted search leads to storing an
# incorrect evaluation in the transposition table.
usage() {
    echo "usage: $0 [white|black] pgnfile depth" >&2
    exit 1
}
if [ $# -lt 3 ] ; then
    echo "$#"
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

egrep  "Black |White |Result "  "$pgnfile" | while read line ; do
    echo "# ${line/ /: }" | tr -d '[]"'
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
for j in $(seq $((side - 1)) 2 $(echo $moves | wc -w)) ; do
    # Show move number and UCI moves for white and black
    num=$((j / 2 + 1))
    move_uci=$(echo $moves | cut -d' ' -f$((num * 2 - 1))-$((num * 2)))
    echo "# $num. $move_uci"

    if [ $j -eq 0 ] ; then
        position="startpos"
    else
        position="startpos moves $(echo $moves | cut -d' ' -f1-$j)"
    fi
    echo "position $position"
    echo "d"
    echo "go depth $depth"
done)
