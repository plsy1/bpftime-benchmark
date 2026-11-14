#!/usr/bin/env python3
"""Compare the two step-0 passes of 2026-11-15 (pass 1 05:04-05:53, pass 2 06:04-06:54).
Run with python3 -I compare_passes.py > compare-passes.md (from this directory)."""
import csv, json, os, re, statistics, sys

S = os.path.dirname(os.path.abspath(__file__))
P = {
    1: dict(e1='e1-ids/runs/20261115-052322', e7w='e7-warp/runs/20261115-052338', e2='e2-cost/results/run-20261115-050452',
            e4='e4-publish/results/run-20261115-052530-full', e3='e3-copyout/out/20261115-051249',
            e5='e5-atomics/results/run-20261115-052352', e6a='e6-drain/results/a-20261115-050712',
            e6b='e6-drain/results/b-20261115-050843', e6c='e6-drain/results/c-20261115-051143',
            e6l='e6-drain/results/b-llama-20261115-051755', e8='e8-package/results/run-20261115-052452',
            e7l='e7-llama/results/1115-054506', e9='e9-hooks/results/1115-055111'),
    2: dict(e1='e1-ids/runs/20261115-061747', e7w='e7-warp/runs/20261115-061804', e2='e2-cost/results/run-20261115-060441',
            e4='e4-publish/results/run-20261115-061956-full', e3='e3-copyout/out/20261115-061240',
            e5='e5-atomics/results/run-20261115-061818', e6a='e6-drain/results/a-20261115-060701',
            e6b='e6-drain/results/b-20261115-060833', e6c='e6-drain/results/c-20261115-061133',
            e6l='e6-drain/results/b-llama-20261115-064102', e8='e8-package/results/run-20261115-061918',
            e7l='e7-llama/results/1115-064629', e9='e9-hooks/results/1115-065217'),
}


def path(p, k, *rest):
    return os.path.join(S, P[p][k], *rest)


def read(p, k, *rest):
    with open(path(p, k, *rest), encoding='utf-8') as f:
        return f.read()


def pct(a, b):
    return 'n/a' if not a else f'{100.0 * (b - a) / a:+.1f}%'


def row(name, a, b, fmt='{:.3f}'):
    print(f'| {name} | {fmt.format(a)} | {fmt.format(b)} | {pct(a, b)} |')


def verdict_lines(text, keys):
    out = []
    for line in text.splitlines():
        if any(k in line for k in keys):
            out.append(re.sub(r'/home/\S+', '<dir>', line.strip()))
    return out


print('# Step 0: pass 1 against pass 2 (2026-11-15)\n')
print('Pass 1 ran 05:04-05:53 JST, pass 2 06:04-06:54 JST, same scripts and builds, clocks fixed in both.')
print("apt's daily services ran only during check 4 of pass 2 (06:24:18-06:24:49 and 06:28:21-06:29:07).\n")

# ---- check 2
print('## Check 2 (e2-cost): every resolved timing row, pass 2 / pass 1\n')
rows = {p: {r['id']: r for r in csv.DictReader(open(path(p, 'e2', 'summary.csv')))} for p in (1, 2)}
ratios = []
outliers = []
for k, r1 in rows[1].items():
    r2 = rows[2].get(k)
    if not r2 or r1['resolved'] != 'True' or r2['resolved'] != 'True':
        continue
    a, b = float(r1['delta_us_med']), float(r2['delta_us_med'])
    if a <= 0:
        continue
    q = b / a
    ratios.append(q)
    if abs(q - 1) > 0.05:
        outliers.append((abs(q - 1), k, a, b))
ratios.sort()
print(f'- rows compared: {len(ratios)}; median ratio {statistics.median(ratios):.3f}; '
      f'min {ratios[0]:.3f}; max {ratios[-1]:.3f}; rows off by more than 5%: {len(outliers)}; by more than 10%: '
      f'{sum(1 for o in outliers if o[0] > 0.10)}')
if outliers:
    print('- rows off by more than 5% (delta us, pass 1 -> pass 2):')
    for d, k, a, b in sorted(outliers, reverse=True)[:15]:
        print(f'  - `{k}`: {a:.1f} -> {b:.1f} ({100 * (b / a - 1):+.1f}%)')
print('\n| headline value | pass 1 | pass 2 | change |\n|---|---:|---:|---:|')
def e2v(p, key, col):
    return float(rows[p][key][col])
