import pickle, os, collections, re
d = pickle.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "runs.pkl"), "rb"))
runs, cells = d['runs'], d['cells']
bad = []
tm = []
for k, x in runs.items():
    r, c, s = k; cd = cells[c]
    why = []
    if x['env_set'] != s: why.append('set mismatch')
    if not x['env_cell_ok']: why.append('cell params in env')
    if not x['v_params_ok']: why.append('victim params')
    if x['n_state'] != 2: why.append('state lines %d' % x['n_state'])
    if any(cl != ('1728000', '1728000', '1020000000') for cl in x['clk']): why.append('clk %s' % x['clk'])
    if x['vrc'] != [0]: why.append('vrc %s' % x['vrc'])
    if s != 'P' and x['lrc'] != [0]: why.append('lrc %s' % x['lrc'])
    if cd['load'] != 'off' and x['ldrc'] != 0: why.append('ldrc %s' % x['ldrc'])
    if x['notes']: why.append('notes %s' % x['notes'])
    if s != 'P':
        exp = min(cd['launches'] + 1, 4096) if cd['threads'] == 1 else None
        if x['count'] != x['nvals']: why.append('count %d != nvals %d' % (x['count'], x['nvals']))
        if exp is not None and x['count'] != exp: why.append('count %d exp %d' % (x['count'], exp))
        if cd['threads'] > 1: x['_mlcount'] = x['count']
        if s not in ('M100',) and x['stats_lines'] != 1: why.append('stats lines %d' % x['stats_lines'])
        if s not in ('M100',) and x['st']:
            req = int(x['st']['req']); expreq = (cd['launches'] + 1) * cd['threads']
            if req != expreq: why.append('req %d exp %d' % (req, expreq))
            w = sum(int(x['st'][k]) for k in ('win0', 'win20', 'win80', 'win300'))
            if w != req: why.append('winsum %d req %d' % (w, req))
    if cd['load'] != 'off':
        s0, e0 = x['win_ns']
        pts = [p for p in x['loadpts'] if s0 <= p[0] <= e0]
        if len(pts) < 2: why.append('load pts %d' % len(pts))
    tm.append((x['tmax'], k))
    if why: bad.append((k, why))
print('runs', len(runs), 'bad', len(bad))
for b in bad[:40]: print(b)
tm.sort(); print('tmax', tm[-3:], 'min', tm[:2])
# multi-lane counts
print(collections.Counter((k[1], x['count']) for k, x in runs.items() if k[2] != 'P' and cells[k[1]]['threads'] > 1))
# stats policy/wmode fields
print(collections.Counter((k[2], x['st'].get('policy'), x['st'].get('v2'), x['st'].get('wmode'), x['st'].get('win_max'), x['st'].get('gap_lb')) for k, x in runs.items() if k[2] not in ('P', 'M100')))
print(collections.Counter(k[2] for k, x in runs.items() if k[2] == 'M100' and x.get('stats_lines')))
