#!/usr/bin/env python3
"""memleak on the 6-thread mallocloop, SIGUSR1 to the workload after 4 s: the agent's detach
worker runs perform_detach (gum_interceptor_detach) with mmap probed. Expect the workload to
finish its 10 s and print its ops. Usage: detach_mt.py BUILD OUT_JSONL (harness audit.py)."""
import json, sys, threading, time
from pathlib import Path
sys.path.insert(0, '/home/jetson/src/realprog-20261026-btf/harness')
import audit

build, out = sys.argv[1], Path(sys.argv[2])
# optional third argument: workload directory (default the 6-thread build)
wl = sys.argv[3] if len(sys.argv) > 3 else '/home/jetson/src/realprog-20261026-btf/workloads-mt/t6'
audit.WL = Path(wl)
for r in range(3):
    audit.cleanup(build)
    def usr1():
        time.sleep(4)
        audit.run(['sudo', '-n', 'pkill', '-USR1', '-x', 'mallocloop'])
    threading.Thread(target=usr1, daemon=True).start()
    t0 = time.time()
    try:
        res, tout, _ = audit.TOOLS['memleak'](build, 10)
    except Exception as e:
        res, tout = dict(error=repr(e)), ''
    audit.cleanup(build)
    res.update(tool='memleak-detach', backend=build, workload=wl, round=r, seconds=round(time.time() - t0, 1))
    (out.parent / f'detach.{build}.{Path(wl).name}.r{r}.txt').write_text(tout)
    with out.open('a') as f:
        f.write(json.dumps(res) + '\n')
    print(json.dumps(res), flush=True)
