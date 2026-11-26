"""Analysis of passes h1.. (RULES-sum-map-build.md): old vs new build.
usage: python3 -I analyze_h.py OUTDIR [OUTDIR ...]"""
import json, os, re, statistics, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze as A  # step0 counts and the four-kernel checks

C = 3137339392
WANT = 5 * C + C // 128


def load(d):
    items = {}
    for f in sorted(os.listdir(d)):
        if not f.startswith('run-'):
            continue
        wl, var = f[4:].split('-', 1)
        run = os.path.realpath(os.path.join(d, f))
        s = A.samples(run)
        items[(wl, var)] = (statistics.median(s) if s else None, run)
    return items


def correct(wl, var, run):
    tool = var.split('-', 1)[1] if var.startswith(('old-', 'new-', 'newk-', 'cur-', 'o52-', 'n70-', 'tv-')) else var
    if tool not in ('base',) and not tool.split('@')[0] in ('lat_mmvq', 'lat', 'hist'):
        raise SystemExit('unknown tool ' + var)
    out = []
    if '@' in tool:  # a PTX override: it must have been applied
        n = len(re.findall('PTX override', open(os.path.join(run, 'client.log'), errors='replace').read()))
        out.append(('override applied', n >= 1, '%d' % n))
        tool = tool.split('@')[0]
    if tool == 'lat_mmvq':
        s = None
        for mm in json.load(open(os.path.join(run, 'maps.json')))['maps']:
            if mm['type'] == 1531:
                s = mm['sum']
        w = A.words(os.path.join(run, 'hist.bin'))
        out.append(('sum', s == WANT, '%s want %d' % (s, WANT)))
        out.append(('bucket 194', w[194] == 0, '%d' % w[194]))
    elif tool in ('lat', 'hist'):
        out += A.check(wl, tool, run)
    return out


res = {}
for d in sys.argv[1:]:
    print('==', d)
    it = load(d)
    q = {}
    for wl in ('tg', 'pp'):
        if (wl, 'base') not in it:
            continue
        b = it[(wl, 'base')][0]
        for (w, var), (ts, run) in sorted(it.items()):
            if w != wl or ts is None:
                continue
            chk = correct(w, var, run)
            bad = [c for c in chk if not c[1]]
            ns = (128 / ts - 128 / b) / C * 1e9 if (wl == 'tg') else None
            print('  %s %-16s %7.2f t/s  %.3f s/run  overhead %5.1f%%%s  %s' % (
                wl, var, ts, (128 if wl == 'tg' else 512) / ts, 100 * (1 - ts / b),
                ('  %+.4f ns/thread' % ns) if (ns is not None and 'mmvq' in var) else '',
                'ok' if not bad else 'INVALID ' + '; '.join('%s %s' % (c[0], c[2]) for c in bad)))
            q[(wl, var)] = dict(ts=ts, oh=100 * (1 - ts / b), ns=ns, ok=not bad,
                                counts=[c[2] for c in chk if c[0].endswith(('exits timed', 'exits without start', ' calls')) or c[0] in ('sum', 'bucket 194')])
    for wl, tool, A_, B_ in [(w, t, a, b) for (w, t) in (('tg', 'lat_mmvq'), ('tg', 'lat'), ('pp', 'lat')) for (a, b) in (('old-', 'new-'), ('new-', 'newk-'), ('o52-', 'n70-'), ('n70-', 'tv-'))]:
        o, n = q.get((wl, A_ + tool)), q.get((wl, B_ + tool))
        if o and n:
            same = o['counts'] == n['counts']
            print('  %s %-9s %s%.2f -> %s%.2f t/s, overhead %.1f%% -> %.1f%%%s; counts %s' % (
                wl, tool, A_, o['ts'], B_, n['ts'], o['oh'], n['oh'],
                ('; ns %.4f -> %.4f, saved %.4f' % (o['ns'], n['ns'], o['ns'] - n['ns'])) if tool == 'lat_mmvq' else '',
                'same' if same else 'DIFFER'))
    g = [q.get(('tg', v)) for v in ('cur-lat_mmvq', 'cur-lat_mmvq@g0', 'cur-lat_mmvq@gnomap', 'cur-lat_mmvq@gbyval')]
    if all(g):
        print('  attribution (ns/thread): control g0 - cur %+.4f  map part g0 - gnomap %+.4f  by value saves g0 - gbyval %+.4f  rest (gnomap over base) %+.4f' % (
            g[1]['ns'] - g[0]['ns'], g[1]['ns'] - g[2]['ns'], g[1]['ns'] - g[3]['ns'], g[2]['ns']))
        q['_g'] = dict(ctl=g[1]['ns'] - g[0]['ns'], map=g[1]['ns'] - g[2]['ns'], byval=g[1]['ns'] - g[3]['ns'], rest=g[2]['ns'])
    v = [q.get(('tg', x)) for x in ('n70-lat_mmvq', 'n70-lat_mmvq@tv0', 'n70-lat_mmvq@tvtight', 'n70-lat_mmvq@tvpass')]
    if all(v):
        print('  by value (ns/thread): control tv0 - n70 %+.4f  tvpass saves tv0 - tvpass %+.4f  tvtight saves %+.4f  pass trails tight by %+.4f' % (
            v[1]['ns'] - v[0]['ns'], v[1]['ns'] - v[3]['ns'], v[1]['ns'] - v[2]['ns'], v[3]['ns'] - v[2]['ns']))
        q['_v'] = dict(ctl=v[1]['ns'] - v[0]['ns'], tvpass=v[1]['ns'] - v[3]['ns'], tvtight=v[1]['ns'] - v[2]['ns'], gap=v[3]['ns'] - v[2]['ns'])
    o, n = q.get(('tg', 'newk-hist')), q.get(('tg', 'cur-hist'))
    if o and n:
        print('  hist tg128: before %.2f -> after %.2f t/s, overhead %.1f%% -> %.1f%%; counts %s' % (o['ts'], n['ts'], o['oh'], n['oh'], 'same' if o['counts'] == n['counts'] else 'DIFFER'))
    tl = [q.get(('tg', v)) for v in ('new-lat_mmvq', 'new-lat_mmvq@tl0', 'new-lat_mmvq@tlK', 'new-lat_mmvq@tlKL', 'new-lat_mmvq@tlKLW')]
    if all(tl):
        names = ('control tl0 - new', 'K keys forwarded', 'L local accesses', 'W 64-bit accesses')
        steps = [tl[1]['ns'] - tl[0]['ns'], tl[1]['ns'] - tl[2]['ns'], tl[2]['ns'] - tl[3]['ns'], tl[3]['ns'] - tl[4]['ns']]
        print('  thread-local steps (ns/thread saved; control: difference): ' + '  '.join('%s %+.4f' % (n, v) for n, v in zip(names, steps)))
        q['_tl'] = dict(zip(('ctl', 'K', 'L', 'W'), steps))
    o = q.get(('tg', 'old-lat_mmvq'))
    if o:
        print("  link: old-lat_mmvq - base %.4f (0.283 +- 0.007: %s)" % (o['ns'], abs(o['ns'] - 0.283) <= 0.007))
    res[d] = q

