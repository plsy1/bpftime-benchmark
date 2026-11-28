"""S8 parts A and C (S8-RULES.md): the pt_victim matrix, 1533 against the
host path, and the delivery cost.
usage: python3 -I analyze_s8_victim.py 'out/v*' 'out/c*'"""
import glob, json, os, re, statistics, struct, sys

KIDX = {'D': 0, 'G': 1, 'D4': 0, 'G8': 1}  # pt_kdiv, pt_kflat


def words(path):
    b = open(path, 'rb').read()
    return struct.unpack('<%dQ' % (len(b) // 8), b)


def kv(path):
    b = open(path, 'rb').read()
    out = {}
    for i in range(0, len(b), 16):
        k, _, v = struct.unpack('<IIQ', b[i:i + 16])
        out[k] = v
    return out


def item(run):
    meta = open(os.path.join(run, 'meta.txt')).read()
    m = re.search(r'client_rc=(\d+) client_ms=(\d+)', meta)
    srv = re.search(r'server_rc=(\d+)', meta)
    cl = open(os.path.join(run, 'client.log'), errors='replace').read()
    sv = open(os.path.join(run, 'server.log'), errors='replace').read()
    inline = [int(x) for x in re.findall(r'Device hash maps in [^:]*: (\d+) lookups inline', cl)]
    out = dict(rc=int(m.group(1)) if m else None, ms=int(m.group(2)) if m else None,
               src=int(srv.group(1)) if srv else None, inline=inline,
               compile_errors=cl.count('Unable to compile') + cl.count('requires call prototype'))
    mj = os.path.join(run, 'maps.json')
    if os.path.exists(mj):
        for mp in json.load(open(mj))['maps']:
            if mp['name'] == 'h':
                out['type'] = mp['type']
                out['hsum'] = int(mp['sum'])
                out['entries'] = mp.get('entries')
    if os.path.exists(os.path.join(run, 'chk.bin')):
        out['chk'] = words(os.path.join(run, 'chk.bin'))
    if os.path.exists(os.path.join(run, 'h.kv')):
        out['kv'] = kv(os.path.join(run, 'h.kv'))
    c = re.search(r'map h_st \(companion\):(.*)', sv)
    if c:
        out['comp'] = dict((a, int(b)) for a, b in re.findall(r'(\w+)=(\d+)', c.group(1)))
    d = re.search(r'device hash map \((\d+) entries, (\d+) shards\): (\d+) deliveries, (\d+) us in all, longest (\d+) us', cl)
    if d:
        out['deliv'] = tuple(int(x) for x in d.groups())
    return out


def calls(t, c, k):
    runs, miss = c[k * 8 + 4], c[k * 8 + 1]
    base = t.split('-', 1)[1]
    if base in ('pre', 'miss'):
        return runs
    if base.startswith('cnt'):
        return runs + miss  # lookup, plus an insert after each miss
    return 0


def checks(t, s, it):
    """validity, then the counts of S8-RULES.md A"""
    r = []
    dev = t.startswith('d-')
    r.append(('client_rc 0', it['rc'] == 0, it['rc']))
    r.append(('server_rc 0', it['src'] == 0, it['src']))
    if t == 'empty':
        return r
    if dev:
        r.append(('routed (inline lookups)', bool(it['inline']) and max(it['inline']) >= 1, it['inline']))
        r.append(('no compile error', it['compile_errors'] == 0, it['compile_errors']))
    else:
        r.append(('not routed', not any(it['inline']), it['inline']))
    k = KIDX[s]
    c = it.get('chk')
    if c is None:
        r.append(('chk dumped', False, None))
        return r
    runs, hit, miss, insf = c[k * 8 + 4], c[k * 8 + 0], c[k * 8 + 1], c[k * 8 + 3]
    base = t.split('-', 1)[1]
    if not dev:
        if base == 'pre':
            r.append(('sum = runs', it.get('hsum') == runs, '%s / %d' % (it.get('hsum'), runs)))
        return r
    comp = it.get('comp', {})
    vals = it.get('kv', {})
    ee, e2 = comp.get('eexist'), comp.get('e2big')
    if base == 'pre':
        r.append(('value(k) = runs', vals.get(k) == runs, '%s / %d' % (vals.get(k), runs)))
        r.append(('no NULL', miss == 0, miss))
        r.append(('companion EEXIST = E2BIG = 0', ee == 0 and e2 == 0, '%s %s' % (ee, e2)))
    elif base == 'miss':
        r.append(('map empty', it.get('entries') == 0, it.get('entries')))
        r.append(('hits 0, misses = runs', hit == 0 and miss == runs, '%d %d / %d' % (hit, miss, runs)))
    elif base.startswith('cnt') or base.startswith('kmod'):
        hs = it.get('hsum')
        ok = ee is not None and e2 is not None and hs == runs - ee - e2
        r.append(('sum = runs - EEXIST - E2BIG', ok, '%s = %d - %s - %s' % (hs, runs, ee, e2)))
        r.append(('EEXIST + E2BIG = failed inserts', ee is not None and ee + e2 == insf, '%s + %s / %d' % (ee, e2, insf)))
    return r


def load(pattern):
    out = {}
    for d in sorted(glob.glob(pattern)):
        for f in os.listdir(d):
            if f.startswith('run-'):
                s, t = f[4:].split('-', 1)
                out.setdefault((s, t), []).append((os.path.basename(d), item(os.path.realpath(os.path.join(d, f)))))
    return out


A = load(sys.argv[1]) if len(sys.argv) > 1 else {}
C = load(sys.argv[2]) if len(sys.argv) > 2 else {}

print('== A: validity and counts (every pass, every size)')
bad = 0
for (s, t), runs in sorted(A.items()):
    for p, it in runs:
        for name, ok, val in checks(t, s, it):
            if not ok:
                bad += 1
                print('  FAIL %s %s %s: %s (%s)' % (p, s, t, name, val))
print('  %d failed checks over %d runs' % (bad, sum(len(v) for v in A.values())))

print('\n== A: counts of the counter idiom (losses; 1533: EEXIST from the companion)')
for t in ('cnt-t', 'cnt-t-sm', 'cnt-b-blk', 'cnt-b', 'cnt-l'):
    for s in ('D', 'G'):
        for side in ('h', 'd'):
            for p, it in A.get((s, side + '-' + t), []):
                c, k = it.get('chk'), KIDX[s]
                if not c:
                    continue
                runs = c[k * 8 + 4]
                print('  %-10s %-2s %s %-3s runs %7d  counted %7s  lost %6d  failed inserts %6d%s' % (
                    t, s, side, p, runs, it.get('hsum'), runs - (it.get('hsum') or 0), c[k * 8 + 3],
                    ('  EEXIST %s (unstable %s) E2BIG %s' % (it['comp'].get('eexist'), it['comp'].get('unstable_eexist'),
                                                          it['comp'].get('e2big'))) if 'comp' in it else ''))

print('\n== A: ms per run (median over passes) and per call by slope (per pass)')
print('  item          D ms   D4 ms    G ms   G8 ms   per call D4-D (per pass)              per call G8-G (per pass)')
for t in ('empty', 'h-pre', 'd-pre', 'h-miss', 'd-miss', 'h-cnt-t', 'd-cnt-t', 'h-cnt-t-sm', 'd-cnt-t-sm',
          'h-cnt-b-blk', 'd-cnt-b-blk', 'h-cnt-b', 'd-cnt-b', 'h-cnt-l', 'd-cnt-l'):
    med = {}
    for s in ('D', 'D4', 'G', 'G8'):
        v = [it['ms'] for p, it in A.get((s, t), []) if it['ms'] is not None]
        med[s] = statistics.median(v) if v else None
    slopes = {}
    for big, sm in (('D4', 'D'), ('G8', 'G')):
        per = []
        bp = dict(A.get((big, t), []))
        for p, it in A.get((sm, t), []):
            jt = bp.get(p)
            if not jt or t == 'empty' or 'chk' not in it or 'chk' not in jt:
                continue
            k = KIDX[sm]
            dc = calls(t, jt['chk'], k) - calls(t, it['chk'], k)
            if dc > 0:
                per.append((p, (jt['ms'] - it['ms']) * 1e3 / dc))  # us per call
        slopes[big] = per
    fmt = lambda per: ' '.join('%s %.3f' % (p, v) for p, v in per) + (' us' if per else '')
    print('  %-12s %6s %7s %7s %7s   %-38s %s' % (t, med['D'], med['D4'], med['G'], med['G8'], fmt(slopes['D4']), fmt(slopes['G8'])))

print('\n== A: resolution of a slope (spread of D and D4 times over passes / extra calls)')
for t in ('d-pre', 'd-miss', 'd-cnt-t'):
    for big, sm in (('D4', 'D'), ('G8', 'G')):
        a = [it['ms'] for p, it in A.get((sm, t), [])]
        b = [it['ms'] for p, it in A.get((big, t), [])]
        cs = [calls(t, it['chk'], KIDX[sm]) for p, it in A.get((sm, t), []) if 'chk' in it]
        cb = [calls(t, it['chk'], KIDX[big]) for p, it in A.get((big, t), []) if 'chk' in it]
        if a and b and cs and cb:
            dc = statistics.median(cb) - statistics.median(cs)
            spread = (max(a) - min(a)) + (max(b) - min(b))
            print('  %-8s %s-%s: spread %d ms over %d extra calls = %.1f ns per call' % (t, big, sm, spread, dc, spread * 1e6 / dc))

print('\n== C: delivery cost (pt_victim 4 0, all rows live)')
for (s, t), runs in sorted(C.items()):
    for p, it in runs:
        chk = checks(t, s, it)
        badc = [c for c in chk if not c[1]]
        comp = it.get('comp', {})
        n = int(t.replace('d-kmod', ''))
        extra = [('live = N', comp.get('live') == n, comp.get('live')), ('count = N', comp.get('count') == n, comp.get('count'))]
        badc += [c for c in extra if not c[1]]
        dv = it.get('deliv')
        if dv:
            ent, sh, nd, tot, longest = dv
            print('  %-11s %s: %d entries, %d deliveries, %.0f us per delivery, longest %d us%s  %s' % (
                t, p, ent, nd, tot / max(nd, 1), longest, '  ABOVE 2 ms' if tot / max(nd, 1) > 2000 else '',
                'ok' if not badc else 'INVALID ' + '; '.join('%s %s' % (c[0], c[2]) for c in badc)))
        else:
            print('  %-11s %s: no delivery line  %s' % (t, p, badc))
