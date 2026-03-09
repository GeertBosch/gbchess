#!/usr/bin/env bash
set -euo pipefail

if ! command -v xctrace >/dev/null 2>&1; then
    echo "xctrace not found. Install Xcode and ensure xctrace is on PATH."
    exit 1
fi

if ! command -v perl >/dev/null 2>&1; then
    echo "perl not found on PATH."
    exit 1
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_DIR="${ROOT_DIR}/build/profile"
TRACE_PATH="${PROFILE_DIR}/perft8.xctrace.trace"
TIME_PROFILE_XML="${PROFILE_DIR}/perft8.time-profile.xml"
FOLDED_PATH="${PROFILE_DIR}/perft8.xctrace.folded"
SVG_PATH="${PROFILE_DIR}/perft8.xctrace.flame.svg"
TARGET_STDOUT="${PROFILE_DIR}/perft8.xctrace.stdout.txt"

DURATION="15s"
DEPTH="8"
TITLE="gbchess build/perft 8 CPU Flame Graph (xctrace)"
OPEN_RESULT=0

while (($#)); do
    case "$1" in
        --duration)
            DURATION="$2"
            shift 2
            ;;
        --depth)
            DEPTH="$2"
            TITLE="gbchess build/perft ${DEPTH} CPU Flame Graph (xctrace)"
            shift 2
            ;;
        --title)
            TITLE="$2"
            shift 2
            ;;
        --open)
            OPEN_RESULT=1
            shift
            ;;
        -h|--help)
            cat <<EOF
Usage: ./make-flamegraph.sh [options]

Options:
  --depth N         perft depth (default: 8)
  --duration T      xctrace time limit, e.g. 10s, 2m (default: 15s)
  --title TEXT      flame graph title
  --open            open resulting SVG in VS Code
  -h, --help        show this help

Outputs:
  ${TRACE_PATH}
  ${TIME_PROFILE_XML}
  ${FOLDED_PATH}
  ${SVG_PATH}
EOF
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Run ./make-flamegraph.sh --help"
            exit 1
            ;;
    esac
done

mkdir -p "${PROFILE_DIR}"

if [[ ! -d /tmp/FlameGraph ]]; then
    git clone --depth 1 https://github.com/brendangregg/FlameGraph.git /tmp/FlameGraph
fi

if [[ -f "${TRACE_PATH}" ]]; then
    rm -rf "${TRACE_PATH}"
fi

echo "Recording Time Profiler trace for build/perft ${DEPTH} (${DURATION})..."
xctrace record \
    --no-prompt \
    --template 'Time Profiler' \
    --output "${TRACE_PATH}" \
    --time-limit "${DURATION}" \
    --target-stdout "${TARGET_STDOUT}" \
    --launch -- "${ROOT_DIR}/build/perft" "${DEPTH}"

echo "Exporting time-profile XML..."
xctrace export \
    --input "${TRACE_PATH}" \
    --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' \
    --output "${TIME_PROFILE_XML}"

echo "Converting to folded stacks..."
python3 "${ROOT_DIR}/xctrace_to_folded.py" \
    --input "${TIME_PROFILE_XML}" \
    --output "${FOLDED_PATH}"

echo "Rendering flame graph SVG..."
perl /tmp/FlameGraph/flamegraph.pl \
    --title "${TITLE}" \
    --countname samples \
    --minwidth 0.1 \
    "${FOLDED_PATH}" > "${SVG_PATH}"

echo "Done"
echo "  Folded: ${FOLDED_PATH}"
echo "  SVG:    ${SVG_PATH}"

if [[ "${OPEN_RESULT}" == "1" ]]; then
    if command -v code >/dev/null 2>&1; then
        code -r "${SVG_PATH}"
    else
        open "${SVG_PATH}"
    fi
fi
