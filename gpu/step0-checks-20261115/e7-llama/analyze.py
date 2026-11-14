#!/usr/bin/env python3
"""step0 e7-llama analysis: active lanes per exit-probe call on llama.cpp kernels.

usage: python3 -I analyze.py RESULTS_DIR [--census-dir DIR]

RESULTS_DIR/runs.tsv (written by run_all.sh): label test kidx run_dir json
Each json is an e7_loader output. For each (test, kernel) this prints and
writes:
  - the histogram of active lanes per exit-probe call (records per call);
  - checks against the nsys census of the same llama-bench arguments
    (launches and thread executions, exact) and the anomaly counters;
  - for chunk sizes C = 1..32: expected unused slots per call
    (ceil(k/C)*C - k, averaged over calls) and the unused share of reserved
    slots;
  - the verdicts of README.md's decision rules.
Outputs: summary.json, hist.csv, waste.csv, summary.txt in RESULTS_DIR.
Standard library only (sqlite3 for the census).
"""
import csv
import json
import math
import os
import sqlite3
import sys

CENSUS_DIR = '/home/jetson/src/bpftime-gpu-work/llama/census'
KNAME = ['rms', 'mmq', 'mmvq', 'ropek', 'ctrl', 'ctrldiv', 'ctrlmerge']
CTRL_KERNELS = ('ctrl', 'ctrldiv')      # pass/fail controls (README rule 1)
REPORTED_CTRL = ('ctrlmerge',)          # reported only: do two exit groups merge inside the probe?
# the control run's compiled-code checks (check_ptx.py outputs in its run directory)
PTX_CHECKS = {'ctrl': 'ptxcheck.check.json', 'ctrldiv': 'ptxcheck-div.check.json',
              'ctrlmerge': 'ptxcheck-merge.check.json'}
MANGLED = {
    'rms': '_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f',
    'mmq': '_Z9mul_mat_qIL9ggml_type12ELi128ELb0EL9ggml_prec30ELb1EEvPKcS3_PKiS5_PK25ggml_cuda_mm_id_tile_specPfS9_PKf11ggml_glu_opfSB_SB_5uint3iiiiiSD_SD_iiiSD_SD_iiiSD_',
    'mmvq': '_Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj',
    'ropek': '_Z9rope_normILb1ELb0Ef6__halfEvPKT1_PT2_iiiiiiiiiiiPKifff14rope_corr_dimsfPKfPKlib',
}
# census README / census/*.csv (fallback when the sqlite files are missing)
CENSUS_FALLBACK = {
    ('pp512', 'rms'): (45, 11273216), ('pp512', 'mmq'): (21, 1892352),
    ('pp512', 'mmvq'): (2, 983040), ('pp512', 'ropek'): (22, 11534336),
    ('tg128', 'rms'): (5760, 2949120), ('tg128', 'mmq'): (0, 0),
    ('tg128', 'mmvq'): (7040, 3137339392), ('tg128', 'ropek'): (2816, 2883584),
    ('pp32', 'rms'): (45, 705536),
    # smoke only, [arith] from tg128: 45 rms_norm launches of 512 threads per token
    ('tg16', 'rms'): (720, 368640),
    # smoke only, [arith] from tg128: 55 launches of mul_mat_vec_q per token, 24,510,464 threads per token
    ('tg8', 'mmvq'): (440, 196083712),
}
SQLITE = {'pp512': 'pp512-graph.sqlite', 'tg128': 'tg128-graph.sqlite', 'pp32': 'pp32-graph.sqlite'}
# e7_ctrl defaults (run_all.sh passes them explicitly)
CTRL_LAUNCHES, CTRL_BLOCKS = 10, 64

# (test, kernel) pairs the census never runs (mul_mat_q does not run in decode): reported as not run
EXPECTED_ABSENT = {('tg128', 'mmq')}

# Decision thresholds (README.md, written before any run)
FULL_SHARE = 0.99      # "full warps": share of calls with 32 lanes
WASTE_OK = 0.10        # acceptable unused share of reserved slots
LOW_MEAN = 16.0        # mean records per call below this: per-warp chunks of 32 waste > 50%


