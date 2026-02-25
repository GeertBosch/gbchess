#!/usr/bin/env bash
set -euo pipefail

input=${1:-lichess/lichess_db_puzzle.csv}
out_dir=${2:-lichess}

if [[ ! -f "$input" ]]; then
    echo "Missing input puzzle CSV: $input" >&2
    exit 1
fi

mkdir -p "$out_dir"

mate123="$out_dir/ci_mate123_4000.csv"
mate45="$out_dir/ci_mate45_100.csv"
nonmate="$out_dir/ci_nonmate_100.csv"

awk -F, '
NR == 1 { print; next }
$8 ~ /mateIn[123]/ {
    print
    count++
    if (count == 4000) exit
}
' "$input" > "$mate123"

awk -F, '
NR == 1 { print; next }
$8 ~ /mateIn[456]/ {
    print
    count++
    if (count == 100) exit
}
' "$input" > "$mate45"

awk -F, '
NR == 1 { print; next }
$8 !~ /^mateIn[0-9]+$/ {
    print
    count++
    if (count == 100) exit
}
' "$input" > "$nonmate"

wc -l "$mate123" "$mate45" "$nonmate"
echo "Generated CI puzzle fixtures in $out_dir"
