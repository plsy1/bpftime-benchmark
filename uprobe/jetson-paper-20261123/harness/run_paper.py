#!/usr/bin/env python3
"""Paper campaign ssl-nginx runner (runs inside the container, --pid=host).

Derived from x86-nuc-ssl-nginx-20261026/run_fair.py (itself a copy of the Jetson
ssl-nginx-2x2-20261023 runner). Shared by both platforms; platform settings come from
--config (a JSON file: build paths, affinity policies, wrk threads).

Leg syntax:  NAME[@STAGE][#POLICY][+SWITCH...]
  NAME    B (nginx only), K (kernel sslsniff on the nginx worker), or a build key from
          the config ("O", "Odiag", "L3port", "D", "Opc", "Dpc", ...). A "-stock" suffix
          (K-stock, O-stock, ...) uses the unmodified sslsniff loader (stage S4 only).
  STAGE   S1 S2h S2hm S2m S3 S4 (default S4): selects /sslsniff/sslsniff-<stage>.
  POLICY  affinity policy name from the config (default: config "default_policy").
  SWITCH  NO_PIN UIDGID PIDTID ALL3 NOOP_OUTPUT (Odiag switches), AOT0, BPFSTATS (kernel
          run_cnt/run_time), X (sslsniff -x, stdout captured), or ENV=VALUE.

Per leg: every process restarted; sslsniff stops with SIGINT after nginx has stopped so
the counting loader drains the buffer once more and prints SSLSNIFF_CNT on stderr.
The expected work comes from nginx's own access log (log_format paper: status,
bytes_sent), independent of the tracer. One JSON line per leg is appended to --out.
"""
import argparse, hashlib, json, os, random, re, signal, subprocess, sys, time

BENCH = '/bench'
URL = 'https://127.0.0.1:4043/index.html'
SWITCHES = {
    'NO_PIN': {'BPFTIME_DIAG_NO_PIN': '1'},
    'UIDGID': {'BPFTIME_DIAG_CACHE_UIDGID': '1'},
    'PIDTID': {'BPFTIME_DIAG_CACHE_PIDTID': '1'},
    'ALL3': {'BPFTIME_DIAG_NO_PIN': '1', 'BPFTIME_DIAG_CACHE_UIDGID': '1',
             'BPFTIME_DIAG_CACHE_PIDTID': '1'},
    'NOOP_OUTPUT': {'BPFTIME_DIAG_NOOP_OUTPUT': '1'},
    'AOT0': {'BPFTIME_AOT_CACHE': '0'},
}
STAGES = ('S1', 'S2h', 'S2hm', 'S2m', 'S3', 'S4')
AGENT_ENV_DROP = ('BPFTIME_VM_NAME', 'BPFTIME_DISABLE_JIT', 'BPFTIME_SYSCALL_LAZY',
                  'BPFTIME_HASH_ZERO_SEED', 'BPFTIME_MAP_PRODUCTION_AB', 'BPFTIME_ARRAY_JIT_INLINE')


def sh(*args, **kw):
    return subprocess.run(list(args), capture_output=True, text=True, **kw)


def parse_leg(text, cfg):
    m = re.fullmatch(r'([A-Za-z0-9_-]+)(?:@(\w+))?(?:#(\w+))?((?:\+[A-Za-z0-9_=./-]+)*)', text)
    if not m:
        raise SystemExit(f'bad leg {text}')
    name, stage, policy, rest = m.group(1), m.group(2) or 'S4', m.group(3), m.group(4)
    stock = name.endswith('-stock')
    base = name[:-6] if stock else name
    if stage not in STAGES:
        raise SystemExit(f'bad stage in {text}')
    if stock and stage != 'S4':
        raise SystemExit(f'stock loader only at S4: {text}')
    if base not in ('B', 'K') and base not in cfg['builds']:
        raise SystemExit(f'unknown build {base} in {text}')
    policy = policy or cfg['default_policy']
    if policy not in cfg['policies']:
        raise SystemExit(f'unknown policy {policy} in {text}')
    env, flags = {}, set()
    for sw in filter(None, rest.split('+')):
        if '=' in sw:
            k, v = sw.split('=', 1)
            env[k] = v
        elif sw in SWITCHES:
            env.update(SWITCHES[sw])
        elif sw in ('BPFSTATS', 'X', 'REPORT'):
            flags.add(sw)
        else:
            raise SystemExit(f'unknown switch {sw} in {text}')
    return dict(text=text, base=base, stock=stock, stage=stage, policy=policy, env=env, flags=flags)


