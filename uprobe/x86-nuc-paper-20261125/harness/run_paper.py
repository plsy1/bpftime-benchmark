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
          S4lat (H2): latency loader, see below.
  POLICY  affinity policy name from the config (default: config "default_policy").
  SWITCH  NO_PIN UIDGID PIDTID ALL3 NOOP_OUTPUT (Odiag switches), AOT0, BPFSTATS (kernel
          run_cnt/run_time), X (sslsniff -x, stdout captured), or ENV=VALUE.
          LATW (H2): accepted for old plan lines; no effect any more (S4lat always anchors).

Per leg: every process restarted; sslsniff stops with SIGINT after nginx has stopped so
the counting loader drains the buffer once more and prints SSLSNIFF_CNT on stderr.
The expected work comes from nginx's own access log (log_format paper: status,
bytes_sent), independent of the tracer. One JSON line per leg is appended to --out.

H2 additions (inactive unless used):
  Stage S4lat runs the latency loader S4LAT_LOADER (/sslsniff/sslsniff-s4lat2: stage-4 BPF
  object; sslsniff-s4lat is the superseded first build, kept unchanged for the sha256 list).
  The loader runs with SSLSNIFF_LAT_ANCHOR=usr2 (a leg ENV=VALUE switch can override it) and
  gets SIGUSR2 just before the load generator starts, so its 1 s warm-up is the first second
  of load and everything before the load (nginx readiness probe included) is excluded on
  every leg; SIGUSR1 right after the load generator returns (before nginx stops) closes the
  window. The SSLSNIFF_LAT line goes to row['lat'] (raw loader output); row['lat_q'] adds
  interpolated quantiles and +inf quantiles that also count losses the build did not report
  (upstream Ol/Ol730x/O drop perf records without PERF_RECORD_LOST; see derive()). Invalid:
  'lat_missing', 'lat_usr2_missing', 'lat_empty'. S4lat rows are not interchangeable with S4
  rows (one extra clock_gettime per event; delivery latency includes the loader's per-event
  printf+fflush of earlier events in the same batch): do not pool their rps/CPU with S4.
  --rate R: open loop at R requests/s in total instead of closed loop: the same wrk with a
  Lua pacing script (OPEN_LOOP_LUA; the image has no h2load or wrk2), written to the leg's
  capdir. row['rate_achieved'] = send rate over the schedule measured by the script,
  row['rate_completed'] = completed requests over the same span, row['rate_wrk'] = wrk's
  Requests/sec (its run time includes connection setup); invalid ('rate_miss') when
  rate_achieved or rate_completed differs from R by more than 2%. Schedule lag (send time
  minus slot time) is in row['openloop'].
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
STAGES = ('S1', 'S2h', 'S2hm', 'S2m', 'S3', 'S4', 'S4lat', 'S4pb', 'S4mp')  # S4pb: I4, + SSLSNIFF_PB_PAGES; S4mp: I2, -p per worker
S4LAT_LOADER = '/sslsniff/sslsniff-s4lat2'  # H2 (rebuilt after review; see module doc)
PERF = {'mode': None, 'start': 2.0, 'dur': 16.0}
PERF_EVENTS = {
    'cyc': 'cpu_core/cycles/u,cpu_core/cycles/k,cpu_core/instructions/u,cpu_core/instructions/k,task-clock',
    'sc': ('raw_syscalls:sys_enter,syscalls:sys_enter_sched_setaffinity,syscalls:sys_enter_sched_getaffinity,'
           'syscalls:sys_enter_getuid,syscalls:sys_enter_getgid,syscalls:sys_enter_getpid,'
           'syscalls:sys_enter_gettid,syscalls:sys_enter_process_vm_readv'),
}
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
        elif sw in ('BPFSTATS', 'X', 'REPORT', 'LATW'):
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


def agg(f, w):
    """I2: a per-process metric for one worker (int) or summed over several (list); None if any is None."""
    if not isinstance(w, list):
        return f(w)
    vals = [f(x) for x in w]
    if any(v is None for v in vals):
        return None
    if isinstance(vals[0], dict):
        return {k: sum(v[k] for v in vals) for k in vals[0]}
    return sum(vals)


