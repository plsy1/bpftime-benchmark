#!/usr/bin/env python3
"""Like `run_fair.py --audit`, but also keeps sslsniff's stderr and sums its
"lost N events on CPU #c" lines (libbpf's lost callback, fed by PERF_RECORD_LOST).

Per tracing leg: one wrk run (default -c 10 -d 3 as the audit), stdout and stderr captured.
Reports events/request, lost/request and delivered/(delivered+lost).
Reuses run_fair's helpers so process setup is identical.
"""
import argparse, json, os, re, signal, subprocess, sys

sys.path.insert(0, '/bench')
import run_fair as rf  # noqa: E402

LOST = re.compile(r'lost (\d+) events on CPU')


def run_leg(leg, conns, secs, cap):
    rf.kill_all()
    build = None if leg == 'kernel' else leg
    rf.remove_shm(build)
    sniff = nginx = None
    out, err = open(cap, 'w'), open(cap + '.stderr', 'w')
    try:
        if leg == 'kernel':
            nginx, worker = rf.start_nginx()
            sniff = subprocess.Popen([rf.SSLSNIFF, '-p', str(worker)], stdout=out, stderr=err,
                                     start_new_session=True)
            rf.time.sleep(2)
            assert sniff.poll() is None, 'kernel sslsniff exited'
        else:
            env = dict(os.environ, LD_PRELOAD=f'{rf.BUILDS}/{build}/libbpftime-syscall-server.so')
            sniff = subprocess.Popen([rf.SSLSNIFF], env=env, stdout=out, stderr=err,
                                     start_new_session=True)
            rf.time.sleep(2)
            assert sniff.poll() is None, 'bpftime sslsniff exited'
            env = dict(os.environ, LD_PRELOAD=f'{rf.BUILDS}/{build}/libbpftime-agent.so')
            nginx, _ = rf.start_nginx(env)
        rps, total, _, errs = rf.wrk(conns, secs)
        return rps, total, errs
    finally:
        rf.stop(nginx)
        if sniff:
            sniff.send_signal(signal.SIGINT)
            try: sniff.wait(timeout=10)
            except subprocess.TimeoutExpired:
                sniff.kill(); sniff.wait()
        out.close(); err.close()
        rf.kill_all()
        rf.remove_shm(build)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--payload', type=int, required=True)
    ap.add_argument('--legs', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--conns', type=int, default=10)
    ap.add_argument('--secs', type=int, default=3)
    a = ap.parse_args()
    rf.write_payload(a.payload)
    result = {}
    for leg in a.legs.split(','):
        cap = f'{a.out}.{leg}.sslsniff.txt'
        rps, total, errs = run_leg(leg, a.conns, a.secs, cap)
        events = sum(1 for line in open(cap, errors='replace')
                     if line.startswith(('READ/RECV', 'WRITE/SEND')))
        lost = sum(int(m.group(1)) for line in open(cap + '.stderr', errors='replace')
                   for m in [LOST.search(line)] if m)
        result[leg] = dict(rps=rps, requests=total, events=events, lost=lost,
                           events_per_request=events / total if total else None,
                           lost_per_request=lost / total if total else None,
                           delivered_fraction=events / (events + lost) if events + lost else None,
                           wrk_errors=errs)
        print(f'LOST-AUDIT leg={leg} requests={total} events={events} lost={lost} '
              f'per_request={events / max(total, 1):.3f} lost_per_request={lost / max(total, 1):.3f}',
              flush=True)
    json.dump(result, open(a.out, 'w'), indent=1)


if __name__ == '__main__':
    sys.exit(main())
