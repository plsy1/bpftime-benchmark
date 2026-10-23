#!/usr/bin/env python3
"""Re-check of the July 21 claim that the host network namespace (Tailscale/netfilter)
costs bpftime more than kernel eBPF on ssl-nginx (archive/bpftime-v020-jetson-host-network-validation.md).

Runs on the host. Two otherwise identical containers: netns-private (Docker bridge,
wrk -> nginx over the container's own loopback) and netns-host (--network host, loopback
passes the host netfilter INPUT -> ts-input chain). Per rep, leg order rotates; each leg
runs once in each container back to back, container order alternating, so every
private/host pair is close in time. Each leg is one bench/run_fair.py run (restart,
wrk -c 100 -d 10). The runner's cleanup kills nginx/sslsniff/wrk machine-wide
(--pid=host), so legs wait while any foreign build/benchmark process exists, and a leg
that saw one is repeated.

Per leg it records req/s, reader (sslsniff) and nginx-worker CPU cores while wrk runs,
and for host legs the INPUT/ts-input packet counters (read only; Tailscale is not touched).
"""
import argparse, json, re, subprocess, time
from pathlib import Path

OUT = Path(__file__).resolve().parent
CONTAINERS = {'private': 'netns-private', 'host': 'netns-host'}
HEAVY = {'cc1plus', 'cc1', 'lto1', 'lto-wrapper', 'ld', 'ld.bfd', 'ld.gold', 'collect2', 'ninja',
         'make', 'cmake', 'bpftime_runtime', 'simple-benchma', 'clang', 'clang-15', 'clang-16',
         'wrk', 'nginx', 'sslsniff', 'test', 'uprobe'}
TICK = 100.0

def sh(*a):
    return subprocess.run(list(a), capture_output=True, text=True)

def container_ids():
    return {n: sh('sudo', '-n', 'docker', 'inspect', '-f', '{{.Id}}', n).stdout.strip() for n in CONTAINERS.values()}

def procs():
    """pid -> (comm, cgroup text, ppid, cpu ticks)"""
    out = {}
    for p in Path('/proc').iterdir():
        if not p.name.isdigit():
            continue
        try:
            stat = (p / 'stat').read_text()
            cg = (p / 'cgroup').read_text()
        except OSError:
            continue
        comm = stat[stat.index('(') + 1:stat.rindex(')')]
        rest = stat[stat.rindex(')') + 2:].split()
        out[int(p.name)] = (comm, cg, int(rest[1]), int(rest[11]) + int(rest[12]))
    return out

def foreign_heavy(ps, ours):
    return sorted({c for c, cg, _, _ in ps.values() if c in HEAVY and not any(i in cg for i in ours)})

def wait_clear(ours):
    t0 = time.time()
    while True:
        f = foreign_heavy(procs(), ours)
        if not f:
            return time.time() - t0
        print(f'WAIT foreign={f}', flush=True)
        time.sleep(10)

def counters():
    def rows(chain):
        r = sh('sudo', '-n', 'iptables', '-L', chain, '-v', '-n', '-x')
        return [int(l.split()[0]) for l in r.stdout.splitlines()[2:] if l.split()]
    return dict(INPUT=rows('INPUT'), ts_input=rows('ts-input'))

def run_leg(netns, leg, payload, ours):
    cid = ours[CONTAINERS[netns]]
    before = counters()
    p = subprocess.Popen(['sudo', '-n', 'docker', 'exec', CONTAINERS[netns], 'python3', '/bench/run_fair.py',
                          '--payload', str(payload), '--reps', '1', '--legs', leg, '--out', '/bench/last.json'],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    first, last, seen = {}, {}, set()
    while p.poll() is None:
        ps = procs()
        seen.update(foreign_heavy(ps, ours.values()))
        mine = {pid: v for pid, v in ps.items() if cid in v[1]}
        if any(v[0] == 'wrk' for v in mine.values()):
            now = time.monotonic()
            for pid, (comm, _, ppid, ticks) in mine.items():
                if comm in ('sslsniff', 'nginx'):
                    first.setdefault(pid, (now, ticks, comm, ppid))
                    last[pid] = (now, ticks)
        time.sleep(0.5)
    text = p.stdout.read()
    after = counters()
    m = re.search(r'SAMPLE rep=1 leg=\S+ rps=([\d.]+) requests=(\d+) wrk_errors=(\S+)', text)
    assert p.returncode == 0 and m, text
    cores = {'sslsniff': 0.0, 'nginx_worker': 0.0}
    for pid, (t0, c0, comm, ppid) in first.items():
        t1, c1 = last[pid]
        if t1 - t0 < 5:
            continue
        c = (c1 - c0) / TICK / (t1 - t0)
        if comm == 'sslsniff':
            cores['sslsniff'] += c
        else:
            cores['nginx_worker'] = max(cores['nginx_worker'], c)
    requests = int(m.group(2))
    return dict(netns=netns, leg=leg, rps=float(m.group(1)), requests=requests, wrk_errors=m.group(3) == 'True',
                reader_cores=round(cores['sslsniff'], 3), nginx_cores=round(cores['nginx_worker'], 3),
                input_pkts_per_request=(after['INPUT'][0] - before['INPUT'][0]) / requests,
                ts_input_rule_deltas=[a - b for a, b in zip(after['ts_input'], before['ts_input'])],
                foreign=sorted(seen))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--reps', type=int, default=10)
    ap.add_argument('--payload', type=int, default=1024)
    ap.add_argument('--legs', default='baseline,kernel,official,opt-noaff-fork')
    ap.add_argument('--out', default='recheck.json')
    a = ap.parse_args()
    legs = a.legs.split(',')
    ours = container_ids()
    meta = dict(started=time.strftime('%Y-%m-%dT%H:%M:%S%z'), payload=a.payload, legs=legs, reps=a.reps,
                tailscale=sh('tailscale', 'status').stdout, input_rules=sh('sudo', '-n', 'iptables', '-S', 'INPUT').stdout,
                ts_input_rules=sh('sudo', '-n', 'iptables', '-S', 'ts-input').stdout, samples=[])
    k = 0
    for rep in range(a.reps):
        order = legs[rep % len(legs):] + legs[:rep % len(legs)]
        for leg in order:
            nets = ['private', 'host'] if k % 2 == 0 else ['host', 'private']
            k += 1
            for netns in nets:
                for attempt in range(4):
                    waited = wait_clear(ours.values())
                    s = run_leg(netns, leg, a.payload, ours)
                    s.update(rep=rep + 1, attempt=attempt, waited_s=round(waited, 1))
                    if not s['foreign']:
                        break
                    print(f'RETRY foreign={s["foreign"]}', flush=True)
                meta['samples'].append(s)
                print(f'SAMPLE rep={rep + 1} netns={netns} leg={leg} rps={s["rps"]:.1f} reader={s["reader_cores"]} '
                      f'nginx={s["nginx_cores"]} input/req={s["input_pkts_per_request"]:.2f} foreign={s["foreign"]}', flush=True)
                (OUT / a.out).write_text(json.dumps(meta, indent=1))
    meta['finished'] = time.strftime('%Y-%m-%dT%H:%M:%S%z')
    (OUT / a.out).write_text(json.dumps(meta, indent=1))

if __name__ == '__main__':
    main()
