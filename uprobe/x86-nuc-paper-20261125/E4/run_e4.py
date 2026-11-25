#!/usr/bin/env python3
"""E4: per-CPU lost-update microbenchmark (kernel eBPF vs bpftime builds).

One run: start the loader (src/pcpu_lost, one program: a, c, or b<W>) for the leg, run
src/tickloop T N (process bound to CPUs 2-7, threads not pinned, so T=16/64 oversubscribe), write
the target's JSON to a file, stop the loader with SIGINT, and read its JSON: the sum of the
per-CPU values, the program runs counted exactly (atomic per-thread .bss slots), NULL lookups.
  lost = expected - sum; missed_runs = expected - runs (probe hits that did not run the program,
  e.g. a #730 per-CPU lock skip); lost_vs_runs = runs - nulls - sum (true lost updates).
Variants: a (PERCPU_ARRAY *v += 1), c (same on PERCPU_HASH), b<W> (volatile load, exactly W
bpf_ktime_get_ns() calls, volatile store; W in 0,1,2,4,8 is a compile-time constant per
program). "b" alone means b<--widen>.
Validity (row["valid"]): target rc 0, loader/target agree on expected, no stale shm, no NULL
lookups, sum <= expected, and
  counting rows: runs == expected, or runs < expected with agent skip warnings on stderr
    (then skipped_runs = true; this is the #730 -EAGAIN path), never runs == 0;
  --no-count rows (timing): the program ran (seen == 1, sum > 0); verified_runs = false because
    missed probe hits cannot be told from lost updates there.
invalid_reason says why a row is not valid.
Leg K runs both processes without preload (root); bpftime legs run the loader with
LD_PRELOAD=<build>/libbpftime-syscall-server.so and the target with libbpftime-agent.so, as
harness/run_paper.py does. /dev/shm/bpftime* is removed (bpftimetool remove) before and after
every bpftime run. Leg order is shuffled per (T, variant, rep) with a seed derived by sha256.
One JSON line per run is appended to --out.
Interpretation caveat: a K result of 0 lost shows what the kernel does in this box's preempt
mode (recorded by run-e4.sh, "voluntary" here). Kernel uprobe BPF programs run under
migrate_disable + RCU-tasks-trace (bpf_prog_run_array_uprobe), not preempt_disable: under
preempt=full they can be preempted mid-program. So K = 0 must not be attributed to "the
kernel's non-preemption" in general.
"""
import argparse, contextlib, fcntl, hashlib, json, os, random, re, signal, subprocess, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / 'src'
BUILDS = Path('/home/y/src/bpftime-x86-runs/paper-20261124/builds')
LEGS = {'K': None, 'Ol': 'olatest', 'Ol730x': 'ol730x', 'Dn': 'dnew', 'Dn730': 'dn730', 'Dn730f': 'dn730f',
        'Dnpc': 'dnewpc', 'Dxn': 'dxnewpc', 'O': 'official'}
SKIP_RE = re.compile(r'execution skipped|per-CPU lock|Unable to lock bpftime cpu')
ERR_RE = re.compile(r'\[(error|critical)\]|failed to|Failed to', re.I)
WIDENS = (0, 1, 2, 4, 8)


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def shm_left():
    return sorted(f for f in os.listdir('/dev/shm') if f.startswith('bpftime'))


