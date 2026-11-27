"""S0.2 timing analysis (S0-RULES.md, S0.2 timing and validity).
usage: python3 -I analyze_s02.py OUTDIR [OUTDIR ...]   (run_meas3.sh pass dirs)

Per pass: tg128 tokens/s (median of llama-bench's samples), the probe's
cost per thread over base, and the routed form against today's 1531 form
(dh_route - hd_t0). Validity checks as written in S0-RULES.md."""
import json, os, re, statistics, sys
sys.path.insert(0, '/home/jetson/src/results/gpu/hash-round1-20261126/c2')
import analyze as A  # samples(), words()

C = 3137339392            # probe calls per tg128 run (C2)
WANT = 5 * C + C // 128   # five runs and llama-bench's warm-up


def sum1531(run):
    for mm in json.load(open(os.path.join(run, 'maps.json')))['maps']:
        if mm['type'] == 1531:
            return mm['sum']
    return None


def checks(var, run):
    out = []
    if '@' in var:
        n = len(re.findall('PTX override', open(os.path.join(run, 'client.log'), errors='replace').read()))
        out.append(('override applied', n >= 1, str(n)))
        form = var.split('@')[1]
        s = sum1531(run)
        want = {'hd_t0': WANT, 'dh_check': WANT, 'dh_route': 0}[form]
        out.append(('1531 sum', s == want, '%s want %d' % (s, want)))
        w = A.words(os.path.join(run, 'hist.bin'))
        out.append(('bucket 194', w[194] == 0, str(w[194])))
    return out


res = {}
for d in sys.argv[1:]:
    items = {}
    for f in sorted(os.listdir(d)):
        if not f.startswith('run-tg-'):
            continue
        var = f[len('run-tg-'):]
        run = os.path.realpath(os.path.join(d, f))
        s = A.samples(run)
        items[var] = (statistics.median(s) if s else None, run)
    print('==', d)
    b = items['base'][0]
    ok_all = True
    q = {}
    for var, (ts, run) in sorted(items.items()):
        chk = checks(var, run)
        bad = [c for c in chk if not c[1]]
        ok_all &= not bad
        ns = (128 / ts - 128 / b) / C * 1e9
        q[var] = ns
        print('  %-22s %7.2f t/s  %.3f s/run  overhead %5.1f%%  %+.4f ns/thread  %s' % (
            var, ts, 128 / ts, 100 * (1 - ts / b), ns,
            'ok' if not bad else 'INVALID ' + '; '.join('%s %s' % (c[0], c[2]) for c in bad)))
    r = q['tvf-lat_mmvq@dh_route'] - q['tvf-lat_mmvq@hd_t0']
    print('  routed 1533 form minus 1531 form: %+.4f ns/thread%s' % (r, '' if ok_all else '  (pass INVALID)'))
    res[d] = (r, ok_all)
if len(res) > 1:
    v = [r for r, ok in res.values() if ok]
    print('== across valid passes: %s  spread %.4f' % (' '.join('%+.4f' % x for x in v), (max(v) - min(v)) if v else float('nan')))
