#!/bin/bash
usage() {
    echo -e "usage:  $0 <engine> pgnfile [goopts]\n" >&2
    echo -e "goopts: additional options to pass to the engine, such as:" >&2
    echo -e "        depth <n>: set the search depth to <n>" >&2
    echo -e "        movetime <n>: set the maximum time to search to <n> milliseconds per move" >&2
    exit 1
}
if [ $# -lt 2 ] ; then
    echo "$#"
    usage
fi
engine=$1
shift

pgnfile=$1
shift

extraopt=$*

egrep  "Black |White |Result "  "$pgnfile"
moves=$(build/pgn-test -v "$pgnfile" | awk '/^Moves: / { sub(/^Moves: /, ""); print $0 }')

position="startpos moves"
for move in $moves ; do
    position="$position $move"
    echo -e "position $position\ngo $extraopt\n"
done | (
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
tee engine.out |
awk -v "moves=$moves" '
# moves contains the list of UCI moves like "g1f3 d7d6 b1c3 "...
BEGIN {
    split(moves, move)
    movenr = 1
    diff = 0
}
/depth .*score / {
    sub(/^.+ score /, "")
    scoreType=$1
    rawScore=$2 + 0
    scoreValue=(movenr % 2 ? -rawScore : rawScore)
    if (scoreType == "mate") {
        scoreLabel="mate " scoreValue
        scoreNumeric=(scoreValue > 0 ? 10000 - scoreValue : -10000 - scoreValue)
    } else {
        scoreLabel="cp " scoreValue
        scoreNumeric=scoreValue
    }
}
/^bestmove / {
    color=(movenr % 2 ? "w" : "b")
    diff = scoreNumeric - lastscore
    print int((movenr + 1) / 2) " "  color " " move[movenr] " " scoreLabel " diff " diff " " $0
    lastscore = scoreNumeric
    ++movenr
#    print "<" $0
}
'
