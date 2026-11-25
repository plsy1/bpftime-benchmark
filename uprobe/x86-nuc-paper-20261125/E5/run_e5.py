#!/usr/bin/env python3
"""E5 runner: field-level differential audit of identity helpers, kernel eBPF vs bpftime.

One leg = the kernel tool (src/e5tool, no preload) on audit_point_k, plus, unless the leg is K,
the bpftime tool (the same loader under LD_PRELOAD=<build>/libbpftime-syscall-server.so) on
audit_point_u; then ONE TARGET PROCESS PER SCENARIO (src/e5target, under
LD_PRELOAD=<build>/libbpftime-agent.so on bpftime legs); both loaders stop with SIGINT and dump
their maps; compare_e5.py joins by seq. Order follows run_paper.py: loaders first, the
agent-preloaded target after the bpftime loader is ready. Everything runs as root via sudo -n.

Locking: run this script bare. It runs each leg as a child
  flock -n -E 75 <lock> timeout --signal=TERM --kill-after=10 <leg-budget> run_e5.py --one-leg ...
(retry every 60 s, at most 20 tries), so the measurement lock is held per leg, never more than
leg-budget + 10 s (default 280 + 10 = 290 s < 300 s). The child refuses to run unless it was
started that way and the lock is held. Inside the child every step has a deadline derived from
the leg budget; on SIGTERM it kills its loaders and targets at once. The bpftime side uses a
private shm name (BPFTIME_GLOBAL_SHM_NAME=e5shm_<pid>, not bpftime*), and after each leg the
parent checks for leftover e5 processes and new /dev/shm entries and removes only its own.

Legs: K, O, Ol, Dn, Dnpc, Dxn (dxnewpc with BPFTIME_DIAG_EXACT=1), or any build key of
harness/config-x86.json; append +ENV=VALUE to add environment for the bpftime side.
Rows (JSON lines, appended to --out): kind=leg (one per leg, with status and metadata), then the
comparer's rows: kind=field per scenario/point/side/field (point = 0/1/2 and 'all'),
kind=scenario, kind=target_meta.

Example:
  python3 run_e5.py --legs K,Dn,Ol --scenarios W1,W8a,W8b,W8c,W8d --iters 50 --out results/x.jsonl
"""
import argparse, datetime, glob, hashlib, json, os, re, shutil, signal, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
R = Path('/home/y/src/bpftime-x86-runs/paper-20261124')
LOCK = R / '.measure.lock'
CFG = R / 'harness' / 'config-x86.json'
BUILDS = R / 'builds'
LEG_ENV = {'Dxn': {'BPFTIME_DIAG_EXACT': '1'}}
# points per iteration (must match e5target.c)
SCEN_POINTS = {'W1': 3, 'W2': 3, 'W3': 3, 'W3c': 2, 'W7g': 3, 'W7r': 3, 'W8a': 3, 'W8b': 3,
               'W8c': 3, 'W8d': 3, 'W12': 2, 'W5': 3, 'W6': 3, 'W6t': 3, 'W6tc': 2}
MAP_MAX = 262144  # e5audit.bpf.c recs max_entries
TARGET = str(SRC / 'e5target')
TOOL = str(SRC / 'e5tool')
PAT_TARGET = '^' + re.escape(TARGET) + '( |$)'
PAT_TOOL = '^' + re.escape(TOOL) + '( |$)'


class Budget(Exception):
    pass


def sh(cmd, timeout=15, **kw):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, errors='replace',
                              timeout=timeout, **kw)
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(cmd, 'timeout', '', 'timeout')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def build_dir(key):
    builds = json.load(open(CFG))['builds']
    if key not in builds:
        raise SystemExit(f'unknown build key {key}')
    return BUILDS / Path(builds[key]).name


def leg_parts(leg):
    name, *extra = leg.split('+')
    extra_env = dict(e.split('=', 1) for e in extra)
    return name, name, extra_env


def shm_entries():
    return set(os.listdir('/dev/shm'))


def leftovers():
    procs = []
    for pat in (PAT_TARGET, PAT_TOOL):
        r = sh(['pgrep', '-a', '-f', pat])
        procs += [l for l in (r.stdout or '').splitlines() if l.strip()]
    shm = sorted(e for e in shm_entries() if e.startswith('e5shm_'))
    return procs, shm


def kill_ours():
    for pat in (PAT_TARGET, PAT_TOOL):
        sh(['sudo', '-n', 'pkill', '-9', '-f', pat])


def remove_shm(names):
    for n in names:
        assert re.fullmatch(r'e5shm_\d+', n), n
        sh(['sudo', '-n', 'rm', '-f', f'/dev/shm/{n}'])


