#!/bin/bash
# Runs puzzle-test for multiple engines and prints a Markdown table.
# Usage: puzzle-table.sh [--depth N[,N...]] [--nodes N[,N...]] [--movetime N[,N...]]
#                        <engine1> [-opt...] [<engine2> [-opt...] ...]
#
# Each option accepts a single value or a comma-separated list of values.
# Exactly one option may carry a list; the table has one row per list entry.
# Fixed single values repeat on every row and appear as key columns.

set -euo pipefail

PUZZLE_TEST="${PUZZLE_TEST:-build/puzzle-test}"
CSV="${CSV:-lichess/ci_nonmate_100.csv}"

die()   { echo "Error: $*" >&2; exit 1; }
usage() {
    echo "Usage: $0 [--depth N[,N...]] [--nodes N[,N...]] [--movetime N[,N...]] <engine1> [-opt...] [...]" >&2
    exit 1
}

depth=""
nodes=""
movetime=""

# Parse script-level options (double-dash only)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --depth)    [[ $# -ge 2 ]] || usage; depth="$2";    shift 2 ;;
        --nodes)    [[ $# -ge 2 ]] || usage; nodes="$2";    shift 2 ;;
        --movetime) [[ $# -ge 2 ]] || usage; movetime="$2"; shift 2 ;;
        --*)        die "Unknown option: $1" ;;
        *)          break ;;
    esac
done

[[ $# -eq 0 ]] && usage
[[ -n "$depth" || -n "$nodes" || -n "$movetime" ]] || usage

# Split comma-separated values into arrays (only if the option was given)
depth_vals=()
nodes_vals=()
movetime_vals=()
[[ -n "$depth"    ]] && IFS=',' read -ra depth_vals    <<< "$depth"
[[ -n "$nodes"    ]] && IFS=',' read -ra nodes_vals    <<< "$nodes"
[[ -n "$movetime" ]] && IFS=',' read -ra movetime_vals <<< "$movetime"

# Validate all values are positive integers
validate_ints() {
    local opt="$1"; shift
    for v; do
        [[ "$v" =~ ^[0-9]+$ ]] && [[ "$v" -gt 0 ]] || die "$opt values must be positive integers (got: '$v')"
    done
}
[[ ${#depth_vals[@]}    -gt 0 ]] && validate_ints "--depth"    "${depth_vals[@]}"
[[ ${#nodes_vals[@]}    -gt 0 ]] && validate_ints "--nodes"    "${nodes_vals[@]}"
[[ ${#movetime_vals[@]} -gt 0 ]] && validate_ints "--movetime" "${movetime_vals[@]}"

# Enforce: at most one option may be a list
seqs=0
[[ ${#depth_vals[@]}    -gt 1 ]] && seqs=$((seqs+1)) || true
[[ ${#nodes_vals[@]}    -gt 1 ]] && seqs=$((seqs+1)) || true
[[ ${#movetime_vals[@]} -gt 1 ]] && seqs=$((seqs+1)) || true
[[ $seqs -gt 1 ]] && die "at most one of --depth/--nodes/--movetime may be a list"

# Number of table rows = length of the one list (or 1 for all-scalar)
num_rows=1
[[ ${#depth_vals[@]}    -gt $num_rows ]] && num_rows=${#depth_vals[@]}
[[ ${#nodes_vals[@]}    -gt $num_rows ]] && num_rows=${#nodes_vals[@]}
[[ ${#movetime_vals[@]} -gt $num_rows ]] && num_rows=${#movetime_vals[@]}

# Parse engines and their single-dash options
declare -a cmds=()
current_cmd=""
for arg; do
    if [[ "$arg" == -* ]]; then
        [[ -z "$current_cmd" ]] && die "Option '$arg' before any engine name"
        current_cmd="$current_cmd $arg"
    else
        [[ -n "$current_cmd" ]] && cmds+=("$current_cmd")
        current_cmd="$arg"
    fi
done
[[ -n "$current_cmd" ]] && cmds+=("$current_cmd")
[[ ${#cmds[@]} -eq 0 ]] && usage

# Build display labels: basename of engine + any options
declare -a labels=()
for cmd in "${cmds[@]}"; do
    first="${cmd%% *}"
    rest="${cmd#"$first"}"
    rest="${rest# }"
    label="$(basename "$first")"
    [[ -n "$rest" ]] && label="$label $rest"
    labels+=("$label")
done

# Header row: one key column per specified constraint, then one per engine
[[ ${#depth_vals[@]}    -gt 0 ]] && printf "| Depth "
[[ ${#nodes_vals[@]}    -gt 0 ]] && printf "| Nodes "
[[ ${#movetime_vals[@]} -gt 0 ]] && printf "| Movetime "
for label in "${labels[@]}"; do printf "| %s " "$label"; done
printf "|\n"

# Separator row (right-aligned)
[[ ${#depth_vals[@]}    -gt 0 ]] && printf "| ---: "
[[ ${#nodes_vals[@]}    -gt 0 ]] && printf "| ---: "
[[ ${#movetime_vals[@]} -gt 0 ]] && printf "| ---: "
for _ in "${cmds[@]}"; do printf "| ---: "; done
printf "|\n"

# Data rows
for ((row=0; row<num_rows; row++)); do
    go_opts=""

    if [[ ${#depth_vals[@]} -gt 0 ]]; then
        idx=$(( row < ${#depth_vals[@]} ? row : ${#depth_vals[@]}-1 ))
        val="${depth_vals[$idx]}"
        go_opts="$go_opts --depth $val"
        printf "| %s " "$val"
    fi

    if [[ ${#nodes_vals[@]} -gt 0 ]]; then
        idx=$(( row < ${#nodes_vals[@]} ? row : ${#nodes_vals[@]}-1 ))
        val="${nodes_vals[$idx]}"
        go_opts="$go_opts --nodes $val"
        printf "| %s " "$val"
    fi

    if [[ ${#movetime_vals[@]} -gt 0 ]]; then
        idx=$(( row < ${#movetime_vals[@]} ? row : ${#movetime_vals[@]}-1 ))
        val="${movetime_vals[$idx]}"
        go_opts="$go_opts --movetime $val"
        printf "| %s " "$val"
    fi

    for cmd in "${cmds[@]}"; do
        # shellcheck disable=SC2086
        result=$($PUZZLE_TEST $go_opts $cmd "$CSV" 2>/dev/null \
                 | grep -E '[0-9]+ rating$' | tr -d ,) || true
        if [[ -z "$result" ]]; then
            printf "| — "
        else
            correct=$(awk '{print $3}'      <<< "$result")
            rating=$( awk '{print $(NF-1)}' <<< "$result")
            printf "| %s / %s " "$correct" "$rating"
        fi
    done
    printf "|\n"
done
