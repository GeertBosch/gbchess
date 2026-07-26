#!/usr/bin/env python3
"""Summarize fastchess SPRT PGN output for human + LLM analysis.

Usage:
    tools/sprt_summary.py build/sprt-YYYYMMDD-HHMMSS.pgn [more.pgn ...]
    tools/sprt_summary.py --new gbchess-new --base gbchess-base FILE.pgn

It opens with a plain-language verdict — accepted / rejected / which way it is leaning, how
many more games a decision would take and how long that is at the observed pace — computed
from the pentanomial pair statistics. It reproduces the numbers fast-chess prints when a match
ends: Elo, normalized Elo (nElo), likelihood of superiority (LOS) and the sequential test's
log-likelihood ratio (LLR). The bounds come from the sidecar test/sprt.sh writes, or from
--elo0/--elo1/--alpha/--beta. Like fast-chess, elo0/elo1 are read as *normalized* Elo, not
logistic Elo, so the default elo1=5 asks for roughly 2 ordinary Elo.

For each PGN it then prints: match summary, pentanomial, per-color and per-opening scores,
the WDL cause breakdown (mate / 3-fold / fifty-move / insufficient material / stalemate /
time forfeit / crash) by engine, then the decisive games split by colour and by first mover
as percentages against a 50/50 neutral baseline, conversion & "throw" diagnostics from the
in-PGN engine evals, and game-length stats.

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
from datetime import datetime

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


def sidecar(path):
    """Read the run metadata test/sprt.sh wrote next to the PGN, if present."""
    try:
        with open(re.sub(r'\.pgn$', '', path) + '.engines') as f:
            return dict(line.strip().split('=', 1) for line in f if '=' in line)
    except OSError:
        return {}


def detect_engines(games, new_override, base_override):
    names = Counter()
    for g in games:
        for k in ('White', 'Black'):
            if g['tags'].get(k):
                names[g['tags'][k]] += 1
    names = list(names)
    if new_override and base_override:
        return new_override, base_override
    # Engine names are derived from the binary + options, so they no longer reliably
    # contain "new"; the caller passes the sidecar value in as new_override.
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


def color_split(games, new, base):
    """Each engine's score with each colour.

    The difference column is an algebraic identity in any clean head-to-head, not a
    measurement: the games where new plays white are exactly the games where base plays
    black, so new_white + base_black = 100 and both differences equal twice the overall edge.
    That holds even when games are unpaired. It is printed because the score *levels* show
    the opening set's colour bias, and because a disagreement between the two rows means the
    file is not a clean head-to-head at all.
    """
    out = {}
    for color in ('white', 'black'):
        tag = color.capitalize()
        # Each engine's own games with this colour. Scoring base from its own games rather
        # than as new's complement is what gives the check something to catch: in a clean
        # head-to-head the two are the same games, so any disagreement means the file is not
        # one — concatenated matches, a third engine, or the wrong --new/--base names.
        as_new = [g for g in games if g['tags'].get(tag) == new]
        as_base = [g for g in games if g['tags'].get(tag) == base]
        if not as_new or not as_base:
            return None
        new_pct = 100 * sum(new_points(g, new) for g in as_new) / len(as_new)
        base_pct = 100 * sum(new_points(g, base) for g in as_base) / len(as_base)
        out[color] = (new_pct, base_pct, new_pct - base_pct, len(as_new))
    return out


def opening_stats(games, new):
    """Per-opening score, length and information content.

    An opening whose pairs always come out balanced cannot separate the engines however long
    it runs: a mate the engines both find, or a dead draw, scores 1.0 out of 2 every time.
    Those are worth spotting because they inflate the game count and shrink the pair spread
    that normalized Elo is measured against.
    """
    by_fen = defaultdict(list)
    for g in games:
        by_fen[g['tags'].get('FEN', 'start position')].append(g)
    stats = []
    for fen, gs in by_fen.items():
        rounds = defaultdict(list)
        for g in gs:
            rounds[g['tags'].get('Round')].append(new_points(g, new))
        pair_scores = [sum(v) / 2 for v in rounds.values() if len(v) == 2]
        balanced = sum(1 for s in pair_scores if s == 0.5)
        sd = st.pstdev(pair_scores) if len(pair_scores) > 1 else 0.0
        plies = [int(g['tags'].get('PlyCount') or 0) for g in gs]
        stats.append({
            'fen': fen, 'games': len(gs), 'pairs': len(pair_scores),
            'score': 100 * sum(new_points(g, new) for g in gs) / len(gs),
            'median_plies': st.median(plies) if plies else 0,
            'se': 100 * sd / math.sqrt(len(pair_scores)) if pair_scores else 0.0,
            'balanced_pct': 100 * balanced / len(pair_scores) if pair_scores else 0.0,
        })
    return stats


def warnings_for(stats, games):
    """Things that make a run less informative than its game count suggests."""
    out = []
    total = sum(s['games'] for s in stats) or 1
    dead = [s for s in stats if s['balanced_pct'] >= 95]
    if dead:
        share = 100 * sum(s['games'] for s in dead) / total
        out.append(f"**{len(dead)} of {len(stats)} openings carry no information** "
                   f"({share:.0f}% of games): every pair comes out balanced, so they cannot "
                   f"separate the engines however long the run continues. Drop them from the "
                   f"openings file. They also shrink the pair spread, which inflates "
                   f"normalized Elo and makes the SPRT bound easier to reach than it looks.")
    short = sum(1 for g in games if int(g['tags'].get('PlyCount') or 0) <= 10)
    if short > 0.1 * len(games):
        out.append(f"**{100*short/len(games):.0f}% of games last 10 plies or fewer** — decided "
                   f"in the opening position rather than by play. Fine if intended, but the "
                   f"game count overstates how much was actually tested.")
    if len(stats) < 5:
        what = ("a single opening" if len(stats) == 1
                else f"only {len(stats)} distinct openings")
        out.append(f"**The run uses {what}** — results lean heavily on the character of those "
                   f"positions and may not generalise. Note that games from one opening are "
                   f"still independent as long as the engines vary their play.")
    return out


def wall_clock_rate(games):
    """Games actually completed per second, from the PGN timestamps.

    Measured as games divided by the span from the first start to the last end, so it already
    accounts for whatever --concurrency the run used and for machine load.
    """
    stamps = []
    for g in games:
        for tag in ('GameStartTime', 'GameEndTime'):
            v = g['tags'].get(tag)
            if v:
                try:
                    stamps.append(datetime.strptime(v, '%Y-%m-%dT%H:%M:%S %z'))
                except ValueError:
                    pass
    if len(stamps) < 2:
        return None
    span = (max(stamps) - min(stamps)).total_seconds()
    return len(games) / span if span > 0 else None


def format_duration(seconds):
    if seconds < 90:
        return f"{seconds:.0f} seconds"
    if seconds < 90 * 60:
        return f"{seconds/60:.0f} minutes"
    if seconds < 48 * 3600:
        return f"{seconds/3600:.1f} hours"
    return f"{seconds/86400:.1f} days"


def first_mover(g):
    """Colour that moved first: the FEN tag's side to move, else white (start position)."""
    parts = g['tags'].get('FEN', '').split()
    return 'black' if len(parts) > 1 and parts[1] == 'b' else 'white'


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


