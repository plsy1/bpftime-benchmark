#!/usr/bin/env python3
"""Torn reads through a hash lookup pointer: kernel vs bpftime (src/torn.bpf.c).

One run: start src/attach with torn(-noprealloc).bpf.o (uprobes on write_event and read_event
of src/tornloop) for kernel or bpftime, run tornloop THREADS SECS (thread 0 writes, the others
read), stop the loader with SIGINT and record reads, lookup misses, torn reads and writes. The
run is valid when the probe counts equal the workload's calls and no write failed; torn reads
are the result, not a failure. One JSON line per run.
"""
import argparse, itertools, json, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend):
    for name in ('attach', 'tornloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    if backend != 'kernel':
        sh(['sudo', '-n', str(BUILDS / backend / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def env(backend, role):
    if backend == 'kernel':
        return ['sudo', '-n', 'env']
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    extra = ['BPFTIME_SHM_MEMORY_MB=256'] if role == 'tool' else []
    return ['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / backend / so}', 'BPFTIME_LOG_OUTPUT=console', *extra]


def num(pattern, text):
    m = re.search(pattern, text)
    return int(m.group(1)) if m else None


def run_one(backend, obj, threads, secs):
    cleanup(backend)
    res = dict(backend=backend, obj=obj, threads=threads)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = None
    try:
        tool = subprocess.Popen(env(backend, 'tool') + [str(SRC / 'attach'), str(SRC / obj), str(SRC / 'tornloop'),
                                'on_write=write_event', 'on_read=read_event'],
                                stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.time() + 30
        while time.time() < deadline and tool.poll() is None:
            out.seek(0)
            if 'attached' in out.read():
                break
            time.sleep(0.2)
        out.seek(0)
        if 'attached' not in out.read():
            raise RuntimeError('loader did not attach')
        w = sh(env(backend, 'workload') + [str(SRC / 'tornloop'), str(threads), str(secs)], timeout=secs + 120)
        res['workload_rc'] = w.returncode
        res['calls_writes'] = num(r'(?m)^writes (\d+)', w.stdout)
        res['calls_reads'] = num(r'(?m)^reads (\d+)', w.stdout)
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
        tool.wait(timeout=20)
        out.seek(0)
        text = out.read()
        for i, k in enumerate(('reads', 'misses', 'torn', 'writes', 'failed')):
            res[k] = num(rf'sum{i} (\d+)', text)
        res['ok'] = (res['reads'] == res['calls_reads'] and res['writes'] == res['calls_writes']
                     and res['failed'] == 0)
        if not res['ok']:
            res['tool_tail'] = text[-400:]
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        cleanup(backend)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backends', default='kernel,09421773')
    ap.add_argument('--objs', default='torn.bpf.o,torn-noprealloc.bpf.o')
    ap.add_argument('--threads', default='2,4,6')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=5)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    for rep in range(a.reps):
        for obj, threads in itertools.product(a.objs.split(','), [int(t) for t in a.threads.split(',')]):
            order = backends[rep % len(backends):] + backends[:rep % len(backends)]
            for backend in order:
                t0 = time.time()
                res = run_one(backend, obj, threads, a.secs)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
