#!/usr/bin/env python3
"""summarize_e8.py RESULTS_DIR : apply README's decision rules to results.jsonl
and base.info.txt; write summary.json, print a table. Standard library only."""
import json
import os
import re
import sys


def main():
    R = os.path.abspath(sys.argv[1])
    rows = [json.loads(l) for l in open(os.path.join(R, 'results.jsonl')) if l.strip()]
    s = {'results_dir': R, 'cases': [], 'constant': {}}
    base = next((x['r'] for x in rows if x['label'] == 'base'), None)
    s['base_ok'] = bool(base and base.get('load_rc') == 0 and base.get('map_info_size') == 40960)
    s['negative_control_ok'] = bool(base and base.get('bogus_rc') == 500)  # CUDA_ERROR_NOT_FOUND
    for x in rows:
        r = x.get('r', {})
        if x.get('skipped'):
            s['cases'].append({'label': x['label'], 'skipped': True})
            continue
        if not r.get('mode') or r['mode'] in ('base', 'constpad'):
            continue
        c = {'label': x['label'], 'mode': r['mode'], 'N': r['N'], 'jit': 'JIT' in r.get('path', ''),
             'compile_rc': r.get('compile_rc'), 'load_rc': r.get('load_rc'),
             'found': r.get('global_rc') == 0, 'size_ok': r.get('global_size_ok') == 1,
             'host_rw_ok': r.get('memset_rc') == 0 and r.get('readback_rc') == 0 and r.get('memset_readback_mismatch_bytes') == 0}
        if r['mode'] == 'kernel':
            c['kernel_ok'] = (r.get('kernel_rc') == 0 and r.get('kernel_symbol_addr_matches') == 1 and
                              r.get('kernel_wrong_written') == 0 and r.get('kernel_wrong_other') == 0 and
                              r.get('kernel_words_written', 0) > 0)
        if r.get('mem_free_after_map_info') is not None and r.get('mem_free_after_global') is not None:
            # informational only: cuMemGetInfo on this iGPU reports system memory, which other processes move
            c['free_drop_at_getglobal_mib'] = round((r['mem_free_before_load'] - r['mem_free_after_global']) / 1048576, 1)
        c['works'] = c['found'] and c['size_ok'] and c['host_rw_ok'] and c.get('kernel_ok', True)
        s['cases'].append(c)
    # constant memory
    info = os.path.join(R, 'base.info.txt')
    if os.path.exists(info):
        t = open(info, errors='replace').read().replace('\x00', '')
        m = re.search(r'(\d+) bytes gmem, (\d+) bytes cmem\[3\](?:, (\d+) bytes cmem\[4\])?', t)
        if m:
            s['constant']['module_gmem'] = int(m.group(1))
            s['constant']['cmem3_used'] = int(m.group(2))
            s['constant']['cmem4_used'] = int(m.group(3) or 0)
            s['constant']['cmem3_left_of_65536'] = 65536 - int(m.group(2))
        k0 = [int(x) for x in re.findall(r'(\d+) bytes cmem\[0\]', t)]
        k2 = [int(x) for x in re.findall(r'(\d+) bytes cmem\[2\]', t)]
        s['constant']['per_kernel_cmem0_max'] = max(k0) if k0 else None
        s['constant']['per_kernel_cmem2_max'] = max(k2) if k2 else None
        s['constant']['kernels_compiled'] = len(re.findall(r'Compiling entry function', t))
    for x in rows:
        r = x.get('r', {})
        if r.get('mode') == 'constpad':
            s['constant']['constpad_%d' % r['N']] = 'compiles' if r.get('compile_rc') == 0 else (
                'fails: ' + r.get('compile_error', '').strip())
    # README rules: valid only with base + map_info 40,960 B + bogus -> 500. Per N (nvPTXCompiler path):
    # yes if plain works, or visible and kernel both work; untested if skipped; no otherwise.
    valid = s['base_ok'] and s['negative_control_ok']
    per_n = {}
    for c in s['cases']:
        if c.get('skipped'):
            n = int(c['label'].split('-', 1)[1])
            per_n.setdefault(n, {})['skipped'] = True
        elif not c.get('jit'):
            per_n.setdefault(c['N'], {})[c['mode']] = bool(c.get('works'))
    verdict_n = {}
    for n, m in sorted(per_n.items()):
        if m.get('skipped'):
            verdict_n[n] = 'untested (memory)'
        elif m.get('plain') or (m.get('visible') and m.get('kernel')):
            verdict_n[n] = 'yes' + ('' if m.get('plain') else ' (needs .visible)')
        else:
            verdict_n[n] = 'no (%s)' % ', '.join('%s=%s' % kv for kv in sorted(m.items()))
    yes = [n for n, v in verdict_n.items() if v.startswith('yes')]
    c3 = s['constant']
    left = c3.get('cmem3_left_of_65536')
    const_ok = (left is not None and c3.get('constpad_%d' % left) == 'compiles'
                and str(c3.get('constpad_%d' % (left + 8), '')).startswith('fails'))
    s['decision'] = {
        'valid': valid,
        'per_N': {str(k): v for k, v in verdict_n.items()} if valid else 'inconclusive (base or negative control failed)',
        'packaging_works_up_to_N': (max(yes) if yes else None) if valid else None,
        'constant_bank3_left_bytes': left,
        'constant_left_confirmed': const_ok,
        'rule': 'valid only if base loads, map_info is 40,960 B and a bogus name is not found (500); packaging works '
                'at N if plain, or visible and kernel, at N compile, load, are found with size N and read back what '
                'was written; constant left stands if a .const of exactly that size compiles and 8 B more fails',
    }
    json.dump(s, open(os.path.join(R, 'summary.json'), 'w'), indent=1)
    print('base_ok=%s negative_control_ok=%s' % (s['base_ok'], s['negative_control_ok']))
    print('%-28s %-8s %-11s %-5s %-6s %-6s %-6s %-6s %s' % ('case', 'mode', 'N', 'jit', 'load', 'found', 'size', 'rw', 'kernel / free drop MiB'))
    for c in s['cases']:
        if c.get('skipped'):
            print('%-28s skipped' % c['label'])
            continue
        print('%-28s %-8s %-11d %-5s %-6s %-6s %-6s %-6s %s / %s' % (
            c['label'], c['mode'], c['N'], c['jit'], c['load_rc'], c['found'], c['size_ok'], c['host_rw_ok'],
            c.get('kernel_ok', '-'), c.get('free_drop_at_getglobal_mib', '-')))
    print('constant:', json.dumps(s['constant']))
    print('decision:', json.dumps(s['decision']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
