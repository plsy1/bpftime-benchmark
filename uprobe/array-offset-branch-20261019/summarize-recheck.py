#!/usr/bin/env python3
"""Report control-subtracted and raw paired data without updating full tables."""
from pathlib import Path
import csv
import json
import statistics

root = Path(__file__).resolve().parent
directory = root/'precommit-recheck-ab6'
metadata = json.loads((directory/'environment.json').read_text())
assert metadata['status'] == 'success'
rows = list(csv.DictReader((directory/'wall-raw.csv').open()))
summary = json.loads((directory/'summary.json').read_text())
report = []
lines = ['# Precommit paired recheck', '',
    'Same frozen binaries as full ten-round test; no rebuild. Six rounds, 100,000 BPF invocations per measured case, 3,000 warmup, CPU 5 at 1728 MHz. Initial baseline/candidate order is reversed relative to the original screen, then alternated each round. Each invocation performs 1,000 helpers.', '',
    'Positive change means candidate is slower. Net timings subtract the same-process control; raw timings do not. Neither replaces the formal full-suite table.', '',
    '| Operation | Net baseline ns | Net candidate ns | Net median change | Raw median change | Paired net losses / 6 | Paired net change range |',
    '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
for entry in summary:
    case = entry['case']
    b = entry['baseline']['raw_values']
    c = entry['candidate']['raw_values']
    assert len(b) == len(c) == 6
    deltas = [100*(candidate/base-1) for base, candidate in zip(b,c)]
    raw = {engine: [float(row['raw_ns_per_invocation'])/1000 for row in rows
                    if row['case'] == case and row['engine'] == engine]
           for engine in ('baseline','candidate','kernel')}
    assert all(len(values) == 6 for values in raw.values())
    item = dict(case=case,net_baseline_median=entry['baseline']['median'],
        net_candidate_median=entry['candidate']['median'],
        net_median_change_percent=-entry['reduction_percent'],
        raw_median_change_percent=100*(statistics.median(raw['candidate'])/statistics.median(raw['baseline'])-1),
        paired_net_change_percent=deltas,paired_net_change_median=statistics.median(deltas),
        paired_net_losses=sum(delta>0 for delta in deltas),raw_ns_per_helper=raw)
    report.append(item)
    lines.append(f'| {case} | {item["net_baseline_median"]:.6f} | {item["net_candidate_median"]:.6f} | {item["net_median_change_percent"]:+.3f}% | {item["raw_median_change_percent"]:+.3f}% | {item["paired_net_losses"]} | {min(deltas):+.3f}% to {max(deltas):+.3f}% |')
lines += ['', 'This reports observed differences only. It does not establish zero regression or a confidence bound below the measurement resolution. The completed full-ten table remains unchanged.', '']
(root/'recheck-summary.json').write_text(json.dumps(report,indent=2)+'\n')
(root/'recheck-summary.md').write_text('\n'.join(lines))
print('\n'.join(lines))
