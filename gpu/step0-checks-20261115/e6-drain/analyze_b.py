#!/usr/bin/env python3
"""analyze_b.py RESULTS_DIR

Match the GPU copy batches (bw.jsonl, CLOCK_MONOTONIC bounds) with the drain
phases (drain.jsonl) and report, per mode, the GPU bandwidth of batches that
lie wholly inside a phase (with a 0.2 s guard at each phase edge), the
slowdown against the off phases, and the drain rate achieved meanwhile.
Decision rules: README.md (part b). Writes summary_b.json. Standard library only.
"""
import json
import os
import statistics as st
import sys

GUARD_NS = 200_000_000
LIMIT_PCT = 5.0  # README: the drain is sustainable while the GPU slowdown stays below 5 %
MIN_BATCHES = 3  # a phase with fewer whole batches inside its guards does not count


def rows(p):
    out = []
    if os.path.exists(p):
        for l in open(p):
            l = l.strip()
            if l.startswith('{'):
                try:
                    out.append(json.loads(l))
                except ValueError:
                    pass
    return out


def main():
    R = os.path.abspath(sys.argv[1])
    bw = [r for r in rows(os.path.join(R, 'bw.jsonl')) if 'gpu_ms' in r]
    dr = rows(os.path.join(R, 'drain.jsonl'))
    phases = [r for r in dr if r.get('part') in ('b-phase', 'b-const')]
    res = {'results_dir': R, 'batches': len(bw), 'phases': len(phases), 'limit_pct': LIMIT_PCT}
    by_mode, by_phase = {}, []
    for ph in phases:
        lo, hi = ph['t0_ns'] + GUARD_NS, ph['t1_ns'] - GUARD_NS
        inside = [b['gb_per_s'] for b in bw if b['t0_ns'] >= lo and b['t1_ns'] <= hi]
        by_phase.append({'round': ph.get('round'), 'mode': ph['mode'], 'n': len(inside),
                         'median_gbs': st.median(inside) if inside else None,
                         'drain_ops_per_s': ph.get('ops_per_s'), 'drain_bytes_per_s': ph.get('bytes_per_s'),
                         'drain_bad': ph.get('bad')})
        by_mode.setdefault(ph['mode'], []).extend(inside)
    res['per_phase'] = by_phase
    off = by_mode.get('off', [])
    off_med = st.median(off) if off else None
    # noise: spread of the off phases' medians
    offs = [p['median_gbs'] for p in by_phase if p['mode'] == 'off' and p['median_gbs']]
    noise = 100.0 * (max(offs) - min(offs)) / off_med if off_med and len(offs) > 1 else None
    res['off_median_gbs'] = off_med
    res['off_phase_spread_pct'] = noise
    modes = {}
    for m, v in by_mode.items():
        if not v:
            continue
        med = st.median(v)
        d = {'batches': len(v), 'median_gbs': med,
             'slowdown_pct_vs_off': (100.0 * (off_med - med) / off_med) if off_med else None}
        # paired: each non-off phase against the off phase just before it in the same round
        pairs = []
        for i, p in enumerate(by_phase):
            if p['mode'] == m and m != 'off' and i > 0 and by_phase[i - 1]['mode'] == 'off' \
                    and p['median_gbs'] and by_phase[i - 1]['median_gbs']:
                pairs.append(100.0 * (by_phase[i - 1]['median_gbs'] - p['median_gbs']) / by_phase[i - 1]['median_gbs'])
        d['paired_slowdowns_pct'] = pairs
        rates = [p['drain_ops_per_s'] for p in by_phase if p['mode'] == m and p['drain_ops_per_s']]
        d['drain_ops_per_s_median'] = st.median(rates) if rates else None
        brates = [p['drain_bytes_per_s'] for p in by_phase if p['mode'] == m and p['drain_bytes_per_s']]
        d['drain_bytes_per_s_median'] = st.median(brates) if brates else None
        modes[m] = d
    res['modes'] = modes
    # decision (README part b)
    short = ['%s r%s (%d batches)' % (p['mode'], p['round'], p['n']) for p in by_phase if p['n'] < MIN_BATCHES]
    res['short_phases'] = short
    sched_end = [r for r in dr if r.get('part') == 'b-schedule-end']
    res['schedule_interrupted'] = bool(sched_end and sched_end[-1].get('interrupted')) or not sched_end
    hog = modes.get('hog4', {}).get('slowdown_pct_vs_off')
    thr = max(LIMIT_PCT, 3 * noise) if noise is not None else LIMIT_PCT
    control_ok = hog is not None and hog > thr and noise is not None
    d40m = modes.get('d40', {})
    d40 = d40m.get('slowdown_pct_vs_off')
    d40_phases = [p for p in by_phase if p['mode'] == 'd40']
    d40_bad = sum(p['drain_bad'] or 0 for p in d40_phases)
    d40_rate = d40m.get('drain_ops_per_s_median')
    if short or res['schedule_interrupted']:
        verdict = 'inconclusive: phases with < %d batches (%s) or the schedule did not finish' % (
            MIN_BATCHES, ', '.join(short) or 'none')
    elif not control_ok:
        verdict = 'inconclusive: the hog4 control did not slow the GPU by more than max(5 %%, 3 x off spread) = %.1f %%' % thr
    elif d40 is None or not d40_rate:
        verdict = 'inconclusive: no d40 batches or no d40 drain'
    elif d40_bad:
        verdict = 'inconclusive: %d failed or wrong d40 lookups' % d40_bad
    elif not d40m.get('paired_slowdowns_pct'):
        verdict = 'inconclusive: no d40 phase with an off phase just before it'
    elif noise >= LIMIT_PCT:
        verdict = 'inconclusive: the off phases spread by %.1f %%, not finer than the %.0f %% limit' % (noise, LIMIT_PCT)
    elif d40 < LIMIT_PCT and all(x < LIMIT_PCT for x in d40m['paired_slowdowns_pct']):
        verdict = ('sustainable: one reader at %.3g 40-B records/s (the d40 phases\' median) slows the GPU copy by '
                   '%.1f %% (< %.0f %%, every paired value too)' % (d40_rate, d40, LIMIT_PCT))
    else:
        verdict = ('not sustainable at full speed: one reader at %.3g 40-B records/s slows the GPU copy by %.1f %% '
                   '(pooled), paired %s (limit %.0f %%)' % (
                       d40_rate, d40, ['%.1f' % x for x in d40m['paired_slowdowns_pct']], LIMIT_PCT))
    # d1m: the same rule for whole-value reads of 1-MiB records, in bytes/s (README part b)
    d1m = modes.get('d1m', {})
    d1m_bad = sum(p['drain_bad'] or 0 for p in by_phase if p['mode'] == 'd1m')
    if verdict.startswith('inconclusive: phases') or not control_ok:
        v1 = 'inconclusive (as for d40)'
    elif d1m.get('slowdown_pct_vs_off') is None or not d1m.get('drain_bytes_per_s_median') or d1m_bad \
            or not d1m.get('paired_slowdowns_pct') or noise >= LIMIT_PCT:
        v1 = 'inconclusive: no d1m result, failed lookups, no paired off phase, or off spread >= limit'
    elif d1m['slowdown_pct_vs_off'] < LIMIT_PCT and all(x < LIMIT_PCT for x in d1m['paired_slowdowns_pct']):
        v1 = 'sustainable: %.3g B/s of 1-MiB reads slows the GPU copy by %.1f %%' % (
            d1m['drain_bytes_per_s_median'], d1m['slowdown_pct_vs_off'])
    else:
        v1 = 'not sustainable at full speed: %.3g B/s of 1-MiB reads slows the GPU copy by %.1f %%' % (
            d1m['drain_bytes_per_s_median'], d1m['slowdown_pct_vs_off'])
    res['verdict_d1m'] = v1
    res['control_ok'] = control_ok
    res['d40_drain_ops_per_s_median'] = d40_rate
    res['d40_drain_bad'] = d40_bad
    res['detect_threshold_pct'] = thr
    res['verdict'] = verdict
    json.dump(res, open(os.path.join(R, 'summary_b.json'), 'w'), indent=1)
    print('e6 part b (%s): %d batches, %d phases' % (R, len(bw), len(phases)))
    print('  off median %.2f GB/s, off-phase spread %s %%' % (off_med or 0, '%.2f' % noise if noise is not None else '-'))
    for m in ('off', 'd40', 'd1m', 'hog1', 'hog4'):
        if m in modes:
            x = modes[m]
            print('  %-5s n=%-4d median %.2f GB/s slowdown %s %% paired %s drain %s ops/s %s B/s' % (
                m, x['batches'], x['median_gbs'],
                '%.2f' % x['slowdown_pct_vs_off'] if x['slowdown_pct_vs_off'] is not None else '-',
                ['%.1f' % y for y in x['paired_slowdowns_pct']],
                '%.3g' % x['drain_ops_per_s_median'] if x['drain_ops_per_s_median'] else '-',
                '%.3g' % x['drain_bytes_per_s_median'] if x['drain_bytes_per_s_median'] else '-'))
    if short:
        print('  phases with < %d batches: %s' % (MIN_BATCHES, ', '.join(short)))
    print('  control_ok=%s threshold=%.1f %%' % (control_ok, thr))
    print('  verdict:', verdict)
    print('  verdict d1m:', res['verdict_d1m'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
