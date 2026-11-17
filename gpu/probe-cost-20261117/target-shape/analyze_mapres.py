"""Analysis of the map-residual passes (DECISION-RULES.md addendum 3).

usage: python3 -I analyze_mapres.py OUTDIR [OUTDIR ...]   (meas/out/f1-..., f2-...)
Per pass: median t/s of each item, s per run, ns per thread vs base and vs
the family's own no-probe kernel, the count checks, then A, C, M, T, F per
family and the validity checks V1, V2, V4. V3 (registers) was settled before
the runs; V5 and R1-R4 are applied across passes at the end.
"""
import glob, json, os, re, statistics, struct, sys

C = 3137339392
WANT = 5 * C + C // 128
FULL = {'locc', 'loccap', 'regcapc', 'chain', 'loc', 't0'}
ZERO = {'none', 'nonecap', 'empty', 'emptycap', 'nomap', 'nomapcap',
        'locc-noadd', 'loccap-noadd', 'regc-noadd', 'regcapc-noadd'}
FAM = {
    'U': dict(none='none', empty='empty', nomap='nomap', noadd='locc-noadd', full='locc'),
    'K': dict(none='nonecap', empty='emptycap', nomap='nomapcap', noadd='loccap-noadd', full='loccap'),
    'Kreg': dict(none='nonecap', empty='emptycap', nomap='nomapcap', noadd='regcapc-noadd', full='regcapc'),
    'R64': dict(noadd='regc-noadd', full='chain'),
}


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
    for k in sorted(med, key=lambda k: t[k]):
        ov, s, b, isov = info[k]
        chk = []
        if isov:
            chk.append('override=%d' % ov + ('' if ov >= 1 else ' MISSING'))
        if k in FULL:
            chk.append('sum %s %s b194=%s%s' % (s, 'exact' if s == WANT else 'WRONG', b, '' if b == 0 else ' WRONG'))
        elif k in ZERO:
            chk.append('sum %s %s' % (s, 'ok(0)' if s == 0 else ('MISSING' if s is None else 'WRONG')))
        print('  %-14s %6.2f t/s  %.3f s/run  %+.4f ns/thread vs base  %s' % (
            k, med[k], t[k], ns(k, 'base'), ' '.join(chk)))
    q = {}
    for fam, m in FAM.items():
        if not all(x in t for x in m.values()):
            print('  family %s incomplete' % fam)
            continue
        if 'nomap' in m:
            q[fam] = dict(A=ns(m['full'], m['noadd']), C=ns(m['noadd'], m['nomap']),
                          M=ns(m['full'], m['nomap']), T=ns(m['nomap'], m['empty']),
                          F=ns(m['empty'], m['none']), floor=ns(m['nomap'], m['none']),
                          full=ns(m['full'], m['none']))
            order = [m[x] for x in ('none', 'empty', 'nomap', 'noadd', 'full')]
            v4 = all(t[order[i]] <= t[order[i + 1]] + 0.006e-9 * C for i in range(4))
            q[fam]['V4'] = v4
            q[fam]['V6'] = q[fam]['T'] >= 0.012
        else:
            q[fam] = dict(A=ns(m['full'], m['noadd']))
            q[fam]['V4'] = t[m['noadd']] <= t[m['full']] + 0.006e-9 * C
        print('  %-5s ' % fam + '  '.join('%s=%+.4f' % (k, v) if isinstance(v, float) else '%s=%s' % (k, v)
                                          for k, v in q[fam].items()))
    v1 = (abs(ns('t0', 'base') - 0.283) <= 0.007, abs(ns('loc', 'base') - 0.117) <= 0.007)
    print('  V1 t0-base %.4f (0.282-0.284 +-0.006: %s)  loc-base %.4f (0.116-0.118 +-0.006: %s)' % (
        ns('t0', 'base'), v1[0], ns('loc', 'base'), v1[1]))
    res[d] = q

if len(res) >= 2:
    print('== across passes')
    for fam in FAM:
        for k in ('A', 'C', 'M', 'T', 'F', 'floor', 'full'):
            vals = [res[d][fam][k] for d in res if fam in res[d] and k in res[d][fam]]
            if len(vals) >= 2:
                spread = max(vals) - min(vals)
                print('  %-5s %-5s %s  spread %.4f  median %+.4f%s' % (fam, k, ' '.join('%+.4f' % v for v in vals), spread,
                                                         statistics.median(vals), '  > 0.006: third pass (V5)' if spread > 0.006 and len(vals) == 2 else ''))
