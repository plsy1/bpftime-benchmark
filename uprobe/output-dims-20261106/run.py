#!/usr/bin/env python3
"""Event output on 1-6 threads, by path, record size, workload shape and consumer cost:
kernel bpf vs bpftime builds (output-dims-20261106; from reviews-20261031/refixes2/output-ab).

One run: start src/outattach with OBJ (uprobe on count_event) for kernel or bpftime, run the
workload (steady: countloop THREADS SECS 1; burst: burstloop THREADS SECS BURST PAUSE_US GAP_NS
PERIOD_NS), stop the
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
               pause_us=a.pause_us, gap_ns=a.gap_ns, period_ns=a.period_ns,
               consumer_ns=a.consumer_ns, pb_pages=a.pb_pages)
    for k in ('procs', 'first_gap_ns', 'flush', 'call_hist', 'cpus'):
        if getattr(a, k):
            res[k] = getattr(a, k)
    tool = out = None
    if a.workload == 'steady':
        wl = [str(COUNTLOOP), str(threads), str(a.secs), '1']
        binary = COUNTLOOP
    else:
        wl = [str(SRC / 'burstloop'), str(threads), str(a.secs), str(a.burst), str(a.pause_us),
              str(a.gap_ns), str(a.period_ns)]
        binary = SRC / 'burstloop'
    try:
        if backend != 'none':
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            extra = [f'PB_PAGES={a.pb_pages}', f'CONSUMER_NS={a.consumer_ns}',
                     f'LATENCY={int(a.latency)}']
            if a.first_gap_ns:
                extra.append(f'FIRST_GAP_NS={a.first_gap_ns}')
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
        wenv = [f'FLUSH={int(a.flush)}', f'CALL_HIST={int(a.call_hist)}']
        if a.procs <= 1:
            if a.cpus:
                wenv.append(f'CPUS={a.cpus}')
            w = sh(env(backend, 'workload', wenv) + wl, timeout=a.secs + 120)
            res['ops_per_sec'] = num(r'ops_per_sec_median ([\d.]+)', w.stdout)
            res['total_ops'] = num(r'total_ops (\d+)', w.stdout)
            res['burst_us'] = num(r'burst_us_median ([\d.]+)', w.stdout)
            res['workload_rc'] = w.returncode
            if a.call_hist:
                for k in ('p50', 'p90', 'p99', 'p999', 'max', 'n'):
                    res[f'call_{k}'] = num(rf'(?m)^call_{k} (\d+)$', w.stdout)
        else:
            # PROCS processes of THREADS threads each, process k's threads on CPUs
            # 5 - k*THREADS - i (mod 6): the same calls in separate address spaces.
            ps = []
            for k in range(a.procs):
                cpus = ','.join(str((5 - k * threads - i) % 6) for i in range(threads))
                ps.append(subprocess.Popen(env(backend, 'workload', wenv + [f'CPUS={cpus}']) + wl,
                                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                           text=True, errors='replace'))
            outs = [q.communicate(timeout=a.secs + 180)[0] for q in ps]
            tot = [num(r'total_ops (\d+)', o) for o in outs]
            bus = sorted(num(r'burst_us_median ([\d.]+)', o) or 0 for o in outs)
            res['total_ops'] = sum(tot) if None not in tot else None
            res['ops_per_sec'] = res['total_ops'] / a.secs if res['total_ops'] else None
            res['burst_us'] = bus[len(bus) // 2]
            res['workload_rc'] = max(q.returncode for q in ps)
            if a.call_hist:
                # Per process percentiles: the worst process's, and the median process's p99.
                for k in ('p99', 'p999', 'max'):
                    v = sorted(num(rf'(?m)^call_{k} (\d+)$', o) or 0 for o in outs)
                    res[f'call_{k}_worst'] = v[-1]
                    res[f'call_{k}_median'] = v[len(v) // 2]
        if tool:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
            try:
                tool.wait(timeout=60)
            except subprocess.TimeoutExpired:
                # The loader did not stop: keep every thread's stack for the record.
                pid = sh(['pgrep', '-s', str(tool.pid), '-x', 'outattach']).stdout.split()
                if pid:
                    bt = sh(['sudo', '-n', 'timeout', '120', 'gdb', '-p', pid[0], '-batch',
                             '-ex', 'thread apply all bt'])
                    res['hang_bt'] = bt.stdout[-6000:]
                raise
            out.seek(0)
            text = out.read()
            res['hits'] = num(r'sum0 (\d+)', text)
            res['failed'] = num(r'sum1 (\d+)', text)
            res['received'] = num(r'received (\d+)', text)
            res['lost'] = num(r'lost (\d+)', text)
            if a.latency:
                for k in ('lat_negative', 'lat_n', 'lat_p50', 'lat_p90', 'lat_p99', 'lat_max',
                          'first_n', 'first_p50', 'first_p90', 'first_p99', 'first_max'):
                    res[k] = num(rf'(?m)^{k} (\d+)$', text)
            control = 'control' in obj
            if res['hits']:
                res['delivered'] = round((res['received'] or 0) / res['hits'], 5)
            res['ok'] = (res['hits'] == res['total_ops'] and
                         (control or (res['received'] or 0) + (res['failed'] or 0) == res['hits']))
            if a.procs * threads > 32 and not control:
                # The BPF side counts hits in 32 per-thread slots (tid & 31) without atomics: with
                # more than 32 producers two of them can share a slot and lose increments. Check
                # the outputs against the workload's own count instead.
                res['hits_slots_shared'] = True
                res['ok'] = (res['received'] or 0) + (res['failed'] or 0) == res['total_ops']
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
    ap.add_argument('--period-ns', type=int, default=0,
                    help='burst: call j at the burst start + j * PERIOD_NS (matched arrival)')
    ap.add_argument('--latency', action='store_true',
                    help='records carry their send time; the consumer reports delivery delays')
    ap.add_argument('--consumer-ns', type=int, default=0)
    ap.add_argument('--procs', type=int, default=1, help='workload processes (THREADS each)')
    ap.add_argument('--first-gap-ns', type=int, default=0,
                    help='latency: first-of-burst threshold (default the tool\'s 1 ms)')
    ap.add_argument('--flush', action='store_true',
                    help='burst: 3 more calls per thread 50 ms after the run (pending lost counts)')
    ap.add_argument('--call-hist', action='store_true',
                    help='burst: per-call duration percentiles of count_event')
    ap.add_argument('--cpus', default='', help='burst: CPU of thread i (one process)')
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
