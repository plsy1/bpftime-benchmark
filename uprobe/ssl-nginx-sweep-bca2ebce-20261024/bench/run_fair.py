#!/usr/bin/env python3
"""ssl-nginx fair-scope comparison of several bpftime builds (runs in the container).

Legs per rep, order rotated each rep, every leg restarts its processes:
  baseline         nginx only
  kernel           kernel sslsniff attached to the nginx worker only (-p PID)
  <bpftime build>  sslsniff with LD_PRELOAD=syscall-server, nginx with LD_PRELOAD=agent
Load: wrk https://127.0.0.1:4043/index.html -c 100 -d 10 (as benchmark/ssl-nginx).
Method follows the July fair validation (path-ablation-20260726, "Fair 口径验证").
--audit instead runs one short traced load per tracing leg with sslsniff output
captured and reports events per request, to prove tracing works for each build.
"""
import argparse, json, os, re, signal, subprocess, sys, time

BENCH = '/bench'
SSLSNIFF = '/sslsniff/sslsniff'
BUILDS = '/builds'
URL = 'https://127.0.0.1:4043/index.html'

def sh(*args, **kw):
    return subprocess.run(list(args), capture_output=True, text=True, **kw)

def write_payload(size):
    body = 'X' * max(0, size - len('<html></html>'))
    with open(f'{BENCH}/index.html', 'w') as f:
        f.write('<html>' + body + '</html>')
    assert os.path.getsize(f'{BENCH}/index.html') == size

def kill_all():
    for name in ('nginx', 'sslsniff', 'wrk'):
        sh('pkill', '-9', '-x', name)
    time.sleep(0.3)

def remove_shm(build):
    if build:
        sh(f'{BUILDS}/{build}/bpftimetool', 'remove')

def start_nginx(env=None):
    for f in ('access.log', 'error.log', 'nginx.pid'):
        try: os.remove(f'{BENCH}/{f}')
        except FileNotFoundError: pass
    p = subprocess.Popen(['nginx', '-c', f'{BENCH}/nginx.conf', '-p', BENCH], env=env,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    deadline = time.time() + 15
    while time.time() < deadline:
        r = sh('curl', '-k', '-s', '-o', '/dev/null', '-w', '%{http_code}', URL)
        if r.stdout == '200':
            workers = sh('pgrep', '-P', str(p.pid), '-x', 'nginx').stdout.split()
            assert len(workers) == 1, workers
            return p, int(workers[0])
        assert p.poll() is None, 'nginx exited'
        time.sleep(0.2)
    raise RuntimeError('nginx not ready')

def stop(p):
    if p and p.poll() is None:
        p.send_signal(signal.SIGTERM)
        try: p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill(); p.wait()

def wrk(conns=100, secs=10):
    r = sh('wrk', URL, '-c', str(conns), '-d', str(secs), timeout=secs + 20)
    m = re.search(r'Requests/sec:\s+([\d.]+)', r.stdout)
    total = re.search(r'(\d+) requests in', r.stdout)
    assert m and total, r.stdout + r.stderr
    errs = re.search(r'Non-2xx or 3xx responses: (\d+)|Socket errors: (.*)', r.stdout)
    return float(m.group(1)), int(total.group(1)), r.stdout, bool(errs)

def run_leg(leg, conns=100, secs=10, capture=None):
    """Returns (rps, requests, wrk_had_errors)."""
    kill_all()
    build = None if leg in ('baseline', 'kernel') else leg
    remove_shm(build)
    sniff = nginx = None
    out = open(capture, 'w') if capture else subprocess.DEVNULL
    try:
        if leg == 'baseline':
            nginx, _ = start_nginx()
        elif leg == 'kernel':
            nginx, worker = start_nginx()
            sniff = subprocess.Popen([SSLSNIFF, '-p', str(worker)], stdout=out,
                                     stderr=subprocess.DEVNULL, start_new_session=True)
            time.sleep(2)
            assert sniff.poll() is None, 'kernel sslsniff exited'
        else:
            env = dict(os.environ, LD_PRELOAD=f'{BUILDS}/{build}/libbpftime-syscall-server.so')
            sniff = subprocess.Popen([SSLSNIFF], env=env, stdout=out,
                                     stderr=subprocess.DEVNULL, start_new_session=True)
            time.sleep(2)
            assert sniff.poll() is None, 'bpftime sslsniff exited'
            env = dict(os.environ, LD_PRELOAD=f'{BUILDS}/{build}/libbpftime-agent.so')
            nginx, _ = start_nginx(env)
        rps, total, _, errs = wrk(conns, secs)
        return rps, total, errs
    finally:
        stop(nginx)
        if sniff:
            sniff.send_signal(signal.SIGINT)
            try: sniff.wait(timeout=10)
            except subprocess.TimeoutExpired:
                sniff.kill(); sniff.wait()
        if capture: out.close()
        kill_all()
        remove_shm(build)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--payload', type=int, required=True)
    ap.add_argument('--reps', type=int, default=10)
    ap.add_argument('--legs', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--audit', action='store_true')
    a = ap.parse_args()
    legs = a.legs.split(',')
    write_payload(a.payload)
    if a.audit:
        result = {}
        for leg in legs:
            if leg == 'baseline':
                continue
            cap = f'{a.out}.{leg}.sslsniff.txt'
            rps, total, errs = run_leg(leg, conns=10, secs=3, capture=cap)
            events = sum(1 for line in open(cap, errors='replace')
                         if line.startswith(('READ/RECV', 'WRITE/SEND')))
            result[leg] = dict(rps=rps, requests=total, events=events,
                               events_per_request=events / total if total else None, wrk_errors=errs)
            print(f'AUDIT leg={leg} requests={total} events={events} per_request={events / max(total, 1):.3f}', flush=True)
        json.dump(result, open(a.out, 'w'), indent=1)
        return
    samples = {leg: [] for leg in legs}
    for rep in range(a.reps):
        order = legs[rep % len(legs):] + legs[:rep % len(legs)]
        for leg in order:
            rps, total, errs = run_leg(leg)
            samples[leg].append(rps)
            print(f'SAMPLE rep={rep + 1} leg={leg} rps={rps:.2f} requests={total} wrk_errors={errs}', flush=True)
        json.dump(dict(payload_bytes=a.payload, legs=legs, samples=samples), open(a.out, 'w'), indent=1)

if __name__ == '__main__':
    sys.exit(main())
