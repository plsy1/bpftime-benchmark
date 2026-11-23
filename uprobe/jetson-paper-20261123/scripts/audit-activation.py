#!/usr/bin/env python3
"""E0 activation audit (paper campaign, Jetson): for each frozen arm, load benchmark/uprobe under
the arm's syscall server, run benchmark/test 1 1000 under the arm's agent, require all 17 cases.
D-tree arms (D, Dpc) must also log array-jit-inline. Not timing data. Adapted from
baseline-177a1dc-jit-llvm16-20261021/audit-full-activation.py."""
import hashlib, json, os, re, subprocess, sys, time
from pathlib import Path
R = Path.home() / 'src/paper-runs-20261124'
runner = R / 'runner'
out = R / 'e0/activation'
out.mkdir(parents=True, exist_ok=True)
arms = sys.argv[1:] or ['O', 'Odiag', 'L3port', 'D', 'Opc', 'Dpc']
sel = dict(SPDLOG_LEVEL='info', BPFTIME_LOG_OUTPUT='console')
base = {k: v for k, v in os.environ.items() if not k.startswith('BPFTIME_') and k != 'LD_PRELOAD'}
def cmd(env, *a):
    return ['sudo', '-n', 'taskset', '-c', '5', 'env'] + [f'{k}={v}' for k, v in env.items()] + list(map(str, a))
def stop(p):
    if p is None or p.poll() is not None: return
    subprocess.run(['sudo', '-n', 'kill', '-TERM', '--', f'-{p.pid}'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try: p.wait(timeout=15)
    except subprocess.TimeoutExpired:
        subprocess.run(['sudo', '-n', 'kill', '-KILL', '--', f'-{p.pid}']); p.wait(timeout=10)
summary = {}
for arm in arms:
    b = R / 'builds' / arm
    d = out / arm; d.mkdir(exist_ok=False)
    rm = ['sudo', '-n', str(b / 'bpftimetool'), 'remove']
    subprocess.run(rm, cwd=runner, env=base, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert not any(f.startswith('bpftime') for f in os.listdir('/dev/shm')), 'shm left'
    proc = None; res = dict(arm=arm)
    try:
        with (d / 'loader.log').open('w') as log:
            proc = subprocess.Popen(cmd(dict(sel, LD_PRELOAD=b / 'libbpftime-syscall-server.so'), runner / 'benchmark/uprobe/uprobe'),
                                    cwd=runner, env=base, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            t = time.monotonic() + 40
            while 'Successfully started!' not in (d / 'loader.log').read_text():
                assert proc.poll() is None, 'loader exited'
                assert time.monotonic() < t, 'loader start timeout'
                time.sleep(.1)
            with (d / 'victim.log').open('w') as s:
                v = subprocess.Popen(cmd(dict(sel, LD_PRELOAD=b / 'libbpftime-agent.so'), runner / 'benchmark/test', 1, 1000),
                                     cwd=runner, env=base, stdout=s, stderr=subprocess.STDOUT, start_new_session=True)
                try: st = v.wait(timeout=60)
                finally: stop(v)
            assert st == 0, st
            assert proc.poll() is None, 'loader died'
        text = (d / 'victim.log').read_text()
        # with the dev AOT cache (default on) the inlining happens in the loader that compiles
        # the AOT objects, so the inline log lines are looked for in both logs
        both = text + (d / 'loader.log').read_text()
        samples = re.findall(r'Benchmarking (__bench_\w+) in thread 1\nAverage time usage ([\d.]+) ns, iter 1000 times', text)
        res.update(cases=len(dict(samples)), inlined=both.count('[array-jit-inline] inlined '),
                   ns={k: float(v) for k, v in samples})
        ok = res['cases'] == 17 and all(x > 0 for x in res['ns'].values())
        if arm in ('D', 'Dpc'):
            ok = ok and res['inlined'] >= 2 and bool(re.search(r'\[array-jit-inline\] program=array_map_look\w* retained 1 maps', both))
        res['passed'] = ok
    except Exception as e:
        res.update(passed=False, error=repr(e))
    finally:
        stop(proc)
        subprocess.run(rm, cwd=runner, env=base, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    summary[arm] = res
    print(arm, 'passed' if res['passed'] else 'FAILED', res.get('cases'), 'inlined', res.get('inlined'), res.get('error', ''), flush=True)
(out / 'summary.json').write_text(json.dumps(summary, indent=1) + '\n')
