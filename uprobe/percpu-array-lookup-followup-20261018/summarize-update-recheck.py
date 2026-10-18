#!/usr/bin/env python3
"""Report update controls with engine labels mapped back to source versions."""
import csv
import json
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parent
EXPERIMENTS = (
    ('Original short A/B', 'inline-leaf/ab5', False),
    ('Balanced six-round A/B', 'update-recheck-ab6', False),
    ('Reversed-label six-round A/B', 'update-reverse-ab6', True),
)
records = []
for label, directory, reversed_labels in EXPERIMENTS:
    folder = ROOT / directory
    environment = json.loads((folder / 'environment.json').read_text())
    assert environment['status'] == 'success', (directory, environment['status'])
    summary = json.loads((folder / 'summary.json').read_text())
    old_engine, new_engine = (('candidate', 'baseline') if reversed_labels
                              else ('baseline', 'candidate'))
    raw = list(csv.DictReader((folder / 'wall-raw.csv').open()))
    for item in summary:
        old, new = item[old_engine], item[new_engine]
        deltas = [n-o for o,n in zip(old['raw_values'], new['raw_values'])]
        raw_old = [float(row['raw_ns_per_invocation'])/1000 for row in raw
                   if row['engine'] == old_engine and row['case'] == item['case']]
        raw_new = [float(row['raw_ns_per_invocation'])/1000 for row in raw
                   if row['engine'] == new_engine and row['case'] == item['case']]
        records.append({
            'experiment': label, 'directory': directory, 'case': item['case'],
            'baseline_net_median_ns': old['median'],
            'inline_net_median_ns': new['median'],
            'median_change_percent': 100*(new['median']/old['median']-1),
            'mean_change_percent': 100*(new['mean']/old['mean']-1),
            'paired_mean_delta_ns': statistics.mean(deltas),
            'paired_delta_stdev_ns': statistics.stdev(deltas),
            'paired_deltas_ns': deltas,
            'raw_baseline_median_ns': statistics.median(raw_old),
            'raw_inline_median_ns': statistics.median(raw_new),
            'binary_sha256': environment['binary_sha256'],
        })
lines = [
    '# Per-CPU array update regression investigation', '',
    'All rechecks use the same frozen baseline and inline-leaf binaries, CPU 5',
    'locked at 1728000 kHz, and unchanged BPF test objects. No runtime source',
    'change was made during this investigation. The two new tests alternate',
    'AB/BA three times each; the second also exchanges the harness labels.',
    'Times below are control-subtracted ns/helper, not raw full-suite times.', '',
    '| Test | Old update | Inline update | Median change | Paired mean delta |',
    '|---|---:|---:|---:|---:|',
]
for r in records:
    if r['case'] == 'percpu_array_update':
        lines.append(f"| {r['experiment']} | {r['baseline_net_median_ns']:.5f} | "
                     f"{r['inline_net_median_ns']:.5f} | {r['median_change_percent']:+.3f}% | "
                     f"{r['paired_mean_delta_ns']:+.5f} ns |")
lines += ['', '## Lookup benefit check', '',
          '| Test | Old lookup | Inline lookup | Median change |',
          '|---|---:|---:|---:|']
for r in records:
    if r['case'] == 'percpu_array_lookup':
        lines.append(f"| {r['experiment']} | {r['baseline_net_median_ns']:.5f} | "
                     f"{r['inline_net_median_ns']:.5f} | {r['median_change_percent']:+.3f}% |")
lines += ['', '## Assembly evidence', '',
          'The 264-byte update leaf and 168-byte specialized update helper have',
          'identical normalized instruction sequences between builds, including',
          'the same calls, checks, stack frames and data movement. The leaf moves',
          'from 0x4f2234 to 0x4f2230; the helper from 0x4e8ec0 to 0x4e8f40.',
          'This excludes added update instructions as the explanation; it does',
          'not prove which microarchitectural or environmental effect caused the',
          'original timing difference. See [assembly comparison](update-assembly/summary.json).', '',
          '## Interpretation limits', '',
          'Retain the original full-run observation: 14.3241 to 14.4416 ns',
          '(+0.82%). Matched rechecks determine whether that magnitude is stable;',
          'they do not erase the observation or prove the absence of a smaller',
          'effect. No padding/alignment workaround is justified without a',
          'reproducible layout experiment. These are process-level samples, not',
          'independent samples for every helper invocation.', '']
(ROOT / 'update-investigation.json').write_text(json.dumps(records, indent=2)+'\n')
(ROOT / 'update-investigation.md').write_text('\n'.join(lines))
print('\n'.join(lines))
