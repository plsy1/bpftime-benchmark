#!/usr/bin/env python3
"""Many probes in one process (more-dims-20261106/n4-many-probes): ../n2-startup/src/many-M has M
probe-able functions f0..f(M-1); the tool attaches out-control's program to every one (M = 1,
10, 100, 1000). Per cell: the tool's attach time (outattach start to "attached"); start-up of the
traced program (spawnloop, 50 starts of "many-M start", which calls nothing); the calls (many-M
run 3: f_i round robin on CPU 5, ns per call, its VmRSS at the end). Checks: hits == calls.
Backends none (untraced), kernel bpf, fix-final."""
import argparse, json, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

SRC = HERE.parent / 'n2-startup' / 'src'
OBJ = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src/out-control.bpf.o')


def run_cell(backend, m, secs, starts):
    common.cleanup(backend, ('outattach', f'many-{m}', 'spawnloop'))
    binary = SRC / f'many-{m}'
    res = dict(backend=backend, m=m)
    tool = None
    try:
        if backend != 'none':
            tool = common.Tool(backend, OBJ, binary, [f'on_count=f{i}' for i in range(m)],
                               shm_mb=512, timeout=600)
            res['attach_s'] = round(tool.attach_s, 3)
        pre = []
        if backend not in ('none', 'kernel'):
            pre = [f'CHILD_PRELOAD={common.BUILDS / backend / "libbpftime-agent.so"}']
        s = common.sh(['sudo', '-n', 'timeout', '900', 'env', *pre, str(SRC / 'spawnloop'),
                       str(starts), str(binary), 'start'])
        for k in ('failed', 'p50_us', 'p90_us', 'mean_us', 'max_us'):
            res[f'start_{k}'] = common.num(rf'^spawn_{k} ([\d.]+)$', s.stdout)
        w = common.sh(['timeout', str(secs + 600)] + common.env(backend, 'workload') +
                      [str(binary), 'run', str(secs)])
        res['total_ops'] = common.num(r'^total_ops (\d+)$', w.stdout)
        res['vmrss_kb'] = common.num(r'^vmrss_kb (\d+)$', w.stdout)
        if res['total_ops']:
            res['ns_per_call'] = round(secs * 1e9 / res['total_ops'], 1)
        if tool:
            text = tool.stop()
            res['hits'] = tool.sums(text)['sum0']
        res['ok'] = (w.returncode == 0 and res['start_failed'] == 0 and
                     (tool is None or res['hits'] == res['total_ops']))
        if not res['ok']:
            res['tail'] = (w.stdout + w.stderr)[-400:]
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        if tool:
            tool.kill()
        common.cleanup(backend, ('outattach', f'many-{m}', 'spawnloop'))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ms', default='1,10,100,1000')
    ap.add_argument('--backends', default='none,kernel,fix-final')
    ap.add_argument('--reps', type=int, default=2)
    ap.add_argument('--secs', type=int, default=3)
    ap.add_argument('--starts', type=int, default=50)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    for rep in range(a.reps):
        for m in [int(x) for x in a.ms.split(',')]:
            for backend in backends[rep % len(backends):] + backends[:rep % len(backends)]:
                t0 = time.time()
                r = run_cell(backend, m, a.secs, a.starts)
                r.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(r) + '\n')
                print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
