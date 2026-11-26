#!/usr/bin/env python3
"""Throughput of real-tool workloads under no tracing, kernel eBPF and bpftime builds.

Reuses the tool runners of audit.py: each run starts the tool, runs the workload for SECS seconds
and records the workload's median ops/s plus the tool's event count (so a fast but broken run is
visible). Backend "none" runs the workload alone (same sudo env, no preload). Backends are rotated
each round so drift does not favour one of them. One JSON line per run goes to OUT.
"""
import argparse, hashlib, json, random, sys, time
from pathlib import Path
# J2 (ATC P2', 11-25): copy of prep/E14/measure.py; only change: per (round, tool) the backend order is a
# random permutation seeded by sha256(session:round:tool) (was: rotation per round, tool outer loop unchanged).
# Paper campaign copy (prep/E14) of ~/src/realprog-20261026-btf/harness/measure.py. Changed only: the
# audit module is the campaign copy in prep/E12 (frozen builds/<arm>/, official 4708f409 examples).
# x86 NUC copy (E6, 2026-11-26) of the Jetson's prep/J2/measure.py: only the audit import path changed (x86 audit.py
# next to this file, with the x86 paths and CPU pinning) and --first-round (one round per session plan line, same
# seeds as one call with --rounds 5).
sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit  # noqa: E402

BASE = {
    'gethostlatency': lambda s: [str(audit.WL / 'resolveloop'), str(s)],
    'malloc-example': lambda s: [str(audit.WL / 'mallocloop'), str(s), '64', '1024'],
    'funclatency-bpftime-example': lambda s: [str(audit.WL / 'callloop'), str(s)],
    'funclatency-libbpf': lambda s: [str(audit.WL / 'callloop'), str(s)],
    'memleak': lambda s: [str(audit.WL / 'mallocloop'), str(s), '64', '1024'],
    'bpftrace-uprobe-count': lambda s: [str(audit.WL / 'callloop'), str(s)],
    'bpftrace-uretprobe-hist': lambda s: [str(audit.WL / 'callloop'), str(s)],
    'bpftrace-gethostlatency': lambda s: [str(audit.WL / 'resolveloop'), str(s)],
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tools', default=','.join(BASE))
    ap.add_argument('--backends', default='none,kernel,official,a7d54d7e')
    ap.add_argument('--rounds', type=int, default=5)
    ap.add_argument('--first-round', type=int, default=0)  # x86: a session runs one round per plan line
    ap.add_argument('--secs', type=int, default=10)
    ap.add_argument('--out', required=True)
    ap.add_argument('--session', required=True)
    a = ap.parse_args()
    out = Path(a.out)
    raw = out.with_suffix('')
    raw.mkdir(parents=True, exist_ok=True)
    backends = a.backends.split(',')
    for r in range(a.first_round, a.first_round + a.rounds):
        for tool in a.tools.split(','):
            seed = int(hashlib.sha256(f'{a.session}:{r}:{tool}'.encode()).hexdigest(), 16)
            order = list(backends)
            random.Random(seed).shuffle(order)
            for backend in order:
                audit.cleanup(backend if backend != 'none' else 'kernel')
                t0 = time.time()
                if backend == 'none':
                    rc, wout = audit.workload(BASE[tool](a.secs), 'kernel')
                    res, tout = dict(workload_rc=rc, ops_per_sec=audit.ops(wout)), ''
                else:
                    res, wout, tout = audit.TOOLS[tool](backend, a.secs)
                audit.cleanup(backend if backend != 'none' else 'kernel')
                res.update(tool=tool, backend=backend, round=r, order=order.index(backend), session=a.session, seconds=round(time.time() - t0, 1))
                stem = f'{tool}.{backend}.r{r}'
                (raw / f'{stem}.workload.txt').write_text(wout)
                if len(tout) > 2_000_000:
                    tout = tout[:200_000] + f'\n[... {len(tout)} chars, trimmed ...]\n' + tout[-200_000:]
                (raw / f'{stem}.tool.txt').write_text(tout)
                with out.open('a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)

if __name__ == '__main__':
    main()