def analyze(path, new_override, base_override, bounds=None):
    text = open(path).read()
    games = [g for g in parse_games(text) if g['tags'].get('Result') in ('1-0', '0-1', '1/2-1/2')]
    meta = sidecar(path)
    new, base = detect_engines(games, new_override or meta.get('new'),
                               base_override or meta.get('base'))
    # SPRT bounds: explicit flags win, then whatever test/sprt.sh recorded, then its defaults.
    bounds = bounds or {}
    limits = {k: float(bounds.get(k) if bounds.get(k) is not None else meta.get(k, d))
              for k, d in (('elo0', 0), ('elo1', 5), ('alpha', 0.05), ('beta', 0.05))}

    n = len(games)
    pts = sum(new_points(g, new) for g in games)
    wdl = Counter()
    color_wdl = {'white': Counter(), 'black': Counter()}
    # cause split by who lost (or draw)
    cause_when_new_loses = Counter()
    cause_when_base_loses = Counter()
    cause_draw = Counter()
    # same causes, split by the colour that lost rather than by engine
    cause_by_loser_color = {'white': Counter(), 'black': Counter()}
    # ... and by whether the loser was the side that moved first in the game. Time
    # forfeits track this rather than colour: the first mover spends from a budget that
    # is a fraction of its own remaining clock before its opponent does, so its clock
    # sits below the opponent's at every ply and it hits the flag threshold first.
    cause_by_loser_role = {'first': Counter(), 'second': Counter()}
    first_movers = Counter()
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

        first_movers[first_mover(g)] += 1

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
            loser_color = 'white' if r == '0-1' else 'black'
            cause_by_loser_color[loser_color][reason] += 1
            role = 'first' if loser_color == first_mover(g) else 'second'
            cause_by_loser_role[role][reason] += 1
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
        'cause_by_loser_color': cause_by_loser_color,
        'cause_by_loser_role': cause_by_loser_role,
        'first_movers': first_movers,
        'timeouts': timeouts, 'plies': plies,
        'new_threw': new_threw, 'new_robbed': new_robbed,
        'max_swing_against': max_swing_against,
        'byopen': byopen, 'games': games, 'rate': wall_clock_rate(games),
        'color_split': color_split(games, new, base),
        'openings': opening_stats(games, new),
        'sprt': sprt_stats(*pentanomial(games, new), limits['elo0'], limits['elo1'],
                           limits['alpha'], limits['beta']),
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


