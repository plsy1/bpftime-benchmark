"""Analysis of the add-path passes (DECISION-RULES.md addendum 4).

usage: python3 -I analyze_addpath.py OUTDIR [OUTDIR ...]   (meas/out/g1-..., g2-...)
Per pass: median t/s of each item, s per run, ns per thread vs base, the
count checks, the end-to-end quantities (A_cur, A_min, saved), the timed
splits (space+checks, arith, guard, atomic, addr) and the checks V1', V4'.
Across passes: spread, median, the third-pass trigger and the rules R5-R7.
"""
import glob, json, os, re, statistics, struct, sys

C = 3137339392
WANT = 5 * C + C // 128
FULL = {'locc', 'addg', 'addone', 'addmin', 'addq', 'addmingen', 'addmina', 't0'}
ZERO = {'none', 'locc-noadd', 'addminnored'}
# the add path shortened step by step, then the floor without the atomic
ORDER = ['locc', 'addg', 'addone', 'addmin', 'addminnored', 'locc-noadd']
TOL = 0.006
RES = 0.012


def load(d):
    med, info = {}, {}
    for f in glob.glob(d + '/run-tg-*'):
        item = f.split('run-tg-')[1]
        name = item.split('@')[1] if '@' in item else item
        log = open(f + '/client.log').read()
        m = re.findall(r'"samples_ts": \[([^\]]*)\]', log)
        if not m:
            print('  NO RESULT', item)
            continue
        med[name] = statistics.median(float(x) for x in m[-1].split(','))
        s = None
        try:
            for mm in json.load(open(f + '/maps.json'))['maps']:
                if mm['type'] == 1531:
                    s = mm['sum']
        except Exception:
            pass
        b194 = None
        if os.path.exists(f + '/hist.bin'):
            n = os.path.getsize(f + '/hist.bin') // 8
            b194 = struct.unpack('<%dQ' % n, open(f + '/hist.bin', 'rb').read())[194]
        info[name] = (len(re.findall('PTX override', log)), s, b194, '@' in item)
    return med, info


res = {}
for d in sys.argv[1:]:
    print('==', d)
    med, info = load(d)
    t = {k: 128 / v for k, v in med.items()}
    ns = lambda a, b: (t[a] - t[b]) / C * 1e9
    bad = []
    for k in sorted(med, key=lambda k: t[k]):
        ov, s, b, isov = info[k]
        chk = []
        if isov:
            chk.append('override=%d' % ov + ('' if ov >= 1 else ' MISSING'))
            if ov < 1:
                bad.append(k)
        if k in FULL:
            ok = s == WANT and b == 0
            chk.append('sum %s %s b194=%s%s' % (s, 'exact' if s == WANT else 'WRONG', b, '' if b == 0 else ' WRONG'))
        elif k in ZERO:
            ok = s == 0
            chk.append('sum %s %s' % (s, 'ok(0)' if s == 0 else ('MISSING' if s is None else 'WRONG')))
        else:
            ok = True
        if not ok:
            bad.append(k)
        print('  %-12s %6.2f t/s  %.3f s/run  %+.4f ns/thread vs base  %s' % (
            k, med[k], t[k], ns(k, 'base'), ' '.join(chk)))
    q = {}
    q['A_cur'] = ns('locc', 'locc-noadd')
    q['A_min'] = ns('addmin', 'locc-noadd')
    q['saved'] = ns('locc', 'addmin')
    q['space+checks'] = ns('locc', 'addone')
    q['space'] = ns('locc', 'addg')
    q['checks'] = ns('addg', 'addone')
    q['arith'] = ns('addone', 'addmin')
    q['guard'] = ns('addq', 'addmin')
    q['atomic'] = ns('addmin', 'addminnored')
    q['addr'] = ns('addminnored', 'locc-noadd')
    for extra in ('addmingen', 'addmina'):
        if extra in t:
            q[extra + '-addmin'] = ns(extra, 'addmin')
    q['t0-base'] = ns('t0', 'base')
    q['none-base'] = ns('none', 'base')
    v1 = (0.059 <= q['A_cur'] <= 0.077, abs(q['t0-base'] - 0.283) <= 0.007)
    v4 = [(ORDER[i], ORDER[i + 1]) for i in range(len(ORDER) - 1)
          if not t[ORDER[i]] >= t[ORDER[i + 1]] - TOL * 1e-9 * C]
    print('  ' + '  '.join('%s=%+.4f' % kv for kv in q.items()))
    print("  V1' A_cur in 0.059..0.077: %s; t0-base 0.283+-0.007: %s" % v1)
    print("  V4' order %s: %s" % (' >= '.join(ORDER), 'ok' if not v4 else 'VIOLATED at ' + str(v4)))
    print('  count checks: %s' % ('all ok' if not bad else 'FAILED ' + str(bad)))
    q['_valid'] = all(v1) and not bad
    q['_v4'] = v4
    res[d] = q

if len(res) >= 2:
    print('== across passes')
    keys = [k for k in next(iter(res.values())) if not k.startswith('_')]
    med = {}
    for k in keys:
        vals = [res[d][k] for d in res if k in res[d]]
        spread = max(vals) - min(vals)
        med[k] = statistics.median(vals)
        third = k in ('A_cur', 'A_min', 'saved') and spread > TOL and len(vals) == 2
        print('  %-18s %s  spread %.4f  median %+.4f%s' % (
            k, ' '.join('%+.4f' % v for v in vals), spread, med[k], '  > 0.006: third pass' if third else ''))
    P = list(res.values())
    if not all(p['_valid'] for p in P):
        print('  a pass failed V1\' or a count check: rules not judged')
    else:
        both = lambda f: all(f(p) for p in P) if len(P) == 2 else (f({k: med[k] for k in keys}) and sum(map(f, P)) >= 2)
        print('  R5 (A_min <= A_cur / 2): %s' % both(lambda p: p['A_min'] <= p['A_cur'] / 2))
        print('  R6 split of the rest: atomic >= addr + 0.012: %s; addr >= atomic + 0.012: %s (neither: not resolved by timing)' % (
            both(lambda p: p['atomic'] >= p['addr'] + RES), both(lambda p: p['addr'] >= p['atomic'] + RES)))
        print('  R7 guard >= 0.012: %s (else not resolved by timing)' % both(lambda p: p['guard'] >= RES))
        for k in ('space', 'checks', 'arith', 'space+checks'):
            print('  split %-12s resolved (>= 0.012): %s' % (k, both(lambda p: p[k] >= RES)))
