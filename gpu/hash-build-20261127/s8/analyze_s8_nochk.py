"""S8 addendum (S8-RULES.md): the pt_victim items without side counts
(-DHS_NOCHK): per call by slope, counts from the map, delivery cost.
usage: python3 -I analyze_s8_nochk.py 'out/n*'"""
import glob, os, statistics, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location('av', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'analyze_s8_victim.py'))
av = importlib.util.module_from_spec(spec)
saved = sys.argv
sys.argv = [sys.argv[0]]  # the victim analysis reports over argv on import
import contextlib, io
with contextlib.redirect_stdout(io.StringIO()):
    spec.loader.exec_module(av)
sys.argv = saved

TL = {'D': 100096, 'D4': 400384, 'G': 57600, 'G8': 230400}  # threads x launches
KIDX = av.KIDX


def checks(t, s, it):
    r = [('client_rc 0', it['rc'] == 0, it['rc']), ('server_rc 0', it['src'] == 0, it['src'])]
    if t == 'empty':
        return r
    dev = t.startswith('hdn')
    if dev:
        r.append(('routed', bool(it['inline']) and max(it['inline']) >= 1 and it['compile_errors'] == 0,
                  '%s %d' % (it['inline'], it['compile_errors'])))
    n = TL[s]
    k = KIDX[s]
    base = t.split('-', 1)[1]
    comp = it.get('comp', {})
    if base == 'pre':
        if dev:
            r.append(('value(k) = threads x launches', it.get('kv', {}).get(k) == n, '%s / %d' % (it.get('kv', {}).get(k), n)))
        else:
            r.append(('sum = threads x launches', it.get('hsum') == n, '%s / %d' % (it.get('hsum'), n)))
    elif base == 'miss':
        r.append(('map empty', it.get('entries') == 0, it.get('entries')))
    elif base.startswith('cnt') or base.startswith('kmod'):
        ee, e2 = comp.get('eexist'), comp.get('e2big')
        ok = ee is not None and it.get('hsum') + ee + e2 == n
        r.append(('sum + EEXIST + E2BIG = threads x launches', ok, '%s + %s + %s / %d' % (it.get('hsum'), ee, e2, n)))
    return r


def calls(t, s, it):
    base = t.split('-', 1)[1]
    n = TL[s]
    if base in ('pre', 'miss'):
        return n
    comp = it.get('comp', {})
    return n + comp.get('eexist', 0) + comp.get('e2big', 0) + 1  # an insert after each miss (one wins)


N = av.load(sys.argv[1])
print('== addendum: validity and counts')
bad = 0
for (s, t), runs in sorted(N.items()):
    for p, it in runs:
        for name, ok, val in checks(t, s, it):
            if not ok:
                bad += 1
                print('  FAIL %s %s %s: %s (%s)' % (p, s, t, name, val))
print('  %d failed checks over %d runs' % (bad, sum(len(v) for v in N.values())))

print('\n== addendum: ms per run (median) and per call by slope (per pass)')
print('  item         D ms   D4 ms    G ms   G8 ms   D4-D per call            G8-G per call')
for t in ('empty', 'hdn-pre', 'hdn-miss', 'hdn-cnt-t', 'hsn-pre'):
    med = {}
    for s in ('D', 'D4', 'G', 'G8'):
        v = [it['ms'] for p, it in N.get((s, t), []) if it['ms'] is not None]
        med[s] = statistics.median(v) if v else None
    out = []
    for big, sm in (('D4', 'D'), ('G8', 'G')):
        bp = dict(N.get((big, t), []))
        per = []
        for p, it in N.get((sm, t), []):
            jt = bp.get(p)
            if not jt:
                continue
            if t == 'empty':
                per.append('%s %+d ms' % (p[:2], jt['ms'] - it['ms']))
                continue
            dc = calls(t, big, jt) - calls(t, sm, it)
            per.append('%s %.1f ns' % (p[:2], (jt['ms'] - it['ms']) * 1e6 / dc))
        out.append(', '.join(per))
    print('  %-11s %6s %7s %7s %7s   %-24s %s' % (t, med['D'], med['D4'], med['G'], med['G8'], out[0], out[1]))
e = {s: [it['ms'] for p, it in N.get((s, 'empty'), [])] for s in ('D', 'D4', 'G', 'G8')}
for big, sm in (('D4', 'D'), ('G8', 'G')):
    if e[big] and e[sm]:
        spread = (max(e[big]) - min(e[big])) + (max(e[sm]) - min(e[sm]))
        print('  resolution %s-%s: the empty item\'s spread %d ms over %d extra threads = %.1f ns per call' % (
            big, sm, spread, TL[big] - TL[sm], spread * 1e6 / (TL[big] - TL[sm])))

print('\n== addendum: delivery cost (D4, all rows live, no side counts)')
import re
for (s, t), runs in sorted(N.items()):
    if 'kmod' not in t:
        continue
    for p, it in runs:
        run = None
        for d in glob.glob(sys.argv[1]):
            if os.path.basename(d) == p:
                run = os.path.realpath(os.path.join(d, 'run-%s-%s' % (s, t)))
        cl = open(os.path.join(run, 'client.log'), errors='replace').read()
        per = [int(x) for x in re.findall(r'GPU map delivery \d+: (\d+) us', cl)]
        ex = re.search(r'GPU map delivery \d+ \(exit\): (\d+) us', cl)
        setup = re.search(r'(\d+) us of set-up', cl)
        dv = it.get('deliv')
        chk = checks(t, s, it)
        badc = [c for c in chk if not c[1]]
        comp = it.get('comp', {})
        print('  %-13s %s: deliveries logged %s us (the first includes set-up %s us); exit %s us; per hash map: %s deliveries, %s us in all, longest %s us; live %s count %s  %s' % (
            t, p[:2], per, setup.group(1) if setup else '?', ex.group(1) if ex else '?',
            dv[2] if dv else '?', dv[3] if dv else '?', dv[4] if dv else '?', comp.get('live'), comp.get('count'),
            'ok' if not badc else 'INVALID ' + '; '.join('%s %s' % (c[0], c[2]) for c in badc)))
