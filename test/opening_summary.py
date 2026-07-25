#!/usr/bin/env python3
"""Summarize the opening phase of a fastchess PGN for book/opening work.

Usage:
    test/opening_summary.py build/sprt-openings-YYYYMMDD-HHMMSS.pgn [more.pgn ...]
    test/opening_summary.py --plies 10 --min-games 30 FILE.pgn

Where sprt_summary.py answers "did the change gain Elo", this answers "which
opening lines are we actually losing points in". It is meant for runs started
from the initial position (`make sprt-openings`), where the engines' own book
picks the opening, so every line in the report is one the book chose to play.

For each PGN it prints:
  * how deep the book took each engine, and how the engine scores by exit depth
  * the engine eval at book exit (first searched move), per line, from White's
    point of view -- lines that hand back a big minus are book bugs
  * a move tree (depth 1..N) with White's score % and exit eval per node
  * the worst and best full opening lines by White score

All score percentages are from WHITE's point of view unless stated otherwise,
because an opening line is a property of the position, not of an engine. Use
--by-engine to additionally split each line by which engine played it.
"""
import argparse
import re
import statistics as st
import sys
from collections import defaultdict

TAG = re.compile(r'\[(\w+)\s+"([^"]*)"\]')
# In move-text order: a comment, a move number, or a SAN move.
TOKEN = re.compile(r'\{([^}]*)\}|(\d+\.(?:\.\.)?)|([OKQRBNa-h][^\s{}]*)')
SCORE = re.compile(r'([+-]?)(M)?(\d+(?:\.\d+)?)/(\d+)')  # sign, mate?, value, depth


def parse_games(text):
    games = []
    for block in re.split(r'(?=\[Event )', text):
        block = block.strip()
        if not block:
            continue
        tags = dict(TAG.findall(block))
        parts = block.split('\n\n', 1)
        games.append({'tags': tags, 'moves_text': parts[1] if len(parts) > 1 else ''})
    return games


def parse_moves(moves_text):
    """Return list of (san, cp, depth); cp/depth are None for un-annotated moves.

    cp is from the mover's point of view; mate scores map to +/-100000 - distance.
    """
    moves = []
    for m in TOKEN.finditer(moves_text):
        comment, _number, san = m.groups()
        if san:
            moves.append([san, None, None])
        elif comment and moves:
            s = SCORE.search(comment)
            if s:
                sign, mate, val, depth = s.groups()
                cp = 100000 - int(float(val)) if mate else int(round(float(val) * 100))
                moves[-1][1] = -cp if sign == '-' else cp
                moves[-1][2] = int(depth)
    return [tuple(m) for m in moves]


def book_exit(moves, color):
    """Ply index (0-based, in `moves`) of the first searched move by `color`.

    A move is "searched" when the engine reported a score at depth >= 1; book
    moves are played instantly with no info line, so they carry no score.
    Returns len(moves) if the side never left book (game ended inside it).
    """
    start = 0 if color == 'W' else 1
    for i in range(start, len(moves), 2):
        if moves[i][2]:
            return i
    return len(moves)


def final_reason(moves_text):
    """Classify how a game ended from its final move comment."""
    comments = re.findall(r'\{([^}]*)\}', moves_text)
    lc = comments[-1].lower() if comments else ''
    for needle, name in (('mates', 'mate'), ('3-fold', 'repetition'),
                         ('repetition', 'repetition'), ('fifty', 'fifty-move'),
                         ('insufficient', 'insufficient-material'),
                         ('stalemate', 'stalemate'), ('adjudic', 'adjudication')):
        if needle in lc:
            return name
    return 'other'


def white_points(result):
    return {'1-0': 1.0, '0-1': 0.0, '1/2-1/2': 0.5}.get(result)


