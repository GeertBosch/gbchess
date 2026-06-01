#!/usr/bin/env python3
"""Summarize fastchess SPRT PGN output for human + LLM analysis.

Usage:
    tools/sprt_summary.py build/sprt-YYYYMMDD-HHMMSS.pgn [more.pgn ...]
    tools/sprt_summary.py --new gbchess-new --base gbchess-base FILE.pgn

For each PGN it prints a markdown report: match summary, pentanomial, per-color
and per-opening scores, the WDL cause breakdown (mate / 3-fold / fifty-move /
insufficient material / stalemate / time forfeit / crash), conversion &
"throw" diagnostics from the in-PGN engine evals, and game-length stats.

When two or more PGNs are given it also prints a side-by-side delta table of the
cause breakdown, which is the quickest way to see what a change actually moved.

The "new" engine is auto-detected as the player whose name contains "new"; the
other player is treated as "base". Override with --new/--base. All WDL numbers
are reported from the NEW engine's perspective unless stated otherwise.
"""
import argparse
import math
import re
import statistics as st
import sys
from collections import Counter, defaultdict

TAG = re.compile(r'\[(\w+)\s+"([^"]*)"\]')
# A move with an engine comment, e.g.  Qd5 {-0.55/9 0.479s}  or  Nf7# {+M1/2 0.001s, White mates}
MOVE_COMMENT = re.compile(r'([a-hKQRBNO][^\s{}]*)\s*\{([^}]*)\}')
SCORE = re.compile(r'([+-]?)(M)?(\d+(?:\.\d+)?)/(\d+)')  # sign, mate?, value, depth


def parse_games(text):
    games = []
    for block in re.split(r'(?=\[Event )', text):
        block = block.strip()
        if not block:
            continue
        tags = dict(TAG.findall(block))
        # Move section is after the blank line that follows the header.
        parts = block.split('\n\n', 1)
        moves_text = parts[1] if len(parts) > 1 else ''
        games.append({'tags': tags, 'moves_text': moves_text})
    return games


def detect_engines(games, new_override, base_override):
    names = Counter()
    for g in games:
        for k in ('White', 'Black'):
            if g['tags'].get(k):
                names[g['tags'][k]] += 1
    names = list(names)
    if new_override and base_override:
        return new_override, base_override
    new = new_override or next((n for n in names if 'new' in n.lower()), None)
    if new is None:
        # fall back to first name
        new = names[0] if names else 'NEW'
    base = base_override or next((n for n in names if n != new), 'BASE')
    return new, base


def final_reason(moves_text, result):
    """Classify how a game ended from its final comment / result."""
    m = list(MOVE_COMMENT.finditer(moves_text))
    last_comment = m[-1].group(2) if m else ''
    lc = last_comment.lower()
    if 'mates' in lc:
        return 'mate'
    if '3-fold' in lc or 'repetition' in lc:
        return 'repetition'
    if 'fifty' in lc:
        return 'fifty-move'
    if 'insufficient' in lc:
        return 'insufficient-material'
    if 'stalemate' in lc:
        return 'stalemate'
    if 'adjudic' in lc:
        return 'adjudication'
    # No annotated reason: rely on Termination tag (set by caller) — return unknown.
    return 'other'


def parse_evals(moves_text):
    """Return list of (mover, cp) where mover in {'W','B'} and cp is from mover POV.

    Mate scores map to +/-100000 minus distance so they sort correctly.
    Side to move alternates W,B,... starting from whoever moves first. We infer the
    first mover from the move-number token ("1." => White, "1..." => Black to start).
    """
    # Determine first mover: a leading "N..." means black starts (set-up position).
    first_black = bool(re.match(r'\s*\d+\.\.\.', moves_text))
    evals = []
    mover = 'B' if first_black else 'W'
    for mc in MOVE_COMMENT.finditer(moves_text):
        comment = mc.group(1) is not None and mc.group(2) or ''
        s = SCORE.search(comment)
        if s:
            sign, mate, val, depth = s.groups()
            if mate:
                cp = 100000 - int(float(val))
                cp = -cp if sign == '-' else cp
            else:
                cp = int(round(float(val) * 100))
                cp = -cp if sign == '-' else cp
            evals.append((mover, cp))
        mover = 'B' if mover == 'W' else 'W'
    return evals


def new_points(g, new):
    r = g['tags'].get('Result')
    if r == '1/2-1/2':
        return 0.5
    if g['tags'].get('White') == new:
        return 1.0 if r == '1-0' else 0.0
    return 1.0 if r == '0-1' else 0.0


def elo_from_score(p):
    if p <= 0 or p >= 1:
        return float('inf') if p >= 1 else float('-inf')
    return -400 * math.log10(1 / p - 1)


