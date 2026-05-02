#!/usr/bin/env bash
set -euo pipefail

if ! command -v xctrace >/dev/null 2>&1; then
    echo "xctrace not found. Install Xcode and ensure xctrace is on PATH."
    exit 1
fi

if ! command -v flamelens >/dev/null 2>&1; then
    echo "flamelens not found on PATH."
    echo "Install it with: cargo install flamelens --locked"
    exit 1
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_DIR="${ROOT_DIR}/build/profile"

DURATION="15s"
TITLE=""
GEN_SVG=0
CHILD_PROCESSES=0
PROCESS_FILTER=""

while (($#)); do
    case "$1" in
        --duration)
            DURATION="$2"
            shift 2
            ;;
        --title)
            TITLE="$2"
            shift 2
            ;;
        --svg)
            GEN_SVG=1
            shift
            ;;
        --all)
            CHILD_PROCESSES=1
            shift
            ;;
        --process)
            PROCESS_FILTER="$2"
            CHILD_PROCESSES=1
            shift 2
            ;;
        -h|--help)
            cat <<'EOF'
Usage: ./make-flamegraph.sh [options] <command> [args...]

Options:
  --duration T        xctrace time limit, e.g. 10s, 2m (default: 15s)
  --title TEXT        flame graph title
  --svg               generate SVG file instead of opening flamelens
  --all               trace all processes (captures forked children);
                      uses xctrace --all-processes instead of --launch,
                      so target stdout is not captured
  --process NAME      only include samples from processes whose name contains
                      NAME (case-insensitive); implies --all
  -h, --help          show this help

Outputs go to build/profile/flame-<command>.*
EOF
            exit 0
            ;;
        -*)
            echo "Unknown option: $1"
            echo "Run ./make-flamegraph.sh --help"
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

if (($# == 0)); then
    echo "Error: no command specified."
    echo "Run ./make-flamegraph.sh --help"
    exit 1
fi

CMD_PATH="$1"
CMD_ARGS=("${@:2}")
CMD_NAME="$(basename -- "${CMD_PATH}")"
BASE="flame-${CMD_NAME}"

TRACE_PATH="${PROFILE_DIR}/${BASE}.xctrace.trace"
TIME_PROFILE_XML="${PROFILE_DIR}/${BASE}.time-profile.xml"
FOLDED_PATH="${PROFILE_DIR}/${BASE}.xctrace.folded"
SVG_PATH="${PROFILE_DIR}/${BASE}.xctrace.flame.svg"
TARGET_STDOUT="${PROFILE_DIR}/${BASE}.xctrace.stdout.txt"

if [[ -z "${TITLE}" ]]; then
    if (( ${#CMD_ARGS[@]} > 0 )); then
        TITLE="${CMD_NAME} ${CMD_ARGS[*]} CPU Flame Graph (xctrace)"
    else
        TITLE="${CMD_NAME} CPU Flame Graph (xctrace)"
    fi
fi

mkdir -p "${PROFILE_DIR}"

if [[ "${GEN_SVG}" == "1" ]] && [[ ! -d /tmp/FlameGraph ]]; then
    git clone --depth 1 https://github.com/brendangregg/FlameGraph.git /tmp/FlameGraph
fi

rm -rf "${TRACE_PATH}"
rm -f "${TIME_PROFILE_XML}" "${FOLDED_PATH}" "${SVG_PATH}" "${TARGET_STDOUT}"

if (( ${#CMD_ARGS[@]} > 0 )); then
    echo "Recording Time Profiler trace for ${CMD_NAME} ${CMD_ARGS[*]} (${DURATION})..."
else
    echo "Recording Time Profiler trace for ${CMD_NAME} (${DURATION})..."
fi

if [[ "${CHILD_PROCESSES}" == "1" ]]; then
    # --all-processes and --launch are mutually exclusive in xctrace.
    # Use --notify-tracing-started so we know when xctrace is ready before
    # launching the target, avoiding a race at startup.
    NOTIFY_NAME="com.gbchess.flamegraph.$$"
    notifyutil -1 "${NOTIFY_NAME}" >/dev/null 2>&1 &
    NOTIFY_PID=$!
    xctrace record \
        --no-prompt \
        --template 'Time Profiler' \
        --output "${TRACE_PATH}" \
        --time-limit "${DURATION}" \
        --notify-tracing-started "${NOTIFY_NAME}" \
        --all-processes &
    XCTRACE_PID=$!
    wait "${NOTIFY_PID}" 2>/dev/null || true

    "${CMD_PATH}" "${CMD_ARGS[@]}" &
    CMD_PID=$!

    # Stop recording early if the target exits before the time limit.
    # Use ps state to avoid treating a zombie as still-running.
    ( while
          state=$(ps -p "${CMD_PID}" -o state= 2>/dev/null) && [[ "${state}" != "Z" ]]
      do sleep 0.2; done
      kill -INT "${XCTRACE_PID}" 2>/dev/null || true ) &
    WATCHER_PID=$!

    wait "${XCTRACE_PID}" || true
    kill "${CMD_PID}" "${WATCHER_PID}" 2>/dev/null || true
    wait "${CMD_PID}" "${WATCHER_PID}" 2>/dev/null || true
else
    xctrace record \
        --no-prompt \
        --template 'Time Profiler' \
        --output "${TRACE_PATH}" \
        --time-limit "${DURATION}" \
        --target-stdout "${TARGET_STDOUT}" \
        --launch -- "${CMD_PATH}" "${CMD_ARGS[@]}" || true
fi

[[ -d "${TRACE_PATH}" ]] || { echo "Error: trace file not created."; exit 1; }

echo "Exporting time-profile XML..."
xctrace export \
    --input "${TRACE_PATH}" \
    --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' \
    --output "${TIME_PROFILE_XML}"

echo "Converting to folded stacks..."
FILTER_ARGS=()
[[ -n "${PROCESS_FILTER}" ]] && FILTER_ARGS=("--process" "${PROCESS_FILTER}")
python3 "${ROOT_DIR}/xctrace_to_folded.py" \
    --input "${TIME_PROFILE_XML}" \
    --output "${FOLDED_PATH}" \
    "${FILTER_ARGS[@]+"${FILTER_ARGS[@]}"}"

echo "Done"
echo "  Folded: ${FOLDED_PATH}"

if [[ "${GEN_SVG}" == "1" ]]; then
    echo "Rendering flame graph SVG..."
    perl /tmp/FlameGraph/flamegraph.pl \
        --title "${TITLE}" \
        --countname samples \
        --minwidth 0.1 \
        "${FOLDED_PATH}" > "${SVG_PATH}"
    echo "  SVG:    ${SVG_PATH}"
else
    flamelens "${FOLDED_PATH}"
fi
