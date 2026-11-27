#!/usr/bin/env python3
"""P10 ring-buffer reserve+submit microbenchmark (eval plan Nice P10; outline: 1 thread and 4 processes, K, O, Tern).

Run inside a pinned session (harness/session.sh plan line starting with '!'), never bare: it refuses unless
P10_IN_SESSION=1. CPUs (session pins 2,4,6 at 3.5 GHz and 8-11 at 2.5 GHz): loader/consumer on 4; '1t' = one victim
on CPU 2; '4p' = four victim processes at once on CPUs 8,9,10,11 (E cores, one shared L2).

Per rep (leg order shuffled per rep): B (victim without any tool) for 1t and 4p; then for each leg and program
(empty, rbprog): start p10tool (kernel: plain; bpftime: under libbpftime-syscall-server.so, private shm name), run the
victim(s) (bpftime: LD_PRELOAD=libbpftime-agent.so, SPDLOG_LEVEL=off), stop the tool, record ns/call per victim and
the tool's records/drops. Expected records for rbprog = (N + WARM) x victims.
Rows: JSON lines to --out.
"""
import argparse, json, os, random, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
R = Path('/home/y/src/bpftime-x86-runs/paper-20261124')
CFG = R / 'harness' / 'config-x86.json'
VICTIM = str(HERE / 'p10victim')
TOOL = str(HERE / 'p10tool')
OBJ = str(HERE / 'p10.bpf.o')
CONS_CPU = '4'
CONF = {'1t': ['2'], '4p': ['8', '9', '10', '11']}


def sh(cmd, timeout=20):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired as e:
        return subprocess.CompletedProcess(cmd, 124, e.stdout or '', e.stderr or '')


def build_dir(key):
    return R / 'builds' / Path(json.load(open(CFG))['builds'][key]).name


def victims(cpus, env, n, warm):
    ps = [subprocess.Popen(['sudo', '-n', 'taskset', '-c', c, 'env', *env, VICTIM, str(n), str(warm)],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for c in cpus]
    out = []
    for p in ps:
        try:
            o, e = p.communicate(timeout=120)
        except subprocess.TimeoutExpired:
            p.kill()
            o, e = p.communicate()
        ns = None
        for line in o.splitlines():
            if line.startswith('ns_per_call'):
                ns = float(line.split()[1])
        out.append(dict(rc=p.returncode, ns=ns, err=e[-300:] if p.returncode else None))
    return out


def run_tool_leg(leg, prog, conf, n, warm, rep):
    build = None if leg == 'K' else build_dir(leg)
    shm = f'p10shm_{os.getpid()}'
    tenv, venv = [], []
    if build:
        common = [f'BPFTIME_GLOBAL_SHM_NAME={shm}', 'BPFTIME_LOG_OUTPUT=console']
        tenv = [f'LD_PRELOAD={build}/libbpftime-syscall-server.so', 'BPFTIME_SHM_MEMORY_MB=256', *common]
        venv = [f'LD_PRELOAD={build}/libbpftime-agent.so', *common, 'SPDLOG_LEVEL=off']
    row = dict(kind='p10', leg=leg, prog=prog, conf=conf, rep=rep, n=n, warm=warm)
    log = open(f'/dev/shm/p10tool-{os.getpid()}.log', 'w+')
    tp = subprocess.Popen(['sudo', '-n', 'taskset', '-c', CONS_CPU, 'env', *tenv, TOOL, OBJ, prog, VICTIM,
                           'p10_point'], stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        ready = False
        for _ in range(300):
            log.seek(0)
            if 'attached' in log.read():
                ready = True
                break
            if tp.poll() is not None:
                break
            time.sleep(0.1)
        if not ready:
            log.seek(0)
            row['error'] = 'tool not ready: ' + log.read()[-400:]
            return row
        row['victims'] = victims(CONF[conf], venv, n, warm)
        time.sleep(0.5)  # let the consumer drain
    finally:
        if tp.poll() is None:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tp.pid}'])
            try:
                tp.wait(timeout=20)
            except subprocess.TimeoutExpired:
                sh(['sudo', '-n', 'kill', '-9', '--', f'-{tp.pid}'])
        log.seek(0)
        text = log.read()
        log.close()
        os.unlink(log.name)
        if build:
            sh(['sudo', '-n', 'env', f'BPFTIME_GLOBAL_SHM_NAME={shm}', str(build / 'bpftimetool'), 'remove'])
            sh(['sudo', '-n', 'rm', '-f', f'/dev/shm/{shm}'])
    for line in text.splitlines():
        if line.startswith('records'):
            p = line.split()
            row['records'], row['drops'] = int(p[1]), int(p[3])
    row['tool_rc'] = tp.returncode
    row['expected'] = (n + warm) * len(CONF[conf]) if prog in ('rbprog', 'rbnw') else 0
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--legs', default='K,O,Ol,Dn')
    ap.add_argument('--progs', default='empty,rbprog')
    ap.add_argument('--reps', type=int, default=5)
    ap.add_argument('--n', type=int, default=2000000)
    ap.add_argument('--warm', type=int, default=100000)
    ap.add_argument('--seed', type=int, default=20261127)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    if os.environ.get('P10_IN_SESSION') != '1':
        raise SystemExit('refusing: run P10 from a pinned session plan line (P10_IN_SESSION=1)')
    rnd = random.Random(a.seed)
    legs = a.legs.split(',')
    with open(a.out, 'a') as f:
        for rep in range(a.reps):
            for conf in CONF:
                r = dict(kind='p10', leg='B', prog='none', conf=conf, rep=rep, n=a.n, warm=a.warm,
                         victims=victims(CONF[conf], [], a.n, a.warm))
                f.write(json.dumps(r) + '\n')
                f.flush()
            order = [(l, p, c) for l in legs for p in a.progs.split(',') for c in CONF]
            rnd.shuffle(order)
            for leg, prog, conf in order:
                r = run_tool_leg(leg, prog, conf, a.n, a.warm, rep)
                f.write(json.dumps(r) + '\n')
                f.flush()
                ns = [v['ns'] for v in r.get('victims', [])]
                print(f"P10 rep {rep} {leg:3s} {prog:6s} {conf} ns {ns} records {r.get('records')} "
                      f"drops {r.get('drops')} exp {r.get('expected')} err {r.get('error')}", flush=True)


if __name__ == '__main__':
    main()