# Normalized Elo scale factor: nElo = kNormalizedElo * (mean - 0.5) / pair_stdev.
# fast-chess (like fishtest) interprets -sprt elo0/elo1 on this scale by default, not on the
# logistic one, so the hypotheses have to be converted through the observed pair variance.
kNormalizedElo = 400 * math.sqrt(2) / math.log(10)


def phi(x):
    """Standard normal CDF."""
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def sprt_stats(penta, pairs, elo0, elo1, alpha, beta):
    """Pentanomial SPRT statistics: Elo with a 95% interval, LOS, and the GSPRT LLR.

    Pairs (the same opening played with both colours) are the independent observations;
    using them instead of individual games removes the correlation between the two games
    of a pair, which is what makes the interval honest. Each pair scores 0, 0.5, 1, 1.5 or
    2 points, normalised here to a [0, 1] score fraction.
    """
    if not pairs:
        return None
    mean = sum(pts / 2.0 * n for pts, n in penta.items()) / pairs
    var = sum(n * (pts / 2.0 - mean) ** 2 for pts, n in penta.items()) / pairs
    sigma = math.sqrt(var / pairs) if var > 0 else 0.0  # standard error of the mean

    elo = elo_from_score(mean)
    if sigma > 0:
        lo, hi = mean - 1.96 * sigma, mean + 1.96 * sigma
        elo_lo = elo_from_score(min(max(lo, 1e-9), 1 - 1e-9))
        elo_hi = elo_from_score(min(max(hi, 1e-9), 1 - 1e-9))
        los = phi((mean - 0.5) / sigma)
    else:
        elo_lo = elo_hi = elo
        los = 0.5

    # Generalised SPRT, matching fast-chess: the bounds are normalized Elo, so convert them to
    # score means through the observed pair spread, then LLR = pairs/2 * log(var0 / var1) where
    # var_i is the variance under hypothesis i.
    sd = math.sqrt(var) if var > 0 else 0.0
    nelo = kNormalizedElo * (mean - 0.5) / sd if sd else 0.0
    nelo_err = kNormalizedElo * 1.96 * sigma / sd if sd else 0.0
    s0 = 0.5 + elo0 * sd / kNormalizedElo
    s1 = 0.5 + elo1 * sd / kNormalizedElo
    if var > 0:
        llr = pairs / 2 * math.log((var + (mean - s0) ** 2) / (var + (mean - s1) ** 2))
    else:
        llr = 0.0
    upper = math.log((1 - beta) / alpha)
    lower = math.log(beta / (1 - alpha))
    return {'mean': mean, 'sigma': sigma, 'elo': elo, 'elo_lo': elo_lo, 'elo_hi': elo_hi,
            'nelo': nelo, 'nelo_err': nelo_err, 'los': los, 'llr': llr,
            'upper': upper, 'lower': lower, 'pairs': pairs, 'elo0': elo0, 'elo1': elo1,
            # the H1 bound expressed as ordinary Elo, which is what the prose quotes
            'elo1_logistic': elo_from_score(s1)}


