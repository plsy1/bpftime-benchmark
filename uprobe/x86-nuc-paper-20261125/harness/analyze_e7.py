#!/usr/bin/env python3
"""E7 counters (plan 3.5 item 5 and E7): per-request syscalls and cycles of the nginx worker,
cross-method closure against the E6 throughput breakdown, and kernel BPF stats.
Usage: analyze_e7.py <E7 rows.jsonl> <E6 rows.jsonl> <mhz>
"""
import json, math, statistics as st, sys

e7 = [json.loads(l) for l in open(sys.argv[1])]
e6 = [json.loads(l) for l in open(sys.argv[2])]
mhz = float(sys.argv[3])


def per_req(mode):
    out = {}
    for r in e7:
        if r.get('perf_mode') != mode or not r.get('valid') or not r.get('perf_per_req'):
            continue
        out.setdefault(r['leg'], []).append(r['perf_per_req'])
    return out


def med(vals, key):
    v = [x[key] for x in vals if x.get(key) is not None]
    return st.median(v) if v else None


print('## Syscalls per request (nginx worker, perf stat, median of reps)\n')
sc = per_req('sc')
keys = ['raw_syscalls:sys_enter', 'syscalls:sys_enter_sched_setaffinity', 'syscalls:sys_enter_sched_getaffinity',
        'syscalls:sys_enter_getuid', 'syscalls:sys_enter_getgid', 'syscalls:sys_enter_getpid',
        'syscalls:sys_enter_gettid', 'syscalls:sys_enter_process_vm_readv']
print('| leg | n | ' + ' | '.join(k.split(':')[1].replace('sys_enter_', '') for k in keys) + ' |')
print('|---|---:|' + '---:|' * len(keys))
base = med(sc.get('B', []), 'raw_syscalls:sys_enter')
for leg, vals in sc.items():
    print(f'| {leg} | {len(vals)} | ' + ' | '.join(f'{med(vals, k):.3f}' if med(vals, k) is not None else '' for k in keys) + ' |')
if base is not None:
    print(f'\nBaseline (B) syscalls per request: {base:.3f}; extra per request = sys_enter - baseline.')

print('\n## Cycles per request (nginx worker; cpu_core user + kernel)\n')
cy = per_req('cyc')
ck = [k for k in (cy.get('B') or [{}])[0].keys()]
def cyc(vals):
    tot = []
    for x in vals:
        u = x.get('cpu_core/cycles/u') or x.get('cycles:u') or x.get('cycles/u')
        k = x.get('cpu_core/cycles/k') or x.get('cycles:k') or x.get('cycles/k')
        if u is not None and k is not None:
            tot.append((u, k, x.get('cpu_core/instructions/u') or x.get('instructions:u'), x.get('cpu_core/instructions/k') or x.get('instructions:k')))
    return tot
print('| leg | n | cycles user | cycles kernel | total | insn user | insn kernel | us/req at %.0f MHz |' % mhz)
print('|---|---:|---:|---:|---:|---:|---:|---:|')
cyt = {}
for leg, vals in cy.items():
    t = cyc(vals)
    if not t:
        continue
    u = st.median(a for a, _, _, _ in t); k = st.median(b for _, b, _, _ in t)
    iu = st.median(c for _, _, c, _ in t if c is not None) if any(c is not None for _, _, c, _ in t) else float('nan')
    ik = st.median(d for _, _, _, d in t if d is not None) if any(d is not None for _, _, _, d in t) else float('nan')
    cyt[leg] = u + k
    print(f'| {leg} | {len(t)} | {u:.0f} | {k:.0f} | {u + k:.0f} | {iu:.0f} | {ik:.0f} | {(u + k) / mhz:.3f} |')

print('\n## Cross-method closure: delta cycles/req / f vs E6 throughput delta x (us/req)\n')
by = {}
for r in e6:
    if r.get('valid'):
        by.setdefault((r['round'], r['rep']), {})[r['leg']] = r
def dx(a, b):
    v = [1e6 / L[a]['rps'] - 1e6 / L[b]['rps'] for L in by.values() if a in L and b in L]
    return st.fmean(v) if v else None
pairs = [('K@S1', 'B'), ('K@S3', 'K@S1'), ('K@S4', 'K@S3'), ('Odiag@S1', 'B'), ('Odiag@S2h', 'Odiag@S1'),
         ('Odiag@S4', 'Odiag@S2h'), ('Odiag@S4', 'Odiag@S4+NO_PIN'), ('Odiag@S4', 'Odiag@S4+ALL3'),
         ('D@S1', 'B'), ('D@S4', 'D@S1'), ('Dpc@S4', 'D@S4')]
print('| step | delta cycles/req -> us | E6 delta x us/req | ratio |')
print('|---|---:|---:|---:|')
for a, b in pairs:
    if a in cyt and b in cyt:
        c = (cyt[a] - cyt[b]) / mhz
        x = dx(a, b)
        xs = f'{x:.3f}' if x is not None else 'n/a (not in E6)'
        ratio = f'{c / x:.2f}' if x else ''
        print(f'| {a} - {b} | {c:.3f} | {xs} | {ratio} |')

print('\n## Kernel BPF stats (run_cnt and run_time per request)\n')
for r in e7:
    if r.get('bpf_stats') and r.get('valid'):
        req = r['req_nginx']
        s = {x['prog']: x for x in r['bpf_stats']}
        hits = sum(x['run_cnt'] for x in r['bpf_stats']) / req
        ns = sum(x['run_time_ns'] for x in r['bpf_stats']) / req
        print(f"{r['leg']} rep {r['rep']}: hits/req {hits:.3f}, program time {ns:.0f} ns/req; " +
              ', '.join(f"{p}: {x['run_cnt'] / req:.3f} hits, {x['run_time_ns'] / max(x['run_cnt'], 1):.0f} ns/hit" for p, x in s.items() if x['run_cnt']))
