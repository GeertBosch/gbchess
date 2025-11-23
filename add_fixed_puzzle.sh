#!/bin/bash
ALL_PUZZLES=lichess/lichess_db_puzzle.csv
FIXED_PUZZLES=lichess/fixed_puzzles.csv

set -e

if [ -z "$1" ] ; then
    echo "missing puzzle id argument" >&2
    exit 2
fi

if [ ! -f "${ALL_PUZZLES}" ] ; then
    echo "puzzles file not found: ${ALL_PUZZLES}" >&2
    exit 2
fi

if [ ! -f "${FIXED_PUZZLES}" ] ; then
    head -1 "${ALL_PUZZLES}" > "${FIXED_PUZZLES}"
fi

if grep -q "$1" "${FIXED_PUZZLES}" ; then
    echo "already in ${FIXED_PUZZLES}: $1" >&2
    exit 1
fi

grep "$1" "${ALL_PUZZLES}" | tee -a ${FIXED_PUZZLES}

if ! grep -q "$1" "${FIXED_PUZZLES}" ; then
    echo "$1 not found in ${ALL_PUZZLES}" >&2
    exit 2
fi

echo "$1 added to ${FIXED_PUZZLES}"
