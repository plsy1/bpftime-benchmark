#!/usr/bin/env python3
"""E3 summary (latency / notification-policy factorial, H2 loader). Per round and leg, over valid non-warm-up rows:
rps median, latency quantiles from lat_q (the *_inf variants, which count lost/unreported events as +inf),
median across reps with [min, max], consumer (sniff) CPU ns per event and nginx CPU ns per request (schedstat),
lost and unreported events. Round 0 also prints the paired GMR of rps vs K@S4lat (95% t-CI).
Usage: summarize_e3.py rows.jsonl [--json]
"""
import collections, json, math, statistics, sys

T975 = {4: 2.776, 9: 2.262}


def q(rows, key):
    v = [r['lat_q'].get(key) for r in rows if r.get('lat_q') and r['lat_q'].get(key) is not None]
    return v


def fmt_us(v):
    if not v:
        return '-'
    m, lo, hi = statistics.median(v) / 1e3, min(v) / 1e3, max(v) / 1e3
    f = (lambda x: f'{x:.1f}') if m < 100 else (lambda x: f'{x:.0f}')
    return f'{f(m)} [{f(lo)},{f(hi)}]'


def main():
    rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
    rows = [r for r in rows if not r.get('warmup')]
    out = []
    for rnd in sorted({r['round'] for r in rows}):
        rr = [r for r in rows if r['round'] == rnd]
        legs = []
        for r in rr:
            if r['leg'] not in legs:
                legs.append(r['leg'])
        by = collections.defaultdict(dict)
        for r in rr:
            if r.get('valid'):
                by[r['rep']][r['leg']] = r
        mode = 'closed loop' if rnd == 0 else f"open loop {rr[0].get('rate') or ''}"
        rate = [r.get('rate_achieved') for r in rr if r.get('rate_achieved')]
        print(f'\n## round {rnd} ({mode}); rows {len(rr)}, valid {sum(1 for r in rr if r.get("valid"))}'
              + (f'; achieved rate median {statistics.median(rate):.1f}, min {min(rate):.1f}' if rate else ''))
        print('| leg | n | rps med | GMR vs K | p50 us | p90 us | p99 us | p99.9 us | sniff ns/ev | nginx ns/req | lost max | unrep max | lag_max ms |')
        print('|---|---:|---:|---|---|---|---|---|---:|---:|---:|---:|---:|')
        for leg in legs:
            v = [r for r in rr if r['leg'] == leg and r.get('valid')]
            rps = [r['rps'] for r in v]
            g = ''
            if rnd == 0 and leg != 'K@S4lat':
                lr = [math.log(by[k][leg]['rps'] / by[k]['K@S4lat']['rps']) for k in by if leg in by[k] and 'K@S4lat' in by[k]]
                if len(lr) > 1:
                    m = statistics.fmean(lr)
                    h = T975.get(len(lr) - 1, 2.0) * statistics.stdev(lr) / math.sqrt(len(lr))
                    g = f'{math.exp(m):.3f} [{math.exp(m - h):.3f},{math.exp(m + h):.3f}]'
            sn = [r['sniff_cpu_ns'] / max(1, r['cnt']['read_ev'] + r['cnt']['write_ev']) for r in v if r.get('sniff_cpu_ns') and r.get('cnt')]
            ng = [r['nginx_cpu_ns'] / r['req_nginx'] for r in v if r.get('nginx_cpu_ns') and r.get('req_nginx')]
            lost = [r['cnt']['lost'] for r in v if r.get('cnt')]
            unrep = [r.get('unreported_ev') or 0 for r in v]
            lag = [r['openloop']['lag_max_ms'] for r in v if r.get('openloop')]
            row = dict(round=rnd, leg=leg, n=len(v), rps=statistics.median(rps) if rps else None,
                       p50=q(v, 'p50_inf'), p90=q(v, 'p90_inf'), p99=q(v, 'p99_inf'), p999=q(v, 'p999_inf'),
                       sniff_ns_ev=statistics.median(sn) if sn else None, nginx_ns_req=statistics.median(ng) if ng else None,
                       lost_max=max(lost) if lost else None, unrep_max=max(unrep) if unrep else None)
            out.append(row)
            print(f"| {leg} | {len(v)} | {row['rps']:.0f} | {g} | {fmt_us(row['p50'])} | {fmt_us(row['p90'])} | {fmt_us(row['p99'])} | "
                  f"{fmt_us(row['p999'])} | {row['sniff_ns_ev']:.0f} | {row['nginx_ns_req']:.0f} | {row['lost_max']} | {row['unrep_max']} | "
                  f"{max(lag) if lag else '-'} |")
    if '--json' in sys.argv:
        json.dump(out, open(sys.argv[1] + '.e3summary.json', 'w'))


if __name__ == '__main__':
    main()
