#!/usr/bin/env python3
"""step0 e9-hooks (check 9): do the launch hooks see every geometry before
graph replays run it?

usage: python3 -I analyze_hooks.py --log GEOMETRY.tsv --sqlite NSYS.sqlite
                                  --out OUT_PREFIX [--label NAME]
                                  [--control CG_GEOMETRY.tsv ...]
       python3 -I analyze_hooks.py --controls-only --out OUT_PREFIX
                                  --control CG_GEOMETRY.tsv ...
         (checks the controls only: OUT_PREFIX.controls.json, exit 0 iff
          at least one control was given and all pass)

GEOMETRY.tsv: the instrumentation build's BPFTIME_EXP_GEOMETRY_LOG (one
line per launch-hook entry). NSYS.sqlite: nsys export of a separate run with
the same llama-bench arguments (CUPTI_ACTIVITY_KIND_KERNEL joined with
StringIds on mangledName; graph kernels traced node by node; each kernel row
linked to the runtime API call that launched it by correlationId).
--control: geometry logs of the cudagraph example runs (vec_add_graph and
vec_add_graph_legacy); each must show the direct launch and the three
graph-node hooks with the right name and geometry (instrument check).

Hook classes: direct (cudaLaunchKernel[_ptsz] / cuLaunchKernel, stream not
capturing), capture (same hooks, stream capturing), unknown (capture query
failed), node_add (cuGraphAddKernelNode*), node_set
(cuGraphKernelNodeSetParams*), exec_set (cuGraphExecKernelNodeSetParams*).
Lines with depth > 0 (a logged hook inside another: cudart's
cudaLaunchKernel calls the hooked cuLaunchKernel) count for geometry sets but
not for line counts.

nsys kernel rows are classed by the API that launched them: graph
(cudaGraphLaunch: replays), hooked (a launch API the agent hooks), unhooked
(any other API, e.g. cudaLaunchKernelExC: a coverage gap by construction).

Validity (README decision rules; any failure makes the verdict INCONCLUSIVE):
  V0 the log has lines, every name resolves, no capture-status query failed;
  V1 depth-0 runtime-launch lines = nsys hooked launch API calls;
  V2 depth-0 capturing lines = nsys hooked launch API calls that launched no
     kernel row (launches recorded into a graph);
  V3 depth-0 graph-node lines = nsys graph-node API calls, per class;
  V4 per kernel: depth-0 direct lines = nsys kernel rows launched directly
     by hooked APIs (capture + node lines >= the kernel's graph nodes is
     reported only: nodes recorded through an unhooked API fall short there
     by design, and V2 checks the capture total);
  V5 every (kernel, geometry) at a hook was also run in the nsys run;
  V6 the cudagraph instrument controls passed (when none is given: fails).
Verdict when valid: YES when, for every kernel nsys ran, its largest
geometry (threads per launch) was seen at some hook, and for every kernel
with graph launches the largest geometry among its graph launches was seen
at a capture or node hook (structurally before any replay). NO otherwise:
NO for kernels whose largest geometry never reached a hook, NO-direct-only
for graph kernels whose largest graph geometry was seen only at direct
launches. Also reported (not part of the verdict): the strict form (every
graph launch's own geometry seen before replay, every geometry seen),
kernel rows launched by unhooked APIs.
Outputs OUT_PREFIX.kernels.csv, OUT_PREFIX.summary.json,
OUT_PREFIX.summary.txt. Standard library only.
"""
import csv
import json
import sqlite3
import sys
from collections import defaultdict

RUNTIME_HOOKS = {'cudaLaunchKernel', 'cudaLaunchKernel_ptsz', 'cuLaunchKernel'}
PRE_REPLAY = {'capture', 'node_add', 'node_set', 'exec_set'}
NODE_CLASSES = ('node_add', 'node_set', 'exec_set')

# cudagraph example (example/gpu/cudagraph/vec_add_graph.cu): N = 1 << 20,
# 256 threads per block -> 4096 blocks; vectorAdd(const float*, const float*, float*)
CONTROL_NAME = '_Z9vectorAddPKfS0_Pf'
CONTROL_GEOM = (4096, 1, 1, 256, 1, 1)


def hook_class(hook, capturing):
    if hook in RUNTIME_HOOKS:
        if capturing == '1':
            return 'capture'
        if capturing == '0':
            return 'direct'
        return 'unknown'
    if hook.startswith('cuGraphAddKernelNode'):
        return 'node_add'
    if hook.startswith('cuGraphKernelNodeSetParams'):
        return 'node_set'
    if hook.startswith('cuGraphExecKernelNodeSetParams'):
        return 'exec_set'
    return 'other'


