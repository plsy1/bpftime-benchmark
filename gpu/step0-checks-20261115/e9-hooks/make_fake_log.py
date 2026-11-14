#!/usr/bin/env python3
"""Fabricate geometry logs (and, for some modes, a modified copy of the nsys
sqlite) for the analyze_hooks.py smoke test and its controls (no GPU).

usage: python3 -I make_fake_log.py NSYS.sqlite MODE OUT_PREFIX
Writes OUT_PREFIX.tsv; for the modes that change what nsys recorded also
OUT_PREFIX.sqlite (a copy, the source is opened read-only). Prints the
sqlite path the analysis must use on its last line ("sqlite: PATH").
MODE:
  complete          every direct launch as a cudaLaunchKernel line
                    (capturing 0), every graph node once as a capturing
                    line: expect YES
  graphs-off        as complete, but graph nodes logged as direct launches
                    (a hook run with graphs off): expect INCONCLUSIVE (V2,
                    V4) where the sqlite has graph launches, else YES
  unhooked-largest  the largest direct geometry of the kernel with the most
                    threads per launch is launched through an unhooked API
                    in nsys (its API rows renamed to cudaLaunchKernelExC in
                    the copy) and has no hook line: expect NO when that
                    kernel has no graph launch of that geometry
  capture-unhooked  the last stream capture recorded its launches through an
                    unhooked API (renamed in the copy) and its graph's nodes
                    have no capture line: expect NO with NO-direct-only
                    kernels (their largest graph geometry was seen only at
                    the eager direct launches)
  blank-names       as complete with every name '?' and a pointer per line:
                    expect INCONCLUSIVE (V0)
  unknown-capture   as complete, capture status 'e900' on the capturing
                    lines: expect INCONCLUSIVE (V0) where there are any
  control-pass      a cudagraph control log (direct launch + node add/set/
                    exec-set, vectorAdd 4096x256): passes the instrument check
  control-nonode    the same without graph-node lines: fails it
"""
import sqlite3
import sys

UNHOOKED_API = 'cudaLaunchKernelExC_v11060'


def thr(g):
    p = 1
    for x in g:
        p *= x
    return p


def copy_db(src, dst):
    s = sqlite3.connect('file:%s?mode=ro' % src, uri=True)
    d = sqlite3.connect(dst)
    s.backup(d)
    s.close()
    return d


def rename_api(db, corrs):
    nid = db.execute('select coalesce(max(id), 0) + 1 from StringIds').fetchone()[0]
    db.execute('insert into StringIds (id, value) values (?, ?)', (nid, UNHOOKED_API))
    db.executemany('update CUPTI_ACTIVITY_KIND_RUNTIME set nameId = ? where correlationId = ?',
                   [(nid, c) for c in corrs])
    db.commit()