def verdict(s, rate=None):
    """One-paragraph plain-language reading of the SPRT state.

    `rate` is games completed per second, used to turn "games remaining" into wall-clock time.
    """
    if s is None:
        return "Not enough paired games to judge the result yet.\n"
    elo, lo, hi = s['elo'], s['elo_lo'], s['elo_hi']
    games = 2 * s['pairs']
    band = (f"{elo:+.1f} Elo (95% confidence interval {lo:+.1f} to {hi:+.1f})")
    size = 'large' if abs(elo) >= 15 else 'small'

    if s['llr'] >= s['upper']:
        return (f"**Accepted: the change gains Elo** — a {size} gain of {band} over {games} games. "
                f"The test crossed its upper bound, so the gain is as real as this test can "
                f"establish (5% false-positive rate by design). Nothing more to run.\n")
    if s['llr'] <= s['lower']:
        return (f"**Rejected: no gain of {s['elo1_logistic']:+.1f} Elo** — measured {band} over {games} "
                f"games. The test crossed its lower bound. This does not prove the change is "
                f"harmful, only that it does not deliver the improvement being tested for.\n")

    # Still running: describe the trend and what it would take to finish.
    progress = 100 * abs(s['llr']) / (s['upper'] if s['llr'] >= 0 else -s['lower'])
    heading = 'accept' if s['llr'] >= 0 else 'reject'
    target = s['upper'] if s['llr'] >= 0 else s['lower']
    more = int(games * (target - s['llr']) / s['llr']) if abs(s['llr']) > 1e-9 else 0
    if 0 < more < 10_000_000:
        eta = f"at the current rate roughly {more:,} more games would {heading} it"
        if rate:
            eta += f", about {format_duration(more / rate)} of play at the observed pace"
    elif abs(s['llr']) > 1e-9:
        eta = "the trend is too weak to project a finish"
    else:
        eta = "there is no trend yet to project a finish from"

    if s['los'] > 0.95:
        lead = f"**Probably a gain, not yet proven** — {band}, {100*s['los']:.0f}% likely positive."
    elif s['los'] < 0.05:
        lead = (f"**Probably a regression, not yet proven** — {band}, "
                f"{100*(1-s['los']):.0f}% likely negative.")
    elif hi - lo < 8 and abs(elo) < 2:
        lead = (f"**Likely no meaningful change** — {band}. The interval is already tight enough "
                f"to rule out anything larger.")
    else:
        lead = f"**Inconclusive so far** — {band}, {100*s['los']:.0f}% likely positive."

    return (f"{lead} After {games:,} games the test is {progress:.0f}% of the way to a decision "
            f"(testing the null hypothesis of {s['elo0']:+.0f} against {s['elo1']:+.0f} "
            f"normalized Elo, i.e. is the gain at least {s['elo1_logistic']:+.1f} Elo); "
            f"{eta}.\n")


# Causes that can only end a game one way: a draw by rule never has a loser, and a mate or a
# flag never ends in a draw. Those cells get a dash rather than a 0, which would leave the
# reader wondering whether it could have been non-zero.
DRAW_ONLY_CAUSES = {'fifty-move', 'insufficient-material', 'repetition', 'stalemate'}
DECISIVE_ONLY_CAUSES = {'mate', 'time forfeit'}
NA = '—'


