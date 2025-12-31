#!/bin/bash
if [ $# -lt 3 ] ; then
    echo "usage: $0 [stockfish|gbchess] \"<fen>\" <depth>" >&2
    exit 1
fi
engine=$1
shift

echo -e "position fen \"$1\"\ngo depth $2\nquit\n" |
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
