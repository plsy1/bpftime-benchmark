#!/usr/bin/env python3
"""Where a traced process's 22-28 ms of start-up go (improve-20261106, item 5e; only measured).
For fix-final-probecheck and dev-3f8981e4-probecheck: the control program attached to
shortlived's count_event by outattach under the syscall server, then
1. LD_DEBUG=statistics of one shortlived start under the agent (the dynamic loader's own time),
2. perf record -g over 200 starts (spawnloop), reported by DSO and by symbol,
3. the agent without a tool (no segment): perf over 50 starts.
perf.data in the scratchpad; reports here."""
import subprocess, sys
from pathlib import Path
sys.path.insert(0, '/home/jetson/src/results/uprobe/more-dims-20261106')
import common  # noqa: E402

HERE = Path(__file__).resolve().parent
SRC = Path('/home/jetson/src/results/uprobe/more-dims-20261106/n2-startup/src')
OBJ = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src/out-control.bpf.o')
PERF = '/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf'
SCR = Path('/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/startup')
SCR.mkdir(parents=True, exist_ok=True)


def report(data, name):
    for sort, suffix in (('dso', 'dso'), ('dso,sym', 'sym')):
        r = common.sh(['sudo', '-n', PERF, 'report', '-i', str(data), '--no-children', '--sort', sort,
                       '--stdio', '-g', 'none', '--comm', 'shortlived'])
        lines = [l for l in r.stdout.splitlines() if l.strip() and not l.startswith('#')]
        (HERE / f'{name}-{suffix}.txt').write_text('\n'.join(lines[:60]) + '\n')


for b in ('fix-final-probecheck', 'dev-3f8981e4-probecheck'):
    agent = common.BUILDS / b / 'libbpftime-agent.so'
    common.cleanup(b, ('outattach', 'shortlived', 'spawnloop'))
    tool = common.Tool(b, OBJ, SRC / 'shortlived', ['on_count=count_event'])
    try:
        r = common.sh(['sudo', '-n', 'env', f'LD_PRELOAD={agent}', 'LD_DEBUG=statistics',
                       str(SRC / 'shortlived'), 'nohit'])
        (HERE / f'{b}-ld-statistics.txt').write_text(r.stdout + r.stderr)
        data = SCR / f'{b}.data'
        r = common.sh(['sudo', '-n', PERF, 'record', '-g', '-F', '4999', '-o', str(data), '--',
                       'env', f'CHILD_PRELOAD={agent}', str(SRC / 'spawnloop'), '200',
                       str(SRC / 'shortlived'), 'nohit'])
        (HERE / f'{b}-spawn.txt').write_text(r.stdout + r.stderr[-2000:])
        report(data, b)
    finally:
        tool.kill()
        common.cleanup(b, ('outattach', 'shortlived', 'spawnloop'))
    data = SCR / f'{b}-noshm.data'
    r = common.sh(['sudo', '-n', PERF, 'record', '-g', '-F', '4999', '-o', str(data), '--',
                   'env', f'CHILD_PRELOAD={agent}', str(SRC / 'spawnloop'), '50',
                   str(SRC / 'shortlived'), 'nohit'])
    (HERE / f'{b}-noshm-spawn.txt').write_text(r.stdout + r.stderr[-2000:])
    report(data, f'{b}-noshm')
    print(b, 'done', flush=True)
