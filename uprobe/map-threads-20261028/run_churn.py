#!/usr/bin/env python3
"""Entry/exit churn on 1-6 threads: kernel eBPF vs bpftime vs no tracing.

One run: start the loader (src/churn with one BPF object) for kernel or bpftime, run
src/churnloop THREADS SECS (thread i pinned to CPU 5 - i), stop the loader with SIGINT and check
that its uretprobe hits equal the workload's exact call count with no misses. Backend `none` runs
the workload alone. bpftime backends are build directories under BUILDS (the tool through the
syscall server, the workload through the agent). Backend order rotates per rep. One JSON line per
run.
"""
import argparse, itertools, json, re, signal, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend):
    for name in ('churn', 'churnloop'):
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


def run_one(backend, obj, threads, secs, cpus=None):
    cleanup(backend)
    res = dict(backend=backend, obj=obj, threads=threads, cpus=cpus or '5,4,3,2,1,0')
    tool, out = None, None
    try:
        if backend != 'none':
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            tool = subprocess.Popen(env(backend, 'tool') + [str(SRC / 'churn'), str(SRC / obj),
                                    str(SRC / 'churnloop'), 'target_func'],
                                    stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
            deadline = time.time() + 20
            while time.time() < deadline:
                out.seek(0)
                if 'attached' in out.read():
                    break
                if tool.poll() is not None:
                    break
                time.sleep(0.2)
            else:
                raise RuntimeError('loader did not attach')
            if tool.poll() is not None:
                out.seek(0)
                raise RuntimeError('loader exited: ' + out.read()[-500:])
        w = sh(env(backend, 'workload') + [str(SRC / 'churnloop'), str(threads), str(secs)] +
               ([cpus] if cpus else []),
               timeout=secs + 60)
        m = re.search(r'ops_per_sec_median ([\d.]+)', w.stdout)
        n = re.search(r'total_ops (\d+)', w.stdout)
        res['ops_per_sec'] = float(m.group(1)) if m else None
        res['total_ops'] = int(n.group(1)) if n else None
        res['workload_rc'] = w.returncode
        if tool:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
            tool.wait(timeout=15)
            out.seek(0)
            text = out.read()
            h = re.search(r'hits (\d+)', text)
            x = re.search(r'misses (\d+)', text)
            res['hits'] = int(h.group(1)) if h else None
            res['misses'] = int(x.group(1)) if x else None
            res['ok'] = res['hits'] == res['total_ops'] and res['misses'] == 0
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
    ap.add_argument('--objs', default='churn-k4-v8.bpf.o,churn-k8-v112.bpf.o,control.bpf.o')
    ap.add_argument('--threads', default='1,2,4,6')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=8)
    ap.add_argument('--cpus', help='CPU list for the workload threads, e.g. 0,1,2,3')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    for rep in range(a.reps):
        for obj, threads in itertools.product(a.objs.split(','), [int(t) for t in a.threads.split(',')]):
            order = backends[rep % len(backends):] + backends[:rep % len(backends)]
            for backend in order:
                if backend == 'none' and obj != a.objs.split(',')[0]:
                    continue  # no tracing does not depend on the object
                t0 = time.time()
                res = run_one(backend, obj if backend != 'none' else '-', threads, a.secs, a.cpus)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
