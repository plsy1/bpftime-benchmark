#!/usr/bin/env python3
"""analyze_b_llama.py RESULTS_DIR : llama-bench pp512 tokens/s per arm (off, d40,
hog4) from llama-r<round>-<arm>.json, the slowdown of d40 and hog4 against off
(per round and pooled), and the drain rate the loader reached meanwhile
(drain-r<round>-<arm>.jsonl). Decision rules: README.md part (b).
Writes summary_b_llama.json. Standard library only."""
import glob
import json
import os
import re
import statistics as st
import sys

LIMIT_PCT = 5.0


def main():
    R = os.path.abspath(sys.argv[1])
    per = {}
    for p in sorted(glob.glob(os.path.join(R, 'llama-r*-*.json'))):
        m = re.search(r'llama-r(\d+)-(\w+)\.json$', p)
        r, arm = int(m.group(1)), m.group(2)
        try:
            j = json.load(open(p))
            samples = j[0]['samples_ts']
        except Exception:
            samples = None
        rate, bad, nwin = None, None, 0
        dp = os.path.join(R, 'drain-r%d-%s.jsonl' % (r, arm))
        if os.path.exists(dp):
            win = [json.loads(l) for l in open(dp) if '"b-const"' in l]
            nwin = len(win)
            secs = sum((w['t1_ns'] - w['t0_ns']) / 1e9 for w in win)
            rate = sum(w['ops'] for w in win) / secs if secs else None
            bad = sum(w.get('bad', 0) for w in win)
        per.setdefault(r, {})[arm] = {'samples_ts': samples,
                                      'median_ts': st.median(samples) if samples else None,
                                      'drain_windows': nwin, 'drain_bad': bad,
                                      'drain_ops_per_s': rate}
    res = {'results_dir': R, 'rounds': per, 'limit_pct': LIMIT_PCT}
    pooled = {}
    for arm in ('off', 'd40', 'hog4'):
        v = [x for r in per.values() if arm in r and r[arm]['samples_ts'] for x in r[arm]['samples_ts']]
        pooled[arm] = st.median(v) if v else None
    res['pooled_median_ts'] = pooled
    slow = {}
    for arm in ('d40', 'hog4'):
        pr = [100.0 * (r['off']['median_ts'] - r[arm]['median_ts']) / r['off']['median_ts']
              for r in per.values() if 'off' in r and arm in r and r['off']['median_ts'] and r[arm]['median_ts']]
        slow[arm] = {'per_round_pct': pr, 'median_pct': st.median(pr) if pr else None}
    offs = [r['off']['median_ts'] for r in per.values() if 'off' in r and r['off']['median_ts']]
    spread = 100.0 * (max(offs) - min(offs)) / st.median(offs) if len(offs) > 1 else None
    res['slowdown'] = slow
    res['off_round_spread_pct'] = spread
    thr = max(LIMIT_PCT, 3 * spread) if spread is not None else LIMIT_PCT
    h = slow['hog4']['median_pct']
    d = slow['d40']['median_pct']
    d40 = [r['d40'] for r in per.values() if 'd40' in r]
    d40_bad = sum(x['drain_bad'] or 0 for x in d40)
    d40_rates = [x['drain_ops_per_s'] for x in d40 if x['drain_ops_per_s']]
    d40_rate = st.median(d40_rates) if d40_rates else None
    res['d40_drain_ops_per_s_median'] = d40_rate
    res['d40_drain_bad'] = d40_bad
    if spread is None or h is None or h <= thr:
        verdict = 'inconclusive: hog4 did not slow llama-bench by more than %.1f %% (or < 2 off rounds)' % thr
    elif d is None or not d40_rate or len(d40_rates) < len(d40):
        verdict = 'inconclusive: no d40 result, or a d40 arm without drain windows'
    elif d40_bad:
        verdict = 'inconclusive: %d failed or wrong d40 lookups' % d40_bad
    elif spread >= LIMIT_PCT:
        verdict = 'inconclusive: the off rounds spread by %.1f %%, not finer than the %.0f %% limit' % (spread, LIMIT_PCT)
    elif d < LIMIT_PCT:
        verdict = ('sustainable: one reader at %.3g 40-B records/s slows pp512 by %.1f %% (median of rounds, < %.0f %%)'
                   % (d40_rate, d, LIMIT_PCT))
    else:
        verdict = 'not sustainable at full speed: one reader at %.3g 40-B records/s slows pp512 by %.1f %%' % (d40_rate, d)
    res['verdict'] = verdict
    json.dump(res, open(os.path.join(R, 'summary_b_llama.json'), 'w'), indent=1)
    print('e6 part b, llama-bench pp512 (%s)' % R)
    print('  pooled median t/s:', pooled, ' off-round spread %:', spread)
    print('  slowdown:', json.dumps(slow))
    print('  verdict:', verdict)
    return 0


if __name__ == '__main__':
    sys.exit(main())
