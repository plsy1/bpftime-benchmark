#!/usr/bin/env python3
"""Hash map churn by key pattern, capacity and load: kernel vs bpftime builds.

One run: start src/attach with keymap-k<K>-m<M>.bpf.o or keymap-control.bpf.o (uprobes on
insert_event and delete_event of src/keyloop) for kernel or bpftime, run keyloop THREADS SECS
LIVE PATTERN, stop the loader with SIGINT and check: inserts seen == the workload's inserts
(prefill + measured), deletes found == its deletes, no delete missed. Backend order rotates per
rep. One JSON line per run. Configs: (object, pattern, live) for capacities 10240 and 102400 at
50% and 90% load; 8-byte keys addr and seq64, 4-byte keys rand32 and shl12.

x86 NUC copy (11/03) of Jetson results uprobe/hash-keypattern-20261103/run_keys.py: BUILDS
points at the frozen x86 builds; a backend named <build>-zeroseed runs <build> with
BPFTIME_HASH_ZERO_SEED=1 in both processes (the map is created by the loader, under the syscall
server); --patterns limits the configs (the control always runs).
"""
import argparse, itertools, json, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
BUILDS = Path('/home/y/src/bpftime-x86-runs/r1-miss-20261104/keypattern/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend):
    for name in ('attach', 'keyloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    if backend not in ('none', 'kernel'):
        sh(['sudo', '-n', str(BUILDS / backend / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def env(backend, role):
    if backend in ('none', 'kernel'):
        return ['sudo', '-n', 'env']
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    extra = ['BPFTIME_SHM_MEMORY_MB=1024'] if role == 'tool' else []
    if backend.endswith('-zeroseed'):
        extra.append('BPFTIME_HASH_ZERO_SEED=1')
    return ['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / backend / so}', 'BPFTIME_LOG_OUTPUT=console', *extra]


def num(pattern, text):
    m = re.search(pattern, text)
    return int(float(m.group(1))) if m else None


def run_one(backend, obj, pattern, threads, live, secs):
    cleanup(backend)
    res = dict(backend=backend, obj=obj, pattern=pattern, threads=threads, live=live)
    tool = out = None
    try:
        if backend != 'none':
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            tool = subprocess.Popen(env(backend, 'tool') + [str(SRC / 'attach'), str(SRC / obj),
                                    str(SRC / 'keyloop'), 'on_insert=insert_event', 'on_delete=delete_event'],
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
        w = sh(env(backend, 'workload') + [str(SRC / 'keyloop'), str(threads), str(secs), str(live), pattern],
               timeout=secs + 120)
        m = re.search(r'ops_per_sec_median ([\d.]+)', w.stdout)
        res['ops_per_sec'] = float(m.group(1)) if m else None
        res['total_ops'] = num(r'total_ops (\d+)', w.stdout)
        res['allocs'] = num(r'allocs (\d+)', w.stdout)
        res['frees'] = num(r'frees (\d+)', w.stdout)
        res['workload_rc'] = w.returncode
        if tool:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
            tool.wait(timeout=20)
            out.seek(0)
            text = out.read()
            res['seen_allocs'] = num(r'sum0 (\d+)', text)
            res['seen_frees'] = num(r'sum1 (\d+)', text)
            res['missed_frees'] = num(r'sum2 (\d+)', text)
            res['ok'] = (res['seen_allocs'] == res['allocs'] and res['seen_frees'] == res['frees']
                         and res['missed_frees'] == 0)
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


def configs():
    out = []
    for m in (10240, 102400):
        for load in (0.5, 0.9):
            for k, pats in ((8, ('addr', 'seq64')), (4, ('rand32', 'shl12'))):
                for p in pats:
                    out.append((f'keymap-k{k}-m{m}.bpf.o', p, int(m * load)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backends', default='kernel,a7d54d7e,dev-a4ac0d31')
    ap.add_argument('--threads', default='1,6')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=5)
    ap.add_argument('--out', required=True)
    ap.add_argument('--patterns', default='addr,seq64,rand32,shl12')
    a = ap.parse_args()
    backends = a.backends.split(',')
    pats = a.patterns.split(',')
    runs = [c for c in configs() if c[1] in pats] + [('keymap-control.bpf.o', 'addr', 5120)]
    for rep in range(a.reps):
        for (obj, pattern, live), threads in itertools.product(runs, [int(t) for t in a.threads.split(',')]):
            order = backends[rep % len(backends):] + backends[:rep % len(backends)]
            for backend in order:
                t0 = time.time()
                res = run_one(backend, obj, pattern, threads, live, a.secs)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
