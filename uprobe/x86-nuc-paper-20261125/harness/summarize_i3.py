#!/usr/bin/env python3
"""I3 payload sweep summary (eval plan I3: ratios with CI per payload, cost normalized by delivery).
Per payload and leg, over valid non-warm-up rows, paired by rep:
  ratio vs K and vs Kb (K+SSLSNIFF_PB_WAKEUP=32): GMR of rps, 95% t-CI;
  del/exp: delivered / expected bytes (median);
  x/ev: extra time per request vs B divided by the events delivered per request (us per delivered event), mean, 95% t-CI;
  x/KB: the same per delivered KiB of payload.
Stock-loader legs have no counters (no delivery), so only their ratios are printed; plus X-stock / X.
Usage: summarize_i3.py rows.jsonl [--exclude leg:round:rep ...] [--json]
"""
import collections, json, math, statistics, sys

T975 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228}
KB = 'K+SSLSNIFF_PB_WAKEUP=32'


def ci(xs):
    n = len(xs)
    if n < 2:
        return (xs[0] if xs else float('nan')), float('nan'), float('nan')
    m = statistics.fmean(xs)
    h = T975.get(n - 1, 2.0) * statistics.stdev(xs) / math.sqrt(n)
    return m, m - h, m + h


def gmr(by, a, b):
    lr = [math.log(v[a]['rps'] / v[b]['rps']) for v in by.values() if a in v and b in v]
    m, lo, hi = ci(lr)
    return (math.exp(m), math.exp(lo), math.exp(hi), len(lr)) if lr else None


def main():
    args = sys.argv[1:]
    excl = set()
    if '--exclude' in args:
        i = args.index('--exclude')
        j = i + 1
        while j < len(args) and not args[j].startswith('--'):
            leg, rnd, rep = args[j].rsplit(':', 2)
            excl.add((leg, int(rnd), int(rep)))
            j += 1
    rows = [json.loads(l) for l in open(args[0]) if l.strip()]
    rows = [r for r in rows if not r.get('warmup') and r.get('valid') and (r['leg'], r['round'], r['rep']) not in excl]
    out = []
    for p in sorted({r['payload'] for r in rows}):
        rr = [r for r in rows if r['payload'] == p]
        by = collections.defaultdict(dict)
        for r in rr:
            by[r['rep']][r['leg']] = r
        legs = []
        for r in sorted(rr, key=lambda r: (r['rep'], r['order_idx'])):
            if r['leg'] not in legs:
                legs.append(r['leg'])
        order = ['B', 'K', KB, 'O', 'Ol', 'L3port', 'Dn', 'Dnpc']
        legs = [l for l in order if l in legs] + [l for l in legs if l not in order]
        print(f'\n## payload {p} B (n per leg {collections.Counter(r["leg"] for r in rr).most_common(1)[0][1]}'
              + (f', excluded {sorted(e for e in excl if e[0] in legs)}' if excl else '') + ')')
        print('| leg | rps med | / K [95% CI] | / Kb [95% CI] | del/exp | ev/req delivered | x vs B us/ev delivered [95% CI] | x us per delivered KiB |')
        print('|---|---:|---|---|---:|---:|---|---:|')
        for leg in legs:
            v = [r for r in rr if r['leg'] == leg]
            rps = statistics.median(r['rps'] for r in v)
            gk = gmr(by, leg, 'K') if leg != 'K' else None
            gb = gmr(by, leg, KB) if leg != KB else None
            f = lambda g: f'{g[0]:.4f} [{g[1]:.4f}, {g[2]:.4f}]' if g else ''
            de = [r.get('del_over_exp', r.get('del_over_exp_bytes')) for r in v if r.get('del_over_exp', r.get('del_over_exp_bytes')) is not None]
            ev = [r['ev_per_req'] for r in v if r.get('ev_per_req')]
            xe, xk = [], []
            for k, legsr in by.items():
                if leg in legsr and 'B' in legsr and leg != 'B' and legsr[leg].get('cnt'):
                    r = legsr[leg]
                    x = 1e6 / r['rps'] - 1e6 / legsr['B']['rps']
                    e = r['ev_per_req']
                    kib = (r['cnt']['read_bytes'] + r['cnt']['write_bytes']) / r['req_nginx'] / 1024
                    if e:
                        xe.append(x / e)
                    if kib:
                        xk.append(x / kib)
            m, lo, hi = ci(xe)
            row = dict(payload=p, leg=leg, rps=rps, vsK=gk, vsKb=gb, del_exp=statistics.median(de) if de else None,
                       ev_req=statistics.median(ev) if ev else None, x_ev=(m, lo, hi) if xe else None,
                       x_kib=statistics.fmean(xk) if xk else None)
            out.append(row)
            print(f"| {leg} | {rps:.0f} | {f(gk)} | {f(gb)} | {'' if row['del_exp'] is None else format(row['del_exp'], '.4f')} | "
                  f"{'' if row['ev_req'] is None else format(row['ev_req'], '.3f')} | {f'{m:.3f} [{lo:.3f}, {hi:.3f}]' if xe else ''} | "
                  f"{'' if not xk else format(row['x_kib'], '.3f')} |")
        for leg in legs:
            if leg.endswith('-stock') and leg[:-6] in legs:
                g = gmr(by, leg, leg[:-6])
                print(f'STOCK {leg} / {leg[:-6]} = {g[0]:.4f} [{g[1]:.4f}, {g[2]:.4f}] n={g[3]}')
    if '--json' in args:
        json.dump(out, open(args[0] + '.i3summary.json', 'w'))


if __name__ == '__main__':
    main()