def read_log(path):
    rows, procs = [], 0
    for line in open(path, errors='replace'):
        if line.startswith('# bpftime-exp-geometry'):
            procs += 1
            continue
        if line.startswith('#') or not line.strip():
            continue
        f = line.rstrip('\n').split('\t')
        if len(f) < 16:
            continue
        rows.append({'ns': int(f[0]), 'tid': f[1], 'depth': int(f[2]), 'hook': f[3], 'name': f[4],
                     'ptr': f[5], 'geom': tuple(int(x) for x in f[6:12]), 'shmem': int(f[12]),
                     'capturing': f[13], 'stream': f[14], 'graph': f[15],
                     'node': f[16] if len(f) > 16 else '-'})
    # backfill names by pointer
    names = {}
    for r in rows:
        if r['name'] != '?':
            names.setdefault(r['ptr'], r['name'])
    unresolved = 0
    for r in rows:
        if r['name'] == '?' and r['ptr'] in names:
            r['name'] = names[r['ptr']]
            r['backfilled'] = True
        if r['name'] == '?':
            unresolved += 1
        r['cls'] = hook_class(r['hook'], r['capturing'])
    return rows, procs, unresolved


def threads(g):
    return g[0] * g[1] * g[2] * g[3] * g[4] * g[5]


def api_kind(name):
    """classify a CUPTI runtime API name"""
    if name is None:
        return 'none'
    if name.startswith('cudaGraphLaunch'):
        return 'graph_launch'
    if (name.startswith('cudaLaunchKernel') or name.startswith('cuLaunchKernel')) and 'Ex' not in name.split('_')[0]:
        return 'hooked_launch'
    for p, k in (('cudaGraphAddKernelNode', 'node_add'), ('cuGraphAddKernelNode', 'node_add'),
                 ('cudaGraphKernelNodeSetParams', 'node_set'), ('cuGraphKernelNodeSetParams', 'node_set'),
                 ('cudaGraphExecKernelNodeSetParams', 'exec_set'), ('cuGraphExecKernelNodeSetParams', 'exec_set')):
        if name.startswith(p):
            return k
    return 'other'


def read_nsys(path):
    c = sqlite3.connect('file:%s?mode=ro' % path, uri=True)
    api_by_corr = {}
    api_counts = defaultdict(int)
    api_rows = []
    for corr, name in c.execute('select r.correlationId, s.value from CUPTI_ACTIVITY_KIND_RUNTIME r '
                                'join StringIds s on s.id = r.nameId'):
        api_by_corr[corr] = name
        api_counts[name] += 1
        api_rows.append((corr, name))
    q = ('select s.value, gridX, gridY, gridZ, blockX, blockY, blockZ, graphId, graphNodeId, start, correlationId '
         'from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id = k.mangledName order by start')
    rows = []
    kernel_corrs = set()
    for name, gx, gy, gz, bx, by, bz, gid, nid, st, corr in c.execute(q):
        api = api_by_corr.get(corr)
        kernel_corrs.add(corr)
        if gid not in (None, 0):
            path_ = 'graph'
        elif api_kind(api) == 'hooked_launch':
            path_ = 'hooked'
        else:
            path_ = 'unhooked'
        rows.append({'name': name, 'geom': (gx, gy, gz, bx, by, bz), 'graph': gid, 'node': nid, 'start': st,
                     'api': api, 'path': path_})
    hooked_calls = sum(1 for corr, n in api_rows if api_kind(n) == 'hooked_launch')
    hooked_no_kernel = sum(1 for corr, n in api_rows if api_kind(n) == 'hooked_launch' and corr not in kernel_corrs)
    node_calls = defaultdict(int)
    for corr, n in api_rows:
        k = api_kind(n)
        if k in NODE_CLASSES:
            node_calls[k] += 1
    return rows, dict(api_counts), hooked_calls, hooked_no_kernel, dict(node_calls)