def start_nginx(cpus, env=None, nworkers=1, conf='nginx.conf'):
    """Returns (master, worker pid) for one worker, (master, [worker pids]) for several (I2)."""
    for f in ('access.log', 'error.log', 'nginx.pid'):
        try:
            os.remove(f'{BENCH}/{f}')
        except FileNotFoundError:
            pass
    p = subprocess.Popen(['nginx', '-c', f'{BENCH}/{conf}', '-p', BENCH], env=env, **affinity(cpus),
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    deadline = time.time() + 20
    while time.time() < deadline:
        r = sh('curl', '-k', '-s', '-o', '/dev/null', '-w', '%{http_code}', URL)
        if r.stdout == '200':
            workers = sh('pgrep', '-P', str(p.pid), '-x', 'nginx').stdout.split()
            assert len(workers) == nworkers, workers
            return (p, int(workers[0])) if nworkers == 1 else (p, sorted(int(w) for w in workers))
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


# --- H1 (eval plan 2.0): ns CPU via schedstat, migrations, all-core busy, IRQ deltas, memory ---
def task_schedstat_ns(pid):
    """Sum of on-CPU ns over all threads of pid (/proc/<pid>/task/*/schedstat field 1)."""
    if pid is None:
        return None
    tot = 0
    try:
        for t in os.listdir(f'/proc/{pid}/task'):
            try:
                tot += int(open(f'/proc/{pid}/task/{t}/schedstat').read().split()[0])
            except (OSError, ValueError, IndexError):
                pass
    except OSError:
        return None
    return tot


def nr_migrations(pid):
    tot = 0
    try:
        for t in os.listdir(f'/proc/{pid}/task'):
            try:
                for line in open(f'/proc/{pid}/task/{t}/sched'):
                    if line.startswith('se.nr_migrations'):
                        tot += int(line.split(':')[1])
            except (OSError, ValueError):
                pass
    except OSError:
        return None
    return tot


def irq_counts():
    """Per-CPU interrupt totals and per-CPU iwlwifi counts from /proc/interrupts."""
    lines = open('/proc/interrupts').read().splitlines()
    ncpu = len(lines[0].split())
    total = [0] * ncpu
    wifi = [0] * ncpu
    for line in lines[1:]:
        f = line.split()
        vals = []
        for x in f[1:1 + ncpu]:
            if x.isdigit():
                vals.append(int(x))
            else:
                break
        for i, v in enumerate(vals):
            total[i] += v
            if 'iwlwifi' in line:
                wifi[i] += v
    return total, wifi


def smaps(pid):
    out = {}
    try:
        for line in open(f'/proc/{pid}/smaps_rollup'):
            f = line.split()
            if f[0] in ('Rss:', 'Pss:', 'Shared_Clean:', 'Shared_Dirty:', 'Private_Dirty:'):
                out[f[0][:-1]] = int(f[1])
    except OSError:
        return None
    return out


def shm_bytes():
    tot = 0
    for f in os.listdir('/dev/shm'):
        if f.startswith('bpftime'):
            try:
                tot += os.stat(f'/dev/shm/{f}').st_blocks * 512
            except OSError:
                pass
    return tot


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


# H2 open loop. The image (bpftime-ssl-nginx:x86-20261026) has wrk 4.1.0 only (no h2load, no
# wrk2), so the rate comes from this wrk Lua script. One global schedule: slot j (j = 0, 1, ...)
# at epoch + j/R, with epoch = wrk start + 200 ms (common to all threads, set in setup()); wrk
# thread i of n owns the slots j = i, i+n, i+2n, ... so the threads interleave instead of
# sending in phase. wrk calls delay() before every request (the first one on each connection
# too) on whichever of the thread's connections just became free; delay() takes the thread's
# next slot and returns the ms to it (CLOCK_MONOTONIC via LuaJIT ffi). Requests therefore go
# out at the schedule's rate whatever the response time, as long as a connection is free;
# slots that fell due while all of the thread's connections were busy go out at once
# (catch-up), so a too-slow server shows up as schedule lag and a missed rate, not as lower
# offered load. request() (called when the request is actually written) pops the oldest
# pending slot and records the lag = send time - slot time; done() prints one OPENLOOP {json}
# line (sent, first/last send, lag mean/min/max, sends more than 1/5 ms late) that open_loop()
# parses. Limits: wrk's timers have 1 ms resolution (delay rounded to whole ms, ms event-loop
# clock), so single sends are off their slot by up to ~1.5 ms either way (smoke: lag_min -1.4,
# lag_max 1.6 ms; visible as lag_min / lag_max / lag_gt1), and above ~1000 req/s per thread
# several sends share a millisecond tick; at most one request is in flight per HTTP/1.1
# connection, and every idle connection holds one future slot (pending_max <= conns/threads).
OPEN_LOOP_LUA = r"""-- run_paper.py H2 open-loop pacing (wrk 4.x + LuaJIT); args: total requests/s, threads
local ffi = require("ffi")
ffi.cdef[[
typedef struct { long tv_sec; long tv_nsec; } h2_timespec_t;
int clock_gettime(int clk_id, h2_timespec_t *tp);
]]
local ts = ffi.new("h2_timespec_t")
local function now_ms()
  ffi.C.clock_gettime(1, ts)  -- CLOCK_MONOTONIC
  return tonumber(ts.tv_sec) * 1000 + tonumber(ts.tv_nsec) / 1e6
end

-- main state: setup() per thread, done() at the end
local main_threads, main_epoch = {}, nil  -- (not h2_*: a local would shadow the thread globals)
function setup(thread)
  if main_epoch == nil then main_epoch = now_ms() + 200 end
  thread:set("h2_tid", #main_threads)
  thread:set("h2_epoch", main_epoch)
  table.insert(main_threads, thread)
end

-- thread states (globals so that done() can read them with thread:get)
local gap, nth, k, req = nil, nil, 0, nil
local q, qh, qt = {}, 1, 0
h2_sent, h2_lag_sum, h2_lag_max, h2_lag_min, h2_gt1, h2_gt5 = 0, 0, -1e18, 1e18, 0, 0
h2_first, h2_last, h2_qmax = -1, -1, 0
function init(args)
  gap = 1000 / tonumber(args[1])
  nth = tonumber(args[2])
  req = wrk.format()
end
function delay()
  local slot = h2_epoch + (h2_tid + k * nth) * gap
  k = k + 1
  qt = qt + 1
  q[qt] = slot
  if qt - qh + 1 > h2_qmax then h2_qmax = qt - qh + 1 end
  local d = slot - now_ms()
  if d <= 0 then return 0 end
  return math.floor(d + 0.5)
end
function request()
  if qh <= qt then  -- (the call wrk makes once to verify the request has no slot)
    local t = now_ms()
    local lag = t - q[qh]
    q[qh] = nil
    qh = qh + 1
    h2_sent = h2_sent + 1
    h2_lag_sum = h2_lag_sum + lag
    if lag > h2_lag_max then h2_lag_max = lag end
    if lag < h2_lag_min then h2_lag_min = lag end
    if lag > 1 then h2_gt1 = h2_gt1 + 1 end
    if lag > 5 then h2_gt5 = h2_gt5 + 1 end
    if h2_first < 0 then h2_first = t end
    h2_last = t
  end
  return req
end
function done(summary, latency, requests)
  local sent, lsum, lmax, lmin, g1, g5, first, last, qmax = 0, 0, -1e18, 1e18, 0, 0, 1e18, -1, 0
  for _, t in ipairs(main_threads) do
    local n = t:get("h2_sent")
    sent = sent + n
    lsum = lsum + t:get("h2_lag_sum")
    g1 = g1 + t:get("h2_gt1")
    g5 = g5 + t:get("h2_gt5")
    if n > 0 then
      lmax = math.max(lmax, t:get("h2_lag_max"))
      lmin = math.min(lmin, t:get("h2_lag_min"))
      first = math.min(first, t:get("h2_first"))
      last = math.max(last, t:get("h2_last"))
    end
    qmax = math.max(qmax, t:get("h2_qmax"))
  end
  if sent == 0 then lmax, lmin, first = 0, 0, -1 end
  io.write(string.format('OPENLOOP {"epoch_ms":%.3f,"first_ms":%.3f,"last_ms":%.3f,"done_ms":%.3f,' ..
    '"threads":%d,"sent":%d,"lag_mean_ms":%.4f,"lag_min_ms":%.4f,"lag_max_ms":%.4f,' ..
    '"lag_gt1":%d,"lag_gt5":%d,"pending_max":%d,"wrk_requests":%d}\n',
    main_epoch or -1, first, last, now_ms(), #main_threads, sent, sent > 0 and lsum / sent or 0, lmin,
    lmax, g1, g5, qmax, summary.requests))
end
"""


def open_loop(cpus, threads, conns, secs, rate, capdir):
    """H2: fixed-rate wrk (OPEN_LOOP_LUA). Returns rps, total, errors, extra-row-fields."""
    threads = min(threads, conns)
    script = f'{capdir}/openloop.lua'
    with open(script, 'w') as f:
        f.write(OPEN_LOOP_LUA)
    cmd = ['wrk', URL, '-t', str(threads), '-c', str(conns), '-d', str(secs), '-s', script, '--',
           f'{rate:.9g}', str(threads)]
    t0 = time.time()
    r = sh(*cmd, timeout=secs + 30, **affinity(cpus))
    wall = time.time() - t0
    m = re.search(r'Requests/sec:\s+([\d.]+)', r.stdout)
    total = re.search(r'(\d+) requests in ([\d.]+)(\w+)', r.stdout)
    ol = re.search(r'^OPENLOOP (\{.*\})$', r.stdout, re.M)
    assert m and total and ol, r.stdout + r.stderr
    ol = json.loads(ol.group(1))
    non2xx = re.search(r'Non-2xx or 3xx responses: (\d+)', r.stdout)
    sock = re.search(r'Socket errors: connect (\d+), read (\d+), write (\d+), timeout (\d+)', r.stdout)
    errors = (int(non2xx.group(1)) if non2xx else 0) + (sum(map(int, sock.groups())) if sock else 0)
    completed = int(total.group(1))
    # Span of the schedule actually used: first slot (epoch) .. last send. A perfect schedule
    # sends floor(span * R) + 1 requests in it, so (sent - 1) / span estimates the offered rate.
    span_s = (ol['last_ms'] - ol['epoch_ms']) / 1000 if ol['sent'] > 1 else None
    achieved = round((ol['sent'] - 1) / span_s, 3) if span_s and span_s > 0 else None
    completed_rate = round((completed - 1) / span_s, 3) if span_s and span_s > 0 else None
    extra = dict(rate_req=rate, rate_method='wrk -s openloop.lua (global interleaved schedule, catch-up)',
                 rate_per_thread=rate / threads, rate_threads=threads, rate_achieved=achieved,
                 rate_completed=completed_rate, rate_span_s=round(span_s, 4) if span_s else None,
                 rate_dev=round(achieved / rate - 1, 5) if achieved else None,
                 rate_completed_dev=round(completed_rate / rate - 1, 5) if completed_rate else None,
                 rate_wrk=float(m.group(1)), rate_by_secs=round(completed / secs, 3),
                 rate_wrk_runtime=total.group(2) + total.group(3), rate_wall_s=round(wall, 3),
                 rate_cmd=' '.join(cmd), rate_stderr=r.stderr[-500:] or None, openloop=ol)
    return float(m.group(1)), completed, errors, extra


def run_leg(cfg, leg, conns, secs, threads, capdir, rate=None):
    pol = cfg['policies'][leg['policy']]
    NW, NCONF = pol.get('nginx_workers', 1), pol.get('nginx_conf', 'nginx.conf')  # I2
    base = leg['base']
    kill_all()
    pre_left = leftovers()
    remove_shm(cfg, base)
    pre_shm = shm_left()
    row = dict(leftovers_pre=len(pre_left), shm_pre=len(pre_shm))
    if base not in ('B', 'K'):
        try:
            row['build_path'] = cfg['builds'][base]
            row['build_sha256'] = open(f"{cfg['builds'][base]}/sha256.txt").read()
        except OSError:
            row['build_sha256'] = None
    sniff = nginx = None
    worker = None
    errpath = f'{capdir}/sniff-stderr.txt'
    outpath = f'{capdir}/sniff-stdout.txt' if 'X' in leg['flags'] else None
    errf = open(errpath, 'w')
    outf = open(outpath, 'w') if outpath else subprocess.DEVNULL
    loader = '/sslsniff/sslsniff-stock' if leg['stock'] else f"/sslsniff/sslsniff-{leg['stage'].lower()}"
    if leg['stage'] == 'S4lat':  # H2
        loader = S4LAT_LOADER
        row['loader'] = loader
        row['loader_sha256'] = hashlib.sha256(open(loader, 'rb').read()).hexdigest()
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
            nginx, worker = start_nginx(pol['nginx'], None, NW, NCONF)
        elif base == 'K':
            nginx, worker = start_nginx(pol['nginx'], None, NW, NCONF)
            env = dict(base_env, **leg['env'])  # E15: loader env such as SSLSNIFF_PB_WAKEUP
            if leg['stage'] == 'S4lat':
                env = dict(base_env, SSLSNIFF_LAT_ANCHOR='usr2', **leg['env'])  # H2
            if 'BPFSTATS' in leg['flags']:
                env['SSLSNIFF_BPF_STATS'] = '1'
            if isinstance(worker, list):  # I2: one -p per worker needs the multi-pid loader
                assert leg['stage'] == 'S4mp', 'several nginx workers need K@S4mp'
                pargs = [a for w in worker for a in ('-p', str(w))]
            else:
                pargs = ['-p', str(worker)]
            sniff = subprocess.Popen(sniff_args + pargs, env=env, stdout=outf, stderr=errf,
                                     start_new_session=True, **affinity(pol['sniff']))
            time.sleep(2)
            assert sniff.poll() is None, 'kernel sslsniff exited'
        else:
            build = cfg['builds'][base]
            env = dict(base_env, LD_PRELOAD=f'{build}/libbpftime-syscall-server.so', **leg['env'])
            if leg['stage'] == 'S4lat':
                env = dict(base_env, LD_PRELOAD=f'{build}/libbpftime-syscall-server.so',
                           SSLSNIFF_LAT_ANCHOR='usr2', **leg['env'])  # H2
            sniff = subprocess.Popen(sniff_args, env=env, stdout=outf, stderr=errf,
                                     start_new_session=True, **affinity(pol['sniff']))
            time.sleep(2)
            assert sniff.poll() is None, 'bpftime sslsniff exited'
            env = dict(base_env, LD_PRELOAD=f'{build}/libbpftime-agent.so', **leg['env'])
            nginx, worker = start_nginx(pol['nginx'], env, NW, NCONF)
        cpus = sorted(set(pol['nginx'] + pol['sniff'] + pol['wrk']))
        t0 = cpu_times()
        w0 = agg(proc_cpu_s, worker)
        s0 = proc_cpu_s(sniff.pid) if sniff else None
        wns0, sns0 = agg(task_schedstat_ns, worker), task_schedstat_ns(sniff.pid if sniff else None)
        mig0 = agg(nr_migrations, worker)
        irq0 = irq_counts()
        perfp = None
        if PERF['mode']:
            events = PERF_EVENTS.get(PERF['mode'], PERF['mode'])
            if PERF['mode'] == 'cyc' and cfg.get('perf_cyc_events'):
                events = cfg['perf_cyc_events']
            perfp = subprocess.Popen(['sh', '-c', f"sleep {PERF['start']}; exec nsenter -t 1 -m -- perf stat -x, "
                                      f"-p {','.join(map(str, worker)) if isinstance(worker, list) else worker} -e {events} -- sleep {PERF['dur']}"],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        win = {}
        if perfp:
            import threading
            def count_window():
                def lines():
                    r = sh('wc', '-l', f'{BENCH}/access.log')
                    return int(r.stdout.split()[0]) if r.stdout else None
                time.sleep(PERF['start'])
                win['l0'] = lines()
                time.sleep(PERF['dur'])
                win['l1'] = lines()
            wt = threading.Thread(target=count_window, daemon=True)
            wt.start()
        if sniff and leg['stage'] == 'S4lat':
            sniff.send_signal(signal.SIGUSR2)  # H2: the latency window opens 1 s after this
            row['lat_usr2_t'] = time.time()
        tt0 = time.time()
        if rate:
            rps, total, errors, extra = open_loop(pol['wrk'], pol.get('wrk_threads', threads), conns, secs, rate, capdir)
            row.update(extra)
        else:
            rps, total, errors, _ = wrk(pol['wrk'], pol.get('wrk_threads', threads), conns, secs)
        tt1 = time.time()
        if sniff and leg['stage'] == 'S4lat':
            sniff.send_signal(signal.SIGUSR1)  # H2: close the latency window before nginx stops
            row['lat_usr1_t'] = time.time()
        if perfp:
            wt.join(timeout=5)
            if win.get('l0') is not None and win.get('l1') is not None:
                row['perf_window_reqs'] = win['l1'] - win['l0']
            out = perfp.communicate(timeout=60)[1]
            perf = {}
            for line in out.splitlines():
                f = line.split(',')
                if len(f) >= 5 and f[2]:
                    try:
                        val = float(f[0])
                    except ValueError:
                        val = None
                    perf[f[2]] = dict(value=val, unit=f[1], run_pct=float(f[4]) if f[4] else None)
            row['perf'] = perf
            row['perf_mode'] = PERF['mode']
            row['perf_dur'] = PERF['dur']
        t1 = cpu_times()
        w1 = agg(proc_cpu_s, worker)
        s1 = proc_cpu_s(sniff.pid) if sniff else None
        wns1, sns1 = agg(task_schedstat_ns, worker), task_schedstat_ns(sniff.pid if sniff else None)
        mig1 = agg(nr_migrations, worker)
        irq1 = irq_counts()
        row['busy_all'] = busy(t0, t1, sorted(t1))
        row['nginx_cpu_ns'] = (wns1 - wns0) if None not in (wns0, wns1) else None
        row['sniff_cpu_ns'] = (sns1 - sns0) if None not in (sns0, sns1) else None
        row['nginx_migrations'] = (mig1 - mig0) if None not in (mig0, mig1) else None
        row['irq_delta'] = {str(c): irq1[0][c] - irq0[0][c] for c in cpus if c < len(irq1[0])}
        row['iwlwifi_delta'] = {str(c): irq1[1][c] - irq0[1][c] for c in cpus if c < len(irq1[1])}
        row['nginx_smaps_kb'] = agg(smaps, worker)
        if isinstance(worker, list):
            row['nginx_workers'] = len(worker)
        row['sniff_smaps_kb'] = smaps(sniff.pid) if sniff else None
        row['shm_bytes'] = shm_bytes()
        try:
            row['nginx_last_cpu'] = int(open(f'/proc/{worker[0] if isinstance(worker, list) else worker}/stat').read().rsplit(')', 1)[1].split()[36])
        except (OSError, ValueError, IndexError):
            row['nginx_last_cpu'] = None
        nb = {str(c): row['busy_all'].get(str(c)) for c in pol['nginx']}
        row['nginx_core'] = int(max(nb, key=lambda c: nb[c] or 0)) if nb else None
        alive = all(os.path.exists(f'/proc/{w}') for w in (worker if isinstance(worker, list) else [worker]))
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
        elif line.startswith('SSLSNIFF_LAT '):
            row['lat'] = json.loads(line[len('SSLSNIFF_LAT '):])
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


def lat_bucket_width(lo, sub_bits):
    if lo < 2 << sub_bits:
        return 1
    return 1 << (lo.bit_length() - 1 - sub_bits)


def lat_quantiles(lat, n_inf, interpolate):
    """H2: quantiles from the loader's buckets, n_inf extra events counted as +inf (None when the
    rank falls among them). Rank = ceil(q * total) in integers, like the loader. interpolate:
    rank-th of the bucket's k values spread evenly over [lo, lo + width); else the bucket's upper
    bound. Both clamped to the exact [min, max]."""
    n, sb = lat['count'], lat['sub_bits']
    total = n + n_inf
    out = {}
    for name, qm in (('p50', 500), ('p90', 900), ('p99', 990), ('p999', 999)):
        rank = max(1, -(-qm * total // 1000))
        if not total or rank > n:
            out[name] = None
            continue
        c = 0
        for lo, k in lat['buckets']:
            if c + k >= rank:
                w = lat_bucket_width(lo, sb)
                v = lo + (rank - c - 0.5) / k * w if interpolate and w > 1 else lo + w - 1
                out[name] = round(min(max(v, lat['min']), lat['max']), 1)
                break
            c += k
    return out


def lat_derive(row, cnt, payload):
    """H2: row['lat_q'] (interpolated quantiles; +inf quantiles that count lost events, including
    the ones the build dropped without a PERF_RECORD_LOST). Upstream Ol/Ol730x/O drop full perf
    records with only a log message, so the loader's lost counter (lat.lost, p*_lost_inf) misses
    them; the shortfall against nginx's access log (unreported_ev at payload <= 1024, else
    missing_ev - lost) is added as +inf. It covers the whole leg, not only the window, so the
    +inf quantiles are an upper bound. The K leg's loader attaches after nginx's readiness probe,
    so K shows 2 unreported events (that request's read and write) without any loss; up to 2 are
    treated as complete. When the loader's own count is not complete, its p*_lost_inf fields are
    moved to lat['reported_only'] and set to None, so they cannot be used by mistake."""
    lat = row['lat']
    if 'buckets' not in lat or 'count' not in lat:
        return
    if payload <= 1024 and row.get('unreported_ev') is not None:
        unrep, src = max(0, row['unreported_ev']), 'unreported_ev'
    elif row.get('missing_ev') is not None and cnt:
        unrep, src = max(0, row['missing_ev'] - cnt.get('lost', 0)), 'missing_ev-lost'
    else:
        unrep, src = None, None
    complete = unrep is not None and unrep <= 2
    reported = lat.get('lost_win', 0) + lat.get('lost_post', 0)
    n_inf = reported + (unrep or 0)
    q = dict(n=lat['count'], n_inf=n_inf, n_inf_reported=reported, n_inf_unreported=unrep,
             unreported_source=src, loss_report_complete=complete)
    for k, v in lat_quantiles(lat, 0, True).items():
        q[k] = v
    for k, v in lat_quantiles(lat, n_inf, True).items():
        q[k + '_inf'] = v
    row['lat_q'] = q
    if not complete and 'reported_only' not in lat:
        keys = [k for k in lat if k.endswith('_lost_inf')]
        lat['reported_only'] = {k: lat[k] for k in keys}
        for k in keys:
            lat[k] = None


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
        if payload <= 1024:
            row['unreported_ev'] = 2 * req - ev - cnt.get('lost', 0)  # C1 audit: missing_ev undercounts writes at 16 B
    reasons = []
    if row.get('req_wrk') and row.get('wrk_errors', 0) > 0.001 * row['req_wrk']:
        reasons.append('wrk_errors')
    if row.get('worker_alive') is False:
        reasons.append('worker_gone')
    if tracing and cnt is None and not leg['stock']:
        reasons.append('cnt_missing')
    if row.get('leftovers_pre') or row.get('shm_pre'):
        reasons.append('leftover_pre')
    if tracing and leg['stage'] == 'S4lat':
        lat = row.get('lat')
        if not lat:
            reasons.append('lat_missing')
        else:
            if lat.get('usr2_missing'):
                reasons.append('lat_usr2_missing')
            if not lat.get('count'):
                reasons.append('lat_empty')
            lat_derive(row, cnt, payload)
    if row.get('rate_req'):
        r = row['rate_req']
        if any(row.get(k) is None or abs(row[k] - r) > 0.02 * r for k in ('rate_achieved', 'rate_completed')):
            reasons.append('rate_miss')
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
    ap.add_argument('--warmup', type=int, default=0, help='first N reps are warm-up (marked warmup=true, excluded by analysis)')
    ap.add_argument('--perf-mode', help='cyc | sc | explicit perf event list; perf stat on the nginx worker')
    ap.add_argument('--perf-start', type=float, default=2.0)
    ap.add_argument('--perf-dur', type=float, default=16.0)
    ap.add_argument('--rate', type=float, help='H2: open loop at this many requests/s in total (wrk + Lua pacing)')
    a = ap.parse_args()
    PERF.update(mode=a.perf_mode, start=a.perf_start, dur=a.perf_dur)
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
                row = run_leg(cfg, leg, a.conns, a.secs, threads, capdir, **({'rate': a.rate} if a.rate else {}))
            except Exception as e:  # recorded, the session decides
                row = dict(error=repr(e))
            if PERF['mode'] and row.get('perf') and row.get('rps'):
                reqs = row.get('perf_window_reqs') or row['rps'] * PERF['dur']
                row['perf_per_req'] = {k: (v['value'] / reqs if v['value'] is not None else None) for k, v in row['perf'].items()}
                if any((v.get('run_pct') or 0) < 99 for v in row['perf'].values()):
                    row.setdefault('flags', []).append('perf_multiplexed')
            row.update(warmup=rep < a.rep_start + a.warmup, platform=cfg['platform'], session_id=a.session, exp=a.exp, round=a.round, rep=rep,
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
            if row.get('lat'):
                lt = row['lat']
                msg += f"lat_n={lt['count']} p50={lt['p50']} p99={lt['p99']} lat_lost={lt['lost']} "
            if row.get('lat_q'):
                lq = row['lat_q']
                msg += f"lat_n_inf={lq['n_inf']} p99_inf={lq['p99_inf']} "
            if row.get('rate_req'):
                msg += (f"rate={row.get('rate_achieved')}/{row['rate_req']} done={row.get('rate_completed')} "
                        f"lag_max_ms={(row.get('openloop') or {}).get('lag_max_ms')} ")
            msg += f"valid={row['valid']} {row.get('invalid_reason') or ''}"
            print(msg, flush=True)
            pred = leg['text']


if __name__ == '__main__':
    sys.exit(main())