if len(res) >= 2:
    print('== across passes')
    tls = [q['_tl'] for q in res.values() if '_tl' in q]
    gs = [q['_g'] for q in res.values() if '_g' in q]
    if len(gs) >= 2:
        for k in ('ctl', 'map', 'byval', 'rest'):
            v = [x[k] for x in gs]
            verdict = ('  helps (>= 0.006 in every pass)' if min(v) >= 0.006 else '  not shown') if k == 'byval' else ''
            print('  attribution %-5s %s spread %.4f%s' % (k, ' '.join('%+.4f' % x for x in v), max(v) - min(v), verdict))
    vs = [q['_v'] for q in res.values() if '_v' in q]
    if len(vs) >= 2:
        for k in ('ctl', 'tvpass', 'tvtight', 'gap'):
            v = [x[k] for x in vs]
            verdict = {'tvpass': '  build (>= 0.006 in every pass)' if min(v) >= 0.006 else '  not shown',
                       'gap': '  trails by >= 0.006 in every pass' if min(v) >= 0.006 else '',
                       'ctl': '  ok' if max(abs(x) for x in v) <= 0.006 else '  control off'}.get(k, '')
            print('  by value %-8s %s spread %.4f%s%s' % (k, ' '.join('%+.4f' % x for x in v), max(v) - min(v), verdict,
                  '  > 0.006: third pass' if k in ('tvpass', 'tvtight') and max(v) - min(v) > 0.006 and len(v) == 2 else ''))
    if len(tls) >= 2:
        for k in ('ctl', 'K', 'L', 'W'):
            v = [t[k] for t in tls]
            verdict = '' if k == 'ctl' else ('  helps (>= 0.006 in every pass)' if min(v) >= 0.006 else '  not shown')
            print('  thread-local %-3s %s spread %.4f%s' % (k, ' '.join('%+.4f' % x for x in v), max(v) - min(v), verdict))
    for (P0, P1), wl, tool in [(p, w, t) for p in (('old-', 'new-'), ('o52-', 'n70-'), ('n70-', 'tv-')) for (w, t) in (('tg', 'lat_mmvq'), ('tg', 'lat'), ('pp', 'lat'))]:
        vals = []
        for d, q in res.items():
            o, n = q.get((wl, P0 + tool)), q.get((wl, P1 + tool))
            if o and n:
                vals.append((o, n))
        if len(vals) < 2:
            continue
        if tool == 'lat_mmvq':
            sv = [o['ns'] - n['ns'] for o, n in vals]
            print('  %s %s%s->%s saved ns %s spread %.4f%s' % (wl, tool, P0, P1, ' '.join('%.4f' % x for x in sv),
                  max(sv) - min(sv), '  > 0.006: third pass' if max(sv) - min(sv) > 0.006 and len(sv) == 2 else ''))
        for k in (P0, P1):
            oh = [(o if k == P0 else n)['oh'] for o, n in vals]
            print('  %s %s %s overhead %s spread %.1f%s' % (wl, tool, k, ' '.join('%.1f%%' % x for x in oh),
                  max(oh) - min(oh), '  > 1 point: third pass' if max(oh) - min(oh) > 1 and len(oh) == 2 else ''))
