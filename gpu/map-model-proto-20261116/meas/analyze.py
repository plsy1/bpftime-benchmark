"""Analyze passes of run_meas.sh (proto/map-model).
usage: python3 -I analyze.py OUTDIR [OUTDIR ...]  -> prints a report.
Decision rules: see README.md (fixed before the real runs)."""
import json, os, re, statistics, struct, sys

# step0/e7-llama, one run each (pp512: -p 512 -n 0, tg128: -p 0 -n 128)
E7 = {'pp': {'rms': 11273216, 'mmq': 1892352, 'mmvq': 983040, 'ropek': 11534336},
      'tg': {'rms': 2949120, 'mmq': 0, 'mmvq': 3137339392, 'ropek': 2883584}}
KN = ['rms', 'mmq', 'mmvq', 'ropek']
RUNS = 5


def samples(run):
    txt = open(os.path.join(run, 'client.log'), errors='replace').read()
    m = re.search(r'"samples_ts":\s*\[([^\]]*)\]', txt)
    return [float(x) for x in m.group(1).split(',')] if m else []


def expected_ok(wl, k, got):
    per = E7[wl][k]
    if wl == 'pp':  # the warmup is a whole prompt
        return got == (RUNS + 1) * per, (RUNS + 1) * per
    # tg: the warmup generates one token
    lo, hi = RUNS * per, RUNS * per + per // 64
    return lo <= got <= hi, (lo, hi)


