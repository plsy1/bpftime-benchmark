#!/usr/bin/env python3
"""E5-real comparer: pair probe records (bpftime uprobe on libc write, or the kernel uprobe of the Kc control) with
truth records (kernel tracepoint sys_enter_write) WITHOUT using any identity field: same (fd, buf, count), truth time
in [t_probe - SLACK, t_probe + WINDOW], earliest unused truth record wins. A probe record whose window holds unused
candidates from >= 2 different truth threads is counted as ambiguous (still paired with the earliest).

Fields compared on pairs: tgid, tid, uid, gid, comm. Output (JSON lines on stdout):
  kind=summary   per run: probe/truth counts, pairs, unpaired probe records, ambiguous, mismatches per field
  kind=byproc    per truth comm: truth records, pairs, mismatches per field, example (truth, probe) values
Usage: compare_e5real.py --truth T.dump --probe P.dump --workload W --leg L --run N
"""
import argparse, bisect, collections, json, sys

WINDOW_NS = 20_000_000
SLACK_NS = 50_000
FIELDS = ('tgid', 'tid', 'uid', 'gid', 'comm')


def load(path):
    recs, stats = [], {}
    with open(path) as f:
        head = f.readline().split()
        # "# stats calls C fail F"
        stats = dict(calls=int(head[3]), fail=int(head[5]))
        for line in f:
            p = line.split()
            if len(p) != 7:
                continue
            t, pt, ug, fd, buf, cnt = (int(x) for x in p[:6])
            comm = bytes.fromhex(p[6]).split(b'\0', 1)[0].decode('latin-1')
            recs.append(dict(t=t, tgid=pt >> 32, tid=pt & 0xffffffff, uid=ug & 0xffffffff, gid=ug >> 32,
                             fd=fd, buf=buf, count=cnt, comm=comm))
    recs.sort(key=lambda r: r['t'])
    return recs, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--truth', required=True)
    ap.add_argument('--probe', required=True)
    ap.add_argument('--workload', required=True)
    ap.add_argument('--leg', required=True)
    ap.add_argument('--run', type=int, required=True)
    a = ap.parse_args()
    truth, tst = load(a.truth)
    probe, pst = load(a.probe)
    idx = collections.defaultdict(list)          # (fd, buf, count) -> [(t, i)] sorted by t
    for i, r in enumerate(truth):
        idx[(r['fd'], r['buf'], r['count'])].append((r['t'], i))
    times = {k: [t for t, _ in v] for k, v in idx.items()}
    used = [False] * len(truth)
    paired_truth = [None] * len(truth)
    pairs, unpaired, ambiguous = [], 0, 0
    for pr in probe:
        key = (pr['fd'], pr['buf'], pr['count'])
        lst = idx.get(key)
        if not lst:
            unpaired += 1
            continue
        ts = times[key]
        lo = bisect.bisect_left(ts, pr['t'] - SLACK_NS)
        hi = bisect.bisect_right(ts, pr['t'] + WINDOW_NS)
        cands = [lst[j][1] for j in range(lo, hi) if not used[lst[j][1]]]
        if not cands:
            unpaired += 1
            continue
        if len({truth[c]['tid'] for c in cands}) > 1:
            ambiguous += 1
        c = cands[0]
        used[c] = True
        paired_truth[c] = pr
        pairs.append((truth[c], pr))
    mism = collections.Counter()
    byproc = {}
    for tr in truth:
        b = byproc.setdefault(tr['comm'], dict(truth=0, pairs=0, mism=collections.Counter(), examples={}))
        b['truth'] += 1
    for tr, pr in pairs:
        b = byproc[tr['comm']]
        b['pairs'] += 1
        for f in FIELDS:
            if tr[f] != pr[f]:
                mism[f] += 1
                b['mism'][f] += 1
                b['examples'].setdefault(f, [tr[f], pr[f]])
    any_mism = sum(1 for tr, pr in pairs if any(tr[f] != pr[f] for f in FIELDS))
    base = dict(workload=a.workload, leg=a.leg, run=a.run)
    print(json.dumps(dict(kind='summary', **base, truth=len(truth), truth_stats=tst, probe=len(probe),
                          probe_stats=pst, pairs=len(pairs), unpaired_probe=unpaired, ambiguous=ambiguous,
                          truth_unpaired=len(truth) - len(pairs), records_with_any_mismatch=any_mism,
                          mismatch={f: mism[f] for f in FIELDS},
                          threads_truth=len({(r['tgid'], r['tid']) for r in truth}),
                          processes_truth=len({r['tgid'] for r in truth}))))
    for comm, b in sorted(byproc.items(), key=lambda kv: -kv[1]['truth']):
        print(json.dumps(dict(kind='byproc', **base, comm=comm, truth=b['truth'], pairs=b['pairs'],
                              mismatch={f: b['mism'][f] for f in FIELDS}, examples=b['examples'])))


if __name__ == '__main__':
    sys.exit(main())