def census(test, kname, census_dir):
    path = os.path.join(census_dir, SQLITE.get(test, ''))
    if test in SQLITE and os.path.exists(path):
        c = sqlite3.connect('file:%s?mode=ro' % path, uri=True)
        r = c.execute(
            'select count(*), coalesce(sum(gridX*gridY*gridZ*blockX*blockY*blockZ),0) '
            'from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.mangledName '
            'where s.value=?', (MANGLED[kname],)).fetchone()
        return int(r[0]), int(r[1]), 'sqlite:' + os.path.basename(path)
    if (test, kname) in CENSUS_FALLBACK:
        a, b = CENSUS_FALLBACK[(test, kname)]
        return a, b, 'fallback-table'
    return None, None, 'none'


def _ptx_pass(x):
    try:
        return bool(json.load(open(os.path.join(x['run_dir'], PTX_CHECKS[x['kernel']]))).get('pass'))
    except Exception:
        return False


def waste_table(hist):
    calls = sum(hist[1:])
    recs = sum(k * hist[k] for k in range(1, 33))
    rows = []
    for C in range(1, 33):
        reserved = sum(math.ceil(k / C) * C * hist[k] for k in range(1, 33))
        unused = reserved - recs
        rows.append({
            'C': C,
            'unused_slots_per_call': unused / calls if calls else None,
            'unused_share_of_reserved': unused / reserved if reserved else None,
            'chunks_per_call': (sum(math.ceil(k / C) * hist[k] for k in range(1, 33)) / calls) if calls else None,
        })
    return rows