def collect(path, plies):
    text = open(path).read()
    games = []
    for g in parse_games(text):
        pts = white_points(g['tags'].get('Result'))
        if pts is None:
            continue
        moves = parse_moves(g['moves_text'])
        if not moves:
            continue
        wexit, bexit = book_exit(moves, 'W'), book_exit(moves, 'B')
        # Eval at book exit, from White's POV: the first searched move of the
        # side that leaves book first tells us what the book walked into.
        exit_ply = min(wexit, bexit)
        exit_cp = None
        if exit_ply < len(moves) and moves[exit_ply][1] is not None:
            exit_cp = moves[exit_ply][1] if exit_ply % 2 == 0 else -moves[exit_ply][1]
        games.append({
            'cause': ('time forfeit' if g['tags'].get('Termination') == 'time forfeit'
                      else final_reason(g['moves_text'])),
            'eco': g['tags'].get('ECO', ''),
            'opening': g['tags'].get('Opening', ''),
            'white': g['tags'].get('White', '?'),
            'black': g['tags'].get('Black', '?'),
            'result': g['tags'].get('Result'),
            'points': pts,
            'line': [m[0] for m in moves[:plies]],
            'plies': len(moves),
            'wexit': wexit,
            'bexit': bexit,
            'exit_cp': exit_cp,
        })
    return games


class Bucket:
    def __init__(self):
        self.n = self.w = self.d = self.l = 0
        self.pts = 0.0
        self.evals = []

    def add(self, g):
        self.n += 1
        self.pts += g['points']
        self.w += g['points'] == 1.0
        self.d += g['points'] == 0.5
        self.l += g['points'] == 0.0
        if g['exit_cp'] is not None:
            self.evals.append(g['exit_cp'])

    @property
    def score(self):
        return 100.0 * self.pts / self.n if self.n else 0.0

    @property
    def median_eval(self):
        return st.median(self.evals) if self.evals else None


def md_table(headers, rows):
    out = ['| ' + ' | '.join(headers) + ' |', '|' + '---|' * len(headers)]
    for r in rows:
        out.append('| ' + ' | '.join(str(c) for c in r) + ' |')
    return '\n'.join(out) + '\n'


def fmt_eval(cp):
    return '' if cp is None else f'{cp:+.0f}'


