#!/bin/bash
set -e 
(stockfish quit | grep Stockfish.12 >&2) || (echo "$0: Need Stockfish 12 in the path" >&2 && false)

phase=${1:-middlegame}

# Use end positions of from puzzles, as they should be relatively quiet for evaluation
echo "cp,fen"
egrep "$phase" lichess/lichess_db_puzzle.csv | 
head -1000 | 
cut -d, -f2,3 | 
tr ',' ' ' |
while read fen1 fen2 fen3 fen4 fen5 fen6 moves ; do 
        echo -e "uci\nposition fen \"$fen1 $fen2 $fen3 $fen4 $fen5 $fen6\" moves $moves\neval\nd"
done | 
stockfish |
awk '
/^NNUE evaluation/ { cp=$3 }
/^Fen/ && cp { $1=cp ; sub(/ /, ",");  print ; cp = "" } 
'