def cleanup(leg):
    for name in ('pcpu_lost', 'tickloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    time.sleep(0.2)
    if LEGS[leg]:
        sh(['sudo', '-n', str(BUILDS / LEGS[leg] / 'bpftimetool'), 'remove'])
    time.sleep(0.1)


def env(leg, role, extra_env):
    e = ['sudo', '-n', 'env']
    if LEGS[leg]:
        so = 'libbpftime-syscall-server.so' if role == 'loader' else 'libbpftime-agent.so'
        e += [f'LD_PRELOAD={BUILDS / LEGS[leg] / so}', 'BPFTIME_LOG_OUTPUT=console']
        e += extra_env
    return e


def last_json(text):
    for line in reversed(text.splitlines()):
        line = line.strip()
        if line.startswith('{'):
            try:
                return json.loads(line)
            except ValueError:
                pass
    return None


def parse_variant(tok, default_widen):
    """'a' | 'c' | 'b' | 'b<W>' -> (variant, widen or None)"""
    if tok in ('a', 'c'):
        return tok, None
    if tok == 'b':
        w = default_widen
    elif tok.startswith('b') and tok[1:].isdigit():
        w = int(tok[1:])
    else:
        raise SystemExit(f'bad variant {tok!r}')
    if w not in WIDENS:
        raise SystemExit(f'widen {w} not in {WIDENS}')
    return 'b', w


def kill_group(p):
    if p and p.poll() is None:
        sh(['sudo', '-n', 'kill', '-9', '--', f'-{p.pid}'])


def judge(res, rc, tj, lj):
    """Return (valid, reason). Fills skipped_runs / verified_runs."""
    exp = tj['expected']
    reasons = []
    if rc != 0:
        reasons.append(f'target rc {rc}')
    if lj.get('expected') != exp:
        reasons.append('loader expected != target expected')
    if res['shm_pre']:
        reasons.append('stale /dev/shm/bpftime* before run')
    if lj.get('nulls') not in (0, None):
        reasons.append(f"{lj.get('nulls')} NULL lookups")
    if lj.get('bss_ok') is False:
        reasons.append('.bss not readable')
    if lj.get('sum') is None or lj['sum'] > exp:
        reasons.append('sum > expected')
    res['skipped_runs'] = False
    if lj.get('count_runs'):
        res['verified_runs'] = True
        runs = lj.get('runs')
        if not runs:
            reasons.append('program never ran (attach/injection failure)')
        elif runs > exp:
            reasons.append('runs > expected')
        elif runs < exp:
            if res.get('skip_warnings'):
                res['skipped_runs'] = True   # #730 -EAGAIN path: counted, row stays valid
            else:
                reasons.append(f'{exp - runs} probe hits did not run the program, no skip warning')
    else:
        res['verified_runs'] = False
        if lj.get('seen') != 1 or not lj.get('sum'):
            reasons.append('program never ran (attach/injection failure)')
    return (not reasons), '; '.join(reasons) or None


def run_one(leg, T, N, variant, widen, a):
    cleanup(leg)
    res = dict(leg=leg, build=LEGS[leg], T=T, N=N, variant=variant, widen=widen,
               cell=variant if widen is None else f'b{widen}',
               cpus=a.cpus, count_runs=not a.no_count, shm_pre=len(shm_left()), valid=False)
    if LEGS[leg]:
        try:
            res['build_sha256'] = hashlib.sha256(
                (BUILDS / LEGS[leg] / 'sha256.txt').read_bytes()).hexdigest()[:16]
        except OSError:
            res['build_sha256'] = None
    extra = [x for x in a.env.split(',') if x] if a.env else []
    tdir = tempfile.mkdtemp(prefix='e4-', dir=a.tmp)
    tjson = os.path.join(tdir, 'target.json')
    lout = open(os.path.join(tdir, 'loader.txt'), 'w+', errors='replace')
    loader = target = None
    try:
        cmd = env(leg, 'loader', extra) + [str(SRC / 'pcpu_lost'), '-v', variant, '-B', str(SRC / 'tickloop'),
                                           '-E', tjson] + (['-w', str(widen)] if variant == 'b' else []) \
            + (['-R'] if a.no_count else [])
        # own session + no stdin: sudo (Defaults use_pty) finds no tty, so it runs the command
        # without a pty and group kills reach the command itself
        loader = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=lout, stderr=subprocess.STDOUT,
                                  start_new_session=True)
        deadline = time.time() + 30
        while True:
            lout.seek(0)
            if 'attached' in lout.read():
                break
            if loader.poll() is not None or time.time() > deadline:
                lout.seek(0)
                raise RuntimeError('loader did not attach: ' + lout.read()[-600:])
            time.sleep(0.1)
        t0 = time.time()
        target = subprocess.Popen(env(leg, 'target', extra) + [str(SRC / 'tickloop'), str(T), str(N), a.cpus],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  text=True, errors='replace', start_new_session=True)
        try:
            t_out, t_err = target.communicate(timeout=a.timeout)
        except subprocess.TimeoutExpired:
            kill_group(target)
            target.communicate(timeout=10)
            raise
        res['target_elapsed_s'] = round(time.time() - t0, 3)
        rc = res['target_rc'] = target.returncode
        tj = last_json(t_out)
        if not tj:
            raise RuntimeError('target printed no JSON: ' + (t_out + t_err)[-600:])
        with open(tjson, 'w') as f:
            f.write(json.dumps(tj) + '\n')
        os.chmod(tdir, 0o755); os.chmod(tjson, 0o644)
        res.update(wall_s=tj['wall_s'], expected=tj['expected'])
        res['skip_warnings'] = len(SKIP_RE.findall(t_err))
        res['target_err_lines'] = sum(1 for l in t_err.splitlines() if ERR_RE.search(l))
        res['target_stderr_lines'] = len(t_err.splitlines())
        if res['skip_warnings'] or res['target_err_lines'] or rc:
            res['target_stderr_tail'] = t_err[-600:]
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{loader.pid}'])
        loader.wait(timeout=30)
        lout.seek(0)
        ltext = lout.read()
        lj = last_json(ltext)
        if not lj:
            raise RuntimeError('loader printed no JSON: ' + ltext[-600:])
        for k in ('prog', 'sum', 'lost', 'runs', 'nulls', 'lost_vs_runs', 'missed_runs', 'seen', 'ncpu',
                  'per_cpu'):
            res[k] = lj.get(k)
        res['loader_expected'] = lj.get('expected')
        if res['expected']:
            res['lost_frac'] = round(res['lost'] / res['expected'], 6) if res['lost'] is not None else None
            # per-call cost per busy CPU (CPUs busy = min(T, ncpus))
            res['ns_per_call_cpu'] = round(res['wall_s'] * 1e9 * min(T, a.ncpus) / res['expected'], 1)
        res['valid'], res['invalid_reason'] = judge(res, rc, tj, lj)
    except subprocess.TimeoutExpired:
        res['error'] = f'target timeout {a.timeout}s'
    except Exception as e:  # keep going, record the failure
        res['error'] = repr(e)[-800:]
    finally:
        kill_group(target)
        kill_group(loader)
        lout.close()
        cleanup(leg)
        res['shm_post'] = len(shm_left())
        sh(['sudo', '-n', 'rm', '-rf', tdir])
    if res.get('error'):
        res['invalid_reason'] = res['error']
    return res


