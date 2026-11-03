#!/usr/bin/env python3
"""Real-tool throughput at 1, 2, 4 and 6 workload threads (2026-11-04).

As measure.py, but the workloads are the multi-thread builds in workloads-mt/t<N> (same file
names, so audit.py's tool runners attach to them unchanged): audit.WL is pointed at the thread
count's directory for each run. Backends rotated per round; one JSON line per run (with
"threads") to OUT.
"""
import argparse, json, time
from pathlib import Path
import audit

MT = Path(__file__).resolve().parent.parent / 'workloads-mt'
BASE = {
    'malloc-example': lambda s: ['mallocloop', str(s), '64', '1024'],
    'memleak': lambda s: ['mallocloop', str(s), '64', '1024'],
    'funclatency-libbpf': lambda s: ['callloop', str(s)],
    'funclatency-bpftime-example': lambda s: ['callloop', str(s)],
    'bpftrace-uretprobe-hist': lambda s: ['callloop', str(s)],
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tools', default=','.join(BASE))
    ap.add_argument('--backends', default='none,kernel,dev-a4ac0d31,dev-7ad5a152')
    ap.add_argument('--threads', default='1,2,4,6')
    ap.add_argument('--rounds', type=int, default=3)
    ap.add_argument('--secs', type=int, default=10)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    out = Path(a.out)
    raw = out.with_suffix('')
    raw.mkdir(parents=True, exist_ok=True)
    backends = a.backends.split(',')
    for r in range(a.rounds):
        order = backends[r % len(backends):] + backends[:r % len(backends)]
        for threads in [int(t) for t in a.threads.split(',')]:
            audit.WL = MT / f't{threads}'
            for tool in a.tools.split(','):
                for backend in order:
                    audit.cleanup(backend if backend != 'none' else 'kernel')
                    t0 = time.time()
                    if backend == 'none':
                        cmd = BASE[tool](a.secs)
                        rc, wout = audit.workload([str(audit.WL / cmd[0])] + cmd[1:], 'kernel')
                        res, tout = dict(workload_rc=rc, ops_per_sec=audit.ops(wout)), ''
                    else:
                        try:
                            res, wout, tout = audit.TOOLS[tool](backend, a.secs)
                        except Exception as e:  # keep measuring the rest
                            res, wout, tout = dict(error=repr(e)), '', ''
                    audit.cleanup(backend if backend != 'none' else 'kernel')
                    res.update(tool=tool, backend=backend, threads=threads, round=r,
                               seconds=round(time.time() - t0, 1))
                    stem = f'{tool}.{backend}.t{threads}.r{r}'
                    (raw / f'{stem}.workload.txt').write_text(wout)
                    if len(tout) > 2_000_000:
                        tout = tout[:200_000] + f'\n[... {len(tout)} chars, trimmed ...]\n' + tout[-200_000:]
                    (raw / f'{stem}.tool.txt').write_text(tout)
                    with out.open('a') as f:
                        f.write(json.dumps(res) + '\n')
                    print(json.dumps(res), flush=True)

if __name__ == '__main__':
    main()