for name, key, col in [
    ("today's 1512 op, anchor, ns/thread", 'anchor|ref1512|reg|f1|R1|k0', 'ns_per_thread_op_med'),
    ('record/warp lane 0 + membar.sys, anchor reg, ns/warp', 'anchor|rec_lane0|reg|f1|R1|k0', 'ns_per_warp_op_med'),
    ('record/warp lane 0, no fence, anchor reg, ns/warp', 'anchor|rec_lane0|reg|f0|R1|k0', 'ns_per_warp_op_med'),
    ('record/warp lane 0 + membar.sys, anchor dev, ns/warp', 'anchor|rec_lane0|dev|f1|R1|k0', 'ns_per_warp_op_med'),
    ('8-B store/thread, anchor dev, no fence, ns/thread', 'anchor|st_lane32|dev|f0|R1|k0', 'ns_per_thread_op_med'),
    ('8-B store/thread, large dev, no fence, ns/thread', 'large|st_lane32|dev|f0|R1|k0', 'ns_per_thread_op_med'),
    ('8-B store/thread, full dev, no fence, ns/thread', 'full|st_lane32|dev|f0|R1|k0', 'ns_per_thread_op_med'),
    ('8-B store/thread, anchor reg, membar.sys, ns/thread', 'anchor|st_lane32|reg|f1|R1|k0', 'ns_per_thread_op_med'),
    ('32-B record/thread, anchor dev, no fence, ns/thread', 'anchor|rec_thread|dev|f0|R1|k0', 'ns_per_thread_op_med'),
    ('atomic add own word, anchor dev, ns/thread', 'anchor|atom_lane|dev|f0|R1|k0', 'ns_per_thread_op_med'),
    ('atomic add own word, anchor reg, ns/thread', 'anchor|atom_lane|reg|f0|R1|k0', 'ns_per_thread_op_med'),
    ('one word per SM, 48 warps, reg, R=16, ns/counted op', 'full|atom_sm|reg|f0|R16|k48', 'ns_per_counted_op_med'),
    ('one word per SM, 48 warps, dev, R=16, ns/counted op', 'full|atom_sm|dev|f0|R16|k48', 'ns_per_counted_op_med'),
]:
    try:
        row(name, e2v(1, key, col), e2v(2, key, col))
    except KeyError as e:
        print(f'| {name} | missing {e} | | |')
for p in (1, 2):
    d = json.load(open(path(p, 'e2', 'derived.json')))
    print(f"\npass {p}: reading verdict: {d['reading'].get('verdict', d['reading']) if isinstance(d['reading'], dict) else d['reading']}"[:300])
    print(f"pass {p}: decisions: {json.dumps(d['decision'])[:400]}")

# ---- check 6
print('\n## Check 6 (e6-drain)\n')
print('| value | pass 1 | pass 2 | change |\n|---|---:|---:|---:|')
def e6a(p):
    out = {}
    for line in read(p, 'e6a', 'summary.txt').splitlines():
        m = re.match(r'\s*(\S+)\s+(\d+) B x\s+\d+:\s+(\d+) records/s\s+([\d.]+) GB/s', line)
        if m:
            out[f'{m.group(1)} {m.group(2)} B'] = (float(m.group(3)), float(m.group(4)))
    return out
a1, a2 = e6a(1), e6a(2)
for k in a1:
    row(f'drain {k}, records/s', a1[k][0], a2[k][0], '{:,.0f}')
def e6b(p):
    out = {}
    for line in read(p, 'e6b', 'summary.txt').splitlines():
        m = re.match(r'\s*(\w+)\s+n=\d+\s+median ([\d.]+) GB/s slowdown\s+(-?[\d.]+) %', line)
        if m:
            out[m.group(1)] = float(m.group(3))
    return out
b1, b2 = e6b(1), e6b(2)
for k in ('d40', 'd1m', 'hog1', 'hog4'):
    print(f'| GPU bandwidth slowdown, {k} | {b1.get(k)}% | {b2.get(k)}% | |')
for p in (1, 2):
    c = read(p, 'e6c', 'summary.txt')
    m = re.search(r'largest working size: (\S+ MiB)', c)
    f2048 = re.search(r'2048 MiB: .*works=(\w+)', c)
    print(f'| pass {p}: largest segment that registers | {m.group(1) if m else "?"} | 2,048 MiB works: {f2048.group(1) if f2048 else "?"} | |')
for p in (1, 2):
    d = json.load(open(path(p, 'e6l', 'summary_b_llama.json')))
    print(f"| pass {p}: llama pp512 slowdown, d40 / hog4 (median %) | {d['slowdown']['d40']['median_pct']:.2f} | {d['slowdown']['hog4']['median_pct']:.2f} | off-round spread {d['off_round_spread_pct']:.2f}% |")

