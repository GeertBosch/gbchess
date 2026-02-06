#!/usr/bin/awk -f

# Process PGN files to count ECO and Opening tag occurrences
# For each ECO, find the most common Opening and report statistics

BEGIN {
    current_eco = ""
    current_opening = ""
}

# Match ECO tag
/^\[ECO "/ {
    gsub(/^\[ECO "/, "", $0)
    gsub(/".*/, "", $0)
    current_eco = $0
}

# Match Opening tag
/^\[Opening "/ {
    gsub(/^\[Opening "/, "", $0)
    gsub(/"].*/, "", $0)
    current_opening = $0
}

# Empty line indicates end of game headers
/^$/ {
    if (current_eco != "" && current_opening != "") {
        # Count this ECO-Opening pair
        pair_count[current_eco, current_opening]++
        # Count total occurrences of this ECO
        eco_count[current_eco]++
    }
    current_eco = ""
    current_opening = ""
}

END {
    # For each ECO, find the most common Opening
    for (eco in eco_count) {
        max_count = 0
        best_opening = ""

        # Find the Opening with highest count for this ECO
        for (pair in pair_count) {
            split(pair, parts, SUBSEP)
            if (parts[1] == eco && pair_count[pair] > max_count) {
                max_count = pair_count[pair]
                best_opening = parts[2]
            }
        }

        if (best_opening != "") {
            total = eco_count[eco]
            percentage = (max_count * 100.0) / total
            printf "%s\t%s\t%d\t%d\t%.1f%%\n", eco, best_opening, max_count, total, percentage
        }
    }
}
