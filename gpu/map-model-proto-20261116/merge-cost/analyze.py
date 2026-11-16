"""merge_cost passes: ns per thread execution over m_none, per kernel and
shape, median of the rounds (each round's batch time / n launches).
usage: python3 -I analyze.py PASSDIR ..."""
import csv, statistics, sys, collections

THREADS = {'anchor': 262144, 'full': 12288, 'large': 4194304}
KS = ('m_carry', 'm_glob', 'm_sm', 'm_lane', 'm_smlane')
res = {}
for d in sys.argv[1:]:
    t = collections.defaultdict(list)
    capped = 0
    for r in csv.DictReader(open(d + '/raw.csv')):
        if int(r['capped']):
            capped += 1
            continue  # the gate gave up before the batch was queued: not timed
        t[(r['kernel'], r['shape'])].append(float(r['ms']) / int(r['n']) * 1e6)
    res[d] = {}
    for s in list(THREADS):
        if not t[('m_none', s)]:
            print('%s: every reference batch capped (host could not queue the batch within the gate): shape dropped' % s)
            del THREADS[s]
    for s in THREADS:
        base = statistics.median(t[('m_none', s)])
        res[d][s] = {k: (statistics.median(t[(k, s)]) - base) / THREADS[s] for k in KS}
    print('##', d, 'capped batches', capped)
    for s in THREADS:
        print('%-7s' % s, '  '.join('%s %.3f' % (k[2:], v) for k, v in res[d][s].items()))
if len(res) == 2:
    a, b = list(res)
    print('## pass 2 / pass 1')
    for s in THREADS:
        print('%-7s' % s, '  '.join('%s %.3f' % (k[2:], res[b][s][k] / res[a][s][k]) for k in KS))
