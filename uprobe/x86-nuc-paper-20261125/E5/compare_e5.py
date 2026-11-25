#!/usr/bin/env python3
"""E5 comparer: join the target's truth file with the kernel and bpftime map dumps by seq.

For each (scenario, point, side, field) it counts:
  match      side value == truth (T1 == T2)
  mismatch   side value != truth (T1 == T2)
  ambiguous  T1 != T2 for this field (record present)
  missing    no record for this seq in the side's dump
Fields: tgid, tid (pid_tgid), uid, gid (uid_gid), comm (bytes up to the first NUL).
Also per (scenario, side): dumps' extra seqs (not in truth), records hit more than once, and the
dump's stats line. upper95 = 3/n when mismatch == 0 (rule of three), n = match + mismatch.

Rows are emitted both per point (point = 0/1/2) and per scenario over all points (point='all').
Usage: compare_e5.py --truth T [--truth T2 ...] --kernel K.dump [--user U.dump] [--leg NAME]
Prints one JSON object per row.
"""
import argparse, json, sys
from collections import defaultdict

FIELDS = ('tgid', 'tid', 'uid', 'gid', 'comm')


def comm(hexs):
    b = bytes.fromhex(hexs)
    return b.split(b'\0', 1)[0].decode('latin-1')


def read_truth(paths):
    recs, meta = {}, []
    for line in (l for p in paths for l in open(p)):
        if line.startswith('#'):
            meta.append(line[2:].strip())
            continue
        f = line.split()
        if len(f) != 13:
            continue
        scen, seq, point = f[0], int(f[1]), int(f[2])
        t1 = dict(tgid=int(f[3]), tid=int(f[4]), uid=int(f[5]), gid=int(f[6]), comm=comm(f[7]))
        t2 = dict(tgid=int(f[8]), tid=int(f[9]), uid=int(f[10]), gid=int(f[11]), comm=comm(f[12]))
        if seq in recs:
            meta.append(f'error duplicate truth seq {seq}')
        recs[seq] = (scen, point, t1, t2)
    return recs, meta


def read_dump(path):
    recs, stats = {}, None
    for line in open(path):
        if line.startswith('# stats'):
            p = line.split()
            stats = dict(calls=int(p[3]), dup=int(p[5]), fail=int(p[7]))
            continue
        f = line.split()
        if len(f) != 5:
            continue
        seq, pt, ug, hits = int(f[0]), int(f[1]), int(f[2]), int(f[3])
        recs[seq] = dict(tgid=pt >> 32, tid=pt & 0xffffffff, uid=ug & 0xffffffff, gid=ug >> 32,
                         comm=comm(f[4]), hits=hits)
    return recs, stats


def scen_of_seq(seq, names):
    return names.get(seq >> 40, f'S{seq >> 40}')


def compare(truth, sides, leg):
    rows = []
    names = {seq >> 40: scen for seq, (scen, _, _, _) in truth.items()}
    for side, (dump, stats) in sides.items():
        cnt = defaultdict(lambda: dict(match=0, mismatch=0, ambiguous=0, missing=0, examples=[]))
        for seq, (scen, point, t1, t2) in truth.items():
            got = dump.get(seq)
            for fld in FIELDS:
                for pk in (point, 'all'):
                    c = cnt[(scen, pk, fld)]
                    if got is None:
                        c['missing'] += 1
                    elif t1[fld] != t2[fld]:
                        c['ambiguous'] += 1
                    elif got[fld] == t1[fld]:
                        c['match'] += 1
                    else:
                        c['mismatch'] += 1
                        if len(c['examples']) < 3:
                            c['examples'].append(dict(seq=seq, point=point, truth=t1[fld],
                                                      got=got[fld]))
        for (scen, point, fld), c in sorted(cnt.items(), key=lambda kv: (kv[0][0], str(kv[0][1]), FIELDS.index(kv[0][2]))):
            n = c['match'] + c['mismatch']
            row = dict(kind='field', leg=leg, side=side, scenario=scen, point=point, field=fld, n=n, **c)
            row['upper95'] = round(3 / n, 4) if n and c['mismatch'] == 0 else None
            if not row['examples']:
                del row['examples']
            rows.append(row)
        per_scen = defaultdict(lambda: dict(records=0, extra=0, multi_hit=0))
        for seq, scen_rec in truth.items():
            per_scen[scen_rec[0]]['records'] += 1
            r = dump.get(seq)
            if r and r['hits'] > 1:
                per_scen[scen_rec[0]]['multi_hit'] += 1
        for seq in dump:
            if seq not in truth:
                per_scen[scen_of_seq(seq, names)]['extra'] += 1
        for scen, s in sorted(per_scen.items()):
            rows.append(dict(kind='scenario', leg=leg, side=side, scenario=scen, dump_stats=stats, **s))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--truth', required=True, action='append')
    ap.add_argument('--kernel', required=True)
    ap.add_argument('--user')
    ap.add_argument('--leg', default='?')
    a = ap.parse_args()
    truth, meta = read_truth(a.truth)
    sides = {'K': read_dump(a.kernel)}
    if a.user:
        sides['U'] = read_dump(a.user)
    for row in compare(truth, sides, a.leg):
        print(json.dumps(row))
    for m in meta:
        print(json.dumps(dict(kind='target_meta', leg=a.leg, text=m,
                              error=m.startswith(('error', 'skipped')))))


if __name__ == '__main__':
    main()
