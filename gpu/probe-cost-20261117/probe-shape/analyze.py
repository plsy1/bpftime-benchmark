"""probe_shape passes: ns per thread execution over the same body without
probes, per probe shape, body and launch shape (median of rounds).
usage: python3 -I analyze.py PASSDIR ..."""
import csv, statistics, sys, collections
res = {}
for d in sys.argv[1:]:
    t = collections.defaultdict(list); thr = {}; capped = 0
    for r in csv.DictReader(open(d + '/raw.csv')):
        if int(r['capped']):
            capped += 1; continue
        t[(r['kernel'], r['shape'])].append(float(r['ms']) / int(r['n']) * 1e6)
        thr[r['shape']] = int(r['threads'])
    print('##', d, 'capped batches', capped)
    kinds = sorted({k.split('/')[0] for k, _ in t}, key=lambda k: ['none','c_empty','c_carry','c_smlane','c_param','r_carry','r_smlane','r_warp'].index(k))
    for body in ('empty', 'mem'):
        for s in sorted(thr):
            base = statistics.median(t[('none/' + body, s)])
            row = ['%s/%s base %.3f ns/thr' % (body, s, base / thr[s])]
            for k in kinds[1:]:
                v = t.get((k + '/' + body, s))
                if v:
                    row.append('%s %+.3f' % (k, (statistics.median(v) - base) / thr[s]))
            print('  '.join(row))