# ---------------------------------------------------------------- one leg (under the lock)

def lock_is_held():
    r = subprocess.run(['flock', '-n', '-E', '75', str(LOCK), 'true'])
    return r.returncode == 75


def start_loader(env, func, dump, log, deadline):
    f = open(log, 'w+')
    p = subprocess.Popen(['sudo', '-n', 'env', *env, TOOL, str(SRC / 'e5audit.bpf.o'), TARGET,
                          func, str(dump)],
                         stdout=f, stderr=subprocess.STDOUT, start_new_session=True)
    end = min(time.time() + 30, deadline)
    while time.time() < end:
        f.seek(0)
        if any(l.strip() == 'attached' for l in f.read().splitlines()):
            return p, f
        if p.poll() is not None:
            break
        time.sleep(0.1)
    f.seek(0)
    text = f.read()[-600:]
    stop_loader(p, f, 2)
    raise RuntimeError(f'loader for {func} not ready: ' + text)


def stop_loader(p, f, wait):
    """SIGINT, wait up to `wait` s, then SIGKILL. Returns (rc, log text)."""
    if p is None:
        return None, ''
    if p.poll() is None:
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'], timeout=5)
        try:
            p.wait(timeout=wait)
        except subprocess.TimeoutExpired:
            sh(['sudo', '-n', 'kill', '-9', '--', f'-{p.pid}'], timeout=5)
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
    f.seek(0)
    return p.returncode, f.read()


def parse_dump_stats(path):
    with open(path) as fh:
        first = fh.readline().split()
    # "# stats calls C dup D fail F"
    return dict(calls=int(first[3]), dup=int(first[5]), fail=int(first[7]))


def meta_row(build, scen_list):
    files = {'e5target': TARGET, 'e5tool': TOOL, 'e5audit.bpf.o': str(SRC / 'e5audit.bpf.o')}
    if build:
        for so in ('libbpftime-agent.so', 'libbpftime-syscall-server.so'):
            files[so] = str(build / so)
    m = {k: sha256(v) for k, v in files.items()}
    recorded = {}
    for line in open(SRC / 'sha256.txt'):
        h, n = line.split()
        recorded[n] = h
    src_ok = all(recorded.get(k) == m[k] for k in ('e5target', 'e5tool', 'e5audit.bpf.o'))
    commit = None
    if build and (build / 'source-state.txt').exists():
        for line in open(build / 'source-state.txt'):
            if line.startswith('commit:'):
                commit = line.split()[1]
                break
    return dict(sha256=m, src_sha_matches_build_record=src_ok, build_commit=commit,
                kernel=os.uname().release,
                started=datetime.datetime.now().astimezone().isoformat(timespec='seconds'))


