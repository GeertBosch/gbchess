#!/bin/bash
if [ $# -lt 3 ] ; then
    echo "usage: $0 [stockfish|gbchess] \"<fen>\" [moves <ucimove> ...] <depth>" >&2
    exit 1
fi
engine=$1
shift
if [ "$1" = "startpos" ] ; then
    position=startpos
else
    position="fen \"$1\""
fi
shift

moves=
if [ "$1" = "moves" ] ; then
    while [ $# -gt 1 ] ; do
        moves="$moves $1"
        shift
    done
    echo "$moves"
fi

echo -e "position $position$moves\ngo depth $1\nquit\n" |
while read line ; do
    echo "$line"
    sleep 0.1
done | (
if [ "$engine" = "stockfish" ] ; then
    Stockfish/src/stockfish
elif [ "$engine" = "gbchess" ] ; then
    build/engine
else
    echo "unknown engine: $engine" >&2
    exit 2
fi )
