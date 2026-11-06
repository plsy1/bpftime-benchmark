#!/usr/bin/env python3
"""Start-up cost of a traced process (more-dims-20261106/n2-startup).

src/spawnloop starts src/shortlived N times one after another and reports the wall time from
just before posix_spawn to the reaped exit. shortlived calls count_event once ("hit") or never
("nohit") and exits. Backends: none (no tool); kernel (outattach with OBJ on count_event: the
kernel arms the probe when the binary is mapped); fix-final (the tool under the syscall server,
children with the agent: each child loads the agent, maps the shared memory, compiles the
program and hooks the function before main); agent-noshm (children with the agent and no tool
running: the agent's own load). spawnloop itself is never traced. One JSON line per cell."""
import argparse, json, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

SRC = HERE / 'src'
OBJS = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src')


def run_cell(backend, obj, mode, n):
    tb = 'fix-final' if backend == 'agent-noshm' else backend
    common.cleanup(tb, ('outattach', 'shortlived', 'spawnloop'))
    res = dict(backend=backend, obj=obj, mode=mode, n=n)
    tool = None
    try:
        if backend in ('kernel', 'fix-final'):
            tool = common.Tool(backend, OBJS / obj, SRC / 'shortlived', ['on_count=count_event'])
            res['attach_s'] = round(tool.attach_s, 3)
        pre = []
        if backend in ('fix-final', 'agent-noshm'):
            pre = [f'CHILD_PRELOAD={common.BUILDS / "fix-final" / "libbpftime-agent.so"}']
        w = common.sh(['sudo', '-n', 'timeout', '600', 'env', *pre, str(SRC / 'spawnloop'), str(n),
                       str(SRC / 'shortlived'), mode])
        for k in ('failed', 'p50_us', 'p90_us', 'p99_us', 'mean_us', 'max_us'):
            res[k] = common.num(rf'^spawn_{k} ([\d.]+)$', w.stdout)
        if tool:
            text = tool.stop()
            res['hits'] = tool.sums(text)['sum0']
        expect = n if (mode == 'hit' and tool) else 0
        res['ok'] = (w.returncode == 0 and res['failed'] == 0 and
                     (tool is None or res['hits'] == expect))
        if not res['ok']:
            res['tail'] = (w.stdout + w.stderr)[-300:]
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        if tool:
            tool.kill()
        common.cleanup(tb, ('outattach', 'shortlived', 'spawnloop'))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--n', type=int, default=200)
    ap.add_argument('--reps', type=int, default=2)
    ap.add_argument('--objs', default='out-control.bpf.o,out-rbres-256.bpf.o,out-perf-256.bpf.o')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    cells = [('none', '-', m) for m in ('nohit', 'hit')]
    cells += [('agent-noshm', '-', m) for m in ('nohit', 'hit')]
    for obj in a.objs.split(','):
        for b in ('kernel', 'fix-final'):
            cells += [(b, obj, m) for m in ('nohit', 'hit')]
    for rep in range(a.reps):
        for backend, obj, mode in (cells if rep % 2 == 0 else cells[::-1]):
            t0 = time.time()
            # The agent without a tool takes about 3 s per start (smoke run): 10 starts.
            r = run_cell(backend, obj, mode, min(a.n, 10) if backend == 'agent-noshm' else a.n)
            r.update(rep=rep, seconds=round(time.time() - t0, 1))
            with open(a.out, 'a') as f:
                f.write(json.dumps(r) + '\n')
            print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
