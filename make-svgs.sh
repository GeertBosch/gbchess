#!/bin/bash
# Find all FEN piece placements in Markdown files in the repository and generate corresponding SVG
# chessboards for them. A FEN has 6 space-separated fields; we match just on the piece-placement
# pattern (8 ranks separated by /).

git grep -woE '[rnbqkpRNBQKP1-8]+(_[rnbqkpRNBQKP1-8]+){7}' |
while IFS=: read md svg ; do
    out="$(dirname $md)/$svg.svg"
    tmp=$(mktemp)
    ./fen2svg.sh -o "$tmp" "${svg//_//}"
    if ! cmp -s "$tmp" "$out"; then
        mv "$tmp" "$out" && echo " ✅ $out updated"
    else
        rm "$tmp"
    fi
done
