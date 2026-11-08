#!/usr/bin/env python3
"""Is the AOT code slower than JIT code? (startup-20261108/aot-ab)

measure_mt.py's loop for two tools whose 1- and 6-thread cells came out 2.2-2.5% slower on the
AOT cache build: old (dev 9c088441), new (dev 9898ed19), noaot = the same binaries
(builds/dev-9898ed19-noaot, hard links) with BPFTIME_AOT_CACHE=0 in the tool, so agents compile
as before. audit.env_prefix is wrapped to add the variable; nothing else changes."""
import json, sys, time
from pathlib import Path
sys.path.insert(0, '/home/jetson/src/realprog-20261026-btf/harness')
import audit
import measure_mt

NOAOT = 'dev-9898ed19-noaot'
_orig = audit.env_prefix
def env_prefix(backend, role):
    pre = _orig(backend, role)
    return pre + ['BPFTIME_AOT_CACHE=0'] if backend == NOAOT and role == 'tool' else pre
audit.env_prefix = env_prefix

OUT = Path(__file__).resolve().parent / 'ab.jsonl'
backends = ['dev-9c088441-probecheck', 'dev-9898ed19-probecheck', NOAOT]
tools = ['funclatency-libbpf', 'bpftrace-uretprobe-hist']
for r in range(5):
    order = backends[r % 3:] + backends[:r % 3]
    for threads in (1, 6):
        audit.WL = measure_mt.MT / f't{threads}'
        for tool in tools:
            for backend in order:
                audit.cleanup(backend)
                t0 = time.time()
                try:
                    res, wout, tout = audit.TOOLS[tool](backend, 10)
                except Exception as e:
                    res = dict(error=repr(e))
                audit.cleanup(backend)
                res.update(tool=tool, backend=backend, threads=threads, round=r,
                           seconds=round(time.time() - t0, 1))
                with OUT.open('a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)
