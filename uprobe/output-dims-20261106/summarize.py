#!/usr/bin/env python3
"""Tables for output-dims-20261106: medians over reps per (matrix setting, object, threads, backend).
ns/call/thread = 1e9 * threads / ops_per_sec (steady runs); output cost = that minus the control
object's; delivered = received / hits."""
import collections, json, statistics as st, sys
from pathlib import Path
D = Path(__file__).resolve().parent
BACKENDS = ('kernel', 'fix-final')


def load(name):
    p = D / name
    return [json.loads(l) for l in open(p)] if p.exists() else []


def med(v):
    return st.median(v) if v else None


def fmt(x, f='{:.0f}'):
    return '-' if x is None else f.format(x)


def m1():
    rows = load('m1-steady.jsonl')
    bad = [r for r in rows if not r.get('ok')]
    g = collections.defaultdict(list)
    for r in rows:
        if r.get('ok') and r.get('ops_per_sec'):
            g[(r['obj'], r['threads'], r['backend'])].append(r)
    print(f'## M1 steady ({len(rows)} runs, {len(bad)} not ok)\n')
    print('| path | size | thr | kernel ns | bpftime ns | kernel out | bpftime out | out ratio | kernel deliv | bpftime deliv |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
    objs = sorted({k[0] for k in g if 'control' not in k[0]},
                  key=lambda o: (o.split('-')[1], int(o.split('-')[2].split('.')[0])))
    for obj in objs:
        path, size = obj.split('-')[1], obj.split('-')[2].split('.')[0]
        for t in (1, 2, 4, 6):
            cells = {}
            for b in BACKENDS:
                rs = g.get((obj, t, b), [])
                cs = g.get(('out-control.bpf.o', t, b), [])
                ns = med([1e9 * t / r['ops_per_sec'] for r in rs])
                cns = med([1e9 * t / r['ops_per_sec'] for r in cs])
                dl = med([r.get('delivered') for r in rs if r.get('delivered') is not None])
                cells[b] = (ns, None if ns is None or cns is None else ns - cns, dl)
            k, u = cells['kernel'], cells['fix-final']
            ratio = None if not k[1] or u[1] is None else u[1] / k[1]
            print(f'| {path} | {size} | {t} | {fmt(k[0])} | {fmt(u[0])} | {fmt(k[1])} | {fmt(u[1])} | '
                  f'{fmt(ratio, "{:.2f}")} | {fmt(None if k[2] is None else 100*k[2], "{:.2f}%")} | '
                  f'{fmt(None if u[2] is None else 100*u[2], "{:.2f}%")} |')
    print()


def m23(name, title, keyf):
    rows = load(name)
    bad = [r for r in rows if not r.get('ok')]
    g = collections.defaultdict(list)
    for r in rows:
        if r.get('ok'):
            g[keyf(r) + (r['backend'],)].append(r)
    print(f'## {title} ({len(rows)} runs, {len(bad)} not ok)\n')
    print('| setting | kernel deliv | bpftime deliv | kernel burst/ns | bpftime burst/ns |')
    print('|---|---:|---:|---:|---:|')
    for key in sorted({k[:-1] for k in g}):
        cells = {}
        for b in BACKENDS:
            rs = g.get(key + (b,), [])
            dl = med([r['delivered'] for r in rs if r.get('delivered') is not None])
            bu = med([r['burst_us'] for r in rs if r.get('burst_us')])
            ns = med([1e9 * r['threads'] / r['ops_per_sec'] for r in rs if r.get('ops_per_sec')])
            cells[b] = (dl, bu if bu else ns)
        k, u = cells['kernel'], cells['fix-final']
        print(f'| {" ".join(str(x) for x in key)} | {fmt(None if k[0] is None else 100*k[0], "{:.1f}%")} | '
              f'{fmt(None if u[0] is None else 100*u[0], "{:.1f}%")} | {fmt(k[1], "{:.1f}")} | {fmt(u[1], "{:.1f}")} |')
    print()


m1()
m23('m2-burst.jsonl', 'M2 bursts, consumer 2 us per record (setting: obj burst gap_ns threads; burst time in us)',
    lambda r: (r['obj'].replace('.bpf.o', ''), r['burst'], r['gap_ns'], r['threads']))
m23('m3-overload.jsonl', 'M3 steady, consumer 2 us per record (setting: obj threads; ns per call)',
    lambda r: (r['obj'].replace('.bpf.o', ''), r['threads']))
