import os
#!/usr/bin/env python3
"""(output-ab copy of map-table-20261028/run_count.py: perf output, ringbuf output, control through
outattach, which consumes a map named "events") Shared counters on 1-6 threads: kernel vs bpftime, per map kind (src/count.bpf.c).

One run: start src/attach with count-<kind>.bpf.o (uprobe on count_event of src/countloop) for
kernel or bpftime, run countloop THREADS SECS NKEYS (all threads update the same NKEYS keys),
stop the loader with SIGINT and check: probe hits == the workload's calls, no failed update, and
the counters (summed over keys and CPUs) == the calls. `none` runs the workload alone. Backend
order rotates per rep. One JSON line per run.
"""
import argparse, itertools, json, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = Path('/home/jetson/src/results/uprobe/map-table-20261028/src')  # countloop
HERE_OBJS = HERE  # outattach, out-*.bpf.o
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend):
    for name in ('outattach', 'countloop'):
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


def run_one(backend, obj, threads, nkeys, secs):
    cleanup(backend)
    res = dict(backend=backend, obj=obj, threads=threads, nkeys=nkeys)
    tool = out = None
    try:
        if backend != 'none':
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            tool = subprocess.Popen(['sudo', '-n', 'taskset', '-c', os.environ['TOOL_CPU']] + env(backend, 'tool')[2:] + [str(HERE_OBJS / 'outattach'), str(HERE_OBJS / obj),
                                    str(SRC / 'countloop'), 'on_count=count_event'],
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
        stat = str(HERE / 'last.perf')
        wenv = env(backend, 'workload')[2:]  # without sudo -n
        w = sh(['sudo', '-n', '/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf', 'stat', '-x,', '-o', stat, '-e',
                'cycles:u,instructions:u,cycles:k,instructions:k,l2d_cache_refill,mem_access'] + wenv +
               [str(SRC / 'countloop'), str(threads), str(secs), str(nkeys)], timeout=secs + 120)
        res['perf'] = {l.split(',')[2]: float(l.split(',')[0]) for l in open(stat)
                       if l.count(',') >= 3 and l.split(',')[0].replace('.', '').isdigit()}
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
            res['received'] = num(r'received (\d+)', text)
            res['lost'] = num(r'lost (\d+)', text)
            control = 'control' in obj
            # Every output is received or failed (dropped: ENOSPC, reported as lost for perf).
            res['ok'] = (res['hits'] == res['total_ops'] and
                         (control or (res['received'] or 0) + (res['failed'] or 0) == res['hits']))
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
    ap.add_argument('--objs', default='out-perf.bpf.o,out-ringbuf.bpf.o,out-control.bpf.o')
    ap.add_argument('--threads', default='1,2,4,6')
    ap.add_argument('--nkeys', default='1,64')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=6)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    objs = a.objs.split(',')
    for rep in range(a.reps):
        for nkeys, obj, threads in itertools.product([int(x) for x in a.nkeys.split(',')], objs,
                                                    [int(t) for t in a.threads.split(',')]):
            order = backends[rep % len(backends):] + backends[:rep % len(backends)]
            for backend in order:
                if backend == 'none' and obj != objs[0]:
                    continue  # no tracing does not depend on the object
                t0 = time.time()
                res = run_one(backend, obj if backend != 'none' else '-', threads, nkeys, a.secs)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