def report(path, games, args, out=sys.stdout):
    def P(*a):
        print(*a, file=out)

    P(f'# Opening summary: `{path}`\n')
    if not games:
        P('No completed games found.\n')
        return

    total = Bucket()
    for g in games:
        total.add(g)
    P(f'{total.n} games, White scores {total.score:.2f}% '
      f'(+{total.w} ={total.d} -{total.l}), '
      f'median eval at book exit {fmt_eval(total.median_eval)}cp\n')

    # --- Book depth -------------------------------------------------------
    P('## Book depth\n')
    per_engine = defaultdict(list)
    for g in games:
        per_engine[g['white']].append(g['wexit'] // 2)
        per_engine[g['black']].append(max(0, g['bexit'] - 1) // 2)
    rows = []
    for name in sorted(per_engine):
        d = per_engine[name]
        rows.append([name, len(d), f'{st.mean(d):.1f}', st.median(d), min(d), max(d)])
    P(md_table(['engine', 'games', 'mean book moves', 'median', 'min', 'max'], rows))
    P('*Book moves = own moves played from book before the first searched move.*\n')

    by_exit = defaultdict(Bucket)
    for g in games:
        by_exit[min(g['wexit'], g['bexit'])].add(g)
    rows = [[ply, b.n, f'{b.score:.1f}', fmt_eval(b.median_eval)]
            for ply, b in sorted(by_exit.items()) if b.n >= args.min_games]
    P('### White score by first book exit (ply)\n')
    P(md_table(['exit ply', 'n', 'white %', 'median exit eval'], rows))

    # --- Cause by colour --------------------------------------------------
    causes = defaultdict(lambda: [0, 0, 0])  # cause -> [white lost, black lost, drawn]
    for g in games:
        slot = 0 if g['points'] == 0.0 else (1 if g['points'] == 1.0 else 2)
        causes[g['cause']][slot] += 1
    rows = [[c, *v] for c, v in sorted(causes.items())]
    rows.append(['**total**', *[sum(v[i] for v in causes.values()) for i in range(3)]])
    P('## Win/loss by cause and colour\n')
    P(md_table(['cause', 'white LOSES by', 'black LOSES by', 'draws by'], rows))

    # --- Named openings ---------------------------------------------------
    # fastchess tags each game with ECO + Opening; that classification is usually a
    # better handle on "which opening" than a raw move prefix.
    named = defaultdict(Bucket)
    for g in games:
        if g['opening']:
            named[(g['eco'], g['opening'])].add(g)
    if named:
        ranked = sorted(named.items(), key=lambda kv: kv[1].score)
        rows = [[eco, name, b.n, f'{b.score:.1f}', f'{b.w}/{b.d}/{b.l}',
                 fmt_eval(b.median_eval)]
                for (eco, name), b in ranked if b.n >= args.min_games]
        P(f'## Named openings (>= {args.min_games} games, worst first)\n')
        P(md_table(['ECO', 'opening', 'n', 'white %', 'W/D/L', 'median exit eval'], rows))

    # --- Move tree --------------------------------------------------------
    P(f'## Opening tree (nodes with >= {args.min_games} games)\n')
    tree = defaultdict(Bucket)
    for g in games:
        for d in range(1, min(args.plies, len(g['line'])) + 1):
            tree[tuple(g['line'][:d])].add(g)
    rows = []
    for line, b in sorted(tree.items(), key=lambda kv: (len(kv[0]), -kv[1].n)):
        if b.n < args.min_games or len(line) > args.tree_plies:
            continue
        rows.append(['`' + ' '.join(line) + '`', len(line), b.n,
                     f'{b.score:.1f}', f'{b.w}/{b.d}/{b.l}', fmt_eval(b.median_eval)])
    P(md_table(['line', 'ply', 'n', 'white %', 'W/D/L', 'median exit eval'], rows))

    # --- Full lines -------------------------------------------------------
    full = {line: b for line, b in tree.items()
            if len(line) == args.plies and b.n >= args.min_games}
    if full:
        ranked = sorted(full.items(), key=lambda kv: kv[1].score)
        show = args.top
        P(f'## Worst {min(show, len(ranked))} lines at {args.plies} plies '
          f'(White %, >= {args.min_games} games)\n')
        P(md_table(['white %', 'n', 'W/D/L', 'median exit eval', 'line'],
                   [[f'{b.score:.1f}', b.n, f'{b.w}/{b.d}/{b.l}',
                     fmt_eval(b.median_eval), '`' + ' '.join(line) + '`']
                    for line, b in ranked[:show]]))
        P(f'## Best {min(show, len(ranked))} lines at {args.plies} plies\n')
        P(md_table(['white %', 'n', 'W/D/L', 'median exit eval', 'line'],
                   [[f'{b.score:.1f}', b.n, f'{b.w}/{b.d}/{b.l}',
                     fmt_eval(b.median_eval), '`' + ' '.join(line) + '`']
                    for line, b in reversed(ranked[-show:])]))

    # --- Per-engine split -------------------------------------------------
    if args.by_engine:
        P(f'## Per-engine score by first move (>= {args.min_games} games)\n')
        split = defaultdict(Bucket)
        for g in games:
            if not g['line']:
                continue
            split[(g['white'], g['line'][0])].add(g)
        rows = [[eng, mv, b.n, f'{b.score:.1f}']
                for (eng, mv), b in sorted(split.items()) if b.n >= args.min_games]
        P(md_table(['engine (as white)', '1st move', 'n', 'white %'], rows))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('pgn', nargs='+')
    ap.add_argument('--plies', type=int, default=8,
                    help='plies that define a full opening line (default: 8)')
    ap.add_argument('--tree-plies', type=int, default=6,
                    help='max depth shown in the opening tree (default: 6)')
    ap.add_argument('--min-games', type=int, default=20,
                    help='hide buckets with fewer games (default: 20)')
    ap.add_argument('--top', type=int, default=15,
                    help='how many worst/best lines to list (default: 15)')
    ap.add_argument('--by-engine', action='store_true',
                    help='also split scores by engine')
    args = ap.parse_args()

    for path in args.pgn:
        report(path, collect(path, args.plies), args)
        print('\n---\n')


if __name__ == '__main__':
    main()
