#!/bin/bash
# Check UCI test output files against expected patterns

set -e

failed=0

for infile in test/uci-*.in; do
    outfile=test/out/$(basename "$infile" .in).out

    if grep -qw "^expect" "$infile"; then
        # Collect missing patterns
        missing=$(mktemp)
        grep -w "^expect" "$infile" | \
        while read expect pattern ; do
            if ! grep -qe "$pattern" "$outfile"; then
                echo "$pattern"
            fi
        done > "$missing"

        if [ -s "$missing" ]; then
            printf "  ❌ %s\n" "$outfile"
            while read pattern; do
                printf "     Missing: \"%s\"\n" "$pattern"
            done < "$missing"
            failed=1
        fi
        rm -f "$missing"
    fi
done

if [ $failed -eq 0 ]; then
    printf "\n✅ All UCI tests passed!\n\n"
else
    printf "\n❌ Some UCI tests failed!\n\n"
    exit 1
fi
