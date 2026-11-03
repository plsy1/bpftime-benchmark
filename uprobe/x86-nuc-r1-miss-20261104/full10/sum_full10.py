import json, statistics as st, sys
from pathlib import Path
run = Path(sys.argv[1])
legs = ['dev-7ad5a152', 'dev-d5279469']
d = {l: json.loads((run / l / 'benchmark-output.json').read_text()) for l in legs}
def m(x, g, k): return st.mean(x[g][k]['raw_values'])
def md(x, g, k): return st.median(x[g][k]['raw_values'])
print('| case | 7ad5a152 | x bpf | d5279469 | x bpf | change (mean) | change (median) |')
print('|---|---:|---:|---:|---:|---:|---:|')
for k in d[legs[0]]['userspace_uprobe']:
    scale = 1000 if 'map' in k else 1
    a, b = m(d[legs[0]], 'userspace_uprobe', k) / scale, m(d[legs[1]], 'userspace_uprobe', k) / scale
    ka, kb = m(d[legs[0]], 'kernel_uprobe', k) / scale, m(d[legs[1]], 'kernel_uprobe', k) / scale
    ma, mb = md(d[legs[0]], 'userspace_uprobe', k), md(d[legs[1]], 'userspace_uprobe', k)
    print(f"| {k.removeprefix('__bench_')} | {a:.2f} | {a/ka:.2f}x | {b:.2f} | {b/kb:.2f}x | {100*(a/b-1):+.1f}% | {100*(ma/mb-1):+.1f}% |")
ea, eb = m(d[legs[0]], 'embed', 'embed'), m(d[legs[1]], 'embed', 'embed')
print(f"\nembed: 7ad5a152 {ea:.2f} ns, d5279469 {eb:.2f} ns ({100*(ea/eb-1):+.1f}%)")
