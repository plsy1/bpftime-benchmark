#!/usr/bin/env python3
"""DeepFlow (upstream v7.2.3 socket-tracer sample) on the Go server, four legs,
as bpftime/benchmark/deepflow but on ARM64 with upstream DeepFlow:

  no-probe       the Go server alone
  no-uprobe      DeepFlow with kprobes/syscall tracepoints only (no Go uprobes)
  kernel-uprobe  DeepFlow with its Go uprobes on the server, all in the kernel
  user-uprobe    as kernel-uprobe, but bpftime_daemon redirects the goroutine
                 tracking uprobes (runtime.execute, runtime.newproc1 entry and
                 return: the paper's choice, -w offsets) to userspace and the
                 agent is injected with `bpftime attach`; TLS uprobes stay in
                 the kernel

DeepFlow attaches Go uprobes 60 s after it matches a running process, so the
uprobe legs wait 70 s before loading. The tracer runs with DF_QUIET=1 (counts
events per source, no printing). Load: wrk pinned to CPUs 4-5, server to 0-3.
Leg order rotates per rep. One JSON line per run.
"""
import argparse, json, re, subprocess, time
from pathlib import Path

D = Path('/home/jetson/src/deepflow-20261026')
I = D / 'bpftime-install'
TRACER = D / 'deepflow-v7.2.3/agent/src/ebpf/samples/rust/socket-tracer/target/release/socket_tracer'
WHITELIST = ['0x41770', '0x464f0', '0x46998']   # runtime.execute, newproc1 entry + RET
LEGS = ['no-probe', 'no-uprobe', 'kernel-uprobe', 'user-uprobe']

def sh(*a, **kw):
    return subprocess.run(list(a), capture_output=True, text=True, errors='replace', **kw)

def cleanup():
    for name in ('wrk', 'socket_tracer', 'bpftime_daemon', 'main'):
        sh('sudo', '-n', 'pkill', '-9', '-x', name)
    time.sleep(0.5)
    sh('sudo', '-n', str(I / 'bpftimetool'), 'remove')

def wait_for(path, pattern, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if path.exists() and pattern in path.read_text(errors='replace'):
            return True
        time.sleep(0.5)
    return False

def last_counts(path):
    m = re.findall(r'EVENT_COUNTS \[([\d, ]+)\] WITH_COROUTINE (\d+)', path.read_text(errors='replace'))
    if not m:
        return None
    counts, coro = m[-1]
    return [int(x) for x in counts.split(',')], int(coro)

def run_one(leg, kind, size, wrk_secs, out):
    cleanup()
    port, scheme = (446, 'https') if kind == 'https' else (447, 'http')
    url = f'{scheme}://127.0.0.1:{port}/'
    stem = out / f'{leg}.{kind}.{size}'
    procs = []
    server = subprocess.Popen(['sudo', '-n', 'taskset', '-c', '0-3', 'env', 'BPFTIME_LOG_OUTPUT=console',
                               './main', str(size)], cwd=D / 'servers' / kind,
                              stdout=open(f'{stem}.server.txt', 'w'), stderr=subprocess.STDOUT)
    procs.append(server)
    time.sleep(1.5)
    pid = sh('pgrep', '-n', '-x', 'main').stdout.strip()
    res = dict(leg=leg, kind=kind, size=size, server_pid=int(pid))
    tracer_log = Path(f'{stem}.tracer.txt')
    try:
        if leg == 'user-uprobe':
            wl = sum((['-w', a] for a in WHITELIST), [])
            procs.append(subprocess.Popen(['sudo', '-n', 'env', 'BPFTIME_LOG_OUTPUT=console', str(I / 'bpftime_daemon'), *wl],
                                          stdout=open(f'{stem}.daemon.txt', 'w'), stderr=subprocess.STDOUT))
            time.sleep(3)
        if leg != 'no-probe':
            env = ['DF_QUIET=1'] + ([f'DF_GO_PIDS={pid}'] if leg != 'no-uprobe' else [])
            procs.append(subprocess.Popen(['sudo', '-n', 'env', *env, str(TRACER)], cwd=out,
                                          stdout=open(tracer_log, 'w'), stderr=subprocess.STDOUT))
            res['tracer_ready'] = wait_for(tracer_log, 'adapt success', 90)
            time.sleep(70 if leg != 'no-uprobe' else 3)
        if leg == 'user-uprobe':
            a = sh('sudo', '-n', 'env', 'BPFTIME_LOG_OUTPUT=console', str(I / 'bpftime'), '-i', str(I), 'attach', pid)
            Path(f'{stem}.attach.txt').write_text(a.stdout + a.stderr)
            res['attached'] = 'Successfully injected' in a.stdout + a.stderr
            time.sleep(2)
        sh('curl', '--http1.1', '-sk', '-o', '/dev/null', url)
        time.sleep(1.5)
        before = last_counts(tracer_log) if tracer_log.exists() else None
        w = sh('sudo', '-n', 'taskset', '-c', '4,5', 'wrk', '-t', '2', '-c', '100', '-d', str(wrk_secs), url)
        Path(f'{stem}.wrk.txt').write_text(w.stdout + w.stderr)
        time.sleep(2.5)
        after = last_counts(tracer_log) if tracer_log.exists() else None
        m = re.search(r'Requests/sec:\s+([\d.]+)', w.stdout)
        n = re.search(r'(\d+) requests in', w.stdout)
        res['req_per_sec'] = float(m.group(1)) if m else None
        res['requests'] = int(n.group(1)) if n else None
        if before and after and res['requests']:
            delta = [b - a for a, b in zip(before[0], after[0])]
            res['events_per_request_by_source'] = {s: round(v / res['requests'], 3) for s, v in enumerate(delta) if v}
            res['coroutine_events_per_request'] = round((after[1] - before[1]) / res['requests'], 3)
        res['server_alive'] = server.poll() is None
    finally:
        cleanup()
        for p in procs:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
    return res

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--legs', default=','.join(LEGS))
    ap.add_argument('--kinds', default='https,http')
    ap.add_argument('--sizes', default='1024,16384,262144')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--wrk-secs', type=int, default=10)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    out = Path(a.out)
    raw = out / 'raw'
    raw.mkdir(parents=True, exist_ok=True)
    legs = a.legs.split(',')
    for r in range(a.reps):
        for kind in a.kinds.split(','):
            for size in [int(s) for s in a.sizes.split(',')]:
                order = legs[r % len(legs):] + legs[:r % len(legs)]
                for leg in order:
                    t0 = time.time()
                    res = run_one(leg, kind, size, a.wrk_secs, raw)
                    res.update(rep=r, seconds=round(time.time() - t0, 1))
                    with (out / 'results.jsonl').open('a') as f:
                        f.write(json.dumps(res) + '\n')
                    print(json.dumps(res), flush=True)

if __name__ == '__main__':
    main()
