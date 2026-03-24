#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-src/README.md}"
ROOT="${2:-src}"
MAX_DEPTH="${3:-2}"
SECTION="${4:-## File Organization}"
VERBOSE=
for arg in "$@"; do [[ "$arg" == "-v" ]] && VERBOSE=1; done

if [[ ! -f "$FILE" ]]; then
    echo "error: markdown file not found: $FILE" >&2
    exit 1
fi

if [[ ! -d "$ROOT" ]]; then
    echo "error: root directory not found: $ROOT" >&2
    exit 1
fi

if ! [[ "$MAX_DEPTH" =~ ^[0-9]+$ ]]; then
    echo "error: depth must be a non-negative integer, got: $MAX_DEPTH" >&2
    exit 1
fi

python3 - "$FILE" "$ROOT" "$MAX_DEPTH" "$SECTION" "${VERBOSE:+verbose}" <<'PY'
from __future__ import annotations

import re
import sys
from pathlib import Path


def find_target_block(lines: list[str], section: str) -> tuple[int, int]:
    heading_index = -1
    for i, line in enumerate(lines):
        if line.strip() == section.strip():
            heading_index = i
            break
    if heading_index < 0:
        raise ValueError(f"section not found: {section}")

    block_open = -1
    for i in range(heading_index + 1, len(lines)):
        stripped = lines[i].strip()
        if stripped.startswith("#") and i != heading_index + 1:
            break
        if stripped == "```":
            block_open = i
            break
    if block_open < 0:
        raise ValueError(f"code block not found after section: {section}")

    block_close = -1
    for i in range(block_open + 1, len(lines)):
        if lines[i].strip() == "```":
            block_close = i
            break
    if block_close < 0:
        raise ValueError("unterminated code block")

    return block_open, block_close


def collect_descriptions(block_lines: list[str]) -> dict[str, str]:
    descriptions: dict[str, str] = {}
    for raw in block_lines:
        if " - " not in raw:
            continue
        left, right = raw.split(" - ", 1)
        match = re.search(r"([A-Za-z0-9_.-]+)/\s*$", left)
        if not match:
            continue
        key = match.group(1)
        if key not in descriptions:
            descriptions[key] = right.strip()
    return descriptions


def build_tree(root: Path, max_depth: int, descriptions: dict[str, str]) -> list[str]:
    root_name = root.name

    def walk_dirs(path: Path, depth: int) -> dict[str, dict]:
        if depth >= max_depth:
            return {}
        children = [p for p in path.iterdir() if p.is_dir()]
        children.sort(key=lambda p: p.name)
        return {child.name: walk_dirs(child, depth + 1) for child in children}

    tree = walk_dirs(root, 0)
    lines: list[str] = []

    def format_line(prefix: str, name: str, desc: str | None) -> str:
        label = f"{prefix}{name}/"
        if not desc:
            return label
        pad = " " * max(1, 28 - len(label))
        return f"{label}{pad}- {desc}"

    lines.append(format_line("", root_name, descriptions.get(root_name)))

    def emit(children: dict[str, dict], prefix: str) -> None:
        names = list(children.keys())
        for index, name in enumerate(names):
            is_last = index == len(names) - 1
            branch = "└── " if is_last else "├── "
            lines.append(format_line(prefix + branch, name, descriptions.get(name)))
            child_prefix = prefix + ("    " if is_last else "│   ")
            emit(children[name], child_prefix)

    emit(tree, "")
    return lines


def main() -> int:
    markdown_path = Path(sys.argv[1])
    root = Path(sys.argv[2])
    max_depth = int(sys.argv[3])
    section = sys.argv[4]
    verbose = len(sys.argv) > 5 and sys.argv[5] == "verbose"

    content = markdown_path.read_text(encoding="utf-8")
    lines = content.splitlines()

    block_open, block_close = find_target_block(lines, section)
    old_block_lines = lines[block_open + 1 : block_close]
    descriptions = collect_descriptions(old_block_lines)
    new_block_lines = build_tree(root, max_depth, descriptions)

    updated = lines[: block_open + 1] + new_block_lines + lines[block_close:]
    updated_content = "\n".join(updated) + "\n"
    if updated_content == content:
        if verbose:
            print(f"unchanged {markdown_path}")
        return 0

    markdown_path.write_text(updated_content, encoding="utf-8")
    if verbose:
        print(f"updated {markdown_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
PY

if [[ -n "$VERBOSE" ]]; then echo "processed $FILE from directory tree rooted at $ROOT (depth=$MAX_DEPTH)"; fi
