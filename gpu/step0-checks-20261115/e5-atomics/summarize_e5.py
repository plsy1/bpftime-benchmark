#!/usr/bin/env python3
"""summarize_e5.py RESULTS_DIR

Combine one run_all.sh result directory (fadd/plain/cas JSON from e5_loader,
ptx/<mode>.atoms.json, ptx/<mode>.sass) with results/compile-only/matrix.csv
and apply the decision rules of README.md. Writes RESULTS_DIR/summary.json
and prints a short text summary. Standard library only.
"""
import csv
import json
import os
import re
import sys

D = os.path.dirname(os.path.abspath(__file__))


def load(p):
    try:
        return json.load(open(p))
    except Exception:
        return None


def sass_atoms(p):
    out = []
    if not os.path.exists(p):
        return out
    for l in open(p, errors='replace'):
        m = re.search(r'/\*([0-9a-f]{4,})\*/\s+((?:@!?P\d\s+)?(?:ATOM|RED|MEMBAR|FENCE)[^;]*);', l)
        if m:
            out.append(m.group(2).strip())
    return out


def norm(s):
    return re.sub(r'\bR\d+\b', 'R', re.sub(r'P\d\b', 'P', s))


def atom_result_used(func_ptx, op):
    """For each atom.<op> in the probe's PTX: is its destination register read afterwards?
    (PTX registers are virtual and assigned once per definition in this output, so a later
    textual use is a read.) None when the PTX is missing."""
    if not func_ptx or not os.path.exists(func_ptx):
        return None
    text = open(func_ptx).read()
    res = []
    for m in re.finditer(r'atom(?:\.[a-z0-9_:]+)*\.' + op + r'\.[usb]64\s+(%rd\d+),', text):
        reg = m.group(1)
        rest = text[m.end():]
        res.append(bool(re.search(re.escape(reg) + r'(?![0-9])', rest)))
    return res


def cas_result_used(func_ptx):
    return atom_result_used(func_ptx, 'cas')


def run_meta(R, m):
    """client_rc / server_rc that run.sh recorded (copied by run_all.sh), and the victim's line."""
    d = {}
    p = os.path.join(R, m + '.runmeta.txt')
    if os.path.exists(p):
        t = open(p).read()
        for k in ('client_rc', 'server_rc'):
            x = re.search(k + r'=(\d+)', t)
            d[k] = int(x.group(1)) if x else None
    d['victim'] = load(os.path.join(R, m + '.victim.json'))
    return d


def run_valid(j, meta):
    """A run decides anything only if the probe fired on every thread of every launch and
    nothing was cut short: the loader was stopped by run.sh's SIGINT (not by its own
    timeout: server_rc 0) after the victim finished cleanly (client_rc 0, its own output
    check 0 mismatches), and every bpf() read of the maps worked. Returns (ok, reason)."""
    if not j:
        return False, 'no loader result'
    why = []
    if j.get('calls') != j.get('expected_calls'):
        why.append('calls %s != expected %s' % (j.get('calls'), j.get('expected_calls')))
    if j.get('timed_out'):
        why.append('loader timed out waiting for SIGINT')
    if j.get('lookup_failures'):
        why.append('%d bpf() map reads failed' % j['lookup_failures'])
    if meta.get('client_rc') is None or meta.get('server_rc') is None:
        why.append('no run.sh meta (client_rc/server_rc)')
    else:
        if meta['client_rc'] != 0:
            why.append('victim rc %d' % meta['client_rc'])
        if meta['server_rc'] != 0:
            why.append('loader rc %d (124 = stopped by its own timeout)' % meta['server_rc'])
    v = meta.get('victim')
    if not v or v.get('output_mismatches') != 0:
        why.append('victim line missing or its output check failed')
    return (not why), '; '.join(why)


