import json, sys, statistics as st, collections
# Median ops/s per tool, threads and backend; before = exp-c98951a4-gumfix, now = dev-ff5515d8.
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
d = collections.defaultdict(list); err = collections.Counter()
for r in rows:
    k = (r['tool'], r.get('threads', 1), r['backend'])
    if 'error' in r or not r.get('ops_per_sec'):
        err[k] += 1; continue
    d[k].append(r['ops_per_sec'])
bes = ['kernel', 'exp-c98951a4-gumfix', 'dev-ff5515d8']
f = lambda x: '-' if x is None else (f'{x/1e6:.2f}M' if x >= 1e6 else f'{x/1e3:.0f}k')
q = lambda a, b: '-' if not a or not b else f'{a/b:.2f}'
print('| tool | thr | bpf | before | now | before/bpf | now/bpf | now/before | rounds before | rounds now | errors |')
print('|---|--:|--:|--:|--:|--:|--:|--:|---|---|---|')
for t, n in sorted({(t, n) for t, n, b in list(d) + list(err)}):
    m = {b: (st.median(d[(t, n, b)]) if d[(t, n, b)] else None) for b in bes}
    rb = '/'.join(f'{x/1e3:.0f}' for x in d[(t, n, bes[1])])
    rn = '/'.join(f'{x/1e3:.0f}' for x in d[(t, n, bes[2])])
    e = ' '.join(f'{b}:{err[(t, n, b)]}' for b in bes if err[(t, n, b)])
    print(f'| {t} | {n} | ' + ' | '.join(f(m[b]) for b in bes) +
          f' | {q(m[bes[1]], m["kernel"])} | {q(m[bes[2]], m["kernel"])} | {q(m[bes[2]], m[bes[1]])} | {rb} | {rn} | {e} |')