# ---- check 3
print('\n## Check 3 (e3-copyout)\n')
for p in (1, 2):
    d = json.load(open(path(p, 'e3', 'decisions.json')))
    lay = {k: (v['verdict'], v['viol_chunks']) for k, v in d['A']['layouts'].items()}
    print(f"- pass {p}: overall {d['overall']}; A {d['A']['answer']}; layouts {lay}")
def e3b(p):
    out = {}
    for line in read(p, 'e3', 'summary.md').splitlines():
        m = re.match(r'\| (noshim|idle|c1m1ms|c16m1ms|cont) \| (pp512|tg128) \| [^|]+\| ([\d.]+) \| ([^|]+) \|', line)
        if m:
            out[(m.group(1), m.group(2))] = (float(m.group(3)), m.group(4).strip())
    return out
e1b, e2b = e3b(1), e3b(2)
print('\n| llama-bench with the copier | pass 1 t/s (loss) | pass 2 t/s (loss) |\n|---|---:|---:|')
for k in e1b:
    print(f'| {k[0]} {k[1]} | {e1b[k][0]:.2f} ({e1b[k][1]}) | {e2b.get(k, (0, "?"))[0]:.2f} ({e2b.get(k, (0, "?"))[1]}) |')
for p in (1, 2):
    s = read(p, 'e3', 'summary.md')
    for key in ('**B0 verdict', '**B: graphs', '**C: tier', 'ctxsync-relaxed'):
        for line in s.splitlines():
            if line.startswith(key) or (key == 'ctxsync-relaxed' and line.startswith('| ctxsync-relaxed')):
                print(f'- pass {p}: {line.strip()[:200]}')

# ---- check 4
print('\n## Check 4 (e4-publish)\n')
print('| run | pass 1 violating / verified | pass 2 violating / verified | rate p1 | rate p2 |\n|---|---:|---:|---:|---:|')
r4 = {p: {r['name']: r for r in csv.DictReader(open(path(p, 'e4', 'summary.csv')))} for p in (1, 2)}
for name, r1 in r4[1].items():
    r2 = r4[2].get(name, {})
    def vv(r):
        if not r:
            return '-'
        if r.get('test') == '2':
            return f"stale {r.get('stale_rounds')} / {r.get('verified')}"
        return f"{r.get('violating_rounds')} / {r.get('verified')}"
    def rate(r):
        try:
            return f"{float(r['rate_rounds_per_s']):,.0f}"
        except Exception:
            return '-'
    print(f'| {name} | {vv(r1)} | {vv(r2)} | {rate(r1)} | {rate(r2)} |')
for p in (1, 2):
    s = read(p, 'e4', 'summary.md')
    for line in s.splitlines():
        if line.startswith('- verdict:') or line.startswith('- attribution:') or 'A_fence_ns_first_round' in line:
            print(f'- pass {p}: {line.strip()[:260]}')

# ---- verdict-only checks
print('\n## Checks 1, 5, 7, 8, 9: verdict lines\n')
spec = [
    ('e1', 'summary.txt', ['S:', 'W:', 'exclusive', '%warpid:', 'entry->exit', '%gridid unique', 'c1:', 'c2:', 'c3:', 'c4:', 'd:', 'ids stable', 'hog_episodes', 'hog_warp_gaps', 'id_changes_total']),
    ('e7w', 'summary.txt', ['correct under partial masks', 'native:', 'jit:']),
    ('e5', 'summary.txt', ['fadd  calls', 'plain calls', 'cas   calls', 'fadd_run', 'cas_run', 'cmpxchg_bug_ptx']),
    ('e8', 'summary.txt', ['base_ok', 'decision:']),
    ('e7l', 'summary.txt', ['valid=', 'overall:', 'chunk_rule']),
    ('e9', 'pp512.summary.txt', ['verdict:', 'all geometries', 'largest threads']),
    ('e9', 'tg128.summary.txt', ['verdict:', 'all geometries', 'largest threads', 'graphs (nsys)']),
]
for k, f, keys in spec:
    l1, l2 = verdict_lines(read(1, k, f), keys), verdict_lines(read(2, k, f), keys)
    same = l1 == l2
    print(f'### {P[1][k].split("/")[0]} {f}: {"identical" if same else "differs"}\n')
    if same:
        for x in l1[:8]:
            print(f'    {x[:200]}')
    else:
        for i in range(max(len(l1), len(l2))):
            x1 = l1[i] if i < len(l1) else ''
            x2 = l2[i] if i < len(l2) else ''
            if x1 != x2:
                print(f'    p1: {x1[:220]}\n    p2: {x2[:220]}')
    print()
