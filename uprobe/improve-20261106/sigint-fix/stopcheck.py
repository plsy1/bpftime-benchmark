#!/usr/bin/env python3
"""SIGINT stop check for the epoll signal fix (improve-20261106, sigint-fix).

outattach (output-dims-20261106/src: main thread in pause(), consumer thread started with
SIGINT blocked, polling with 100 ms) with a ring buffer or perf object on burstloop: SIGINT the
loader's process group (as output-dims run.py) while burstloop runs (load: 4 threads, bursts
of 100 every 1 ms) or after it finished (idle), and time until the loader exits; still running
after 10 s counts as a hang (stacks kept, then killed). Then sslsniff (libbpf-tools,
single-threaded, perf buffer polled with 100 ms) with the syscall server alone: SIGINT after
2 s, time to exit. Build order alternates per rep. One JSON line per run.
"""
import argparse, json, random, re, subprocess, tempfile, time
from pathlib import Path

OD = Path('/home/jetson/src/results/uprobe/output-dims-20261106')
SRC = OD / 'src'
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')
SSLSNIFF = Path('/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx/sslsniff/sslsniff')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(build):
    for name in ('outattach', 'burstloop', 'sslsniff'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    sh(['sudo', '-n', str(BUILDS / build / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def env(build, role):
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    shm = ['BPFTIME_SHM_MEMORY_MB=512'] if role == 'tool' else []
    return ['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / build / so}',
            'BPFTIME_LOG_OUTPUT=console', *shm]


def wait_for(out, proc, word, secs=30):
    deadline = time.time() + secs
    while time.time() < deadline and proc.poll() is None:
        out.seek(0)
        if word in out.read():
            return True
        time.sleep(0.1)
    out.seek(0)
    return word in out.read()


def stop(proc, res, name, hang_s=10):
    t0 = time.monotonic()
    sh(['sudo', '-n', 'kill', '-INT', '--', f'-{proc.pid}'])
    try:
        proc.wait(timeout=hang_s)
        res['stop_ms'] = round((time.monotonic() - t0) * 1000, 1)
        res['hang'] = False
    except subprocess.TimeoutExpired:
        res['stop_ms'] = None
        res['hang'] = True
        pid = sh(['pgrep', '-s', str(proc.pid), '-x', name]).stdout.split()
        if pid:
            bt = sh(['sudo', '-n', 'timeout', '120', 'gdb', '-p', pid[0], '-batch',
                     '-ex', 'thread apply all bt'])
            res['hang_bt'] = bt.stdout[-6000:]
        sh(['sudo', '-n', 'kill', '-9', '--', f'-{proc.pid}'])
        proc.wait()


def run_outattach(build, obj, state, rng):
    cleanup(build)
    res = dict(kind='outattach', build=build, obj=obj, state=state)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = subprocess.Popen(env(build, 'tool') + [str(SRC / 'outattach'), str(SRC / obj),
                                                  str(SRC / 'burstloop'), 'on_count=count_event'],
                            stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        if not wait_for(out, tool, 'attached'):
            out.seek(0)
            res['error'] = 'no attach: ' + out.read()[-300:]
            return res
        wl = subprocess.Popen(env(build, 'workload') +
                              [str(SRC / 'burstloop'), '4', '2', '100', '1000', '0', '4000'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if state == 'load':
            time.sleep(0.5 + rng.random())
            stop(tool, res, 'outattach')
            wl.wait(timeout=60)
        else:
            wl.wait(timeout=60)
            time.sleep(0.2 + 0.3 * rng.random())
            stop(tool, res, 'outattach')
        out.seek(0)
        m = re.search(r'received (\d+)', out.read())
        res['received'] = int(m.group(1)) if m else None
        return res
    finally:
        cleanup(build)


def run_sslsniff(build):
    cleanup(build)
    res = dict(kind='sslsniff', build=build)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = subprocess.Popen(env(build, 'tool') + [str(SSLSNIFF)], stdout=out,
                            stderr=subprocess.STDOUT, start_new_session=True)
    try:
        time.sleep(2)
        if tool.poll() is not None:
            out.seek(0)
            res['error'] = 'exited: ' + out.read()[-300:]
            return res
        stop(tool, res, 'sslsniff')
        return res
    finally:
        cleanup(build)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--builds', required=True)
    ap.add_argument('--reps', type=int, default=15)
    ap.add_argument('--sslsniff-reps', type=int, default=5)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    builds = a.builds.split(',')
    rng = random.Random(20261107)
    with open(a.out, 'a') as f:
        def emit(r):
            f.write(json.dumps(r) + '\n')
            f.flush()
            print(r.get('kind'), r.get('build'), r.get('obj', ''), r.get('state', ''),
                  r.get('stop_ms'), 'HANG' if r.get('hang') else '', r.get('error', ''),
                  flush=True)
        for rep in range(a.reps):
            order = builds[rep % len(builds):] + builds[:rep % len(builds)]
            for obj in ('out-rbres-8.bpf.o', 'out-perf-8.bpf.o'):
                for state in ('load', 'idle'):
                    for b in order:
                        r = run_outattach(b, obj, state, rng)
                        r['rep'] = rep
                        emit(r)
        for rep in range(a.sslsniff_reps):
            order = builds[rep % len(builds):] + builds[:rep % len(builds)]
            for b in order:
                r = run_sslsniff(b)
                r['rep'] = rep
                emit(r)


if __name__ == '__main__':
    main()