def main():
    R = os.path.abspath(sys.argv[1])
    s = {'results_dir': R, 'runs': {}, 'ptx': {}, 'sass': {}}
    s['validity'] = {}
    for m in ('fadd', 'plain', 'cas'):
        j = load(os.path.join(R, m + '.json'))
        if j:
            j['duplicate_indices'] = j.get('lost_writes')
        s['runs'][m] = j
        ok, why = run_valid(j, run_meta(R, m))
        s['validity'][m] = {'valid': ok, 'why_not': why}
        a = load(os.path.join(R, 'ptx', m + '.atoms.json'))
        if a:
            s['ptx'][m] = {
                'atoms': [{k: x[k] for k in ('line', 'op', 'state_space', 'sem', 'scope', 'type')} for x in a.get('atoms', [])],
                'fences_in_probe': a.get('fences'),
                'cas_context': a.get('cas_context'),
                'cas_result_used': cas_result_used(os.path.join(R, 'ptx', m + '.func.ptx')),
                'add_result_used': atom_result_used(os.path.join(R, 'ptx', m + '.func.ptx'), 'add'),
            }
        ks = os.path.join(R, 'ptx', m + '.kernel.sass')  # the kernel's section (older dirs: the whole module)
        s['sass'][m] = sass_atoms(ks if os.path.exists(ks) else os.path.join(R, 'ptx', m + '.sass'))
    # SASS atomics that only one mode has = the probe's own (the trampoline is common)
    sets = {m: set(norm(x) for x in v) for m, v in s['sass'].items() if v}
    if sets:
        common = set.intersection(*sets.values())
        s['sass_probe_specific'] = {m: sorted(v - common) for m, v in sets.items()}
    # compile-only matrix
    mpath = os.path.join(D, 'results', 'compile-only', 'matrix.csv')
    if os.path.exists(mpath):
        rows = list(csv.DictReader(open(mpath)))
        s['compile'] = {
            '%s/%s/%s' % (r['compiler'], r['mcpu'], r['case']): ('ok: ' + r['atomics']) if r['rc'] == '0' else 'FAIL'
            for r in rows}
    # decisions (README "Decision rules")
    f, p, c = s['runs'].get('fadd'), s['runs'].get('plain'), s['runs'].get('cas')
    val = {m: s['validity'][m]['valid'] for m in ('fadd', 'plain', 'cas')}
    dec = {}
    comp = s.get('compile', {})
    # rule 1 (compile-only; inconclusive when the matrix is missing)
    for w in ('64', '32'):
        k = 'clang-15/default/fadd%s_used' % w
        dec['fadd%s_used_compiles_at_default_mcpu' % w] = (
            comp[k].startswith('ok: add%s+fetch' % w) if k in comp else None)
    # rule 2: the plain control must lose, in a valid run
    control_ok = val['plain'] and p.get('lost_writes', 0) > 0
    dec['control_plain_loses'] = control_ok if val['plain'] else None
    # rule 3
    if not val['fadd']:
        dec['fadd_run'] = 'inconclusive: fadd run invalid (%s)' % s['validity']['fadd']['why_not']
    elif not control_ok:
        dec['fadd_run'] = 'inconclusive: the plain control %s' % (
            'did not lose writes (no contention shown)' if val['plain'] else
            'run is invalid (%s)' % s['validity']['plain']['why_not'])
    elif f.get('pass'):
        dec['fadd_run'] = 'yes: counter == calls, 0 gaps, 0 duplicate indices'
    else:
        dec['fadd_run'] = 'no: fetch-add lost or duplicated indices (see fadd.json)'
    # rule 4 (duplicates in a valid cas run decide; no duplicates proves nothing)
    if not val['cas']:
        dec['cas_run'] = 'inconclusive: cas run invalid (%s)' % s['validity']['cas']['why_not']
    elif c.get('lost_writes', 0) > 0:
        dec['cas_run'] = 'unusable: %d duplicate indices of %d calls (counter %d)' % (
            c['lost_writes'], c['calls'], c['counter'])
    elif c.get('pass'):
        dec['cas_run'] = 'no duplicates seen (the bug did not show in this run; the PTX decides)'
    else:
        dec['cas_run'] = 'fail without duplicates (see cas.json)'
    # rule 5
    dec['use_atomics_rule'] = (
        'workable for 64-bit fetch-add at the default -mcpu'
        if dec['fadd64_used_compiles_at_default_mcpu'] and dec['fadd_run'].startswith('yes')
        else 'not shown')
    # rule 6 and the fetch-add counterpart, from the kept PTX and SASS
    xc, xf = s['ptx'].get('cas'), s['ptx'].get('fadd')
    spec = s.get('sass_probe_specific', {})
    if not xc or xc.get('cas_result_used') is None or not xc['cas_result_used']:
        dec['cmpxchg_bug_ptx'] = 'inconclusive: no kept PTX with an atom.cas for the cas program'
    elif not any(xc['cas_result_used']):
        dec['cmpxchg_bug_ptx'] = 'confirmed: no atom.cas destination register is read afterwards'
    else:
        dec['cmpxchg_bug_ptx'] = 'not confirmed: an atom.cas result is read'
    cas_sass = [a for a in spec.get('cas', []) if 'CAS.64' in a]
    dec['cmpxchg_sass_destination_RZ'] = (all(', RZ,' in a for a in cas_sass) if cas_sass else None)
    if not xf or not xf.get('add_result_used'):
        dec['fadd_result_used_ptx'] = None
    else:
        dec['fadd_result_used_ptx'] = any(xf['add_result_used'])
    fadd_sass = [a for a in spec.get('fadd', []) if 'ATOM' in a and 'ADD.64' in a]
    dec['fadd_sass_destination_not_RZ'] = (any(', RZ,' not in a for a in fadd_sass) if fadd_sass else None)
    s['decisions'] = dec
    json.dump(s, open(os.path.join(R, 'summary.json'), 'w'), indent=1)
    print('e5 summary (%s)' % R)
    for m in ('fadd', 'plain', 'cas'):
        j = s['runs'].get(m)
        if j:
            print('  %-5s calls=%d counter=%d gaps=%d duplicate_indices=%d cas_retries=%d cas_caphits=%d pass=%d valid=%s%s' % (
                m, j['calls'], j['counter'], j['gaps'], j['lost_writes'], j['cas_retries'], j['cas_caphits'], j['pass'],
                s['validity'][m]['valid'], '' if s['validity'][m]['valid'] else ' (%s)' % s['validity'][m]['why_not']))
        else:
            print('  %-5s no result' % m)
        x = s['ptx'].get(m)
        if x:
            for a in x['atoms']:
                print('        PTX  %s  [space %s, sem %s, scope %s]' % (a['line'], a['state_space'], a['sem'], a['scope']))
            if x.get('cas_result_used') is not None and x['cas_result_used']:
                print('        atom.cas result read afterwards: %s' % x['cas_result_used'])
        for a in s.get('sass_probe_specific', {}).get(m, []):
            print('        SASS %s' % a)
    for k, v in dec.items():
        print('  %s: %s' % (k, v))
    return 0


if __name__ == '__main__':
    sys.exit(main())
