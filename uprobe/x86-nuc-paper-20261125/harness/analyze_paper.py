#!/usr/bin/env python3
"""Summarize run_paper.py rows (plan 2.0.3): paired log-ratio GMR vs K with 95% t-CI,
paired extra time per request vs B (mean, t-CI), audit columns, TOST helper.
Usage: analyze_paper.py <rows.jsonl>... [--exp E] [--payload N] [--ref K] [--tost legA:legB:margin ...]
Only valid rows are used; a rep is used for a leg pair only if both legs are valid in it.
"""
import argparse, collections, json, math, statistics, sys

T975 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262,
        10: 2.228, 11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131, 16: 2.120, 17: 2.110,
        18: 2.101, 19: 2.093, 20: 2.086, 24: 2.064, 29: 2.045, 39: 2.023, 59: 2.001}
T95 = {1: 6.314, 2: 2.920, 3: 2.353, 4: 2.132, 5: 2.015, 6: 1.943, 7: 1.895, 8: 1.860, 9: 1.833,
       10: 1.812, 11: 1.796, 12: 1.782, 13: 1.771, 14: 1.761, 15: 1.753, 16: 1.746, 17: 1.740,
       18: 1.734, 19: 1.729, 20: 1.725, 24: 1.711, 29: 1.699, 39: 1.685, 59: 1.671}


def tq(table, df):
    if df <= 0:
        return float('nan')
    keys = sorted(table)
    for k in keys:
        if df <= k:
            return table[k]
    return 1.96 if table is T975 else 1.645


def mean_ci(xs, table=T975):
    n = len(xs)
    if n < 2:
        return (xs[0] if xs else float('nan')), float('nan'), float('nan')
    m = statistics.fmean(xs)
    h = tq(table, n - 1) * statistics.stdev(xs) / math.sqrt(n)
    return m, m - h, m + h


NONMEASURED = ('0', '1', '3', '5', '7', '15')  # PREREG-2 §6
NOISY = 0.05


def load(paths, exp, payload, warmup=False):
    rows = []
    for p in paths:
        for line in open(p):
            r = json.loads(line)
            if r.get('warmup') and not warmup:
                continue
            if exp and r.get('exp') != exp:
                continue
            if payload and r.get('payload') != payload:
                continue
            rows.append(r)
    return rows


def index(rows):
    by = collections.defaultdict(dict)  # (session, round, rep) -> leg -> row
    for r in rows:
        if r.get('valid'):
            by[(r['session_id'], r['round'], r['rep'])][r['leg']] = r
    return by


def paired(by, a, b, f):
    out = []
    for key, legs in by.items():
        if a in legs and b in legs:
            v = f(legs[a], legs[b])
            if v is not None:
                out.append(v)
    return out


