#!/usr/bin/env python3
"""Memory footprint and idle CPU (more-dims-20261106/n5-footprint).

Per cell (backend, OBJ, THREADS): the tool (outattach with OBJ on thr's count_event; bpftime's
default 128 MB segment unless --shm-mb) is started; its VmRSS/RssShmem after attaching, and for
the kernel the memlock of its BPF objects (fdinfo); then src/thr THREADS HOLD: each thread makes
one call (so per-thread state exists: Gum's context, a perf shard), main prints its memory, then
idles HOLD seconds with all threads blocked and prints its own CPU time (the agent's background
work); the tool's CPU time over the same idle window (its consumer polling with no events);
the segment's allocated size in /dev/shm. Checks: hits == THREADS."""
import argparse, json, os, re, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

THR = HERE / 'src' / 'thr'
OBJS = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src')
TICK = os.sysconf('SC_CLK_TCK')


def proc_status(pid):
    out = common.sh(['sudo', '-n', 'cat', f'/proc/{pid}/status']).stdout
    return {k: common.num(rf'^{k}:\s+(\d+)', out) for k in ('VmRSS', 'RssShmem', 'Threads')}


def cpu_ms(pid):
    s = common.sh(['sudo', '-n', 'cat', f'/proc/{pid}/stat']).stdout
    f = s.rsplit(')', 1)[1].split()
    return (int(f[11]) + int(f[12])) * 1000 / TICK


def memlock(pid):
    total = 0
    fds = common.sh(['sudo', '-n', 'ls', f'/proc/{pid}/fdinfo']).stdout.split()
    for fd in fds:
        t = common.sh(['sudo', '-n', 'cat', f'/proc/{pid}/fdinfo/{fd}']).stdout
        m = re.search(r'^memlock:\s+(\d+)', t, re.M)
        if m:
            total += int(m.group(1))
    return total


def run_cell(backend, obj, threads, hold, shm_mb):
    common.cleanup(backend, ('outattach', 'thr'))
    res = dict(backend=backend, obj=obj, threads=threads, hold=hold, shm_mb=shm_mb)
    tool = None
    try:
        if backend != 'none':
            tool = common.Tool(backend, OBJS / obj, THR, ['on_count=count_event'], shm_mb=shm_mb)
            time.sleep(1)
            pid = common.sh(['pgrep', '-s', str(tool.p.pid), '-x', 'outattach']).stdout.split()[0]
            st = proc_status(pid)
            res.update(tool_rss_kb=st['VmRSS'], tool_shmem_kb=st['RssShmem'],
                       tool_threads=st['Threads'])
            if backend == 'kernel':
                res['memlock_kb'] = round(memlock(pid) / 1024)
            seg = Path('/dev/shm/bpftime_maps_shm')
            if seg.exists():
                res['segment_alloc_kb'] = seg.stat().st_blocks // 2
                res['segment_size_kb'] = seg.stat().st_size // 1024
            c0 = cpu_ms(pid)
        t0 = time.time()
        w = common.sh(['timeout', str(hold + 120)] + common.env(backend, 'workload') +
                      [str(THR), str(threads), str(hold)])
        for k in ('VmRSS', 'RssAnon', 'RssFile', 'RssShmem', 'Threads', 'idle_cpu_ms'):
            res[k] = common.num(rf'^{k} ([\d.]+)$', w.stdout)
        if tool:
            res['tool_cpu_ms'] = round(cpu_ms(pid) - c0, 1)
            res['tool_window_s'] = round(time.time() - t0, 2)
            st = proc_status(pid)
            res['tool_rss_after_kb'] = st['VmRSS']
            text = tool.stop()
            res['hits'] = tool.sums(text)['sum0']
            res['failed'] = tool.sums(text)['sum1']
            res['received'] = common.num(r'^received (\d+)$', text)
        res['ok'] = w.returncode == 0 and (tool is None or res['hits'] == threads)
        if not res['ok']:
            res['tail'] = (w.stdout + w.stderr)[-300:]
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        if tool:
            tool.kill()
        common.cleanup(backend, ('outattach', 'thr'))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--objs', default='out-control.bpf.o,out-perf-256.bpf.o,out-rbres-256.bpf.o')
    ap.add_argument('--threads', default='1,16,64')
    ap.add_argument('--hold', type=int, default=20)
    ap.add_argument('--shm-mb', type=int, default=0)
    ap.add_argument('--reps', type=int, default=2)
    ap.add_argument('--builds', default='fix-final', help='bpftime builds (comma list)')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    cells = [('none', '-', int(n)) for n in a.threads.split(',')]
    for obj in a.objs.split(','):
        for n in a.threads.split(','):
            cells += [(b, obj, int(n)) for b in ('kernel', *a.builds.split(','))]
    for rep in range(a.reps):
        for backend, obj, n in (cells if rep % 2 == 0 else cells[::-1]):
            t0 = time.time()
            r = run_cell(backend, obj, n, a.hold, a.shm_mb or None)
            r.update(rep=rep, seconds=round(time.time() - t0, 1))
            with open(a.out, 'a') as f:
                f.write(json.dumps(r) + '\n')
            print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
