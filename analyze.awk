#!/bin/bash
awk '
/ nodes 0/ { lastnodes=0 }
/multipv/ { sub(/multipv [1-9] /, "") }
/^info depth [0-9]+ / {
    for (i=1; i < NF; ++i) {
        if ($i == "depth") depth = $(i+1)
        if ($i == "nodes") nodes = $(i+1)
        if ($i == "currmove") currmove = $(i+1)
    }
    delta = nodes - lastnodes
    if (delta) print "depth " depth " move " lastmove " nodes " delta
    lastnodes = nodes
    lastmove = currmove
}
' $*
