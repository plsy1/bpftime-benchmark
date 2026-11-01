#!/usr/bin/env python3
"""The official uprobe suite's userspace leg (benchmark/test with the agent, benchmark/uprobe/uprobe
with the syscall server, as benchmark.py runs them) for several frozen builds, order rotated each
rep, one loader per build per rep. All 17 cases are recorded; the hash cases are the subject.
Run as root under taskset -c 5. Runner binaries: ../full10-6ab9cbfe/full-runner (same as 10/26)."""
import argparse, json, os, re, subprocess, time
from pathlib import Path

RUNNER = Path('/home/jetson/src/results/uprobe/dev-6ab9cbfe-20261101/full10-6ab9cbfe/full-runner/bpftime')
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')
CASES = 17
PERF_ARGS = ['record', '-e', 'cycles:u']
PERF = None  # --perf: record cycles:u of the victim (profiling only, not for timing)

def run_one(build, iters, logdir, tag):
    b = BUILDS / build
    subprocess.run([str(b / 'bpftimetool'), 'remove'], capture_output=True)
    log = logdir / f'{tag}-loader.log'
    env = {'PATH': os.environ['PATH'], 'SPDLOG_LEVEL': 'debug',
           'LD_PRELOAD': str(b / 'libbpftime-syscall-server.so')}
    with log.open('w') as f:
        srv = subprocess.Popen([str(RUNNER / 'benchmark/uprobe/uprobe')], cwd=RUNNER, env=env,
                               stdout=f, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.monotonic() + 30
        while 'Successfully started!' not in log.read_text(errors='replace'):
            assert srv.poll() is None, f'{build}: loader exited'
            assert time.monotonic() < deadline, f'{build}: loader not ready'
            time.sleep(0.1)
        time.sleep(2)
        venv = {'PATH': os.environ['PATH'], 'LD_PRELOAD': str(b / 'libbpftime-agent.so'),
                'BPFTIME_LOG_OUTPUT': 'console', 'BPFTIME_ARRAY_JIT_INLINE': '0'}
        if os.environ.get('FOCUS'):
            venv['FOCUS'] = os.environ['FOCUS']  # focus runner: which cases
        cmd = [str(RUNNER / 'benchmark/test'), '1', str(iters)]
        if PERF:
            # perf itself must not run under the agent: preload only the target.
            cmd = [PERF] + PERF_ARGS + ['-o', str(logdir / f'{tag}.perf.data'), '--',
                   'env', f'LD_PRELOAD={venv.pop("LD_PRELOAD")}'] + cmd
        r = subprocess.run(cmd, cwd=RUNNER, env=venv,
                           capture_output=True, text=True, timeout=600)
        (logdir / f'{tag}-victim.log').write_text(r.stdout + r.stderr)
        assert r.returncode == 0, f'{build}: victim rc {r.returncode}'
        samples = dict((n, float(v)) for n, v in re.findall(
            r'Benchmarking (__bench_\w+) in thread 1\nAverage time usage ([\d.]+) ns', r.stdout))
        assert len(samples) == CASES, f'{build}: {len(samples)} cases'
        return samples
    finally:
        if srv.poll() is None:
            os.killpg(srv.pid, 15)
            try: srv.wait(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(srv.pid, 9); srv.wait()
        subprocess.run([str(b / 'bpftimetool'), 'remove'], capture_output=True)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--builds', required=True)
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--iters', type=int, default=100000)
    ap.add_argument('--out', required=True)
    ap.add_argument('--perf')
    ap.add_argument('--runner', help='runner root with benchmark/test and benchmark/uprobe (default: the full suite)')
    ap.add_argument('--cases', type=int, default=17, help='cases the victim prints')
    ap.add_argument('--perf-args', default='record -e cycles:u', help='perf subcommand and options')
    a = ap.parse_args()
    global PERF, RUNNER, CASES, PERF_ARGS
    PERF = a.perf
    CASES = a.cases
    PERF_ARGS = a.perf_args.split()
    if a.runner:
        RUNNER = Path(a.runner)
    builds = a.builds.split(',')
    out = Path(a.out).resolve(); logdir = out.parent / (out.stem + '-logs'); logdir.mkdir(exist_ok=True)
    assert not Path('/dev/shm/bpftime_maps_shm').exists(), 'another run is using the shm'
    for rep in range(a.reps):
        order = builds[rep % len(builds):] + builds[:rep % len(builds)]
        for build in order:
            s = run_one(build, a.iters, logdir, f'r{rep + 1}-{build}')
            with out.open('a') as f:
                f.write(json.dumps(dict(rep=rep + 1, build=build, iters=a.iters, samples=s)) + '\n')
            print(f'rep {rep + 1} {build} ' + ' '.join(f'{k[8:]} {v:.0f}' for k, v in s.items()), flush=True)

if __name__ == '__main__':
    main()
