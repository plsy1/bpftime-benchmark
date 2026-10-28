#!/usr/bin/env python3
"""Large per-CPU scratch value on 1-6 threads: kernel vs bpftime (src/scratch.bpf.c).

One run: start src/attach with scratch(-control).bpf.o (uprobe on send_event of
src/scratchloop) for kernel or bpftime, run scratchloop THREADS SECS LEN, stop the loader with
SIGINT and check: probe hits == the workload's calls, no failed lookup or copy, and every copy
carried the caller's tag. `none` runs the workload alone. Backend order rotates per rep. One
JSON line per run.
"""
import argparse, itertools, json, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend):
    for name in ('attach', 'scratchloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    if backend not in ('none', 'kernel'):
        sh(['sudo', '-n', str(BUILDS / backend / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def env(backend, role):
    if backend in ('none', 'kernel'):
        return ['sudo', '-n', 'env']
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    extra = ['BPFTIME_SHM_MEMORY_MB=256'] if role == 'tool' else []
    return ['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / backend / so}', 'BPFTIME_LOG_OUTPUT=console', *extra]


def num(pattern, text):
    m = re.search(pattern, text)
    return int(float(m.group(1))) if m else None


def run_one(backend, obj, threads, length, secs):
    cleanup(backend)
    res = dict(backend=backend, obj=obj, threads=threads, length=length)
    tool = out = None
    try:
        if backend != 'none':
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            tool = subprocess.Popen(env(backend, 'tool') + [str(SRC / 'attach'), str(SRC / obj),
                                    str(SRC / 'scratchloop'), 'on_send=send_event'],
                                    stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
            deadline = time.time() + 30
            while time.time() < deadline and tool.poll() is None:
                out.seek(0)
                if 'attached' in out.read():
                    break
                time.sleep(0.2)
            out.seek(0)
            text = out.read()
            if 'attached' not in text:
                raise RuntimeError('loader did not attach: ' + text[-500:])
        w = sh(env(backend, 'workload') + [str(SRC / 'scratchloop'), str(threads), str(secs), str(length)],
               timeout=secs + 120)
        m = re.search(r'ops_per_sec_median ([\d.]+)', w.stdout)
        res['ops_per_sec'] = float(m.group(1)) if m else None
        res['total_ops'] = num(r'total_ops (\d+)', w.stdout)
        res['workload_rc'] = w.returncode
        if tool:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
            tool.wait(timeout=20)
            out.seek(0)
            text = out.read()
            res['hits'] = num(r'sum0 (\d+)', text)
            res['failed'] = num(r'sum1 (\d+)', text)
            res['bad'] = num(r'sum2 (\d+)', text)
            res['ok'] = res['hits'] == res['total_ops'] and res['failed'] == 0 and res['bad'] == 0
            if not res['ok']:
                res['tool_tail'] = text[-400:]
        else:
            res['ok'] = res['total_ops'] is not None
    except Exception as e:  # keep going, record the failure
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        cleanup(backend)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backends', default='none,kernel,09421773')
    ap.add_argument('--objs', default='scratch.bpf.o,scratch-control.bpf.o')
    ap.add_argument('--threads', default='1,2,4,6')
    ap.add_argument('--length', default='0,256,8192')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=6)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    objs = a.objs.split(',')
    for rep in range(a.reps):
        for length, obj, threads in itertools.product([int(x) for x in a.length.split(',')], objs,
                                                    [int(t) for t in a.threads.split(',')]):
            order = backends[rep % len(backends):] + backends[:rep % len(backends)]
            for backend in order:
                if backend == 'none' and obj != objs[0]:
                    continue  # no tracing does not depend on the object
                t0 = time.time()
                res = run_one(backend, obj if backend != 'none' else '-', threads, length, a.secs)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
