"""S8 parts B2 and B3 (S8-RULES.md): 1533 against the host path on llama.cpp.
From analyze_hash_llama.py (hash-current), with the 1533 items (dlink, dblk,
dldr): validity (routed, no compile error), counts against the companion.
usage: python3 -I analyze_s8_llama.py OUTDIR [OUTDIR ...]"""
import json, os, re, statistics, struct, sys

TOK = {'p32': 32, 'pp': 512, 'tg': 128}
# warm-up: a whole prompt for p32/pp, one token for tg
RUNS = {'p32': 6.0, 'pp': 6.0, 'tg': 5.0 + 1.0 / 128}
KS = {'link': [0], 'blk': [0], 'ldr': [0, 1, 2, 3], 'ldrf': [0, 1, 2, 3],
      'dlink': [0], 'dblk': [0], 'dldr': [0, 1, 2, 3],
      'dlinkn': [0], 'linkn': [0], 'dnone': [0, 1, 2, 3],
      'dlinkns': [0], 'dnones': [0, 1, 2, 3], 'dldrs': [0, 1, 2, 3], 'snone': [0, 1, 2, 3]}
# addendum 2: no side counts in dlinkn/linkn, so their probe runs are rms_norm's
# thread count over the run (dlink's count in l1/l2)
NOCHK_RUNS = {'dlinkn': 4233216, 'linkn': 4233216, 'dlinkns': 4233216}


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
            cl = open(os.path.join(run, 'client.log'), errors='replace').read()
            routing = len(re.findall('Thread-local maps in', cl))
            inline = [int(x) for x in re.findall(r'Device hash maps in [^:]*: (\d+) lookups inline', cl)]
            cerr = cl.count('Unable to compile') + cl.count('requires call prototype')
            sv = open(os.path.join(run, 'server.log'), errors='replace').read()
            cm = re.search(r'map h_st \(companion\):(.*)', sv)
            comp = dict((a, int(b)) for a, b in re.findall(r'(\w+)=(\d+)', cm.group(1))) if cm else {}
            dev = v0.startswith('d')
            c = words(os.path.join(run, 'chk.bin'))
            hsum = None
            for mp in json.load(open(os.path.join(run, 'maps.json')))['maps']:
                if mp['name'] == 'h':
                    hsum = int(mp['sum'])
            runs = NOCHK_RUNS.get(v0) or sum(c[k * 8 + 4] for k in KS[v0])
            misses = sum(c[k * 8 + 1] for k in KS[v0])
            insf = sum(c[k * 8 + 3] for k in KS[v0])
            ncalls = 0 if v0 in ('ldrf', 'dnone', 'dnones', 'snone') else runs + (misses if v0 not in ('link', 'dlink', 'dlinkn', 'linkn', 'dlinkns') else 0)
            dv = re.search(r'device hash map \((\d+) entries, \d+ shards\): (\d+) deliveries, (\d+) us in all', cl)
            if dv:
                line += '  [%s-entry map: %s deliveries, %.2f s]' % (dv.group(1), dv.group(2), int(dv.group(3)) / 1e6)
            per = (spr - bspr) / (ncalls / RUNS[wl]) * 1e6 if ncalls else None
            if dev:
                line += '  routed %s compile errors %d %s' % (inline, cerr, 'ok' if inline and max(inline) >= 1 and cerr == 0 else 'INVALID')
            line += '  routing=%d  probe runs %d (%.0f per run)  host calls %d  %s' % (
                routing, runs, runs / RUNS[wl], ncalls, ('%.2f us/call' % per) if per else '-')
            f = it.get((wl, 'ldrf' + sfx))
            if v0 in ('ldr', 'dldr', 'dnone', 'dldrs', 'dnones', 'snone') and f and f[0]:
                fspr = TOK[wl] / f[0]
                line += '  against the filter alone: %+.4f s/run%s' % (spr - fspr, (', %.2f us/call' % ((spr - fspr) / (ncalls / RUNS[wl]) * 1e6)) if ncalls else '')
            if v0 in ('dnone', 'dnones', 'snone'):
                line += '  (1533 declared, unused) map entries %s, probe runs %d' % (
                    [m.get('entries') for m in json.load(open(os.path.join(run, 'maps.json')))['maps'] if m['name'] == 'h'], runs)
                dn = it.get((wl, {'dnone': 'dldr', 'dnones': 'dldrs'}.get(v0, 'none') + sfx))
                if dn and dn[0]:
                    line += '; dldr - dnone: %+.4f s/run' % (TOK[wl] / dn[0] - spr)
            elif v0 == 'ldrf':
                line += '  (filter only) hash sum %s, probe runs %d' % (hsum, runs)
            elif v0 in ('dlinkn', 'linkn', 'dlinkns'):
                vals = None
                if v0 in ('dlinkn', 'dlinkns') and os.path.exists(os.path.join(run, 'h.kv')):
                    kvb = open(os.path.join(run, 'h.kv'), 'rb').read()
                    vals = dict((struct.unpack('<IIQ', kvb[i:i + 16])[0], struct.unpack('<IIQ', kvb[i:i + 16])[2]) for i in range(0, len(kvb), 16))
                got = vals.get(0) if vals is not None else hsum
                line += '  value %s %s' % (got, 'ok (= rms_norm threads)' if got == runs else 'FAIL')
                line += '  per call %.2f ns' % ((spr - bspr) / (runs / RUNS[wl]) * 1e9)
            elif v0 in ('link', 'dlink'):
                line += '  sum %d %s' % (hsum, 'ok (= runs, no NULL)' if hsum == runs and misses == 0 else 'FAIL (misses %d)' % misses)
            elif dev:
                ee, e2 = comp.get('eexist'), comp.get('e2big')
                ok = ee is not None and hsum == runs - ee - e2 and ee + e2 == insf
                line += '  counted %d = runs %d - EEXIST %s - E2BIG %s: %s' % (hsum, runs, ee, e2, 'ok' if ok else 'FAIL')
                line += '  per kernel runs %s' % [c[k * 8 + 4] for k in KS[v0]]
            else:
                line += '  counted %d lost %d (insert failed %d)' % (hsum, runs - hsum, insf)
                line += '  per kernel runs %s' % [c[k * 8 + 4] for k in KS[v0]]
            res.setdefault((wl, var), []).append((ts, 100 * (1 - ts / b[0]), per))
        print(line)
print('== across passes')
for k, v in sorted(res.items()):
    print('  %s %-5s t/s %s  overhead %s  us/call %s' % (k[0], k[1], ' '.join('%.2f' % x[0] for x in v),
          ' '.join('%.1f%%' % x[1] for x in v), ' '.join(('%.4f' % x[2]) if x[2] else '-' for x in v)))
