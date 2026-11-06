#!/usr/bin/env python3
"""Event delay through a real perf-buffer tool: libbpf-tools gethostlatency (uprobe and
uretprobe on getaddrinfo, one perf sample per call; BTF build of realprog-20261026-btf) under
kernel bpf and fix-final, with src/resolveburst making bursts of uniquely named lookups.

The tool runs under stdbuf -oL with its stdout on a pipe read by a thread that stamps each line
with CLOCK_MONOTONIC on arrival; the workload logs each call's send time. delay = arrival - send:
the probe, the output path (perf buffer, wakeup), the tool's printf, the pipe. Same for both
backends except the output path. delivered = distinct calls seen / calls made; "first" = the
first call of each burst (the consumer has slept since the previous burst)."""
import argparse, json, re, subprocess, sys, threading, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / 'more-dims-20261106'))
import common  # noqa: E402

GHL = '/home/jetson/src/realprog-20261026-btf/libbpf-tools/gethostlatency'
WL = HERE / 'src' / 'resolveburst'
HOST = re.compile(r'\b127\.(\d+)\.(\d+)\.(\d+)\s*$')


def pct(v, p):
    return v[(len(v) - 1) * p // 100] if v else None


def run_one(backend, secs, burst, period_us, pause_us, sendlog):
    common.cleanup(backend, ('gethostlatency', 'resolveburst'))
    res = dict(backend=backend, secs=secs, burst=burst, period_us=period_us, pause_us=pause_us)
    lines = []
    tool = subprocess.Popen(common.env(backend, 'tool', shm_mb=1024) + ['stdbuf', '-oL', GHL],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            start_new_session=True, text=True, errors='replace', bufsize=1)

    def reader():
        for line in tool.stdout:
            lines.append((time.monotonic_ns(), line))

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    try:
        deadline = time.time() + 30
        while time.time() < deadline and not any('HOST' in l for _, l in lines):
            time.sleep(0.05)
        if not any('HOST' in l for _, l in lines):
            raise RuntimeError('tool did not start: ' + ''.join(l for _, l in lines)[-500:])
        time.sleep(0.5)
        w = common.sh(['timeout', str(secs + 60)] + common.env(backend, 'workload') +
                      [str(WL), str(secs), str(burst), str(period_us), str(pause_us),
                       str(sendlog)])
        res['workload_rc'] = w.returncode
        res['total_ops'] = common.num(r'^total_ops (\d+)$', w.stdout)
        time.sleep(1.5)
    finally:
        common.sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
        try:
            tool.wait(timeout=20)
        except subprocess.TimeoutExpired:
            common.sh(['sudo', '-n', 'kill', '-9', '--', f'-{tool.pid}'])
        t.join(timeout=5)
        common.cleanup(backend, ('gethostlatency', 'resolveburst'))
    sent = {}
    for l in open(sendlog):
        i, s = l.split()
        sent[int(i)] = int(s)
    seen, delays, first = set(), [], []
    lost = 0
    for arr, line in lines:
        m = re.search(r'lost (\d+) events', line)
        if m:
            lost += int(m.group(1))
        m = HOST.search(line)
        if not m:
            continue
        i = (int(m.group(1)) << 16) | (int(m.group(2)) << 8) | int(m.group(3))
        if i in sent and i not in seen:
            seen.add(i)
            d = arr - sent[i]
            delays.append(d)
            if (i - 1) % burst == 0:
                first.append(d)
    delays.sort()
    first.sort()
    res.update(seen=len(seen), lost=lost, tool_lines=len(lines))
    if res['total_ops']:
        res['delivered'] = round(len(seen) / res['total_ops'], 5)
    for name, v in (('all', delays), ('first', first)):
        for p in (50, 90, 99):
            x = pct(v, p)
            res[f'{name}_p{p}_us'] = round(x / 1000, 1) if x is not None else None
        res[f'{name}_max_us'] = round(v[-1] / 1000, 1) if v else None
    res['ok'] = (res.get('workload_rc') == 0 and res['total_ops'] is not None and
                 len(seen) + lost >= 0.999 * res['total_ops'])
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backends', default='kernel,fix-final')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=5)
    ap.add_argument('--burst', type=int, default=50)
    ap.add_argument('--period-us', type=int, default=20)
    ap.add_argument('--pause-us', type=int, default=10000)
    ap.add_argument('--out', required=True)
    ap.add_argument('--sendlog', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    for rep in range(a.reps):
        for backend in backends[rep % len(backends):] + backends[:rep % len(backends)]:
            t0 = time.time()
            r = run_one(backend, a.secs, a.burst, a.period_us, a.pause_us, a.sendlog)
            r.update(rep=rep, seconds=round(time.time() - t0, 1))
            with open(a.out, 'a') as f:
                f.write(json.dumps(r) + '\n')
            print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