def analyze(path, new_override, base_override):
    text = open(path).read()
    games = [g for g in parse_games(text) if g['tags'].get('Result') in ('1-0', '0-1', '1/2-1/2')]
    new, base = detect_engines(games, new_override, base_override)

    n = len(games)
    pts = sum(new_points(g, new) for g in games)
    wdl = Counter()
    color_wdl = {'white': Counter(), 'black': Counter()}
    # cause split by who lost (or draw)
    cause_when_new_loses = Counter()
    cause_when_base_loses = Counter()
    cause_draw = Counter()
    timeouts = Counter()
    plies = {'new_loss': [], 'new_win': [], 'draw': []}
    # conversion / throw diagnostics from evals (decisive games only)
    new_threw = 0    # new had a winning eval (>= +200) yet did not win
    new_robbed = 0   # new had a losing eval (<= -200) yet did not lose (opponent threw)
    max_swing_against = []  # biggest single-move eval drop suffered by the loser

    byopen = defaultdict(lambda: [0.0, 0])

    for g in games:
        r = g['tags']['Result']
        p = new_points(g, new)
        key = 'W' if p == 1 else ('L' if p == 0 else 'D')
        wdl[key] += 1
        new_color = 'white' if g['tags'].get('White') == new else 'black'
        color_wdl[new_color][key] += 1

        fen = g['tags'].get('FEN', '?')
        byopen[fen][0] += p
        byopen[fen][1] += 1

        term = g['tags'].get('Termination', '')
        reason = final_reason(g['moves_text'], r)
        if term == 'time forfeit':
            reason = 'time forfeit'
            loser_color = 'white' if r == '0-1' else 'black' if r == '1-0' else None
            if loser_color:
                loser = g['tags'].get('White' if loser_color == 'white' else 'Black')
                timeouts[loser] += 1

        ply = int(g['tags'].get('PlyCount') or 0)
        if r == '1/2-1/2':
            cause_draw[reason] += 1
            plies['draw'].append(ply)
        else:
            if p == 0.0:
                cause_when_new_loses[reason] += 1
                plies['new_loss'].append(ply)
            else:
                cause_when_base_loses[reason] += 1
                plies['new_win'].append(ply)

        # eval diagnostics
        evals = parse_evals(g['moves_text'])
        new_side = 'W' if new_color == 'white' else 'B'
        new_evals = [cp for (mv, cp) in evals if mv == new_side]
        if new_evals:
            if max(new_evals) >= 200 and p < 1.0:
                new_threw += 1
            if min(new_evals) <= -200 and p > 0.0:
                new_robbed += 1
        # biggest swing against the eventual loser (consecutive same-side evals),
        # ignoring transitions into/out of mate scores (|cp| >= 30000) which are not
        # comparable cp deltas.
        if r != '1/2-1/2':
            loser_side = 'W' if r == '0-1' else 'B'
            loser_evals = [cp for (mv, cp) in evals if mv == loser_side]
            worst = 0
            for a, b in zip(loser_evals, loser_evals[1:]):
                if abs(a) >= 30000 or abs(b) >= 30000:
                    continue
                worst = min(worst, b - a)
            if worst < 0:
                max_swing_against.append(-worst)

    return {
        'path': path, 'new': new, 'base': base, 'n': n, 'pts': pts,
        'score_pct': 100 * pts / n if n else 0,
        'elo': elo_from_score(pts / n) if n else 0,
        'wdl': wdl, 'color_wdl': color_wdl,
        'cause_new_loss': cause_when_new_loses,
        'cause_base_loss': cause_when_base_loses,
        'cause_draw': cause_draw,
        'timeouts': timeouts, 'plies': plies,
        'new_threw': new_threw, 'new_robbed': new_robbed,
        'max_swing_against': max_swing_against,
        'byopen': byopen, 'games': games,
    }


def pentanomial(games, new):
    by_round = defaultdict(list)
    for g in games:
        by_round[g['tags'].get('Round')].append(g)
    penta = Counter()
    pairs = 0
    for rnd, gs in by_round.items():
        if len(gs) != 2:
            continue
        pairs += 1
        s = sum(new_points(g, new) for g in gs)
        penta[s] += 1
    return penta, pairs


def md_table(headers, rows):
    out = ['| ' + ' | '.join(headers) + ' |',
           '|' + '|'.join('---' for _ in headers) + '|']
    for r in rows:
        out.append('| ' + ' | '.join(str(c) for c in r) + ' |')
    return '\n'.join(out)