def ref_for(leg, legs):
    """K (or K#policy) with the same policy as leg; B likewise."""
    pol = leg.split('#', 1)[1].split('+')[0] if '#' in leg else None
    k = f'K#{pol}' if pol and f'K#{pol}' in legs else 'K'
    b = f'B#{pol}' if pol and f'B#{pol}' in legs else 'B'
    return k, b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rows', nargs='+')
    ap.add_argument('--exp')
    ap.add_argument('--payload', type=int)
    ap.add_argument('--tost', nargs='*', default=[], help='legA:legB:margin (fraction), equivalence of rps')
    ap.add_argument('--diff', nargs='*', default=[], help='legA:legB, paired extra-time difference x(A)-x(B) in us/request')
    ap.add_argument('--ratio', nargs='*', default=[], help='legA:legB, paired GMR of rps with 95%% t-CI')
    ap.add_argument('--tostus', nargs='*', default=[], help='legA:legB:margin_us_per_event (PREREG-2 §3; 16 B = 2 events/request)')
    ap.add_argument('--with-warmup', action='store_true')
    ap.add_argument('--round', type=int, nargs='*')
    a = ap.parse_args()
    rows = load(a.rows, a.exp, a.payload, a.with_warmup)
    if a.round is not None:
        rows = [r for r in rows if r.get('round') in a.round]
    by = index(rows)
    legs = []
    for r in rows:
        if r['leg'] not in legs:
            legs.append(r['leg'])
    allrows = collections.defaultdict(list)
    for r in rows:
        allrows[r['leg']].append(r)
    print(f'rows={len(rows)} valid={sum(1 for r in rows if r.get("valid"))} reps={len(by)} payload={a.payload} exp={a.exp}')
    print('| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |')
    print('|---|---:|---:|---:|---|---|---:|---:|---:|---:|')
    for leg in legs:
        vr = [r for r in allrows[leg] if r.get('valid')]
        rps = [r['rps'] for r in vr]
        if not rps:
            print(f'| {leg} | 0 | | | | | | | | |')
            continue
        med = statistics.median(rps)
        cv = 100 * statistics.pstdev(rps) / statistics.fmean(rps) if len(rps) > 1 else float('nan')
        k, b = ref_for(leg, legs)
        lr = paired(by, leg, k, lambda x, y: math.log(x['rps'] / y['rps']))
        g, lo, hi = mean_ci(lr)
        gm = f'{math.exp(g):.4f} [{math.exp(lo):.4f}, {math.exp(hi):.4f}]' if lr and leg != k else ''
        xs = paired(by, leg, b, lambda x, y: 1e6 / x['rps'] - 1e6 / y['rps'])
        xm, xlo, xhi = mean_ci(xs)
        xt = f'{xm:.3f} [{xlo:.3f}, {xhi:.3f}]' if xs and leg != b else ''
        evq = [r['ev_per_req'] for r in vr if r.get('ev_per_req') is not None]
        dq = [r.get('del_over_exp', r.get('del_over_exp_bytes')) for r in vr if r.get('del_over_exp', r.get('del_over_exp_bytes')) is not None]
        lq = [r['cnt']['lost'] / r['req_nginx'] for r in vr if r.get('cnt') and r.get('req_nginx')]
        mr = [r['missing_ev'] - r['cnt']['lost'] for r in vr if r.get('cnt') and r.get('missing_ev') is not None]
        f = lambda v, d=4: f'{statistics.median(v):.{d}f}' if v else ''
        mrs = f'{min(mr)}..{max(mr)}' if mr else ''
        print(f'| {leg} | {len(rps)} | {med:.0f} | {cv:.2f} | {gm} | {xt} | {f(evq)} | {f(dq, 5)} | {f(lq, 6)} | {mrs} |')
    for spec in a.tost:
        x, y, m = spec.split(':')
        m = float(m)
        lr = paired(by, x, y, lambda p, q: math.log(p['rps'] / q['rps']))
        g, lo, hi = mean_ci(lr, T95)
        ok = lo > math.log(1 - m) and hi < math.log(1 + m)
        print(f'TOST {x} vs {y} margin ±{m:.3f}: ratio {math.exp(g):.4f} 90% CI [{math.exp(lo):.4f}, {math.exp(hi):.4f}] n={len(lr)} -> {"EQUIVALENT" if ok else "NOT shown"}')
    for spec in a.diff:
        x, y = spec.split(':')
        d = paired(by, x, y, lambda p, q: 1e6 / p['rps'] - 1e6 / q['rps'])
        m, lo, hi = mean_ci(d)
        print(f'DIFF x({x}) - x({y}) = {m:.3f} us/request [{lo:.3f}, {hi:.3f}] n={len(d)}; per event (/2) {m / 2:.3f} us')
    for spec in a.ratio:
        x, y = spec.split(':')
        lr = paired(by, x, y, lambda p, q: math.log(p['rps'] / q['rps']))
        g, lo, hi = mean_ci(lr)
        print(f'RATIO {x} / {y} = {math.exp(g):.4f} [{math.exp(lo):.4f}, {math.exp(hi):.4f}] n={len(lr)}')
    for spec in a.tostus:
        x, y, m = spec.split(':')
        m = float(m)
        d = paired(by, x, y, lambda p, q: (1e6 / p['rps'] - 1e6 / q['rps']) / 2)
        dm, lo90, hi90 = mean_ci(d, T95)
        _, lo95, hi95 = mean_ci(d)
        eq = lo90 > -m and hi90 < m
        sig = lo95 > 0 or hi95 < 0
        print(f'TOSTUS x({x}) - x({y}) = {dm:.3f} us/event 90% CI [{lo90:.3f}, {hi90:.3f}] 95% CI [{lo95:.3f}, {hi95:.3f}] n={len(d)} margin ±{m:.3f}'
              f' -> {"EQUIVALENT" if eq else "not equivalent"}, {"different" if sig else "no difference shown"}')
    noisy = [r for r in rows if r.get('busy_all') and any((r['busy_all'].get(c) or 0) > NOISY for c in NONMEASURED)]
    if any('busy_all' in r for r in rows):
        print(f'noisy_core rows (non-measured core busy > {NOISY}): {len(noisy)}', [(r['leg'], r['round'], r['rep']) for r in noisy][:20])
    inval = [r for r in rows if not r.get('valid')]
    if inval:
        print('invalid rows:', [(r['leg'], r['rep'], r.get('invalid_reason')) for r in inval])


if __name__ == '__main__':
    sys.exit(main())
