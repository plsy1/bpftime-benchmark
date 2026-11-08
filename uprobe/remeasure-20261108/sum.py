#!/usr/bin/env python3
"""Tables for remeasure-20261108 (medians over reps/rounds).
Output: ns/call/thread = 1e9 * threads / ops_per_sec; total = whole probe hit; out = minus the
control object's (output-dims summarize.py's method); delivered = received / hits; the 11-06
fix-final column (output-dims m1-steady.jsonl, same settings) for reference.
Real tools: ops/s summed over threads, 'reported/expected' the events the tool counted."""
import collections, json, statistics as st
from pathlib import Path
D = Path(__file__).resolve().parent
OD = Path('/home/jetson/src/results/uprobe/output-dims-20261106')
CUR = 'dev-9c088441-probecheck'
med = lambda v: st.median(v) if v else None
f0 = lambda x: '-' if x is None else f'{x:.0f}'

def load(p):
    return [json.loads(l) for l in open(p)]

def out():
    rows = load(D / 'output-threads.jsonl') + [dict(r, backend='fix-final(11-06)') for r in load(OD / 'm1-steady.jsonl') if r.get('backend') == 'fix-final']
    bad = [r for r in rows if not r.get('ok') and r['backend'] != 'fix-final(11-06)']
    g = collections.defaultdict(list)
    for r in rows:
        if r.get('ok') and r.get('ops_per_sec'):
            g[(r['obj'], r['threads'], r['backend'])].append(r)
    print(f'## Output cost per call ({len(load(D / "output-threads.jsonl"))} runs, {len(bad)} not ok)\n')
    print('| path | thr | bpf total / out ns | bpftime now total / out ns | out now / bpf | 11-06 out / bpf(11-06) | bpf deliv | now deliv |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|')
    for obj in ('out-perf-8.bpf.o', 'out-rbout-8.bpf.o', 'out-rbres-8.bpf.o'):
        for t in (1, 2, 4, 6):
            def cell(b, src=None):
                rs = g.get((obj, t, b), []); cs = g.get(('out-control.bpf.o', t, b), [])
                ns = med([1e9 * t / r['ops_per_sec'] for r in rs]); cns = med([1e9 * t / r['ops_per_sec'] for r in cs])
                dl = med([r['delivered'] for r in rs if r.get('delivered') is not None])
                return ns, (None if ns is None or cns is None else ns - cns), dl
            k, u, old = cell('kernel'), cell(CUR), cell('fix-final(11-06)')
            # 11-06 kernel from the same m1 file
            m1k = [r for r in load(OD / 'm1-steady.jsonl') if r.get('backend') == 'kernel' and r.get('ok') and r.get('ops_per_sec')]
            kk = med([1e9 * t / r['ops_per_sec'] for r in m1k if r['obj'] == obj and r['threads'] == t])
            kc = med([1e9 * t / r['ops_per_sec'] for r in m1k if r['obj'] == 'out-control.bpf.o' and r['threads'] == t])
            oldk = None if kk is None or kc is None else kk - kc
            r_now = None if not k[1] or u[1] is None else u[1] / k[1]
            r_old = None if not oldk or old[1] is None else old[1] / oldk
            pct = lambda x: '-' if x is None else f'{100*x:.2f}%'
            print(f'| {obj[4:-6]} | {t} | {f0(k[0])} / {f0(k[1])} | {f0(u[0])} / {f0(u[1])} | '
                  f'{"-" if r_now is None else f"{r_now:.2f}"} | {"-" if r_old is None else f"{r_old:.2f}"} | {pct(k[2])} | {pct(u[2])} |')
    for t in (1, 2, 4, 6):
        k = med([1e9 * t / r['ops_per_sec'] for r in g.get(('out-control.bpf.o', t, 'kernel'), [])])
        u = med([1e9 * t / r['ops_per_sec'] for r in g.get(('out-control.bpf.o', t, CUR), [])])
        print(f'| control (probe only) | {t} | {f0(k)} | {f0(u)} | | | | |')
    print()

def tools():
    rows = load(D / 'realtools-mt.jsonl')
    g = collections.defaultdict(list)
    for r in rows:
        g[(r['tool'], r['threads'], r['backend'])].append(r)
    old = collections.defaultdict(list)
    for r in load(Path('/home/jetson/src/results/uprobe/vs-pre-mt-20261104/measure-mt/measure.jsonl')):
        old[(r['tool'], r['threads'], r['backend'])].append(r)
    print('## Real tools on threads (ops/s summed, median of 3)\n')
    print('| tool | thr | bpf | bpftime now | now / bpf | 11-04 dev-ff5515d8 / bpf(11-04) | events reported/expected (now) |')
    print('|---|---:|---:|---:|---:|---:|---|')
    for tool in sorted({k[0] for k in g}):
        for t in (1, 2, 4, 6):
            k = med([r['ops_per_sec'] for r in g[(tool, t, 'kernel')] if r.get('ops_per_sec')])
            u = med([r['ops_per_sec'] for r in g[(tool, t, CUR)] if r.get('ops_per_sec')])
            ok_ = med([r['ops_per_sec'] for r in old[(tool, t, 'kernel')] if r.get('ops_per_sec')])
            ou = med([r['ops_per_sec'] for r in old[(tool, t, 'dev-ff5515d8')] if r.get('ops_per_sec')])
            ev = [f"{r.get('reported')}/{r.get('expected')}" for r in g[(tool, t, CUR)]]
            rc = [r.get('workload_rc') for r in g[(tool, t, CUR)] + g[(tool, t, 'kernel')]]
            print(f'| {tool} | {t} | {k/1e3:.0f}k | {u/1e3:.0f}k | {u/k:.2f} | '
                  f'{"-" if not ok_ or not ou else f"{ou/ok_:.2f}"} | {", ".join(ev)}{"" if set(rc) == {0} else " rc " + str(rc)} |')
    print()

out(); tools()