def check_builds(legs):
    out = {}
    for leg in legs:
        if not LEGS[leg]:
            continue
        d = BUILDS / LEGS[leg]
        r = sh(['sha256sum', '--check', '--quiet', 'sha256.txt'], cwd=d)
        if r.returncode:
            raise SystemExit(f'sha256 check failed for {d}: {r.stdout}{r.stderr}')
        out[leg] = hashlib.sha256((d / 'sha256.txt').read_bytes()).hexdigest()[:16]
    r = sh(['sha256sum', '--check', '--quiet', 'sha256.txt'], cwd=SRC)
    if r.returncode:
        raise SystemExit(f'sha256 check failed for {SRC}: {r.stdout}{r.stderr}')
    return out


@contextlib.contextmanager
def measure_lock(path):
    """flock -n semantics: try, and if held sleep 60 s and retry (at most 20 tries)."""
    if not path:
        yield
        return
    with open(path, 'a') as f:
        for i in range(20):
            try:
                fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if i == 19:
                    raise RuntimeError('measure lock busy for 20 tries')
                print(f'lock {path} busy, retry in 60 s', file=sys.stderr, flush=True)
                time.sleep(60)
        try:
            yield
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def n_for(T, a):
    if a.n:
        return a.n
    cpc = a.calls_per_cpu
    if ':' in cpc:   # per-T table "1:5e7,4:2e7,..."; T not listed uses the nearest smaller T
        tab = sorted((int(k), float(v)) for k, v in (x.split(':') for x in cpc.split(',')))
        c = tab[0][1]
        for t, v in tab:
            if t <= T:
                c = v
    else:
        c = float(cpc)
    return max(1, round(c * min(T, a.ncpus) / T))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--legs', default='K,Ol,Ol730x,Dn,Dn730')
    ap.add_argument('--threads', default='1,4,16,64')
    ap.add_argument('--variants', default='a,b,c', help='tokens a, c, b (= b<--widen>), b0, b1, b2, b4, b8')
    ap.add_argument('--reps', type=int, default=5)
    ap.add_argument('--n', type=int, help='calls per thread (overrides --calls-per-cpu)')
    ap.add_argument('--calls-per-cpu', default='1:5e7,4:2e7,16:1.65e7,64:1.6e7',
                    help='N = calls_per_cpu * min(T, ncpus) / T (equal work per busy CPU). Number or a '
                         'per-T table. Default aims at ~8.5 s on Ol and ~5.5 s on Dn (smoke: Ol 164/420/'
                         '514/543 ns, Dn 152/290/356/350 ns per call per CPU at T=1/4/16/64); K takes '
                         '~40-56 s per run')
    ap.add_argument('--cpus', default='2-7', help='target process CPU list')
    ap.add_argument('--ncpus', type=int, default=6, help='number of CPUs in --cpus (for N)')
    ap.add_argument('--widen', type=int, default=1, choices=WIDENS,
                    help='plain "b": bpf_ktime_get_ns calls between load and store')
    ap.add_argument('--no-count', action='store_true', help='loader -R: no per-run counting in the program')
    ap.add_argument('--env', default='', help='extra env for bpftime legs, comma separated K=V')
    ap.add_argument('--seed', default='e4-20261125')
    ap.add_argument('--timeout', type=int, default=110)
    ap.add_argument('--tmp', default=str(HERE / 'tmp'))
    ap.add_argument('--cooldown', type=float, default=2.0, help='idle seconds after each run (inside the lock)')
    ap.add_argument('--no-sha-check', action='store_true')
    ap.add_argument('--lock', help='take this flock (non-blocking, 60 s retry x20) around each single run')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    legs = a.legs.split(',')
    os.makedirs(a.tmp, exist_ok=True)
    for leg in legs:
        assert leg in LEGS, leg
        if LEGS[leg]:
            assert (BUILDS / LEGS[leg] / 'libbpftime-agent.so').exists(), leg
    variants = [parse_variant(v, a.widen) for v in a.variants.split(',')]
    if not a.no_sha_check:
        print(json.dumps({'sha256_ok': check_builds(legs)}), flush=True)
    for rep in range(a.reps):
        for T in [int(t) for t in a.threads.split(',')]:
            for variant, widen in variants:
                cell = variant if widen is None else f'b{widen}'
                key = f'{a.seed}|{T}|{cell}|{rep}'
                order = legs[:]
                random.Random(int(hashlib.sha256(key.encode()).hexdigest(), 16)).shuffle(order)
                for pos, leg in enumerate(order):
                    with measure_lock(a.lock):
                        t0 = time.time()
                        res = run_one(leg, T, n_for(T, a), variant, widen, a)
                        time.sleep(a.cooldown)
                    res.update(rep=rep, order_pos=pos, order=','.join(order), seed=a.seed,
                               seconds=round(time.time() - t0, 1),
                               ts=time.strftime('%Y-%m-%dT%H:%M:%S'))
                    with open(a.out, 'a') as f:
                        f.write(json.dumps(res) + '\n')
                    print(json.dumps({k: res.get(k) for k in (
                        'leg', 'T', 'N', 'cell', 'count_runs', 'rep', 'expected', 'runs', 'sum', 'lost',
                        'lost_vs_runs', 'missed_runs', 'wall_s', 'skip_warnings', 'valid',
                        'invalid_reason')}),
                        flush=True)


if __name__ == '__main__':
    main()
