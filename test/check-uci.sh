#!/bin/bash
# Check UCI test output files against expected patterns

set -e

failed=0

for infile in test/uci-*.in; do
    outfile=build/$(basename "$infile" .in).out
    file_failed=0
    matches=$(mktemp)

    # Check expect-count directives (must match exact count)
    if grep -qw "^expect-count" "$infile"; then
        errors=$(mktemp)
        grep -w "^expect-count" "$infile" | \
        while read expect_count expected_count pattern ; do
            actual_count=$(grep -cE "$pattern" "$outfile" || true)

            # Support: n (exact), +n (greater), -n (less than)
            case "$expected_count" in
                -*) op=lt threshold=${expected_count:1} msg="less than" ;;
                +*) op=gt threshold=${expected_count:1} msg="more than" ;;
                *)  op=eq threshold=$expected_count msg="exactly" ;;
            esac

            if [ "$actual_count" "-$op" "$threshold" ]; then
                echo "expect-count $expected_count \"$pattern\" (found $actual_count)"
                continue
            fi

            echo "Expected $msg $threshold occurrences of \"$pattern\", found $actual_count" >> "$errors"
        done >> "$matches"

        if [ -s "$errors" ]; then
            printf "  ❌ %s\n" "$outfile"
            while read error; do
                printf "     %s\n" "$error"
            done < "$errors"
            failed=$((failed + 1))
            file_failed=1
        fi
        rm -f "$errors"
    fi

    # Check expect directives (pattern must exist at least once)
    if grep -q "^expect " "$infile"; then
        # Collect missing patterns
        missing=$(mktemp)
        grep "^expect " "$infile" | \
        while read expect pattern ; do
            if grep -qE "$pattern" "$outfile"; then
                echo "expect \"$pattern\""
                continue
            fi

            echo "$pattern" >> "$missing"
        done >> "$matches"

        if [ -s "$missing" ]; then
            printf "  ❌ %s\n" "$outfile"
            while read pattern; do
                printf "     Missing: \"%s\"\n" "$pattern"
            done < "$missing"
            failed=$((failed + 1))
            file_failed=1
        fi
        rm -f "$missing"
    fi

    if [ "$file_failed" -eq 0 ] && [ -s "$matches" ]; then
        printf "  ✅ %s\n" "$outfile"
        while read match; do
            printf "     Matched: %s\n" "$match"
        done < "$matches"
    fi
    rm -f "$matches"
done

if [ $failed -ne 0 ]; then
    printf "\n ❌ $failed UCI tests failed!\n\n"
    exit 1
fi