def one_leg(a):
    if os.environ.get('E5_UNDER_LOCK') != '1' or not lock_is_held():
        print('refusing: --one-leg must run under the measurement lock (started by run_e5.py)',
              file=sys.stderr)
        return 3
    t0 = time.time()
    deadline = t0 + a.leg_budget - 20  # reserve for stop/compare/cleanup

    def on_term(signum, frame):
        raise Budget(f'signal {signum}')
    signal.signal(signal.SIGTERM, on_term)
    signal.signal(signal.SIGINT, on_term)

    leg = a.one_leg
    name, build_key, extra_env = leg_parts(leg)
    build = None if name == 'K' else build_dir(build_key)
    env_u = {**LEG_ENV.get(name, {}), **extra_env}
    scens = a.scenarios.split(',')
    d = Path(a.legdir)
    shm_name = f'e5shm_{os.getpid()}'
    shm_before = shm_entries()
    row = dict(kind='leg', leg=leg, build=str(build) if build else None, env=env_u,
               scenarios=a.scenarios, iters=a.iters, dir=str(d), shm_name=shm_name,
               leg_budget=a.leg_budget, **meta_row(build, scens))
    kp = kf = up = uf = None
    rows = []
    scen_status = {}
    common = [f'BPFTIME_GLOBAL_SHM_NAME={shm_name}', 'BPFTIME_LOG_OUTPUT=console']
    try:
        kp, kf = start_loader([], 'audit_point_k', d / 'k.dump', d / 'k-tool.log', deadline)
        target_env = []
        if build:
            up, uf = start_loader([f'LD_PRELOAD={build}/libbpftime-syscall-server.so',
                                   'BPFTIME_SHM_MEMORY_MB=256', *common,
                                   *[f'{k}={v}' for k, v in env_u.items()]],
                                  'audit_point_u', d / 'u.dump', d / 'u-tool.log', deadline)
            target_env = [f'LD_PRELOAD={build}/libbpftime-agent.so', *common,
                          *[f'{k}={v}' for k, v in env_u.items()]]
        for s in scens:
            left = deadline - time.time()
            if left < 5:
                scen_status[s] = dict(rc='skipped_budget')
                continue
            tmo = min(a.timeout, left)
            tt = time.time()
            cmd = ['sudo', '-n', 'env', *target_env, TARGET, '--scenario', s, '--iters',
                   str(a.iters), '--truth', str(d / f'truth-{s}.txt')]
            if s == 'W12':
                cmd += ['--churn-seconds', str(max(1, int(min(a.churn_seconds, tmo - 15))))]
            rc = run_target(cmd, tmo, d / f'target-{s}.log')
            scen_status[s] = dict(rc=rc, seconds=round(time.time() - tt, 2))
        row['targets'] = scen_status
        krc, ktext = stop_loader(kp, kf, 15)
        kp = None
        urc, utext = stop_loader(up, uf, 15) if up else (None, None)
        up = None
        row['k_tool_rc'], row['u_tool_rc'] = krc, urc
        row['k_tool'] = (re.search(r'dumped .*', ktext) or [None])[0] if ktext else None
        row['u_tool'] = (re.search(r'dumped .*', utext) or [None])[0] if utext else None
        sh(['sudo', '-n', 'chown', '-R', f'{os.getuid()}:{os.getgid()}', str(d)])
        truths = [d / f'truth-{s}.txt' for s in scens if (d / f'truth-{s}.txt').exists()]
        problems = []
        for s in scens:
            if scen_status.get(s, {}).get('rc') != 0:
                problems.append(f'target {s} rc {scen_status.get(s, {}).get("rc")}')
            if not (d / f'truth-{s}.txt').exists():
                problems.append(f'truth {s} missing')
        nrec = 0
        for t in truths:
            nrec += sum(1 for l in open(t) if l.strip() and not l.startswith('#'))
        row['truth_records'] = nrec
        sides = [('k', d / 'k.dump', krc)] + ([('u', d / 'u.dump', urc)] if build else [])
        for side, dump, rc in sides:
            if rc != 0:
                problems.append(f'{side} loader rc {rc}')
            if not dump.exists():
                problems.append(f'{side} dump missing')
                continue
            if dump.stat().st_mtime < t0:
                problems.append(f'{side} dump stale')
                continue
            st = parse_dump_stats(dump)
            row[f'{side}_calls'] = st['calls']
            if side == 'k' and st['calls'] != nrec:
                problems.append(f'kernel calls {st["calls"]} != truth records {nrec}')
        if build:
            row['u_complete'] = row.get('u_calls') == nrec
        if truths and (d / 'k.dump').exists():
            cmd = [sys.executable, str(HERE / 'compare_e5.py'), '--kernel', str(d / 'k.dump'),
                   '--leg', leg]
            for t in truths:
                cmd += ['--truth', str(t)]
            if build and (d / 'u.dump').exists():
                cmd += ['--user', str(d / 'u.dump')]
            c = sh(cmd, timeout=60)
            if c.returncode != 0:
                problems.append('compare failed: ' + (c.stderr or '')[-300:])
            else:
                rows = [json.loads(l) for l in c.stdout.splitlines() if l.strip()]
        row['problems'] = problems
    except Exception as e:  # Budget (SIGTERM) included: record and clean up
        row['error'] = repr(e)
    finally:
        if kp:
            stop_loader(kp, kf, 1)
        if up:
            stop_loader(up, uf, 1)
        kill_ours()
        if build:
            sh(['sudo', '-n', 'env', f'BPFTIME_GLOBAL_SHM_NAME={shm_name}',
                str(build / 'bpftimetool'), 'remove'], timeout=10)
        new_shm = sorted(shm_entries() - shm_before)
        remove_shm([n for n in new_shm if n == shm_name])
        procs, _ = leftovers()
        row['leftover_procs'] = procs
        row['leftover_shm'] = sorted(shm_entries() - shm_before)
        sh(['sudo', '-n', 'chown', '-R', f'{os.getuid()}:{os.getgid()}', str(d)])
    row['ok'] = (not row.get('error') and not row.get('problems') and not row['leftover_procs']
                 and not row['leftover_shm'])
    row['seconds'] = round(time.time() - t0, 1)
    with open(d / 'rows.jsonl', 'w') as f:
        for r in [row] + rows:
            f.write(json.dumps(r) + '\n')
    return 0 if row['ok'] else 1