def main():
    args = sys.argv[1:]
    census_dir = CENSUS_DIR
    if '--census-dir' in args:
        i = args.index('--census-dir')
        census_dir = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        print(__doc__)
        return 2
    rd = args[0]
    rows = list(csv.DictReader(open(os.path.join(rd, 'runs.tsv')), delimiter='\t'))
    out = {'runs': [], 'verdicts': {}}
    hist_rows, waste_rows, lines = [], [], []
    for r in rows:
        label, test, kidx = r['label'], r['test'], int(r['kidx'])
        kname = KNAME[kidx]
        rec = {'label': label, 'test': test, 'kernel': kname, 'run_dir': r['run_dir'], 'id': '%s/%s/%s' % (label, test, kname)}
        try:
            j = json.load(open(r['json']))
        except Exception as e:  # missing or truncated output: the run failed
            rec['error'] = 'no loader output: %s' % e
            rec['valid'] = False
            out['runs'].append(rec)
            lines.append('%-6s %-6s %-7s ERROR %s' % (label, test, kname, rec['error']))
            continue
        if kname not in j.get('kernels', {}):
            rec['error'] = 'loader output has no kernel %s (older layout?)' % kname
            rec['valid'] = False
            out['runs'].append(rec)
            lines.append('%-6s %-6s %-7s ERROR %s' % (label, test, kname, rec['error']))
            continue
        k = j['kernels'][kname]
        hist = k['hist']
        calls, threads = k['calls'], k['threads']
        rec.update({'calls': calls, 'threads': threads, 'launches': k['launches'],
                    'anom_self': k['anom_self'], 'anom_zero': k['anom_zero'],
                    'lookup_failures': j.get('lookup_failures'),
                    'hist': hist, 'calls_per_sm': k['calls_per_sm']})
        rec['records_per_call_mean'] = threads / calls if calls else None
        rec['share_calls_32'] = hist[32] / calls if calls else None
        rec['share_records_in_full_calls'] = 32 * hist[32] / threads if threads else None
        # expected
        if kname == 'ctrl':
            # 48-thread blocks: per block one call of 32 and one of 16
            L = int(r.get('ctrl_launches') or CTRL_LAUNCHES)
            G = int(r.get('ctrl_blocks') or CTRL_BLOCKS)
            exp = {'launches': L, 'threads': 48 * G * L, 'hist32': G * L, 'hist16': G * L, 'source': 'e7_ctrl parameters'}
            other = sum(hist[b] for b in range(33) if b not in (16, 32))
            ok = (k['launches'] == L and threads == 48 * G * L and hist[32] == G * L and hist[16] == G * L
                  and other == 0 and k['anom_self'] == 0 and k['anom_zero'] == 0 and j.get('lookup_failures') == 0)
        elif kname == 'ctrldiv':
            # 64-thread blocks, odd lanes leave through their own ret: per warp two calls of 16
            L = int(r.get('ctrl_launches') or CTRL_LAUNCHES)
            G = int(r.get('ctrl_blocks') or CTRL_BLOCKS)
            exp = {'launches': L, 'threads': 64 * G * L, 'hist16': 4 * G * L, 'hist32': 0, 'source': 'e7_ctrl parameters'}
            other = sum(hist[b] for b in range(33) if b != 16)
            ok = (k['launches'] == L and threads == 64 * G * L and hist[16] == 4 * G * L
                  and other == 0 and k['anom_self'] == 0 and k['anom_zero'] == 0 and j.get('lookup_failures') == 0)
        elif kname == 'ctrlmerge':
            # reported only: 2 calls of 32 per block if the two exit groups merge in the probe, 4 of 16 if not
            L = int(r.get('ctrl_launches') or CTRL_LAUNCHES)
            G = int(r.get('ctrl_blocks') or CTRL_BLOCKS)
            exp = {'launches': L, 'threads': 64 * G * L, 'merged_calls32': 2 * G * L, 'separate_calls16': 4 * G * L,
                   'source': 'e7_ctrl parameters (reported, not pass/fail)'}
            ok = (k['launches'] == L and threads == 64 * G * L and k['anom_self'] == 0 and k['anom_zero'] == 0
                  and j.get('lookup_failures') == 0)
            rec['merge_share_of_warps'] = (hist[32] / (2 * G * L)) if G * L else None
        else:
            el, et, src = census(test, kname, census_dir)
            exp = {'launches': el, 'threads': et, 'source': src}
            ok = (el is not None and k['launches'] == el and threads == et and k['anom_self'] == 0
                  and k['anom_zero'] == 0 and j.get('lookup_failures') == 0)
            if el == 0:
                if (test, kname) in EXPECTED_ABSENT and calls == 0 and k['launches'] == 0:
                    # e.g. mul_mat_q in decode (all4 tg128): nothing to measure, not a failure
                    rec['not_run'] = True
                else:
                    # a planned kernel the census never ran: the name matches nothing (probe and census
                    # would both see 0 and the run would vanish from the verdict)
                    ok = False
                    rec['error'] = 'the census has no launch of %s in %s (kernel name mismatch?)' % (kname, test)
        rec['expected'] = exp
        rec['valid'] = bool(ok)
        rec['waste'] = waste_table(hist) if calls else []
        out['runs'].append(rec)
        for b in range(1, 33):
            if hist[b]:
                hist_rows.append({'label': label, 'test': test, 'kernel': kname, 'lanes': b, 'calls': hist[b],
                                  'share_of_calls': hist[b] / calls})
        for w in rec['waste']:
            waste_rows.append(dict(label=label, test=test, kernel=kname, **w))
        lines.append('%-6s %-6s %-7s valid=%s launches=%s/%s threads=%s/%s calls=%s mean_records_per_call=%s share_32=%s anomalies=%d/%d' % (
            label, test, kname, rec['valid'], k['launches'], exp['launches'], threads, exp['threads'], calls,
            '%.3f' % rec['records_per_call_mean'] if calls else '-',
            '%.5f' % rec['share_calls_32'] if calls else '-', k['anom_self'], k['anom_zero']))
        if rec.get('error'):
            lines.append('        error: ' + rec['error'])
        if rec.get('not_run'):
            lines.append('        not run in this test (census: 0 launches); excluded from the verdict')
        nz = ['%d:%d' % (b, hist[b]) for b in range(1, 33) if hist[b]]
        lines.append('        lanes:calls ' + ' '.join(nz))
        if calls:
            ws = rec['waste']
            lines.append('        unused share of reserved slots: ' + ' '.join(
                'C=%d:%.3f' % (w['C'], w['unused_share_of_reserved']) for w in ws if w['C'] in (1, 2, 4, 8, 16, 32)))

    # Verdicts (README decision rules)
    v = out['verdicts']
    ctrl = [x for x in out['runs'] if x['kernel'] in CTRL_KERNELS]
    # rule 1: both controls present and exact (the partial-warp one and the divergence one)
    v['controls_present'] = sorted(set(x['kernel'] for x in ctrl))
    v['control_pass'] = (set(CTRL_KERNELS) <= set(v['controls_present'])) and all(x['valid'] for x in ctrl)
    # rule 2: the control run's compiled-code checks (check_ptx.py outputs in its run directory)
    pc = [_ptx_pass(x) for x in ctrl]
    v['ptx_check_pass'] = bool(pc) and all(pc) and set(CTRL_KERNELS) <= set(v['controls_present'])
    llama = [x for x in out['runs'] if x['kernel'] not in CTRL_KERNELS + REPORTED_CTRL and not x.get('not_run')]
    v['not_run'] = [x['id'] for x in out['runs'] if x.get('not_run')]
    merge = [x for x in out['runs'] if x['kernel'] in REPORTED_CTRL]
    v['merge_observation'] = ['%s: valid=%s, share of warps whose two exit groups made one call of 32 = %s; ptx check pass = %s' % (
        x['id'], x['valid'], x.get('merge_share_of_warps'), _ptx_pass(x)) for x in merge]
    measured = [x for x in llama if x.get('calls')]
    v['all_llama_runs_valid'] = bool(llama) and all(x['valid'] for x in llama)
    if not ctrl:
        v['overall'] = 'INCONCLUSIVE: no positive-control run in this batch'
    elif not v['control_pass']:
        v['overall'] = ('INCONCLUSIVE: a positive control failed or is missing (present: %s; ctrl = helper 520, the election '
                        'or the counting; ctrldiv = the mask under divergence, without which full warps cannot be told '
                        'from a mask that ignores divergence)' % ','.join(v['controls_present']))
    elif not v['ptx_check_pass']:
        v['overall'] = 'INCONCLUSIVE: a PTX/SASS check of the control run failed or is missing (ptxcheck.check.json, ptxcheck-div.check.json)'
    elif not llama:
        v['overall'] = 'CONTROLS VALID; no llama run in this batch'
    elif not v['all_llama_runs_valid']:
        bad = [x['id'] for x in llama if not x['valid']]
        v['overall'] = 'INCONCLUSIVE for ' + ', '.join(bad) + ' (counts differ from the census, anomalies or failed lookups); valid runs are reported'
    else:
        v['overall'] = 'VALID'
    # interpretation only when the controls and the compiled-code checks passed (README rules 1 and 2)
    instrument_ok = v['control_pass'] and v['ptx_check_pass']
    v['interpretation_allowed'] = instrument_ok
    full = [x for x in measured if x['valid'] and x['share_calls_32'] is not None and x['share_calls_32'] >= FULL_SHARE]
    low = [x for x in measured if x['valid'] and x['records_per_call_mean'] is not None and x['records_per_call_mean'] < LOW_MEAN]
    # no interpretation when the gate fails (README rules 1 and 2)
    v['full_warp_kernels'] = [x['id'] for x in full] if instrument_ok else []
    v['low_mean_kernels'] = [x['id'] for x in low] if instrument_ok else []
    cstar = None
    valid_measured = [x for x in measured if x['valid']]
    if valid_measured and instrument_ok:
        for C in range(32, 0, -1):
            if all(x['waste'][C - 1]['unused_share_of_reserved'] <= WASTE_OK for x in valid_measured):
                cstar = C
                break
    v['largest_C_with_unused_share_le_10pct_on_every_valid_kernel'] = cstar
    if not instrument_ok:
        v['chunk_rule'] = 'NONE: the controls or the compiled-code checks did not pass, so no interpretation (raw counts above are not evidence)'
    elif valid_measured:
        if len(full) == len(valid_measured):
            v['chunk_rule'] = 'FULL WARPS: >= 99% of calls have 32 lanes on every valid kernel; per-warp chunks of 32 waste little (see waste table)'
        elif low:
            v['chunk_rule'] = 'PARTIAL: mean records per call < 16 on ' + ', '.join(v['low_mean_kernels']) + '; C = 32 wastes over half the reserved slots there; C* = %s' % cstar
        else:
            v['chunk_rule'] = 'MIXED: largest C with <= 10%% unused on every valid kernel: C* = %s' % cstar
    else:
        v['chunk_rule'] = 'no valid llama run'
    lines.append('')
    for key in ('overall', 'control_pass', 'controls_present', 'ptx_check_pass', 'interpretation_allowed', 'merge_observation', 'not_run', 'chunk_rule', 'full_warp_kernels', 'low_mean_kernels',
                'largest_C_with_unused_share_le_10pct_on_every_valid_kernel'):
        lines.append('%s: %s' % (key, v[key]))

    json.dump(out, open(os.path.join(rd, 'summary.json'), 'w'), indent=1)
    with open(os.path.join(rd, 'hist.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=['label', 'test', 'kernel', 'lanes', 'calls', 'share_of_calls'])
        w.writeheader()
        w.writerows(hist_rows)
    with open(os.path.join(rd, 'waste.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=['label', 'test', 'kernel', 'C', 'unused_slots_per_call', 'unused_share_of_reserved', 'chunks_per_call'])
        w.writeheader()
        w.writerows(waste_rows)
    open(os.path.join(rd, 'summary.txt'), 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    return 0


if __name__ == '__main__':
    sys.exit(main())
