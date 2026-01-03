#!/bin/bash
usage() {
    echo "usage: $0 [stockfish|gbchess] pgnfile depth" >&2
    exit 1
}
if [ $# -lt 3 ] ; then
    echo "$#"
    usage
fi
engine=$1
shift
position=startpos

pgnfile=$1
shift
moves=$(build/pgn-test -v "$pgnfile" | awk '/^Moves: / { sub(/^Moves: /, ""); print $0 }')

depth=$1
shift

for move in $moves ; do
    position="$position $move"
    echo -e "position $position\ngo depth $depth\n"
done | (
    if [ "$engine" = "stockfish" ] ; then
#        Stockfish/src/stockfish
         /usr/local/bin/stockfish
    elif [ "$engine" = "gbchess" ] ; then
        build/engine
    elif [ "$engine" = "gnu" ] ; then
        gnuchess -u
    else
        echo "unknown engine: $engine" >&2
        exit 2
    fi ) | tee engine.out |  awk -v "moves=$moves" '
BEGIN { split(moves, move); movenr = 1 }
/depth '$depth'.*score / { sub(/^.+ score /, ""); score=$1 " " (movenr % 2 ? -$2 : $2) }
/^bestmove / { print move[movenr++] " " score }
'