def run_target(cmd, tmo, log):
    """Run the sudo'ed target with a timeout; on timeout kill it (and its children) by path.
    (Popen.kill cannot signal a root sudo process, so do not rely on subprocess.run's kill.)"""
    with open(log, 'w') as f:
        p = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT)
        try:
            return p.wait(timeout=tmo)
        except subprocess.TimeoutExpired:
            sh(['sudo', '-n', 'pkill', '-9', '-f', PAT_TARGET], timeout=5)
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
            return 'timeout'
        except BaseException:
            sh(['sudo', '-n', 'pkill', '-9', '-f', PAT_TARGET], timeout=5)
            raise


# ---------------------------------------------------------------- parent (no lock held)

def run_leg_locked(a, leg, outdir):
    d = outdir / leg.replace('+', '_').replace('=', '-')
    if d.exists():
        assert d.resolve().is_relative_to(HERE / 'runs') or d.resolve().is_relative_to(outdir.resolve())
        sh(['sudo', '-n', 'rm', '-rf', '--', str(d)])
    d.mkdir(parents=True)
    os.chmod(d, 0o777)
    child = [sys.executable, str(HERE / 'run_e5.py'), '--one-leg', leg, '--legdir', str(d),
             '--scenarios', a.scenarios, '--iters', str(a.iters), '--timeout', str(a.timeout),
             '--leg-budget', str(a.leg_budget), '--churn-seconds', str(a.churn_seconds)]
    env = {**os.environ, 'E5_UNDER_LOCK': '1'}
    rc = None
    for attempt in range(1, a.lock_tries + 1):
        p = subprocess.run(['flock', '-n', '-E', '75', str(LOCK), 'timeout', '--signal=TERM',
                            '--kill-after=10', str(a.leg_budget), *child], env=env)
        rc = p.returncode
        if rc != 75:
            break
        print(f'run_e5: lock held (try {attempt}/{a.lock_tries}), waiting 60 s', file=sys.stderr,
              flush=True)
        time.sleep(60)
    rows = []
    rf = d / 'rows.jsonl'
    if rc != 75 and rf.exists():
        rows = [json.loads(l) for l in open(rf) if l.strip()]
    if not rows:
        rows = [dict(kind='leg', leg=leg, dir=str(d), ok=False,
                     error='lock not acquired' if rc == 75 else f'leg child exited {rc} without rows')]
    procs, shm = leftovers()
    if procs or shm:  # only our own binaries and our own private shm names
        kill_ours()
        remove_shm(shm)
        rows[0]['parent_cleanup'] = dict(procs=procs, shm=shm)
        rows[0]['ok'] = False
    rows[0]['child_rc'] = rc
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--legs', default='Dn')
    ap.add_argument('--scenarios', default='W1,W2,W3,W3c,W7g,W7r,W8a,W8b,W8c,W8d,W12')
    ap.add_argument('--iters', type=int, default=100)
    ap.add_argument('--timeout', type=int, default=60, help='per-scenario target limit, s')
    ap.add_argument('--leg-budget', type=int, default=280,
                    help='hard wall clock per leg (lock held at most this + 10 s); <= 290')
    ap.add_argument('--churn-seconds', type=int, default=120, help='W12 pid-wrap churn limit')
    ap.add_argument('--lock-tries', type=int, default=20)
    ap.add_argument('--outdir', help='default: runs/<timestamp>')
    ap.add_argument('--out', help='JSON lines file (appended)')
    ap.add_argument('--one-leg', help=argparse.SUPPRESS)
    ap.add_argument('--legdir', help=argparse.SUPPRESS)
    a = ap.parse_args()
    if a.leg_budget > 290:
        raise SystemExit('--leg-budget must be <= 290 (lock rule: 5 minutes per command)')
    for s in a.scenarios.split(','):
        if s not in SCEN_POINTS:
            raise SystemExit(f'unknown scenario {s}')
    need = a.iters * sum(SCEN_POINTS[s] for s in a.scenarios.split(','))
    if need > MAP_MAX:
        raise SystemExit(f'{need} records per leg exceed the BPF map capacity {MAP_MAX}; '
                         'lower --iters or split the scenarios')
    if a.one_leg:
        sys.exit(one_leg(a))
    if not a.out:
        raise SystemExit('--out is required')
    outdir = Path(a.outdir) if a.outdir else HERE / 'runs' / time.strftime('%Y%m%d-%H%M%S')
    outdir.mkdir(parents=True, exist_ok=True)
    for leg in a.legs.split(','):
        leg_parts(leg)
        rows = run_leg_locked(a, leg, outdir)
        with open(a.out, 'a') as f:
            for r in rows:
                f.write(json.dumps(r) + '\n')
        r0 = {k: v for k, v in rows[0].items() if k not in ('sha256',)}
        print(json.dumps(r0), flush=True)


if __name__ == '__main__':
    main()
