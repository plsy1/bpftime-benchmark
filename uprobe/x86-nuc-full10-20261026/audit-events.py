#!/usr/bin/env python3
"""Check that every leg of the uprobe suite really runs its probes (no timing claims).

For each leg (no probe, kernel, official, dev) start the loader exactly as
benchmark.py does, run benchmark/test once (1 thread, 100000 iterations) pinned
to the given CPU, and require every one of the 17 cases to be slower than the
unprobed run by at least MIN_EXTRA_NS. The dev leg must also log array JIT inlining.
Usage: audit-events.py <cpu>
"""
import json, os, re, subprocess, sys, time
from pathlib import Path

CPU = sys.argv[1]
RUN = Path('/home/y/src/bpftime-x86-runs/full10-20261026')
OUT = Path(__file__).resolve().parent / 'event-audit'
OUT.mkdir(exist_ok=False)
BENCH = Path('/home/y/src/bpftime-wt-uprobe-177a1dc/benchmark')
MIN_EXTRA_NS = 5.0
base = {k: v for k, v in os.environ.items() if not k.startswith('BPFTIME_') and k != 'LD_PRELOAD'}
assert not Path('/dev/shm/bpftime_maps_shm').exists(), 'another bpftime run is active'

def cmd(env, *argv):
    return ['sudo', '-n', 'taskset', '-c', CPU, 'env'] + [f'{k}={v}' for k, v in env.items()] + [str(a) for a in argv]

def stop(p):
    if p is None or p.poll() is not None:
        return
    subprocess.run(['sudo', '-n', 'kill', '-TERM', '--', f'-{p.pid}'], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        p.wait(timeout=15)
    except subprocess.TimeoutExpired:
        subprocess.run(['sudo', '-n', 'kill', '-KILL', '--', f'-{p.pid}'], check=True)
        p.wait(timeout=10)

def parse(text):
    s = re.findall(r'Benchmarking (__bench_\w+) in thread 1\nAverage time usage ([\d.]+) ns', text)
    return {k: float(v) for k, v in s}

def leg(name, loader_env, victim_env, art):
    loader = None
    try:
        if loader_env is not None:
            log = (OUT / f'{name}-loader.log').open('w')
            loader = subprocess.Popen(cmd(loader_env, BENCH / 'uprobe/uprobe'), cwd=BENCH.parent, env=base,
                                      stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            deadline = time.monotonic() + 40
            while 'Successfully started!' not in (OUT / f'{name}-loader.log').read_text():
                assert loader.poll() is None, f'{name}: loader exited'
                assert time.monotonic() < deadline, f'{name}: loader start timed out'
                time.sleep(0.1)
            time.sleep(2)
        p = subprocess.run(cmd(victim_env, BENCH / 'test', 1, 100000), cwd=BENCH.parent, env=base,
                           capture_output=True, text=True, timeout=300)
        (OUT / f'{name}-victim.log').write_text(p.stdout + p.stderr)
        assert p.returncode == 0, (name, p.returncode)
        if loader is not None:
            assert loader.poll() is None, f'{name}: loader died during the run'
        return parse(p.stdout + p.stderr), p.stdout + p.stderr
    finally:
        stop(loader)
        if art is not None:
            subprocess.run(['sudo', '-n', str(RUN / 'artifacts' / art / 'tools/bpftimetool/bpftimetool'), 'remove'],
                           cwd=BENCH.parent, env=base, check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

results, ok = {}, True
results['none'], _ = leg('none', None, {}, None)
results['kernel'], _ = leg('kernel', {}, {}, None)
logs = {}
for art in ('official', 'dev'):
    a = RUN / 'artifacts' / art
    results[art], logs[art] = leg(art,
        {'BPFTIME_LOG_OUTPUT': 'console', 'LD_PRELOAD': a / 'runtime/syscall-server/libbpftime-syscall-server.so'},
        {'BPFTIME_LOG_OUTPUT': 'console', 'SPDLOG_LEVEL': 'info', 'LD_PRELOAD': a / 'runtime/agent/libbpftime-agent.so'},
        art)
cases = sorted(results['none'])
assert len(cases) == 17, cases
print(f"{'case':38s} " + ' '.join(f'{k:>9s}' for k in results))
for c in cases:
    row = {k: results[k].get(c) for k in results}
    flag = ''
    for k in ('kernel', 'official', 'dev'):
        if row[k] is None or row[k] < row['none'] + MIN_EXTRA_NS:
            flag += f' NO-EVENT?{k}'
            ok = False
    print(f'{c:38s} ' + ' '.join(f'{(v if v is not None else float("nan")):9.2f}' for v in row.values()) + flag)
inline = logs['dev'].count('[array-jit-inline] inlined ')
print(f'dev array-jit-inline log lines: {inline}')
ok = ok and inline >= 2
(OUT / 'summary.json').write_text(json.dumps({'results_ns': results, 'dev_inline_lines': inline,
                                              'min_extra_ns': MIN_EXTRA_NS, 'passed': ok}, indent=1))
print('PASSED' if ok else 'FAILED')
sys.exit(0 if ok else 1)
