import os, re, sys, statistics as st

BASE = '/home/jetson/src/bpftime-gpu-work/policy/logs'
CTL = BASE + '/ctl'
SETS = ['P', 'M100', 'A', 'A0', 'AC', 'AC0']
CELLS = {  # name: (load, launches, threads)
    'c01': ('on6', 3200, 1), 'c02': ('off', 3200, 1), 'c05': ('on6', 2400, 32),
    'c06': ('off', 2400, 32), 'c10': ('on6', 400, 1), 'c15': ('on6', 4000, 1),
    'c17': ('duty6', 3200, 1)}
ROUNDS = [1, 2, 3, 4]
med = st.median


def rd(p):
    with open(p) as f:
        return f.read()


def parse_run(d, cell, s):
    o = d + '/' + cell + '_' + s
    r = {'problems': []}
    env = rd(o + '.env')
    r['env'] = env
    temps = []
    for line in env.splitlines():
        if line.startswith('before') or line.startswith('after'):
            if 'cpu5=1728000' not in line or 'gpu=1020000000' not in line:
                r['problems'].append('clock ' + line)
            m = re.search(r'temps=([\d,]+)', line)
            temps += [int(x) / 1000 for x in m.group(1).split(',')]
    r['tmax_env'] = max(temps)
    if 'victim 1 rc=0' not in env: r['problems'].append('victim rc')
    if s != 'P' and 'loader 1 rc=0' not in env: r['problems'].append('loader rc')
    ld = CELLS[cell][0] if cell in CELLS else None
    if ld and ld != 'off' and 'load rc=0' not in env: r['problems'].append('load rc')
    if 'not attached' in env or 'in use' in env: r['problems'].append('attach/segment')
    if env.count('before') != 1 or env.count('after') != 1: r['problems'].append('state lines')
    v = rd(o + '.1.victim')
    m = re.search(r'cpu_s=([\d.]+) wall_s=([\d.]+)', v)
    if not m: r['problems'].append('no cpu line'); return r
    r['cpu'] = float(m.group(1)); r['wall'] = float(m.group(2))
    r['win'] = tuple(int(x) for x in re.search(r'window_ns=(\d+) (\d+)', v).groups())
    m = re.search(r'per_launch_us mean=([\d.]+) median=([\d.]+)', v)
    r['pl_mean'] = float(m.group(1)); r['pl_med'] = float(m.group(2))
    ve = rd(o + '.1.victim.err')
    if re.search(r'^launch \d+:', ve, re.M): r['problems'].append('launch error')
    if s != 'P':
        lo = rd(o + '.1.loader')
        cnt = int(re.search(r'hostspin count=(\d+)', lo).group(1))
        ns = [int(x) for x in re.search(r'hostspin ns=([\d,]+)', lo).group(1).split(',')]
        r['count'] = cnt; r['ns_all'] = ns; r['ns'] = ns[1:]
        if len(ns) != cnt: r['problems'].append('ns len %d != count %d' % (len(ns), cnt))
        if cell in CELLS:
            L, T = CELLS[cell][1], CELLS[cell][2]
            if cnt != (L + 1) * T: r['problems'].append('count %d expected %d' % (cnt, (L + 1) * T))
    if os.path.exists(o + '.stats'):
        t = rd(o + '.stats').split()
        r['stats'] = {k: v for k, v in (x.split('=', 1) for x in t if '=' in x)}
        if 'count' in r and int(r['stats']['req']) != r['count']:
            r['problems'].append('req %s != count %d' % (r['stats']['req'], r['count']))
    elif s not in ('P', 'M100', 'W'):
        r['problems'].append('no stats')
    if os.path.exists(o + '.load'):
        pts = []
        for line in rd(o + '.load').splitlines():
            p = line.split()
            if len(p) == 2: pts.append((int(p[0]), int(p[1])))
        r['load_pts'] = pts
        a, b = r['win']
        if not pts or pts[0][0] > a or pts[-1][0] < b:
            r['problems'].append('load does not cover window')
        else:
            def interp(t):
                for (t0, n0), (t1, n1) in zip(pts, pts[1:]):
                    if t0 <= t <= t1:
                        return n0 + (n1 - n0) * (t - t0) / (t1 - t0)
            r['load_rate'] = (interp(b) - interp(a)) / (b - a)
            ins = [p for p in pts if a <= p[0] <= b]
            r['load_rate_in'] = (ins[-1][1] - ins[0][1]) / (ins[-1][0] - ins[0][0])
    if os.path.exists(o + '.power'):
        tm = [float(x) for x in re.findall(r'@([\d.]+)C/', rd(o + '.power'))]
        r['tmax_power'] = max(tm) if tm else None
    return r


def gt1(ns):
    return 100.0 * sum(1 for x in ns if x > 1_000_000) / len(ns)


def pct(xs, q):
    xs = sorted(xs)
    return xs[int(len(xs) * q)] if q < 1 else xs[-1]


