"""Analysis of the pt_victim hash matrix (RULES-hash-current.md).
usage: python3 -I analyze_hash.py OUTDIR [OUTDIR ...]"""
import json, os, re, statistics, struct, sys

THREADS = {'D': 100096, 'G': 14400, 'D4': 100096, 'G8': 14400}   # threads per launch of the size's kernel
KIDX = {'D': 0, 'G': 1, 'D4': 0, 'G8': 1}                # pt_kdiv, pt_kflat


def words(path):
    b = open(path, 'rb').read()
    return struct.unpack('<%dQ' % (len(b) // 8), b)


def item(run):
    meta = open(os.path.join(run, 'meta.txt')).read()
    m = re.search(r'client_rc=(\d+) client_ms=(\d+)', meta)
    rc, ms = (int(m.group(1)), int(m.group(2))) if m else (None, None)
    routing = len(re.findall('Thread-local maps in', open(os.path.join(run, 'client.log'), errors='replace').read()))
    out = dict(rc=rc, ms=ms, routing=routing)
    mj = os.path.join(run, 'maps.json')
    if os.path.exists(mj):
        for mp in json.load(open(mj))['maps']:
            if mp['name'] == 'h':
                out['hsum'] = int(mp['sum'])
                out['entries'] = mp.get('entries')
        if os.path.exists(os.path.join(run, 'chk.bin')):
            out['chk'] = words(os.path.join(run, 'chk.bin'))
    return out


def calls(t, c, k):
    runs, hit, miss = c[k * 8 + 4], c[k * 8 + 0], c[k * 8 + 1]
    if t in ('pre', 'miss', 'upd', 'del'):
        return runs
    if t == 'tid':
        return 3 * runs
    if t == 'tidnd':
        return 2 * runs
    if t == 'rmw':
        return 2 * runs  # lookup, then update (hit) or insert (miss)
    if t.startswith('cnt'):
        return runs + miss  # lookup, plus an insert after each miss
    return 0


def checks(t, s, it):
    k = KIDX[s]
    c = it['chk']
    runs, hit, miss, dele, insf = (c[k * 8 + i] for i in (4, 0, 1, 2, 3))
    hs = it.get('hsum', 0)
    r = []
    if t == 'pre':
        r += [('sum = runs', hs == runs, '%d / %d' % (hs, runs)), ('no NULL', miss == 0, miss)]
    elif t == 'miss':
        r += [('every lookup NULL', miss == runs and hit == 0, '%d / %d' % (miss, runs))]
    elif t == 'upd':
        r += [('no update failed', insf == 0, insf)]
    elif t == 'del':
        r += [('no delete succeeded', dele == 0, dele)]
    elif t == 'tid':
        r += [('found = runs', hit == runs, '%d / %d' % (hit, runs)), ('missing 0', miss == 0, miss),
              ('delete failures 0', dele == 0, dele), ('insert failures 0', insf == 0, insf),
              ('entries left 0', it.get('entries') == 0, it.get('entries'))]
    elif t == 'tidnd':
        r += [('found = runs', hit == runs, '%d / %d' % (hit, runs)), ('missing 0', miss == 0, miss),
              ('entries left = threads', it.get('entries') == THREADS[s], '%s / %d' % (it.get('entries'), THREADS[s]))]
    return r


def loss(t, s, it):
    k = KIDX[s]
    c = it['chk']
    if t.startswith('cnt') or t == 'rmw':
        runs = c[k * 8 + 4]
        return dict(runs=runs, counted=it.get('hsum', 0), lost=runs - it.get('hsum', 0),
                    insert_failed=c[k * 8 + 3], misses=c[k * 8 + 1])
    return None


res = {}
for d in sys.argv[1:]:
    print('==', d)
    items = {}
    for f in sorted(os.listdir(d)):
        if f.startswith('run-'):
            s, t = f[4:].split('-', 1)
            items[(s, t)] = item(os.path.realpath(os.path.join(d, f)))
    for s in ('D', 'G', 'D4', 'G8'):
        e = items.get((s, 'empty'))
        for (ss, t), it in sorted(items.items()):
            if ss != s:
                continue
            k = KIDX[s]
            line = '  %s %-10s rc=%s %6s ms routing=%d' % (s, t, it['rc'], it['ms'], it['routing'])
            if 'chk' in it and t != 'empty':
                n = calls(t, it['chk'], k)
                per = (it['ms'] - e['ms']) * 1000.0 / n if (e and n) else None
                line += '  calls %7d  %s' % (n, ('%.2f us/call' % per) if per is not None else '-')
                res.setdefault((s, t), []).append(per)
                ck = checks(t, s, it)
                bad = [x for x in ck if not x[1]]
                if ck:
                    line += '  ' + ('ok' if not bad else 'FAIL ' + '; '.join('%s %s' % (x[0], x[2]) for x in bad))
                ls = loss(t, s, it)
                if ls:
                    line += '  runs %d counted %d lost %d (insert failed %d, misses %d)' % (
                        ls['runs'], ls['counted'], ls['lost'], ls['insert_failed'], ls['misses'])
            print(line)
print('== per call across passes (us, median [min-max])')
for (s, t), v in sorted(res.items()):
    v = [x for x in v if x is not None]
    if v:
        print('  %s %-10s %.2f [%.2f-%.2f] n=%d spread %.0f%%' % (s, t, statistics.median(v), min(v), max(v), len(v),
              100 * (max(v) - min(v)) / statistics.median(v) if statistics.median(v) else 0))
d = {k: statistics.median([x for x in v if x is not None]) for k, v in res.items() if [x for x in v if x is not None]}
for s in ('D', 'G'):
    if (s, 'tid') in d and (s, 'tidnd') in d:
        # tid - tidnd per thread = one delete of a present key; per thread costs
        print('  %s delete of a present key (tid - tidnd, per thread): %.2f us' % (s, 3 * d[(s, 'tid')] - 2 * d[(s, 'tidnd')]))