def affinity(cpus):
    if not cpus:
        return {}
    mask = set(cpus)
    return dict(preexec_fn=lambda: os.sched_setaffinity(0, mask))


def write_payload(size):
    body = 'X' * max(0, size - len('<html></html>'))
    with open(f'{BENCH}/index.html', 'w') as f:
        f.write('<html>' + body + '</html>')
    assert os.path.getsize(f'{BENCH}/index.html') == size


def kill_all():
    for pat in ('^nginx$', '^sslsniff', '^wrk$'):
        sh('pkill', '-9', pat)
    time.sleep(0.3)


def leftovers():
    """Processes whose executable is one of the measured programs (not by argv)."""
    hits = []
    for pid in os.listdir('/proc'):
        if not pid.isdigit():
            continue
        try:
            exe = os.readlink(f'/proc/{pid}/exe')
        except OSError:
            continue
        b = os.path.basename(exe)
        if b in ('nginx', 'wrk') or b.startswith('sslsniff'):
            hits.append((int(pid), exe))
    return hits


def shm_left():
    return sorted(f for f in os.listdir('/dev/shm') if f.startswith('bpftime'))


def remove_shm(cfg, base):
    if base not in ('B', 'K'):
        sh(f"{cfg['builds'][base]}/bpftimetool", 'remove')


