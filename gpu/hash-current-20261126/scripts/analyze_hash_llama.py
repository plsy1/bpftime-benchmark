"""Analysis of the hash maps' current state on llama.cpp (RULES-hash-current.md).
usage: python3 -I analyze_hash_llama.py OUTDIR [OUTDIR ...]"""
import json, os, re, statistics, struct, sys

TOK = {'p32': 32, 'pp': 512, 'tg': 128}
# warm-up: a whole prompt for p32/pp, one token for tg
RUNS = {'p32': 6.0, 'pp': 6.0, 'tg': 5.0 + 1.0 / 128}
KS = {'link': [0], 'blk': [0], 'ldr': [0, 1, 2, 3], 'ldrf': [0, 1, 2, 3]}


def samples(run):
    txt = open(os.path.join(run, 'client.log'), errors='replace').read()
    m = re.search(r'"samples_ts":\s*\[([^\]]*)\]', txt)
    return [float(x) for x in m.group(1).split(',')] if m else []


def words(path):
    b = open(path, 'rb').read()
    return struct.unpack('<%dQ' % (len(b) // 8), b)


res = {}
for d in sys.argv[1:]:
    print('==', d)
    it = {}
    for f in sorted(os.listdir(d)):
        if f.startswith('run-'):
            wl, var = f[4:].split('-', 1)
            run = os.path.realpath(os.path.join(d, f))
            s = samples(run)
            it[(wl, var)] = (statistics.median(s) if s else None, run)
    for (wl, var), (ts, run) in sorted(it.items()):
        if ts is None:
            print('  %s %-5s no samples' % (wl, var))
            continue
        spr = TOK[wl] / ts
        line = '  %-3s %-5s %9.2f t/s  %.4f s/run' % (wl, var, ts, spr)
        sfx = '-ng' if var.endswith('-ng') else ''
        v0 = var[:-3] if sfx else var
        b = it.get((wl, 'base' + sfx))
        if v0 != 'base' and b and b[0]:
            bspr = TOK[wl] / b[0]
            line += '  overhead %.1f%% (x%.1f time)' % (100 * (1 - ts / b[0]), spr / bspr)
            routing = len(re.findall('Thread-local maps in', open(os.path.join(run, 'client.log'), errors='replace').read()))
            c = words(os.path.join(run, 'chk.bin'))
            hsum = None
            for mp in json.load(open(os.path.join(run, 'maps.json')))['maps']:
                if mp['name'] == 'h':
                    hsum = int(mp['sum'])
            runs = sum(c[k * 8 + 4] for k in KS[v0])
            misses = sum(c[k * 8 + 1] for k in KS[v0])
            insf = sum(c[k * 8 + 3] for k in KS[v0])
            ncalls = 0 if v0 == 'ldrf' else runs + (misses if v0 != 'link' else 0)
            per = (spr - bspr) / (ncalls / RUNS[wl]) * 1e6 if ncalls else None
            line += '  routing=%d  probe runs %d (%.0f per run)  host calls %d  %s' % (
                routing, runs, runs / RUNS[wl], ncalls, ('%.2f us/call' % per) if per else '-')
            f = it.get((wl, 'ldrf' + sfx))
            if v0 == 'ldr' and f and f[0]:
                fspr = TOK[wl] / f[0]
                line += '  against the filter alone: +%.4f s/run, %.2f us/call' % (spr - fspr, (spr - fspr) / (ncalls / RUNS[wl]) * 1e6)
            if v0 == 'ldrf':
                line += '  (filter only) hash sum %s, probe runs %d' % (hsum, runs)
            elif v0 == 'link':
                line += '  sum %d %s' % (hsum, 'ok (= runs, no NULL)' if hsum == runs and misses == 0 else 'FAIL (misses %d)' % misses)
            else:
                line += '  counted %d lost %d (insert failed %d)' % (hsum, runs - hsum, insf)
                line += '  per kernel runs %s' % [c[k * 8 + 4] for k in KS[v0]]
            res.setdefault((wl, var), []).append((ts, 100 * (1 - ts / b[0]), per))
        print(line)
print('== across passes')
for k, v in sorted(res.items()):
    print('  %s %-5s t/s %s  overhead %s  us/call %s' % (k[0], k[1], ' '.join('%.2f' % x[0] for x in v),
          ' '.join('%.1f%%' % x[1] for x in v), ' '.join(('%.2f' % x[2]) if x[2] else '-' for x in v)))
