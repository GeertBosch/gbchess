#!/bin/bash
# Check the source file diagram to ensure it shows all dependencies
# It is OK for transitive dependencies to be omitted.

set -e

find src \( -name \*.cpp -o -name \*.h \) -exec grep -H '#include "' {} \; |
awk '
# When to process Mermaid lines
/```mermaid/ { mermaid = 1; next }
/```/ { mermaid = 0 }

# Match Mermaid lines like A[aheader.h]
mermaid && /^ +[A-Z0-9_]+[[][^]]+]$/ {
    sub(/]/, "")
    sub(/[[]/, " ")
    mod2file[$1]=$2
    file2mod[$2]=$1
}

#  Match Mermaid lines like A --> B
mermaid && match($0, /^ +[A-Z0-9_]+ [-]+-> [A-Z0-9_]+$/) {
    sub(/^ +/, "")
    if (!mod2file[$1]) {
        print "Skip dependency for unknown module " $1 " in: " $0
        next
    }
    if (!mod2file[$3]) {
        print "Skip dependency on unknown module " $3 " in: " $0
        ++errors
        next
    }
    if (deps[$1, $3]++) print "Duplicated dependency: " $0
#    print "deps[" $1 "," $3 "]"
}

function basename(path) {
    gsub("^\([a-zA-Z_]+/\)+", "", path)
    return path
}

# Close the dependency graph, so for all modules X, Y, Z we have deps[X, Y] && deps[Y, Z] => deps[X, Z]
function close_deps(x, to) {
    do {
        closed=1;
        for (y in mod2file)  {
            if (deps[x,y]) {
                for (z in mod2file)
                    if (deps[y,z] && !deps[x,z]) {
                        closed = deps[x,z]++
                    }
            }
        }
    } while (!closed)
    return deps[x, to]
}


# src/move/magic/magic.h:#include "core/core.h"
!mermaid && match($0, /:#include "/) {
    sub(/"$/, "")
    from=basename(substr($0, 0, RSTART - 1))
    to=basename(substr($0, RSTART + RLENGTH))
    from_mod=file2mod[from]
    if (!from_mod) next
    to_mod=file2mod[to]
    if (!to_mod) {
        print "Unknown module for " to " referenced from " from
        ++errors
        next
    }
    if (!deps[from_mod, to_mod]) {
        if (close_deps(from_mod, to_mod)) next
        print "Missing dependency: " from_mod " --> " to_mod " for " from " --> " to
        ++errors
        next
    }
}
END {
    if (errors) {
        print errors " errors"
        exit(1)
    }
}
' src/README.md -
echo "✅ src/README.md checked"
