#!/usr/bin/env python3
"""Processes starting at the same time (startup-20261108/concurrent-start; as remeasure-20261108).

A backend "<build>:noaot" runs the tool with BPFTIME_AOT_CACHE=0 (agents compile).

src/spawnburst starts K copies of n2-startup's shortlived ("hit": calls count_event once) at
once, R rounds, and reports each child's wall time (spawn to reaped exit), its CPU time, and
the round's wall time. Backends: none (no tool); kernel (outattach, out-control.bpf.o on
count_event); bpftime builds (the tool under the syscall server, children with the agent: each
loads the agent, opens the segment, compiles the program and hooks the function, holding
shm_lifecycle_lock from opening the segment until attaching is done). spawnburst is never
traced. Child wall far above its CPU time means waiting (a lock, or no free CPU)."""
import argparse, json, sys, time
from pathlib import Path

MD = Path('/home/jetson/src/results/uprobe/more-dims-20261106')
sys.path.insert(0, str(MD))
import common  # noqa: E402

HERE = Path(__file__).resolve().parent
BURST = Path('/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/spawnburst')
CHILD = MD / 'n2-startup' / 'src' / 'shortlived'
OBJ = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src/out-control.bpf.o')
NAMES = ('outattach', 'shortlived', 'spawnburst')


def run_cell(backend, k, rounds):
    build, _, variant = backend.partition(':')
    extra = ['BPFTIME_AOT_CACHE=0'] if variant == 'noaot' else []
    common.cleanup(build, NAMES)
    res = dict(backend=backend, k=k, rounds=rounds)
    tool = None
    try:
        if build != 'none':
            tool = common.Tool(build, OBJ, CHILD, ['on_count=count_event'], extra=extra)
            res['attach_s'] = round(tool.attach_s, 3)
        pre = []
        if build not in ('none', 'kernel'):
            pre = [f'CHILD_PRELOAD={common.BUILDS / build / "libbpftime-agent.so"}']
        w = common.sh(['sudo', '-n', 'timeout', '300', 'env', *pre, str(BURST), str(k),
                       str(rounds), str(CHILD), 'hit'])
        for line in w.stdout.splitlines():
            key, _, v = line.partition(' ')
            if key.startswith(('burst_', 'child_', 'round_')):
                res[key] = float(v)
        if tool:
            res['hits'] = tool.sums(tool.stop())['sum0']
        res['ok'] = (w.returncode == 0 and res.get('burst_failed') == 0 and
                     (tool is None or res['hits'] == k * rounds))
        if not res['ok']:
            res['tail'] = (w.stdout + w.stderr)[-300:]
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        if tool:
            tool.kill()
        common.cleanup(build, NAMES)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ks', default='1,2,4,8,16')
    ap.add_argument('--rounds', type=int, default=20)
    ap.add_argument('--reps', type=int, default=2)
    ap.add_argument('--backends', default='none,kernel,dev-9c088441-probecheck')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    cells = [(b, int(k)) for b in a.backends.split(',') for k in a.ks.split(',')]
    for rep in range(a.reps):
        for backend, k in (cells if rep % 2 == 0 else cells[::-1]):
            t0 = time.time()
            r = run_cell(backend, k, a.rounds)
            r.update(rep=rep, seconds=round(time.time() - t0, 1))
            with open(a.out, 'a') as f:
                f.write(json.dumps(r) + '\n')
            print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
