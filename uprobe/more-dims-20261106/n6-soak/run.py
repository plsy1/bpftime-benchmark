#!/usr/bin/env python3
"""Two hours under bpftime (more-dims-20261106/n6-soak): one tool (outattach, out-soak.bpf.o:
each call updates a 65,536-entry hash map kept about half full by churn and sends a 256-byte
ring buffer record by reserve + submit) on burstloop's count_event, under fix-final with the
default 128 MB segment, running the whole time. Every --every seconds a new burstloop process
starts (2 threads, --life seconds, bursts of 100 at 4 us every 2 ms; CPUs rotating), so about
life/every processes overlap and several hundred start and exit over the run. Every minute:
the tool's VmRSS, threads and CPU time. At the end: hits == the sum of the processes' calls,
received + failed == hits, every process exited 0."""
import argparse, json, os, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

OD = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src')
BL = OD / 'burstloop'
TICK = os.sysconf('SC_CLK_TCK')


def stat(pid):
    s = common.sh(['sudo', '-n', 'cat', f'/proc/{pid}/status']).stdout
    st = common.sh(['sudo', '-n', 'cat', f'/proc/{pid}/stat']).stdout
    f = st.rsplit(')', 1)[1].split() if st else None
    return dict(rss_kb=common.num(r'^VmRSS:\s+(\d+)', s), threads=common.num(r'^Threads:\s+(\d+)', s),
                cpu_s=(int(f[11]) + int(f[12])) / TICK if f else None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backend', default='fix-final')
    ap.add_argument('--secs', type=int, default=7200)
    ap.add_argument('--every', type=int, default=20)
    ap.add_argument('--life', type=int, default=40)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    b = a.backend
    common.cleanup(b, ('outattach', 'burstloop'))
    log = open(a.out, 'a')

    def emit(d):
        log.write(json.dumps(d) + '\n')
        log.flush()

    tool = common.Tool(b, OD / 'out-soak.bpf.o', BL, ['on_count=count_event'])
    pid = common.sh(['pgrep', '-s', str(tool.p.pid), '-x', 'outattach']).stdout.split()[0]
    t0 = time.time()
    procs, done, k = [], [], 0
    next_start, next_sample = t0, t0
    try:
        while time.time() < t0 + a.secs:
            now = time.time()
            if now >= next_start and now + a.life < t0 + a.secs:
                cpus = f'{(5 - 2 * k) % 6},{(4 - 2 * k) % 6}'
                p = subprocess.Popen(['timeout', str(a.life + 120)] + common.env(
                    b, 'workload', [f'CPUS={cpus}']) + [str(BL), '2', str(a.life), '100', '2000',
                                                       '0', '4000'],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
                procs.append((k, now, p))
                k += 1
                next_start += a.every
            for item in list(procs):
                i, started, p = item
                if p.poll() is not None:
                    out = p.stdout.read()
                    total = common.num(r'total_ops (\d+)', out)
                    done.append(dict(proc=i, rc=p.returncode, total_ops=total))
                    if p.returncode or total is None:
                        emit(dict(event='proc_failed', proc=i, rc=p.returncode, tail=out[-300:]))
                    procs.remove(item)
            if now >= next_sample:
                emit(dict(event='sample', t=round(now - t0), live=len(procs), finished=len(done),
                          **stat(pid)))
                next_sample += 60
            time.sleep(0.2)
        for i, started, p in procs:
            out = p.communicate(timeout=a.life + 180)[0]
            done.append(dict(proc=i, rc=p.returncode, total_ops=common.num(r'total_ops (\d+)', out)))
        time.sleep(1)
        st = stat(pid)
        text = tool.stop(timeout=120)
        s = tool.sums(text)
        calls = sum(d['total_ops'] or 0 for d in done)
        res = dict(event='end', secs=round(time.time() - t0), procs=len(done),
                   procs_failed=sum(1 for d in done if d['rc'] or d['total_ops'] is None),
                   calls=calls, hits=s['sum0'], failed=s['sum1'], update_failed=s['sum2'],
                   received=common.num(r'^received (\d+)$', text), final=st)
        res['delivered'] = round((res['received'] or 0) / res['hits'], 5) if res['hits'] else None
        res['ok'] = (res['hits'] == calls and (res['received'] or 0) + (res['failed'] or 0) ==
                     res['hits'] and res['procs_failed'] == 0)
        if not res['ok']:
            res['tool_tail'] = text[-600:]
        emit(res)
        print(json.dumps(res), flush=True)
    finally:
        tool.kill()
        for i, started, p in procs:
            if p.poll() is None:
                p.kill()
        common.cleanup(b, ('outattach', 'burstloop'))


if __name__ == '__main__':
    main()
