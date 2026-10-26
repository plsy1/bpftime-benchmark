#!/usr/bin/env python3
"""funclatency (libbpf-tools) with and without a cgroup filter (-c), on kernel and bpftime builds.
Prints workload calls (ops/s x secs) against the histogram total for each case."""
import json, sys, time
sys.path.insert(0, '/home/jetson/src/realprog-20261025/harness')
import audit

def run(backend, cgroup, secs=4):
    audit.cleanup(backend)
    args = [str(audit.LT / 'funclatency'), '-u', '-d', str(secs + 6)]
    if cgroup:
        args += ['-c', cgroup]
    p = audit.start_tool(args + [f'{audit.WL}/callloop:target_func'], backend)
    time.sleep(3)
    rc, wout = audit.workload([str(audit.WL / 'callloop'), str(secs)], backend)
    tout = audit.stop_tool(p, wait=secs + 10)
    audit.cleanup(backend)
    rate = audit.ops(wout)
    return dict(backend=backend, cgroup=cgroup, workload_rc=rc, ops_per_sec=rate,
                expected=round(rate * secs) if rate else None, reported=audit.hist_total(tout),
                errors=sum(1 for l in (wout + tout).splitlines() if '[error]' in l))

for backend in sys.argv[1].split(','):
    for cg in (None, '/sys/fs/cgroup/user.slice', '/sys/fs/cgroup/init.scope'):
        print(json.dumps(run(backend, cg)), flush=True)