def split_rows(left, right):
    """Rows splitting each cause between two roles that occur exactly once per game.

    Percentages, because the counts alone invite comparison against the wrong baseline: both
    roles occur in every game, so 50/50 is neutral no matter how the openings are distributed.
    The game count stays as its own column so a 60% built from ten games is not mistaken for
    a real effect.
    """
    rows = []
    for c in sorted(set(left) | set(right)):
        l, r = left.get(c, 0), right.get(c, 0)
        if not l + r:
            continue
        rows.append([c, l + r, f"{100*l/(l+r):.0f}%", f"{100*r/(l+r):.0f}%"])
    l, r = sum(left.values()), sum(right.values())
    if l + r:
        rows.append(['**all decisive**', f"**{l+r}**",
                     f"**{100*l/(l+r):.0f}%**", f"**{100*r/(l+r):.0f}%**"])
    return rows


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
    P(verdict(a['sprt'], a['rate']))
    if a['sprt']:
        s = a['sprt']
        P(f"<sub>Log-likelihood ratio (LLR) {s['llr']:+.2f} within its stopping bounds "
          f"({s['lower']:.2f}, {s['upper']:.2f}); likelihood of superiority (LOS) "
          f"{100*s['los']:.1f}%; normalized Elo {s['nelo']:+.2f} +/- {s['nelo_err']:.2f}; "
          f"{s['pairs']} paired games.</sub>\n")

    warn = warnings_for(a['openings'], a['games'])
    if warn:
        P("## Warnings\n")
        for w in warn:
            P(f"> {w}\n")
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

    P("## Each engine by colour\n")
    cs = a['color_split']
    if cs:
        rows = [[c, f"{v[0]:.2f}%", f"{v[1]:.2f}%", f"{v[2]:+.2f}pp", v[3]]
                for c, v in cs.items()]
        P(md_table(['new plays', a['new'], a['base'], 'difference', 'games'], rows))
        gap = abs(cs['white'][2] - cs['black'][2])
        note = ("\n*In a clean head-to-head the two differences are the same number by "
                "construction — the games where new has white are the games where base has "
                "black — and both equal twice the overall edge. So this is a consistency "
                "check on the file, not a measurement of colour strength; a per-colour "
                "weakness cannot be seen this way. The score levels themselves reflect the "
                "opening set: ")
        white_all = 100 * sum(1.0 if g['tags']['Result'] == '1-0' else
                              0.5 if g['tags']['Result'] == '1/2-1/2' else 0.0
                              for g in a['games']) / (a['n'] or 1)
        note += f"white scores {white_all:.1f}% across all games here.*\n"
        P(note)
        if gap > 0.5:
            P(f"\n> ⚠️ The two differences disagree by {gap:.2f}pp when they are equal in any "
              f"clean head-to-head. This file is probably not one: matches concatenated, a "
              f"third engine present, or --new/--base naming the wrong players.\n")
    else:
        P("Not enough games with each colour to split.\n")
    P('')

    P("## WDL cause breakdown\n")
    causes = sorted(set(a['cause_new_loss']) | set(a['cause_base_loss']) | set(a['cause_draw']))
    rows = []
    for c in causes:
        rows.append([c,
                     NA if c in DRAW_ONLY_CAUSES else a['cause_base_loss'].get(c, 0),
                     NA if c in DRAW_ONLY_CAUSES else a['cause_new_loss'].get(c, 0),
                     NA if c in DECISIVE_ONLY_CAUSES else a['cause_draw'].get(c, 0)])
    rows.append(['**total**',
                 sum(a['cause_base_loss'].values()),
                 sum(a['cause_new_loss'].values()),
                 sum(a['cause_draw'].values())])
    P(md_table(['Cause', 'new WINS by', 'new LOSES by', 'draws by'], rows))
    P("\n*(\"new WINS by\" = base lost by that cause; \"new LOSES by\" = new lost by that cause. "
      "A dash marks a combination the rules make impossible: a draw by rule has no loser, and a "
      "mate or a flag is never a draw.)*\n")

    P("## Decisive games by colour\n")
    P(md_table(['Cause', 'games', 'white lost', 'black lost'],
               split_rows(a['cause_by_loser_color']['white'], a['cause_by_loser_color']['black'])))
    P("\n*Every game has one player of each colour, so 50% is the neutral split. Draws are "
      "excluded here; they are in the table above.*\n")

    P("## Decisive games by first mover\n")
    P(md_table(['Cause', 'games', 'first mover lost', 'second mover lost'],
               split_rows(a['cause_by_loser_role']['first'], a['cause_by_loser_role']['second'])))
    fm = a['first_movers']
    total_fm = fm['white'] + fm['black'] or 1
    P(f"\n*Every game has exactly one first mover, so 50% is the neutral split here too — the "
      f"colour of that first mover ({100*fm['white']/total_fm:.0f}% white, "
      f"{100*fm['black']/total_fm:.0f}% black across the openings used) does not move that "
      f"baseline. A heavy skew of time forfeits toward the first mover means an engine is "
      f"budgeting a fraction of its own remaining clock with nothing reserved for per-move "
      f"overhead: the first mover spends before its opponent every cycle, so it reaches the "
      f"flag threshold first. With MoveOverhead reserved the forfeits should spread evenly.*\n")

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

    P("## Openings (new score %, worst first)\n")
    rows = []
    for o in sorted(a['openings'], key=lambda o: o['score']):
        if o['balanced_pct'] >= 95:
            note = 'no information: every pair balanced'
        elif o['median_plies'] <= 10:
            note = 'decided in the opening'
        else:
            note = ''
        rows.append([f"{o['score']:.1f}", o['games'], f"{o['median_plies']:.0f}",
                     f"±{o['se']:.2f}", f"{o['balanced_pct']:.0f}%", note, o['fen']])
    P(md_table(['new %', 'games', 'median plies', 'std err', 'balanced pairs', 'note', 'FEN'],
               rows))
    P("\n*\"Balanced pairs\" is how often the two colour-reversed games cancel out. At 100% the "
      "opening cannot separate the engines at all — both sides find the same mate, or the "
      "position is a dead draw — so those games are pure filler. The standard error is per "
      "opening, from its own pairs.*\n")

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
    # Draw-only causes have no winner or loser at all, so those rows would be all zeros.
    decisive = [c for c in causes if c not in DRAW_ONLY_CAUSES]
    for c in decisive:
        rows.append([f"new LOSES by {c}"] + [a['cause_new_loss'].get(c, 0) for a in analyses])
    for c in decisive:
        rows.append([f"new WINS by {c}"] + [a['cause_base_loss'].get(c, 0) for a in analyses])
    for c in causes:
        if c in DRAW_ONLY_CAUSES:
            rows.append([f"draws by {c}"] + [a['cause_draw'].get(c, 0) for a in analyses])
    rows.append(['new threw'] + [a['new_threw'] for a in analyses])
    rows.append(['new robbed'] + [a['new_robbed'] for a in analyses])
    L.append(md_table(headers, rows))
    return '\n'.join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pgns', nargs='+')
    ap.add_argument('--new')
    ap.add_argument('--base')
    ap.add_argument('--elo0', type=float, help='SPRT H0 (default: from the run, else 0)')
    ap.add_argument('--elo1', type=float, help='SPRT H1 (default: from the run, else 5)')
    ap.add_argument('--alpha', type=float, help='SPRT alpha (default: from the run, else 0.05)')
    ap.add_argument('--beta', type=float, help='SPRT beta (default: from the run, else 0.05)')
    args = ap.parse_args()
    analyses = []
    for p in args.pgns:
        a = analyze(p, args.new, args.base,
                    {'elo0': args.elo0, 'elo1': args.elo1,
                     'alpha': args.alpha, 'beta': args.beta})
        analyses.append(a)
        print(report(a))
        print('\n---\n')
    if len(analyses) > 1:
        print(delta_table(analyses))


if __name__ == '__main__':
    main()
