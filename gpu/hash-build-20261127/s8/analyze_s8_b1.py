"""S8 part B1 (S8-RULES.md): the cost of 1533 on top of 1531 on llama.cpp
(tg128, mul_mat_vec_q, every thread), and the re-check of S0.2.
usage: python3 -I analyze_s8_b1.py OUTDIR [OUTDIR ...] (run_meas3.sh passes)"""
import json, os, re, statistics, struct, sys

C = 3137339392
WANT = 5 * C + C // 128
S02 = (0.0358, 0.0381)  # S0.2's routed 1533 form minus the 1531 form, ns per thread


def samples(run):
    txt = open(os.path.join(run, 'client.log'), errors='replace').read()
    m = re.search(r'"samples_ts":\s*\[([^\]]*)\]', txt)
    return [float(x) for x in m.group(1).split(',')] if m else []


def words(path):
    b = open(path, 'rb').read()
    return struct.unpack('<%dQ' % (len(b) // 8), b)


def checks(var, run):
    out = []
    cl = open(os.path.join(run, 'client.log'), errors='replace').read()
    sv = open(os.path.join(run, 'server.log'), errors='replace').read()
    if '@' in var:
        n = cl.count('PTX override')
        out.append(('override applied', n >= 1, n))
    tool = var.split('@')[0]
    maps = json.load(open(os.path.join(run, 'maps.json')))['maps']
    if tool == 'lat_mmvq' and not var.endswith('dh_route'):
        s = [m['sum'] for m in maps if m['type'] == 1531]
        w = words(os.path.join(run, 'hist.bin'))
        out.append(('1531 sum = WANT', s and s[0] == WANT, s))
        out.append(('bucket 194 = 0', w[194] == 0, w[194]))
    if tool == 'latd_mmvq':
        s = [m['sum'] for m in maps if m['type'] == 1533]
        cm = re.search(r'map hist_st \(companion\):(.*)', sv)
        comp = dict((a, int(b)) for a, b in re.findall(r'(\w+)=(\d+)', cm.group(1))) if cm else {}
        ee, e2 = comp.get('eexist'), comp.get('e2big')
        ok = bool(s) and ee is not None and s[0] + ee + e2 == WANT
        out.append(('1533 sum + EEXIST + E2BIG = WANT', ok, '%s + %s + %s' % (s, ee, e2)))
        kv = open(os.path.join(run, 'hist.kv'), 'rb').read()
        vals = dict((struct.unpack('<IIQ', kv[i:i + 16])[0], struct.unpack('<IIQ', kv[i:i + 16])[2]) for i in range(0, len(kv), 16))
        out.append(('bucket 194 absent or 0', vals.get(194, 0) == 0, vals.get(194)))
        inline = [int(x) for x in re.findall(r'Device hash maps in [^:]*: (\d+) lookups inline', cl)]
        cerr = cl.count('Unable to compile') + cl.count('requires call prototype')
        out.append(('routed', bool(inline) and max(inline) >= 1 and cerr == 0, '%s %d' % (inline, cerr)))
        out.append(('companion', True, 'EEXIST %s (unstable %s) E2BIG %s deliveries %s syncs %s/%s' % (
            ee, comp.get('unstable_eexist'), e2, comp.get('deliveries'), comp.get('syncs_covered'), comp.get('syncs_at_exit'))))
    return out


diffs = []
for d in sys.argv[1:]:
    print('==', d)
    it = {}
    for f in sorted(os.listdir(d)):
        if f.startswith('run-tg-'):
            var = f[len('run-tg-'):]
            run = os.path.realpath(os.path.join(d, f))
            s = samples(run)
            it[var] = (statistics.median(s) if s else None, run)
    b = it.get('base', (None,))[0]
    ns = {}
    for var, (ts, run) in sorted(it.items()):
        if ts is None:
            print('  %-24s no samples' % var)
            continue
        line = '  %-24s %6.2f t/s  %.4f s/run' % (var, ts, 128 / ts)
        if var != 'base' and b:
            ns[var] = (128 / ts - 128 / b) / C * 1e9
            ch = checks(var, run)
            bad = [c for c in ch if not c[1]]
            line += '  %+.4f ns/thread  %s' % (ns[var], 'ok' if not bad else 'INVALID ' + '; '.join('%s %s' % (c[0], c[2]) for c in bad))
            info = [c[2] for c in ch if c[0] == 'companion']
            if info:
                line += '  [' + info[0] + ']'
            if bad:
                ns.pop(var)
        print(line)
    if 'latd_mmvq' in ns and 'lat_mmvq' in ns:
        dd = ns['latd_mmvq'] - ns['lat_mmvq']
        diffs.append(dd)
        print('  latd - lat: %+.4f ns per thread (S0.2: %+.4f to %+.4f)%s' % (
            dd, S02[0], S02[1], '  ABOVE S0.2 + 0.02' if dd > S02[1] + 0.02 else ''))
    if 'lat_mmvq@s8_dh_route' in ns and 'lat_mmvq@s8_hd_t0' in ns:
        print('  S0.2 re-check, dh_route - hd_t0: %+.4f ns per thread' % (ns['lat_mmvq@s8_dh_route'] - ns['lat_mmvq@s8_hd_t0']))
    if 'latd_mmvq' in ns and 'lat_mmvq@s8_dh_route' in ns:
        print('  latd (real pipeline) - dh_route (hand-routed): %+.4f ns per thread' % (ns['latd_mmvq'] - ns['lat_mmvq@s8_dh_route']))
if diffs:
    print('== rule: latd - lat above %.4f in every pass: %s' % (S02[1] + 0.02, all(x > S02[1] + 0.02 for x in diffs)))