def load_runs(base, cells, sets):
    R = {}
    for rr in ROUNDS:
        for c in cells:
            for s in sets:
                R[(rr, c, s)] = parse_run('%s/r%d' % (base, rr), c, s)
    return R


R = load_runs(CTL, list(CELLS), SETS)
print('=== claim 1: runs', len(R))
bad = {k: v['problems'] for k, v in R.items() if v['problems']}
print('invalid:', bad if bad else 'none')
nenv = sum(1 for rr in ROUNDS for f in os.listdir('%s/r%d' % (CTL, rr)) if f.endswith('.env'))
print('env files on disk', nenv)
print('tmax env (thermal zones before/after) = %.3f' % max(v['tmax_env'] for v in R.values()))
pw = [v['tmax_power'] for v in R.values() if v.get('tmax_power')]
print('tmax tegrastats .power (no-load cells only) = %.3f  (n=%d)' % (max(pw), len(pw)))
k = max(R, key=lambda k: R[k]['tmax_env']); print('  hottest env run', k, R[k]['tmax_env'])

# per-cell tables
for c in CELLS:
    print('\n=== %s' % c)
    T = CELLS[c][2]
    for s in SETS:
        cpus = [R[(rr, c, s)]['cpu'] / R[(rr, c, s)]['wall'] for rr in ROUNDS]
        dcpu = [R[(rr, c, s)]['cpu'] / R[(rr, c, s)]['wall'] - R[(rr, c, 'P')]['cpu'] / R[(rr, c, 'P')]['wall'] for rr in ROUNDS]
        plm = [R[(rr, c, s)]['pl_mean'] for rr in ROUNDS]
        line = '%-5s cpu/wall med %.3f  minusP med %+.3f [%s]  pl_mean med %.1f' % (
            s, med(cpus), med(dcpu), ' '.join('%+.3f' % x for x in dcpu), med(plm))
        if CELLS[c][0] != 'off':
            lr = [R[(rr, c, s)]['load_rate'] / R[(rr, c, 'P')]['load_rate'] for rr in ROUNDS]
            li = [R[(rr, c, s)]['load_rate_in'] / R[(rr, c, 'P')]['load_rate_in'] for rr in ROUNDS]
            line += '  loadVsP med %.3f (%.3f-%.3f) [%s] (inside-samples med %.3f)' % (
                med(lr), min(lr), max(lr), ' '.join('%.3f' % x for x in lr), med(li))
        if T > 1 and s != 'P':
            cost = [(R[(rr, c, s)]['pl_mean'] - R[(rr, c, 'P')]['pl_mean']) / 32 for rr in ROUNDS]
            line += '  cost/call med %.1f [%s]' % (med(cost), ' '.join('%.1f' % x for x in cost))
        if s != 'P':
            pool = [x for rr in ROUNDS for x in R[(rr, c, s)]['ns']]
            per = [gt1(R[(rr, c, s)]['ns']) for rr in ROUNDS]
            us = [x / 1000 for x in pool]
            line += '\n      lat pooled n=%d mean %.1f med %.1f p90 %.1f p99 %.1f max %.1f  >1ms %.2f%% per-round [%s]' % (
                len(us), st.mean(us), pct(us, .5), pct(us, .9), pct(us, .99), max(us), gt1(pool),
                ' '.join('%.2f' % x for x in per))
        print(line)
    for s in ['A', 'A0', 'AC', 'AC0']:
        keys = ['req', 'hot_iters', 'defer_sleeps', 'slow_sleeps', 'parks', 'park_timeouts', 'unseen_latch',
                'unseen_hits', 'win0', 'win20', 'win80', 'guard_skips']
        print('  stats %-4s ' % s + ' '.join('%s=%s' % (kk, [int(R[(rr, c, s)]['stats'][kk]) for rr in ROUNDS]) for kk in keys))

print('\n=== claim 7: unseen latch / hits, A-family')
for grp, cells in (('main5', ['c01', 'c02', 'c10', 'c15', 'c17']), ('burst', ['c05', 'c06'])):
    runs = [(rr, c, s) for rr in ROUNDS for c in cells for s in ['A', 'A0', 'AC', 'AC0']]
    lat = sum(int(R[k]['stats']['unseen_latch']) for k in runs)
    hits = [(k, R[k]['stats']['unseen_hits']) for k in runs if int(R[k]['stats']['unseen_hits'])]
    print(grp, 'runs', len(runs), 'latch total', lat, 'runs with hits', hits)

print('\n=== claim 6: main round c10')
M = load_runs(BASE, ['c10'], ['P', 'M100', 'A'])
for s in ['M100', 'A']:
    pool = [x for rr in ROUNDS for x in M[(rr, 'c10', s)]['ns']]
    print('main c10', s, '>1ms pooled %.2f%%' % gt1(pool), 'per-round', ['%.2f' % gt1(M[(rr, 'c10', s)]['ns']) for rr in ROUNDS],
          'n', len(pool), 'problems', [M[(rr, 'c10', s)]['problems'] for rr in ROUNDS])