def words(path, n=None):
    b = open(path, 'rb').read()
    return struct.unpack('<%dQ' % (len(b) // 8), b)


def memory(run):
    """bytes per map: shared values (the CPU's copy, or today's map itself),
    plus for 1531/1532 the device copy (agent log) and the pinned staging
    buffer (the size of the CPU's copy)"""
    p = os.path.join(run, 'maps.json')
    if not os.path.exists(p):
        return {}
    mj = json.load(open(p))
    m = {'shm': 0, 'staging': 0}
    for mp in mj['maps']:
        if mp['type'] == 1530:
            continue
        b = mp['bytes_per_key'] * mp['max_entries']
        m['shm'] += b
        if mp['type'] in (1531, 1532):
            m['staging'] += b
    return m


def check(wl, var, run):
    """correctness of one item: list of (label, ok, detail)"""
    out = []
    in_call = var.endswith('+sync')
    var = var[4:] if var.startswith('dev-') else var
    var = var[:-5] if var.endswith('+sync') else var
    if var in ('base', 'agent'):
        return out
    if var.startswith('lat'):
        w = words(os.path.join(run, 'hist.bin'))
        for i, k in enumerate(KN):
            t = sum(w[i * 65:i * 65 + 64])
            ok, want = expected_ok(wl, k, t)
            out.append(('%s exits timed' % k, ok, '%d want %s' % (t, want)))
            out.append(('%s exits without start' % k, w[i * 65 + 64] == 0, '%d' % w[i * 65 + 64]))
    if var.startswith('hist'):
        w = words(os.path.join(run, 'calls.bin'))
        per_key = len(w) // 4
        for i, k in enumerate(KN):
            t = sum(w[i * per_key:(i + 1) * per_key])
            ok, want = expected_ok(wl, k, t)
            out.append(('%s calls' % k, ok, '%d want %s' % (t, want)))
    if var in ('lat', 'latmix', 'hist'):
        mname = 'hist' if var.startswith('lat') else 'calls'
        st = words(os.path.join(run, mname + '.status'))
        if len(st) >= 5:  # after the status change: covered syncs
            covered = st[3] == st[4] and st[3] > 0
            detail = 'deliveries %d exit %d syncs covered %d at exit %d' % (st[0], st[1], st[3], st[4])
            # judged for in-call deliveries only (README addendum 2)
            out.append(('last sync covered', covered if in_call else True,
                        detail + ('' if in_call else ' (reported: %s)' % ('yes' if covered else 'no'))))
        else:
            out.append(('final delivery', st[1] == 1, 'deliveries %d final %d' % (st[0], st[1])))
    return out


def delivery(run):
    txt = open(os.path.join(run, 'client.log'), errors='replace').read()
    d = {}
    m = re.search(r'(?:final|exit) delivery of (\d+) maps; (\d+) deliveries in all, (\d+) us of snapshots, (\d+) us of set-up', txt)
    if m:
        d.update(deliveries=int(m.group(2)), snapshot_us=int(m.group(3)), setup_us=int(m.group(4)))
    m = re.findall(r'map \((\d+) words, (\d+) shards\): (\d+) deliveries, (\d+) us in all, longest (\d+) us', txt)
    if m:
        d['maps'] = [dict(words=int(a), shards=int(b), n=int(c), us=int(e), max_us=int(f)) for a, b, c, e, f in m]
    m = re.findall(r'GPU device array map \((\d+)\): (\d+) bytes of device memory', txt)
    d['device_bytes'] = sum(int(b) for _, b in m)
    m = re.search(r'client_ms=(\d+)', open(os.path.join(run, 'meta.txt')).read())
    d['client_ms'] = int(m.group(1)) if m else None
    return d


def main():
    passes = {}
    for o in sys.argv[1:]:
        label = os.path.basename(o.rstrip('/'))
        items = {}
        for name in sorted(os.listdir(o)):
            if not name.startswith('run-'):
                continue
            wl, var = name[4:].split('-', 1)
            run = os.path.realpath(os.path.join(o, name))
            s = samples(run)
            items[(wl, var)] = dict(run=run, samples=s,
                                    med=statistics.median(s) if s else None,
                                    checks=check(wl, var, run) if s else [('ran', False, 'no samples')],
                                    delivery=delivery(run), memory=memory(run))
        passes[label] = items
    for label, items in passes.items():
        print('## pass', label)
        for wl in ('pp', 'tg'):
            base = items.get((wl, 'base'), {}).get('med')
            for var in ('base', 'agent', 'empty2', 'empty1', 'carryonly', 'lat', 'lat+sync', 'latmix', 'latold', 'hist', 'hist+sync', 'histold', 'dev-latold', 'dev-histold'):
                it = items.get((wl, var))
                if not it:
                    continue
                ov = '' if not base or not it['med'] else '%+.2f%%' % (100 * (1 - it['med'] / base))
                bad = [c for c in it['checks'] if not c[1]]
                d = it['delivery']
                mm = it['memory']
                print('%s %-11s median %8.2f t/s  overhead %8s  samples %s  checks %s  start %s ms  deliveries %s  memory: device %s, staging %s, shm %s B' % (
                    wl, var, it['med'] or 0, ov, ['%.1f' % x for x in it['samples']],
                    'ok' if not bad else 'FAIL ' + str(bad), d.get('client_ms'), d.get('deliveries'),
                    d.get('device_bytes'), mm.get('staging'), mm.get('shm')))
                for c in it['checks']:
                    print('      %-26s %s %s' % (c[0], 'ok' if c[1] else 'FAIL', c[2]))
                if d.get('maps'):
                    print('      delivery:', d['maps'], 'set-up us', d.get('setup_us'))
    # pass agreement
    if len(passes) >= 2:
        print('## overhead by pass (points)')
        labels = list(passes)
        for wl in ('pp', 'tg'):
            for var in ('agent', 'empty2', 'empty1', 'carryonly', 'lat', 'lat+sync', 'latmix', 'latold', 'hist', 'hist+sync', 'histold', 'dev-latold', 'dev-histold'):
                row = []
                for l in labels:
                    b = passes[l].get((wl, 'base'), {}).get('med')
                    m = passes[l].get((wl, var), {}).get('med')
                    row.append(100 * (1 - m / b) if b and m else None)
                if all(r is None for r in row):
                    continue
                spread = (max(r for r in row if r is not None) - min(r for r in row if r is not None)) if None not in row else None
                print('%s %-8s %s spread %s' % (wl, var, ' '.join('%7.2f' % r if r is not None else '   n/a' for r in row),
                                               '%.2f' % spread if spread is not None else 'n/a'))


main()