def check_control(path):
    """cudagraph example log: direct launch + node add/set/exec-set, right name and geometry"""
    res = {'log': path}
    try:
        rows, procs, unresolved = read_log(path)
    except OSError as e:
        res.update({'pass': False, 'why': 'cannot read: %s' % e})
        return res
    cls = defaultdict(int)
    bad = []
    for r in rows:
        if r['name'] != CONTROL_NAME or r['geom'] != CONTROL_GEOM:
            bad.append('%s %s %s' % (r['hook'], r['name'], 'x'.join(map(str, r['geom']))))
        if r['depth'] == 0:
            cls[r['cls']] += 1
    need = {'direct': 1, 'node_add': 1, 'node_set': 1, 'exec_set': 1}
    missing = [k for k, n in need.items() if cls[k] < n]
    res.update({'lines': len(rows), 'unresolved': unresolved, 'depth0_classes': dict(cls),
                'wrong_name_or_geometry': bad[:5], 'missing_classes': missing,
                'unknown_capture_status': cls['unknown']})
    res['pass'] = bool(rows) and not bad and not missing and unresolved == 0 and cls['unknown'] == 0
    return res


def main():
    a = sys.argv[1:]
    opts = {'control': []}
    while a:
        k = a.pop(0)
        if k in ('--log', '--sqlite', '--out', '--label') and a:
            opts[k[2:]] = a.pop(0)
        elif k == '--control' and a:
            opts['control'].append(a.pop(0))
        elif k == '--controls-only':
            opts['controls_only'] = True
        else:
            print(__doc__)
            return 2
    if opts.get('controls_only'):
        if 'out' not in opts:
            print(__doc__)
            return 2
        controls = [check_control(p) for p in opts['control']]
        ok = bool(controls) and all(c['pass'] for c in controls)
        json.dump({'pass': ok, 'controls': controls}, open(opts['out'] + '.controls.json', 'w'), indent=1)
        for c in controls:
            print('control %s: pass=%s depth0 classes=%s missing=%s wrong=%s' % (
                c['log'], c['pass'], json.dumps(c.get('depth0_classes')), c.get('missing_classes'),
                c.get('wrong_name_or_geometry')))
        print('CONTROLS %s' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1
    if not all(k in opts for k in ('log', 'sqlite', 'out')):
        print(__doc__)
        return 2
    label = opts.get('label', opts['log'])
    try:
        hrows, procs, unresolved = read_log(opts['log'])
    except OSError as e:
        hrows, procs, unresolved = [], 0, 0
        print('cannot read log: %s' % e)
    nrows, api, hooked_calls, hooked_no_kernel, node_calls = read_nsys(opts['sqlite'])
    controls = [check_control(p) for p in opts['control']]

    # hooks: per kernel, geometry -> classes; line counts at depth 0
    hk = defaultdict(lambda: defaultdict(set))
    hk_counts = defaultdict(lambda: defaultdict(int))
    for r in hrows:
        hk[r['name']][r['geom']].add(r['cls'])
        if r['depth'] == 0:
            hk_counts[r['name']][r['cls']] += 1
    # nsys: per kernel, geometry -> launches by path
    nk = defaultdict(lambda: defaultdict(lambda: {'hooked': 0, 'unhooked': 0, 'graph': 0}))
    nodes = defaultdict(set)
    unhooked_apis = defaultdict(int)
    for r in nrows:
        nk[r['name']][r['geom']][r['path']] += 1
        if r['path'] == 'graph':
            nodes[r['name']].add((r['graph'], r['node']))
        if r['path'] == 'unhooked':
            unhooked_apis[r['api'] or '(no API row)'] += 1

    # per graph: its nodes (kernel, geometry), replays, nodes seen before replay
    graphs = defaultdict(lambda: {'nodes': {}, 'launches': 0})
    for r in nrows:
        if r['path'] != 'graph':
            continue
        g = graphs[r['graph']]
        g['launches'] += 1
        g['nodes'].setdefault(r['node'], (r['name'], r['geom']))
    per_graph = []
    for gid, g in sorted(graphs.items(), key=lambda x: str(x[0])):
        seen_pre = sum(1 for (n, geo) in g['nodes'].values() if hk.get(n, {}).get(geo, set()) & PRE_REPLAY)
        seen_any = sum(1 for (n, geo) in g['nodes'].values() if hk.get(n, {}).get(geo))
        nn = len(g['nodes'])
        per_graph.append({'graph': gid, 'nodes': nn, 'kernel_executions': g['launches'],
                          'replays': g['launches'] / nn if nn else 0,
                          'nodes_seen_before_replay': seen_pre, 'nodes_seen_any_hook': seen_any,
                          'max_node_threads': max((threads(geo) for (_, geo) in g['nodes'].values()), default=0)})

    hook_names = set(n for n in hk if n != '?')
    nsys_names = set(nk)
    only_nsys = sorted(nsys_names - hook_names)
    only_hooks = sorted(hook_names - nsys_names)
    hook_geoms_not_in_nsys = sorted('%s %s' % (n, 'x'.join(map(str, g))) for n in hook_names & nsys_names
                                    for g in hk[n] if g not in nk[n])

    per = []
    tot = defaultdict(int)
    for name in sorted(nsys_names, key=lambda n: -sum(threads(g) * sum(v.values()) for g, v in nk[n].items())):
        geoms = nk[name]
        hgeoms = hk.get(name, {})
        launches = sum(sum(v.values()) for v in geoms.values())
        texec = sum(threads(g) * sum(v.values()) for g, v in geoms.items())
        glaunch = sum(v['graph'] for v in geoms.values())
        hooked_direct = sum(v['hooked'] for v in geoms.values())
        unhooked = sum(v['unhooked'] for v in geoms.values())
        maxt = max(threads(g) for g in geoms)
        largest = [g for g in geoms if threads(g) == maxt]
        seen_largest_cls = set()
        for g in largest:
            seen_largest_cls |= hgeoms.get(g, set())
        gmaxt = max((threads(g) for g, v in geoms.items() if v['graph']), default=0)
        glargest = [g for g, v in geoms.items() if v['graph'] and threads(g) == gmaxt]
        seen_glargest_cls = set()
        for g in glargest:
            seen_glargest_cls |= hgeoms.get(g, set())
        hmax_any = max((threads(g) for g in hgeoms), default=0)
        hmax_pre = max((threads(g) for g, cl in hgeoms.items() if cl & PRE_REPLAY), default=0)
        unseen_l = unseen_t = 0
        g_pre = g_direct_only = g_none = 0
        g_pre_t = g_direct_only_t = g_none_t = 0
        for g, v in geoms.items():
            cl = hgeoms.get(g, set())
            n = sum(v.values())
            if not cl:
                unseen_l += n
                unseen_t += n * threads(g)
            if v['graph']:
                if cl & PRE_REPLAY:
                    g_pre += v['graph']; g_pre_t += v['graph'] * threads(g)
                elif cl:
                    g_direct_only += v['graph']; g_direct_only_t += v['graph'] * threads(g)
                else:
                    g_none += v['graph']; g_none_t += v['graph'] * threads(g)
        largest_seen = bool(seen_largest_cls)
        if not largest_seen:
            kv = 'NO'
        elif glaunch and not (seen_glargest_cls & PRE_REPLAY):
            kv = 'NO-direct-only' if seen_glargest_cls else 'NO'
        else:
            kv = 'YES'
        d = {
            'kernel': name, 'nsys_launches': launches, 'nsys_graph_launches': glaunch,
            'nsys_direct_hooked': hooked_direct, 'nsys_direct_unhooked_api': unhooked,
            'nsys_thread_execs': texec, 'nsys_geometries': len(geoms), 'hook_geometries': len(hgeoms),
            'largest_threads': maxt, 'largest_geometries': ';'.join('x'.join(map(str, g)) for g in largest),
            'largest_seen_by': ','.join(sorted(seen_largest_cls)) or '-',
            'largest_graph_threads': gmaxt, 'largest_graph_seen_by': ','.join(sorted(seen_glargest_cls)) or '-',
            'hook_max_threads_any': hmax_any, 'hook_max_threads_pre_replay': hmax_pre,
            'unseen_launches': unseen_l, 'unseen_thread_execs': unseen_t,
            'graph_launches_pre_replay': g_pre, 'graph_launches_direct_only': g_direct_only,
            'graph_launches_unseen': g_none, 'graph_thread_execs_unseen': g_none_t,
            'graph_thread_execs_direct_only': g_direct_only_t,
            'hook_lines_direct': hk_counts[name]['direct'], 'hook_lines_capture': hk_counts[name]['capture'],
            'hook_lines_node': sum(hk_counts[name][c] for c in NODE_CLASSES),
            'hook_lines_unknown': hk_counts[name]['unknown'],
            'nsys_graph_nodes': len(nodes[name]),
            'verdict': kv,
        }
        # validity (V4): direct lines = direct kernel rows launched by hooked APIs. The capture side is
        # checked in total (V2): nodes recorded through an unhooked API would be short here by design.
        d['direct_count_match'] = d['hook_lines_direct'] == d['nsys_direct_hooked']
        d['count_match'] = (d['direct_count_match'] and
                            d['hook_lines_capture'] + d['hook_lines_node'] >= d['nsys_graph_nodes'])
        d['strict_all_seen'] = (unseen_l == 0 and g_direct_only == 0 and g_none == 0)
        per.append(d)
        for k in ('nsys_launches', 'nsys_thread_execs', 'unseen_launches', 'unseen_thread_execs',
                  'nsys_graph_launches', 'nsys_direct_unhooked_api', 'graph_launches_unseen',
                  'graph_launches_direct_only', 'graph_thread_execs_unseen', 'graph_thread_execs_direct_only'):
            tot[k] += d[k]

    cls_lines = defaultdict(int)
    d0 = defaultdict(int)
    for r in hrows:
        cls_lines['%s/%s/depth%d' % (r['hook'], r['cls'], r['depth'])] += 1
        if r['depth'] == 0:
            d0[r['cls']] += 1
    d0_runtime = sum(1 for r in hrows if r['depth'] == 0 and r['hook'] in RUNTIME_HOOKS)

    # validity (README rules V0-V6)
    validity = {
        'V0_log_nonempty': bool(hrows),
        'V0_names_resolved': unresolved == 0,
        'V0_capture_status_known': d0['unknown'] == 0 and not any(r['cls'] == 'unknown' for r in hrows),
        'V1_runtime_lines_eq_nsys_hooked_calls': d0_runtime == hooked_calls,
        'V2_capture_lines_eq_nsys_recorded_calls': d0['capture'] == hooked_no_kernel,
        'V3_node_lines_eq_nsys_node_calls': all(d0[c] == node_calls.get(c, 0) for c in NODE_CLASSES),
        'V4_per_kernel_direct_counts': all(d['direct_count_match'] for d in per) and not only_hooks,
        'V5_hook_geometries_in_nsys': not hook_geoms_not_in_nsys and not only_hooks,
        'V6_instrument_controls': bool(controls) and all(c['pass'] for c in controls),
    }
    detail = {
        'depth0_runtime_lines': d0_runtime, 'nsys_hooked_launch_calls': hooked_calls,
        'depth0_capture_lines': d0['capture'], 'nsys_hooked_calls_without_kernel_row': hooked_no_kernel,
        'depth0_node_lines': {c: d0[c] for c in NODE_CLASSES}, 'nsys_node_calls': node_calls,
        'kernels_direct_count_mismatch': [d['kernel'] for d in per if not d['direct_count_match']],
        'kernels_capture_lines_below_graph_nodes (reported)': [d['kernel'] for d in per if d['direct_count_match'] and not d['count_match']],
        'hook_geometries_not_in_nsys': hook_geoms_not_in_nsys[:20],
    }
    summary = {
        'label': label, 'log': opts['log'], 'sqlite': opts['sqlite'],
        'log_lines': len(hrows), 'log_processes': procs, 'unresolved_lines': unresolved,
        'hook_line_classes': dict(sorted(cls_lines.items())),
        'nsys_runtime_api': {k: v for k, v in api.items() if 'Launch' in k or 'Graph' in k or 'Capture' in k},
        'kernels_only_in_nsys': only_nsys, 'kernels_only_at_hooks': only_hooks,
        'nsys_kernel_rows_by_unhooked_api': dict(unhooked_apis),
        'validity': validity, 'validity_detail': detail, 'controls': controls,
        'totals': dict(tot),
    }
    # global capacity view (one counter map shared by every probed kernel)
    gmax = max((d['largest_threads'] for d in per), default=0)
    hmax_pre_all = max((threads(r['geom']) for r in hrows if r['cls'] in PRE_REPLAY), default=0)
    hmax_any_all = max((threads(r['geom']) for r in hrows), default=0)
    summary['global_largest_threads_nsys'] = gmax
    summary['global_largest_threads_hooks_any'] = hmax_any_all
    summary['global_largest_threads_hooks_pre_replay'] = hmax_pre_all

    failed = [k for k, ok in validity.items() if not ok]
    if not hrows:
        verdict = 'INCONCLUSIVE: the hook log is empty or missing'
    elif failed:
        verdict = 'INCONCLUSIVE: validity check(s) failed: %s (the runs differ, the log lost lines, or the instrument is wrong; see validity_detail)' % ', '.join(failed)
    else:
        no = [d['kernel'] for d in per if d['verdict'] == 'NO']
        nod = [d['kernel'] for d in per if d['verdict'] == 'NO-direct-only']
        if no or nod:
            verdict = 'NO: largest geometry never at a hook for %d kernel(s); graph kernels whose largest graph geometry was seen only at direct launches: %d' % (len(no), len(nod))
            if summary['nsys_kernel_rows_by_unhooked_api']:
                verdict += '; nsys kernel rows launched by unhooked APIs: %s' % json.dumps(summary['nsys_kernel_rows_by_unhooked_api'])
        else:
            verdict = 'YES: every kernel\'s largest geometry was seen at a hook (graph kernels: their largest graph geometry at capture or a node hook)'
    summary['verdict'] = verdict
    summary['strict_every_geometry_seen_before_replay'] = all(d['strict_all_seen'] for d in per) if per else False
    summary['kernel_sets_match'] = not only_nsys and not only_hooks
    summary['hook_lines_depth0_launch_hooks'] = d0_runtime
    summary['all_geometries_seen'] = all(d['unseen_launches'] == 0 for d in per) if per else False
    summary['graphs'] = per_graph
    summary['kernels'] = per

    with open(opts['out'] + '.kernels.csv', 'w', newline='') as f:
        if per:
            w = csv.DictWriter(f, fieldnames=list(per[0].keys()))
            w.writeheader()
            w.writerows(per)
    json.dump(summary, open(opts['out'] + '.summary.json', 'w'), indent=1)
    lines = ['label: %s' % label, 'verdict: %s' % verdict,
             'validity: %s' % ' '.join('%s=%s' % (k, 'ok' if ok else 'FAIL') for k, ok in validity.items()),
             'validity detail: %s' % json.dumps(detail),
             'instrument controls: %s' % ('; '.join('%s pass=%s classes=%s missing=%s wrong=%s' % (
                 c['log'], c['pass'], json.dumps(c.get('depth0_classes')), c.get('missing_classes'),
                 c.get('wrong_name_or_geometry')) for c in controls) or 'none given'),
             'log lines %d (processes %d, unresolved %d); nsys kernel rows %d' % (len(hrows), procs, unresolved, len(nrows)),
             'hook line classes: %s' % json.dumps(summary['hook_line_classes']),
             'nsys runtime API: %s' % json.dumps(summary['nsys_runtime_api']),
             'nsys kernel rows launched by unhooked APIs: %s' % json.dumps(summary['nsys_kernel_rows_by_unhooked_api']),
             'kernels only in nsys: %d, only at hooks: %d (sets match: %s)' % (len(only_nsys), len(only_hooks), summary['kernel_sets_match']),
             'launch-hook lines at depth 0: %d' % d0_runtime,
             'all geometries seen at some hook: %s; strict (every graph launch\'s own geometry before replay): %s' % (
                 summary['all_geometries_seen'], summary['strict_every_geometry_seen_before_replay']),
             'largest threads per launch: nsys %d, hooks %d, hooks before replay %d' % (gmax, hmax_any_all, hmax_pre_all),
             'totals: %s' % json.dumps(dict(tot)),
             'graphs (nsys): ' + ('; '.join('graph %s: %d nodes, %.0f replays, %d nodes seen at capture/node hooks, %d at any hook, max %d threads'
                                           % (g['graph'], g['nodes'], g['replays'], g['nodes_seen_before_replay'], g['nodes_seen_any_hook'], g['max_node_threads'])
                                           for g in per_graph) or 'none'), '',
             '%-6s %-9s %-6s %-6s %-9s %-14s %-9s %-14s %-9s %-9s %-7s %s' % ('k_verd', 'launches', 'graph', 'geoms', 'hookgeom', 'largest(thr)', 'seen_by', 'graph_lg(thr)', 'g_seen_by', 'unseen_l', 'counts', 'kernel')]
    for d in per:
        lines.append('%-6s %-9d %-6d %-6d %-9d %-14d %-9s %-14d %-9s %-9d %-7s %s' % (
            d['verdict'][:6], d['nsys_launches'], d['nsys_graph_launches'], d['nsys_geometries'], d['hook_geometries'],
            d['largest_threads'], d['largest_seen_by'][:9], d['largest_graph_threads'], d['largest_graph_seen_by'][:9],
            d['unseen_launches'], 'ok' if d['count_match'] else 'DIFF', d['kernel'][:90]))
    open(opts['out'] + '.summary.txt', 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    return 0


if __name__ == '__main__':
    sys.exit(main())