def start_nginx(cpus, env=None):
    for f in ('access.log', 'error.log', 'nginx.pid'):
        try:
            os.remove(f'{BENCH}/{f}')
        except FileNotFoundError:
            pass
    p = subprocess.Popen(['nginx', '-c', f'{BENCH}/nginx.conf', '-p', BENCH], env=env, **affinity(cpus),
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    deadline = time.time() + 20
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


def proc_cpu_s(pid):
    try:
        f = open(f'/proc/{pid}/stat').read().rsplit(')', 1)[1].split()
        return (int(f[11]) + int(f[12])) / os.sysconf('SC_CLK_TCK')
    except (OSError, IndexError, ValueError):
        return None


def cpu_times():
    t = {}
    for line in open('/proc/stat'):
        if line.startswith('cpu') and line[3].isdigit():
            f = line.split()
            v = list(map(int, f[1:]))
            t[int(f[0][3:])] = (v[0] + v[2] + v[5] + v[6], sum(v[:8]))  # user+system+irq+softirq, total
    return t


def busy(t0, t1, cpus):
    out = {}
    for c in cpus:
        b = t1[c][0] - t0[c][0]
        tot = t1[c][1] - t0[c][1]
        out[str(c)] = round(b / tot, 4) if tot else None
    return out


def parse_access_log():
    req = 0
    nbytes = 0
    status_bad = 0
    try:
        for line in open(f'{BENCH}/access.log', errors='replace'):
            f = line.split()
            if len(f) < 2:
                continue
            req += 1
            nbytes += int(f[1])
            if f[0] != '200':
                status_bad += 1
    except FileNotFoundError:
        return None, None, None
    return req, nbytes, status_bad


def wrk(cpus, threads, conns, secs):
    r = sh('wrk', URL, '-t', str(threads), '-c', str(conns), '-d', str(secs), timeout=secs + 30, **affinity(cpus))
    m = re.search(r'Requests/sec:\s+([\d.]+)', r.stdout)
    total = re.search(r'(\d+) requests in', r.stdout)
    assert m and total, r.stdout + r.stderr
    non2xx = re.search(r'Non-2xx or 3xx responses: (\d+)', r.stdout)
    sock = re.search(r'Socket errors: connect (\d+), read (\d+), write (\d+), timeout (\d+)', r.stdout)
    errors = (int(non2xx.group(1)) if non2xx else 0) + (sum(map(int, sock.groups())) if sock else 0)
    return float(m.group(1)), int(total.group(1)), errors, r.stdout


def run_leg(cfg, leg, conns, secs, threads, capdir):
    pol = cfg['policies'][leg['policy']]
    base = leg['base']
    kill_all()
    pre_left = leftovers()
    remove_shm(cfg, base)
    pre_shm = shm_left()
    row = dict(leftovers_pre=len(pre_left), shm_pre=len(pre_shm))
    sniff = nginx = None
    worker = None
    errpath = f'{capdir}/sniff-stderr.txt'
    outpath = f'{capdir}/sniff-stdout.txt' if 'X' in leg['flags'] else None
    errf = open(errpath, 'w')
    outf = open(outpath, 'w') if outpath else subprocess.DEVNULL
    loader = '/sslsniff/sslsniff-stock' if leg['stock'] else f"/sslsniff/sslsniff-{leg['stage'].lower()}"
    sniff_args = [loader] + (['-x'] if 'X' in leg['flags'] else [])
    base_env = {k: v for k, v in os.environ.items() if k not in AGENT_ENV_DROP}
    diagpath = None
    if 'REPORT' in leg['flags']:
        diagpath = f'{capdir}/diag.txt'
        os.chmod(capdir, 0o777)  # nginx workers run as nobody
        base_env['BPFTIME_DIAG_REPORT'] = diagpath
    sniff_cpu = None
    try:
        if base == 'B':
            nginx, worker = start_nginx(pol['nginx'])
        elif base == 'K':
            nginx, worker = start_nginx(pol['nginx'])
            env = dict(base_env)
            if 'BPFSTATS' in leg['flags']:
                env['SSLSNIFF_BPF_STATS'] = '1'
            sniff = subprocess.Popen(sniff_args + ['-p', str(worker)], env=env, stdout=outf, stderr=errf,
                                     start_new_session=True, **affinity(pol['sniff']))
            time.sleep(2)
            assert sniff.poll() is None, 'kernel sslsniff exited'
        else:
            build = cfg['builds'][base]
            env = dict(base_env, LD_PRELOAD=f'{build}/libbpftime-syscall-server.so', **leg['env'])
            sniff = subprocess.Popen(sniff_args, env=env, stdout=outf, stderr=errf,
                                     start_new_session=True, **affinity(pol['sniff']))
            time.sleep(2)
            assert sniff.poll() is None, 'bpftime sslsniff exited'
            env = dict(base_env, LD_PRELOAD=f'{build}/libbpftime-agent.so', **leg['env'])
            nginx, worker = start_nginx(pol['nginx'], env)
        cpus = sorted(set(pol['nginx'] + pol['sniff'] + pol['wrk']))
        t0 = cpu_times()
        w0 = proc_cpu_s(worker)
        s0 = proc_cpu_s(sniff.pid) if sniff else None
        tt0 = time.time()
        rps, total, errors, _ = wrk(pol['wrk'], pol.get('wrk_threads', threads), conns, secs)
        tt1 = time.time()
        t1 = cpu_times()
        w1 = proc_cpu_s(worker)
        s1 = proc_cpu_s(sniff.pid) if sniff else None
        alive = os.path.exists(f'/proc/{worker}')
        row.update(rps=rps, req_wrk=total, wrk_errors=errors, wall_s=round(tt1 - tt0, 3),
                   busy=busy(t0, t1, cpus), nginx_cpu_s=(w1 - w0) if None not in (w0, w1) else None,
                   sniff_cpu_s=(s1 - s0) if None not in (s0, s1) else None, worker_alive=alive)
    finally:
        stop(nginx)
        time.sleep(1)
        if sniff:
            sniff.send_signal(signal.SIGINT)
            try:
                sniff.wait(timeout=15)
            except subprocess.TimeoutExpired:
                sniff.kill()
                sniff.wait()
                row['sniff_killed'] = True
        errf.close()
        if outpath:
            outf.close()
        kill_all()
        remove_shm(cfg, base)
    req_nginx, bytes_nginx, bad = parse_access_log()
    row.update(req_nginx=req_nginx, bytes_nginx=bytes_nginx, nginx_non200=bad)
    cnt, stats = None, []
    for line in open(errpath, errors='replace'):
        if line.startswith('SSLSNIFF_CNT '):
            cnt = json.loads(line[len('SSLSNIFF_CNT '):])
        elif line.startswith('SSLSNIFF_STATS '):
            stats.append(json.loads(line[len('SSLSNIFF_STATS '):]))
        elif line.startswith('[paper-diag]'):
            row.setdefault('diag', []).append(line.strip())
    row['cnt'] = cnt
    if diagpath and os.path.exists(diagpath):
        row['diag'] = row.get('diag', []) + [l.strip() for l in open(diagpath) if l.strip()]
    if stats:
        row['bpf_stats'] = stats
    if outpath:
        row['stdout_capture'] = outpath
    row['leftovers_post'] = len(leftovers())
    row['shm_post'] = len(shm_left())
    return row


def derive(row, leg, payload):
    """Audit fields (plan 2.0.4)."""
    tracing = leg['base'] != 'B'
    cnt = row.get('cnt')
    req = row.get('req_nginx') or 0
    if tracing and cnt and req:
        ev = cnt['read_ev'] + cnt['write_ev']
        row['ev_per_req'] = ev / req
        row['exp_read_ev'] = req
        row['exp_write_bytes'] = row['bytes_nginx']
        if payload <= 1024:
            row['del_over_exp'] = ev / (2 * req)
        else:
            row['del_over_exp_bytes'] = cnt['write_bytes'] / row['bytes_nginx'] if row['bytes_nginx'] else None
        missing = (req - cnt['read_ev']) + max(0, -(-(row['bytes_nginx'] - cnt['write_bytes']) // 16384))
        row['missing_ev'] = missing
    reasons = []
    if row.get('req_wrk') and row.get('wrk_errors', 0) > 0.001 * row['req_wrk']:
        reasons.append('wrk_errors')
    if row.get('worker_alive') is False:
        reasons.append('worker_gone')
    if tracing and cnt is None and not leg['stock']:
        reasons.append('cnt_missing')
    if row.get('leftovers_pre') or row.get('shm_pre'):
        reasons.append('leftover_pre')
    row['valid'] = not reasons
    row['invalid_reason'] = reasons


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--config', required=True)
    ap.add_argument('--legs', required=True, help='comma-separated legs')
    ap.add_argument('--payload', type=int, required=True)
    ap.add_argument('--reps', type=int, default=10)
    ap.add_argument('--rep-start', type=int, default=0)
    ap.add_argument('--conns', type=int, default=100)
    ap.add_argument('--secs', type=int, default=10)
    ap.add_argument('--wrk-threads', type=int)
    ap.add_argument('--session', required=True)
    ap.add_argument('--exp', required=True)
    ap.add_argument('--round', type=int, default=0)
    ap.add_argument('--out', required=True, help='JSONL file to append to')
    ap.add_argument('--capdir', required=True)
    a = ap.parse_args()
    cfg = json.load(open(a.config))
    legs = [parse_leg(t, cfg) for t in a.legs.split(',')]
    threads = a.wrk_threads or cfg.get('wrk_threads', 4)
    write_payload(a.payload)
    os.makedirs(a.capdir, exist_ok=True)
    for rep in range(a.rep_start, a.rep_start + a.reps):
        order = list(range(len(legs)))
        seed = int(hashlib.sha256(f'{a.session}:{a.exp}:{a.round}:{rep}'.encode()).hexdigest()[:16], 16)
        random.Random(seed).shuffle(order)
        pred = None
        for idx, li in enumerate(order):
            leg = legs[li]
            capdir = f'{a.capdir}/{a.exp}-r{a.round}-rep{rep}-{idx:02d}'
            os.makedirs(capdir, exist_ok=True)
            t_start = time.time()
            try:
                row = run_leg(cfg, leg, a.conns, a.secs, threads, capdir)
            except Exception as e:  # recorded, the session decides
                row = dict(error=repr(e))
            row.update(platform=cfg['platform'], session_id=a.session, exp=a.exp, round=a.round, rep=rep,
                       order_idx=idx, pred_leg=pred, leg=leg['text'], build=leg['base'], stage=leg['stage'],
                       policy=leg['policy'], env=leg['env'], stock=leg['stock'], payload=a.payload,
                       conns=a.conns, secs=a.secs, t_start=t_start, t_end=time.time())
            if 'error' in row:
                row['valid'] = False
                row['invalid_reason'] = ['exception']
            else:
                derive(row, leg, a.payload)
            with open(a.out, 'a') as f:
                f.write(json.dumps(row, sort_keys=True) + '\n')
            msg = f"ROW rep={rep} idx={idx} leg={leg['text']} rps={row.get('rps')} req={row.get('req_nginx')} "
            if row.get('cnt'):
                c = row['cnt']
                msg += f"ev={c['read_ev'] + c['write_ev']} lost={c['lost']} tail={c['tail_ev']} "
            msg += f"valid={row['valid']} {row.get('invalid_reason') or ''}"
            print(msg, flush=True)
            pred = leg['text']


if __name__ == '__main__':
    sys.exit(main())
