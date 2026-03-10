#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys


MOVE_RE = re.compile(r"^([a-h][1-8][a-h][1-8][qrbn]?):\s+(\d+)$")


def run_cmd(cmd, cwd, stdin=None):
    proc = subprocess.run(cmd, input=stdin, text=True, capture_output=True, cwd=cwd)
    if proc.returncode != 0:
        msg = [f"Command failed: {' '.join(cmd)}"]
        if proc.stdout:
            msg.append(f"stdout:\n{proc.stdout}")
        if proc.stderr:
            msg.append(f"stderr:\n{proc.stderr}")
        raise RuntimeError("\n".join(msg))
    return proc.stdout


def parse_divide(output):
    counts = {}
    for line in output.splitlines():
        match = MOVE_RE.match(line.strip())
        if match:
            counts[match.group(1)] = int(match.group(2))
    return counts


def engine_divide(engine_bin, fen, depth, cwd):
    return parse_divide(run_cmd([engine_bin, fen, str(depth)], cwd=cwd))


def stockfish_divide(stockfish_bin, fen, depth, cwd):
    cmd = f"position fen {fen}\ngo perft {depth}\nquit\n"
    return parse_divide(run_cmd([stockfish_bin], cwd=cwd, stdin=cmd))


def stockfish_fen_after(stockfish_bin, start_fen, moves, cwd):
    move_list = " ".join(moves)
    cmd = f"position fen {start_fen}" + (f" moves {move_list}" if move_list else "") + "\nd\nquit\n"
    out = run_cmd([stockfish_bin], cwd=cwd, stdin=cmd)
    for line in out.splitlines():
        if line.startswith("Fen: "):
            return line[5:].strip()
    raise RuntimeError("Stockfish did not return FEN for move sequence")


def choose_branch(diffs, mode):
    if mode == "first":
        return sorted(diffs, key=lambda item: item[0])[0]
    if mode == "maxabs":
        return max(diffs, key=lambda item: abs(item[3]))
    raise ValueError(f"Unknown pick mode: {mode}")


def main():
    parser = argparse.ArgumentParser(description="Isolate perft divergence against Stockfish.")
    parser.add_argument("fen", help="FEN to analyze")
    parser.add_argument("depth", type=int, help="Perft depth")
    parser.add_argument("--cwd", default=".", help="Working directory (default: current dir)")
    parser.add_argument(
        "--engine",
        default="./build/perft",
        help="Engine perft binary path (default: ./build/perft)",
    )
    parser.add_argument(
        "--stockfish",
        default="stockfish",
        help="Stockfish binary path (default: stockfish)",
    )
    parser.add_argument(
        "--pick",
        choices=["first", "maxabs"],
        default="maxabs",
        help="Branch selection strategy per ply (default: maxabs)",
    )
    args = parser.parse_args()

    if args.depth < 1:
        print("Depth must be >= 1", file=sys.stderr)
        return 2

    start_fen = args.fen
    fen = start_fen
    depth = args.depth
    path = []

    for ply in range(args.depth):
        ours = engine_divide(args.engine, fen, depth, args.cwd)
        sf = stockfish_divide(args.stockfish, fen, depth, args.cwd)

        diffs = []
        for move in sorted(set(ours) | set(sf)):
            ours_count = ours.get(move)
            sf_count = sf.get(move)
            if ours_count != sf_count:
                diffs.append((move, ours_count, sf_count, (ours_count or 0) - (sf_count or 0)))

        print(f"PLY {ply} depth {depth}")
        print(f"FEN {fen}")
        print(
            f"TOTAL ours={sum(ours.values())} stockfish={sum(sf.values())} diff-moves={len(diffs)}"
        )
        if not diffs:
            break

        move, ours_count, sf_count, delta = choose_branch(diffs, args.pick)
        print(f"CHOOSE {move}: ours={ours_count} stockfish={sf_count} delta={delta}")
        path.append(move)
        depth -= 1
        fen = stockfish_fen_after(args.stockfish, start_fen, path, args.cwd)

    print(f"PATH {' '.join(path)}")
    print(f"FINAL_FEN {fen}")

    ours_d1 = engine_divide(args.engine, fen, 1, args.cwd)
    sf_d1 = stockfish_divide(args.stockfish, fen, 1, args.cwd)

    extra = sorted(set(ours_d1) - set(sf_d1))
    missing = sorted(set(sf_d1) - set(ours_d1))
    print(f"DEPTH1_TOTAL ours={sum(ours_d1.values())} stockfish={sum(sf_d1.values())}")
    print(f"EXTRA {extra}")
    print(f"MISSING {missing}")
    for move in sorted(set(ours_d1) | set(sf_d1)):
        if ours_d1.get(move) != sf_d1.get(move):
            print(f"DIFF {move}: ours={ours_d1.get(move)} stockfish={sf_d1.get(move)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())