#!/usr/bin/env python3
"""Summarize E4 JSONL rows per cell (count mode, variant cell, T, leg).

Per cell: n valid / n total, lost fraction median [min, max], how many reps lost anything,
missed probe hits (#730 skips), wall_s median [min, max] and ns per call per busy CPU (median).
Loss is bursty (one preempted thread writes back a stale value and erases a whole time slice of
other threads' updates), so look at the range and the count of reps with loss, not only the
median. Lost fractions are not directly comparable across legs: per-call cost differs, so the
share of time inside the load-store window differs; ns_per_call_cpu is printed for that reason.
Timing (#730 lock price) should be read from count_runs=false rows (no counter on the hot path).
K rows reflect this box's preempt mode (kernel.txt; "voluntary"): kernel uprobe programs run under
migrate_disable, not preempt_disable, so K = 0 is not evidence of kernel non-preemption in general.
Usage: summarize_e4.py FILE.jsonl [--json]
"""
import json, statistics, sys
from collections import defaultdict


def med(xs):
    return statistics.median(xs) if xs else None


def rng(xs, fmt):
    if not xs:
        return '-'
    return f'{fmt(med(xs))} [{fmt(min(xs))}, {fmt(max(xs))}]'


def main():
    path = sys.argv[1]
    as_json = '--json' in sys.argv
    rows = [json.loads(l) for l in open(path) if l.strip()]
    cells = defaultdict(list)
    for r in rows:
        cell = r.get('cell') or (r['variant'] if r['variant'] != 'b' else f"b{r.get('widen')}")
        cells[(not r.get('count_runs', True), cell, r['T'], r['leg'])].append(r)
    out = []
    for key in sorted(cells, key=lambda k: (k[0], k[1], k[2], k[3])):
        rs = cells[key]
        v = [r for r in rs if r.get('valid')]
        fr = [r['lost'] / r['expected'] for r in v if r.get('lost') is not None and r.get('expected')]
        lv = [r['lost'] for r in v if r.get('lost') is not None]
        miss = [r['missed_runs'] for r in v if r.get('missed_runs') is not None]
        out.append(dict(
            mode='timing(no-count)' if key[0] else 'loss(count)', cell=key[1], T=key[2], leg=key[3],
            n_valid=len(v), n=len(rs), N=sorted({r['N'] for r in rs}),
            lost_frac_med=med(fr), lost_frac_min=min(fr) if fr else None, lost_frac_max=max(fr) if fr else None,
            lost_med=med(lv), lost_max=max(lv) if lv else None, reps_with_loss=sum(1 for x in lv if x > 0),
            missed_runs_max=max(miss) if miss else None,
            skipped_rows=sum(1 for r in v if r.get('skipped_runs')),
            wall_s_med=med([r['wall_s'] for r in v]),
            wall=rng([r['wall_s'] for r in v], lambda x: f'{x:.2f}'),
            ns_call_cpu_med=med([r['ns_per_call_cpu'] for r in v if r.get('ns_per_call_cpu')]),
            invalid=sorted({r.get('invalid_reason') or '?' for r in rs if not r.get('valid')})))
    if as_json:
        for o in out:
            print(json.dumps(o))
        return
    hdr = f"{'mode':17} {'cell':4} {'T':>3} {'leg':7} {'ok/n':>5} {'lost% med [min,max]':>28} {'loss reps':>9} {'lost max':>9} {'missed':>7} {'wall_s med [min,max]':>22} {'ns/call/cpu':>11}"
    print(hdr)
    print('-' * len(hdr))
    for o in out:
        lf = '-' if o['lost_frac_med'] is None else (
            f"{100*o['lost_frac_med']:.3f} [{100*o['lost_frac_min']:.3f}, {100*o['lost_frac_max']:.3f}]")
        if o['mode'].startswith('timing'):
            lf = '(unverified) ' + lf
        print(f"{o['mode']:17} {o['cell']:4} {o['T']:>3} {o['leg']:7} {o['n_valid']:>2}/{o['n']:<2} {lf:>28} "
              f"{o['reps_with_loss']:>9} {str(o['lost_max']):>9} {str(o['missed_runs_max']):>7} {o['wall']:>22} "
              f"{str(o['ns_call_cpu_med']):>11}")
        if o['invalid']:
            print(f"{'':17}   invalid: {o['invalid']}")


if __name__ == '__main__':
    main()
