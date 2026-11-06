#!/usr/bin/env python3
"""Request latency of nginx traced by sslsniff (more-dims-20261106/n3-tail; runs in the
container, after ssl-nginx-2x2-20261023's run_fair.py). Legs, order rotated per rep, every leg
restarting its processes: baseline (nginx only), kernel (kernel sslsniff -p worker), and bpftime
builds (sslsniff under the syscall server, nginx under the agent). Loads: light = wrk -c 10 with
each connection waiting 1 ms after every response (latency.lua), well below capacity; closed =
wrk -c 100 back to back (the ssl-nginx benchmark's load). wrk -t 2, 10 s, 16-byte page.
Prints one JSON line per leg."""
import argparse, json, os, re, signal, subprocess, time

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


def kill_all():
    for name in ('nginx', 'sslsniff', 'wrk'):
        sh('pkill', '-9', '-x', name)
    time.sleep(0.3)


def remove_shm(build):
    if build:
        sh(f'{BUILDS}/{build}/bpftimetool', 'remove')


def start_nginx(env=None):
    for f in ('access.log', 'error.log', 'nginx.pid'):
        try:
            os.remove(f'{BENCH}/{f}')
        except FileNotFoundError:
            pass
    p = subprocess.Popen(['nginx', '-c', f'{BENCH}/nginx.conf', '-p', BENCH], env=env,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
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
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()


def wrk(load, secs):
    conns, delay = (10, 1) if load == 'light' else (100, 0)
    env = dict(os.environ, WRK_DELAY_MS=str(delay))
    r = subprocess.run(['wrk', '-t', '2', '-c', str(conns), '-d', str(secs), '-s',
                        f'{BENCH}/latency.lua', URL], capture_output=True, text=True, env=env,
                       timeout=secs + 30)
    res = {}
    for k in ('p50', 'p90', 'p99', 'p99.9', 'p99.99', 'max', 'mean'):
        m = re.search(rf'^lat_{re.escape(k)} ([\d.]+)$', r.stdout, re.M)
        res[f'lat_{k}_us'] = float(m.group(1)) if m else None
    for k in ('req_total', 'err_total'):
        m = re.search(rf'^{k} (\d+)$', r.stdout, re.M)
        res[k] = int(m.group(1)) if m else None
    m = re.search(r'Requests/sec:\s+([\d.]+)', r.stdout)
    res['rps'] = float(m.group(1)) if m else None
    return res


def run_leg(leg, load, secs):
    kill_all()
    build = None if leg in ('baseline', 'kernel') else leg
    remove_shm(build)
    sniff = nginx = None
    try:
        if leg == 'baseline':
            nginx, _ = start_nginx()
        elif leg == 'kernel':
            nginx, worker = start_nginx()
            sniff = subprocess.Popen([SSLSNIFF, '-p', str(worker)], stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL, start_new_session=True)
            time.sleep(2)
            assert sniff.poll() is None, 'kernel sslsniff exited'
        else:
            env = dict(os.environ, LD_PRELOAD=f'{BUILDS}/{build}/libbpftime-syscall-server.so')
            sniff = subprocess.Popen([SSLSNIFF], env=env, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL, start_new_session=True)
            time.sleep(2)
            assert sniff.poll() is None, 'bpftime sslsniff exited'
            env = dict(os.environ, LD_PRELOAD=f'{BUILDS}/{build}/libbpftime-agent.so')
            nginx, _ = start_nginx(env)
        return wrk(load, secs)
    finally:
        stop(nginx)
        if sniff:
            sniff.send_signal(signal.SIGINT)
            try:
                sniff.wait(timeout=10)
            except subprocess.TimeoutExpired:
                sniff.kill()
                sniff.wait()
        kill_all()
        remove_shm(build)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--legs', default='baseline,kernel,fix-final,fix-final-probecheck')
    ap.add_argument('--loads', default='light,closed')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=10)
    ap.add_argument('--payload', type=int, default=16)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    write_payload(a.payload)
    legs = a.legs.split(',')
    for rep in range(a.reps):
        for load in a.loads.split(','):
            for leg in legs[rep % len(legs):] + legs[:rep % len(legs)]:
                r = dict(leg=leg, load=load, rep=rep, payload=a.payload)
                try:
                    r.update(run_leg(leg, load, a.secs))
                except Exception as e:
                    r['error'] = repr(e)
                with open(a.out, 'a') as f:
                    f.write(json.dumps(r) + '\n')
                print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
