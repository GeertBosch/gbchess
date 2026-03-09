#!/usr/bin/env python3
"""Convert xctrace time-profile XML export to FlameGraph folded stacks.

Usage:
  python xctrace_to_folded.py \
    --input build/profile/perft8.time-profile.xml \
    --output build/profile/perft8.xctrace.folded
"""

import argparse
import xml.etree.ElementTree as ET
from collections import Counter


def convert(input_path: str, output_path: str) -> tuple[int, int]:
    root = ET.parse(input_path).getroot()

    frame_name_by_id = {}
    thread_state_fmt_by_id = {}
    backtrace_frame_ids_by_id = {}

    for elem in root.iter():
        if elem.tag == "frame":
            frame_id = elem.attrib.get("id")
            frame_name = elem.attrib.get("name")
            if frame_id and frame_name:
                frame_name_by_id[frame_id] = frame_name
        elif elem.tag == "thread-state":
            state_id = elem.attrib.get("id")
            state_fmt = elem.attrib.get("fmt")
            if state_id and state_fmt:
                thread_state_fmt_by_id[state_id] = state_fmt

    for backtrace in root.iter("backtrace"):
        backtrace_id = backtrace.attrib.get("id")
        if not backtrace_id:
            continue

        frame_ids = []
        for frame in backtrace.findall("frame"):
            ref = frame.attrib.get("ref")
            frame_id = frame.attrib.get("id")
            if ref:
                frame_ids.append(ref)
            elif frame_id:
                frame_ids.append(frame_id)
        backtrace_frame_ids_by_id[backtrace_id] = frame_ids

    folded = Counter()

    for row in root.iter("row"):
        state = None
        backtrace_id = None

        thread_state = row.find("thread-state")
        if thread_state is not None:
            state = thread_state.attrib.get("fmt")
            if state is None:
                state = thread_state_fmt_by_id.get(thread_state.attrib.get("ref", ""))

        backtrace = row.find("backtrace")
        if backtrace is not None:
            backtrace_id = backtrace.attrib.get("id")
            if backtrace_id is None:
                backtrace_id = backtrace.attrib.get("ref")

        if state != "Running" or not backtrace_id:
            continue

        frame_ids = backtrace_frame_ids_by_id.get(backtrace_id)
        if not frame_ids:
            continue

        names = []
        for frame_id in frame_ids:
            name = frame_name_by_id.get(frame_id)
            if not name:
                continue
            names.append(name.replace(";", ":").replace("\n", " "))

        if not names:
            continue

        stack = ";".join(reversed(names))
        folded[stack] += 1

    with open(output_path, "w", encoding="utf-8") as out:
        for stack, count in folded.items():
            out.write(f"{stack} {count}\n")

    return len(folded), sum(folded.values())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="xctrace time-profile XML export path")
    parser.add_argument("--output", required=True, help="Folded stack output path")
    args = parser.parse_args()

    folded_stacks, samples = convert(args.input, args.output)
    print(f"folded_stacks={folded_stacks}")
    print(f"samples={samples}")
    print(f"out={args.output}")


if __name__ == "__main__":
    main()