def report(a):
    L = []
    P = L.append
    P(f"# SPRT summary: `{a['path']}`\n")
    P(f"**{a['new']}** (new) vs **{a['base']}** (base) — all numbers from new's perspective.\n")
    w, d, l = a['wdl']['W'], a['wdl']['D'], a['wdl']['L']
    P(md_table(
        ['Games', 'W', 'D', 'L', 'Points', 'Score %', 'Elo (pt est.)', 'Draw %'],
        [[a['n'], w, d, l, f"{a['pts']:.1f}", f"{a['score_pct']:.2f}",
          f"{a['elo']:+.1f}", f"{100*d/a['n']:.1f}" if a['n'] else 0]]))
    P('')

    penta, pairs = pentanomial(a['games'], a['new'])
    P("## Pentanomial (new pts per reversed-color pair)\n")
    P(md_table(['0', '0.5', '1.0', '1.5', '2.0', 'pairs'],
               [[penta.get(0.0, 0), penta.get(0.5, 0), penta.get(1.0, 0),
                 penta.get(1.5, 0), penta.get(2.0, 0), pairs]]))
    lossish = penta.get(0.0, 0) + penta.get(0.5, 0)
    winish = penta.get(2.0, 0) + penta.get(1.5, 0)
    P(f"\nLoss-leaning pairs: {lossish}  vs  win-leaning pairs: {winish}  "
      f"(ratio {winish/lossish:.2f})" if lossish else "")
    P('')

    P("## New engine by color\n")
    rows = []
    for color in ('white', 'black'):
        c = a['color_wdl'][color]
        tot = sum(c.values()) or 1
        net = c['W'] - c['L']
        rows.append([color, c['W'], c['D'], c['L'],
                     f"{100*(c['W']+0.5*c['D'])/tot:.1f}", f"{net:+d}"])
    P(md_table(['Color', 'W', 'D', 'L', 'Score %', 'Net'], rows))
    P('')

    P("## WDL cause breakdown\n")
    causes = sorted(set(a['cause_new_loss']) | set(a['cause_base_loss']) | set(a['cause_draw']))
    rows = []
    for c in causes:
        rows.append([c, a['cause_base_loss'].get(c, 0), a['cause_new_loss'].get(c, 0),
                     a['cause_draw'].get(c, 0)])
    rows.append(['**total**',
                 sum(a['cause_base_loss'].values()),
                 sum(a['cause_new_loss'].values()),
                 sum(a['cause_draw'].values())])
    P(md_table(['Cause', 'new WINS by', 'new LOSES by', 'draws by'], rows))
    P("\n*(\"new WINS by\" = base lost by that cause; \"new LOSES by\" = new lost by that cause.)*\n")

    P("## Timeouts / crashes\n")
    if a['timeouts']:
        P(md_table(['Engine', 'time forfeits'],
                   [[k, v] for k, v in sorted(a['timeouts'].items())]))
    else:
        P("No time forfeits.")
    P('')

    P("## Eval-based diagnostics (decisive games)\n")
    swings = a['max_swing_against']
    P(md_table(
        ['new threw (won eval, no win)', 'new robbed (lost eval, no loss)',
         'median loser swing (cp)', 'p90 loser swing (cp)'],
        [[a['new_threw'], a['new_robbed'],
          int(st.median(swings)) if swings else 0,
          int(sorted(swings)[int(0.9*len(swings))]) if swings else 0]]))
    P("\n*threw/robbed use a +/-200cp self-eval threshold; swing = biggest single-move "
      "self-eval drop suffered by the losing side.*\n")

    P("## Per-opening (new score %, worst first)\n")
    rows = []
    for fen, (pp, cc) in sorted(a['byopen'].items(), key=lambda kv: kv[1][0]/kv[1][1]):
        rows.append([f"{100*pp/cc:.1f}", cc, fen])
    P(md_table(['new %', 'n', 'FEN'], rows))
    P('')

    P("## Game length (plies)\n")
    rows = []
    for k in ('new_win', 'new_loss', 'draw'):
        v = a['plies'][k]
        if v:
            rows.append([k, len(v), f"{st.mean(v):.0f}", st.median(v), min(v), max(v)])
    P(md_table(['bucket', 'n', 'mean', 'median', 'min', 'max'], rows))
    P('')
    return '\n'.join(L)


def delta_table(analyses):
    """Side-by-side cause breakdown across multiple PGNs."""
    L = ["## Cross-file comparison (cause breakdown, new's perspective)\n"]
    causes = sorted({c for a in analyses
                     for c in set(a['cause_new_loss']) | set(a['cause_base_loss']) | set(a['cause_draw'])})
    headers = ['metric'] + [a['path'].split('/')[-1] for a in analyses]
    rows = []
    rows.append(['score %'] + [f"{a['score_pct']:.2f}" for a in analyses])
    rows.append(['Elo est.'] + [f"{a['elo']:+.1f}" for a in analyses])
    rows.append(['draw %'] + [f"{100*a['wdl']['D']/a['n']:.1f}" for a in analyses])
    rows.append(['new W / L'] + [f"{a['wdl']['W']} / {a['wdl']['L']}" for a in analyses])
    for c in causes:
        rows.append([f"new LOSES by {c}"] + [a['cause_new_loss'].get(c, 0) for a in analyses])
    for c in causes:
        rows.append([f"new WINS by {c}"] + [a['cause_base_loss'].get(c, 0) for a in analyses])
    rows.append(['new threw'] + [a['new_threw'] for a in analyses])
    rows.append(['new robbed'] + [a['new_robbed'] for a in analyses])
    L.append(md_table(headers, rows))
    return '\n'.join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pgns', nargs='+')
    ap.add_argument('--new')
    ap.add_argument('--base')
    args = ap.parse_args()
    analyses = []
    for p in args.pgns:
        a = analyze(p, args.new, args.base)
        analyses.append(a)
        print(report(a))
        print('\n---\n')
    if len(analyses) > 1:
        print(delta_table(analyses))


if __name__ == '__main__':
    main()
