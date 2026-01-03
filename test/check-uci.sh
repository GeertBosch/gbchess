#!/bin/bash
# Check UCI test output files against expected patterns

set -e

failed=0

for infile in test/uci-*.in; do
    outfile=test/out/$(basename "$infile" .in).out

    # Check expect-count directives (must match exact count)
    if grep -qw "^expect-count" "$infile"; then
        errors=$(mktemp)
        grep -w "^expect-count" "$infile" | \
        while read expect_count expected_count pattern ; do
            actual_count=$(grep -ce "$pattern" "$outfile" || true)
            if [ "$actual_count" -ne "$expected_count" ]; then
                echo "Expected $expected_count occurrences of \"$pattern\", found $actual_count"
            fi
        done > "$errors"

        if [ -s "$errors" ]; then
            printf "  ❌ %s\n" "$outfile"
            while read error; do
                printf "     %s\n" "$error"
            done < "$errors"
            failed=1
        fi
        rm -f "$errors"
    fi

    # Check expect directives (pattern must exist at least once)
    if grep -q "^expect " "$infile"; then
        # Collect missing patterns
        missing=$(mktemp)
        grep "^expect " "$infile" | \
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