def control_log(out, with_nodes):
    g = ['4096', '1', '1', '256', '1', '1']
    lines = [['1000', '1', '0', 'cudaLaunchKernel', '_Z9vectorAddPKfS0_Pf', 'aaaa0000'] + g + ['0', '0', '1', '-', '-'],
             ['1001', '1', '1', 'cuLaunchKernel', '_Z9vectorAddPKfS0_Pf', 'bbbb0000'] + g + ['0', '0', '1', '-', '-']]
    if with_nodes:
        for i, h in enumerate(('cuGraphAddKernelNode_v2', 'cuGraphKernelNodeSetParams_v2',
                               'cuGraphExecKernelNodeSetParams_v2')):
            lines.append([str(2000 + i), '1', '0', h, '_Z9vectorAddPKfS0_Pf', 'bbbb0000'] + g + ['0', '-', '0', 'cc', 'dd'])
    with open(out + '.tsv', 'w') as f:
        f.write('# bpftime-exp-geometry v1 pid=0 (fabricated control)\n')
        f.write('\n'.join('\t'.join(x) for x in lines) + '\n')
    print('%s.tsv: %d lines' % (out, len(lines)))


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    path, mode, out = sys.argv[1:]
    if mode in ('control-pass', 'control-nonode'):
        control_log(out, mode == 'control-pass')
        return 0
    sql_path = path
    c = sqlite3.connect('file:%s?mode=ro' % path, uri=True)
    rows = list(c.execute(
        'select s.value, gridX, gridY, gridZ, blockX, blockY, blockZ, dynamicSharedMemory, graphId, graphNodeId, '
        'correlationId, start from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id = k.mangledName order by start'))
    drop_direct = set()   # (name, geom) of direct launches without a hook line
    drop_graphs = set()   # graph ids whose nodes get no capture line
    if mode == 'unhooked-largest':
        direct = [r for r in rows if r[8] in (None, 0)]
        best = max(direct, key=lambda r: thr(r[1:7]))
        key = (best[0], best[1:7])
        drop_direct.add(key)
        corrs = [r[10] for r in direct if (r[0], r[1:7]) == key]
        sql_path = out + '.sqlite'
        db = copy_db(path, sql_path)
        rename_api(db, corrs)
        db.close()
        print('unhooked: %d direct launches of %s %s' % (len(corrs), key[0][:60], key[1]))
    elif mode == 'capture-unhooked':
        caps = list(c.execute(
            "select r.start, r.globalTid, s.value from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s on s.id = r.nameId "
            "where s.value like 'cudaStreamBeginCapture%' or s.value like 'cudaStreamEndCapture%' order by r.start"))
        begins = [x for x in caps if 'Begin' in x[2]]
        ends = [x for x in caps if 'End' in x[2]]
        if not begins:
            print('no stream capture in %s: same as complete' % path)
        else:
            b, e = begins[-1], [x for x in ends if x[0] > begins[-1][0]][0]
            corrs = [x[0] for x in c.execute(
                "select r.correlationId from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s on s.id = r.nameId "
                "where s.value like 'cudaLaunchKernel%' and r.globalTid = ? and r.start > ? and r.start < ?",
                (b[1], b[0], e[0]))]
            # graphs in order of their first kernel row correspond to the captures in order
            order = [g for (g,) in c.execute(
                'select graphId from CUPTI_ACTIVITY_KIND_KERNEL where graphId is not null and graphId != 0 '
                'group by graphId order by min(start)')]
            drop_graphs.add(order[len(begins) - 1])
            sql_path = out + '.sqlite'
            db = copy_db(path, sql_path)
            rename_api(db, corrs)
            db.close()
            print('capture-unhooked: %d launches of the last capture renamed; graph %s gets no capture lines' % (len(corrs), order[len(begins) - 1]))
    ptrs, lines, seen_nodes = {}, [], set()
    ns = 1000
    for i, (name, gx, gy, gz, bx, by, bz, shm, gid, nid, corr, st) in enumerate(rows):
        geom = (gx, gy, gz, bx, by, bz)
        if gid not in (None, 0):
            if (gid, nid) in seen_nodes:
                continue  # replays reach no hook
            seen_nodes.add((gid, nid))
            if gid in drop_graphs:
                continue
            capturing = '0' if mode == 'graphs-off' else '1'
            if mode == 'unknown-capture':
                capturing = 'e900'
        else:
            if (name, geom) in drop_direct:
                continue
            capturing = '0'
        ptr = ptrs.setdefault(name, '%x' % (0xaaaa00000000 + 0x100 * len(ptrs)))
        nm = name
        if mode == 'blank-names':
            nm, ptr = '?', '%x' % (0xbbbb00000000 + i)
        ns += 1000
        lines.append('\t'.join([str(ns), '1', '0', 'cudaLaunchKernel', nm, ptr] + [str(x) for x in geom] +
                               [str(shm), capturing, '1', '-', '-']))
    with open(out + '.tsv', 'w') as f:
        f.write('# bpftime-exp-geometry v1 pid=0 (fabricated from %s, mode %s)\n' % (path, mode))
        f.write('# ns\ttid\tdepth\thook\tname\tptr\tgx\tgy\tgz\tbx\tby\tbz\tshmem\tcapturing\tstream\tgraph\tnode\n')
        f.write('\n'.join(lines) + '\n')
    print('%s.tsv: %d lines (%s)' % (out, len(lines), mode))
    print('sqlite: %s' % sql_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
