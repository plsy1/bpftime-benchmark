#!/usr/bin/env python3
"""Event output on 1-6 threads, by path, record size, workload shape and consumer cost:
kernel bpf vs bpftime builds (output-dims-20261106; from reviews-20261031/refixes2/output-ab).

One run: start src/outattach with OBJ (uprobe on count_event) for kernel or bpftime, run the
workload (steady: countloop THREADS SECS 1; burst: burstloop THREADS SECS BURST PAUSE_US), stop the
loader with SIGINT, and check hits == the workload's calls and received + failed == hits (every
output is received or dropped at the producer). delivered = received / hits. Backend order rotates
per rep. One JSON line per run.
"""
import argparse, itertools, json, os, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
COUNTLOOP = Path('/home/jetson/src/results/uprobe/map-table-20261028/src/countloop')
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend):
    for name in ('outattach', 'countloop', 'burstloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    if backend not in ('none', 'kernel'):
        sh(['sudo', '-n', str(BUILDS / backend / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def env(backend, role, extra=()):
    if backend in ('none', 'kernel'):
        return ['sudo', '-n', 'env', *extra]
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    shm = ['BPFTIME_SHM_MEMORY_MB=512'] if role == 'tool' else []
    return ['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / backend / so}', 'BPFTIME_LOG_OUTPUT=console',
            *shm, *extra]


def num(pattern, text):
    m = re.search(pattern, text)
    return float(m.group(1)) if m else None


def run_one(a, backend, obj, threads):
    cleanup(backend)
    res = dict(backend=backend, obj=obj, threads=threads, workload=a.workload, burst=a.burst,
               pause_us=a.pause_us, gap_ns=a.gap_ns, consumer_ns=a.consumer_ns,
               pb_pages=a.pb_pages)
    tool = out = None
    if a.workload == 'steady':
        wl = [str(COUNTLOOP), str(threads), str(a.secs), '1']
        binary = COUNTLOOP
    else:
        wl = [str(SRC / 'burstloop'), str(threads), str(a.secs), str(a.burst), str(a.pause_us),
              str(a.gap_ns)]
        binary = SRC / 'burstloop'
    try:
        if backend != 'none':
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            extra = [f'PB_PAGES={a.pb_pages}', f'CONSUMER_NS={a.consumer_ns}']
            tool = subprocess.Popen(env(backend, 'tool', extra) +
                                    [str(SRC / 'outattach'), str(SRC / obj), str(binary),
                                     'on_count=count_event'],
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
        w = sh(env(backend, 'workload') + wl, timeout=a.secs + 120)
        res['ops_per_sec'] = num(r'ops_per_sec_median ([\d.]+)', w.stdout)
        res['total_ops'] = num(r'total_ops (\d+)', w.stdout)
        res['burst_us'] = num(r'burst_us_median ([\d.]+)', w.stdout)
        res['workload_rc'] = w.returncode
        if tool:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
            tool.wait(timeout=60)
            out.seek(0)
            text = out.read()
            res['hits'] = num(r'sum0 (\d+)', text)
            res['failed'] = num(r'sum1 (\d+)', text)
            res['received'] = num(r'received (\d+)', text)
            res['lost'] = num(r'lost (\d+)', text)
            control = 'control' in obj
            if res['hits']:
                res['delivered'] = round((res['received'] or 0) / res['hits'], 5)
            res['ok'] = (res['hits'] == res['total_ops'] and
                         (control or (res['received'] or 0) + (res['failed'] or 0) == res['hits']))
            if not res['ok']:
                res['tool_tail'] = text[-400:]
        else:
            res['ok'] = res['total_ops'] is not None
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        if tool and tool.poll() is None:
            sh(['sudo', '-n', 'kill', '-9', '--', f'-{tool.pid}'])
        cleanup(backend)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backends', default='kernel,fix-final')
    ap.add_argument('--objs', required=True)
    ap.add_argument('--threads', default='1,2,4,6')
    ap.add_argument('--workload', choices=('steady', 'burst'), default='steady')
    ap.add_argument('--burst', type=int, default=100)
    ap.add_argument('--pause-us', type=int, default=10000)
    ap.add_argument('--gap-ns', type=int, default=0)
    ap.add_argument('--consumer-ns', type=int, default=0)
    ap.add_argument('--pb-pages', type=int, default=64)
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=6)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    objs = a.objs.split(',')
    for rep in range(a.reps):
        for obj, threads in itertools.product(objs, [int(t) for t in a.threads.split(',')]):
            order = backends[rep % len(backends):] + backends[:rep % len(backends)]
            for backend in order:
                if backend == 'none' and obj != objs[0]:
                    continue
                t0 = time.time()
                res = run_one(a, backend, obj if backend != 'none' else '-', threads)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
