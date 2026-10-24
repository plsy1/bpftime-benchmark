#!/usr/bin/env python3
"""Describe all samples, retaining outliers; do not overwrite full-suite data."""
import json
from pathlib import Path
import random
import statistics

root=Path(__file__).resolve().parent
env=json.loads((root/'environment.json').read_text())
assert env['status']=='success'
row=next(r for r in json.loads((root/'summary.json').read_text()) if r['case']=='__bench_hash_map_update')
previous=row['previous-on']['raw_values']
n=len(previous)
rng=random.Random(20261020)
comparisons={}
for engine in ('current-on','current-off'):
    values=row[engine]['raw_values']
    deltas=[]
    for _ in range(20000):
        indexes=[rng.randrange(n) for _ in range(n)]
        deltas.append(100*(sum(values[i] for i in indexes)/sum(previous[i] for i in indexes)-1))
    deltas.sort()
    comparisons[engine]=dict(mean_change_percent=100*(statistics.mean(values)/statistics.mean(previous)-1),
        paired_bootstrap_95_percent_interval=[deltas[500],deltas[19499]],
        note='Descriptive paired-block bootstrap; assumes independent blocks, not a guarantee under nonstationary process/layout effects.')
bands={}
for engine in ('previous-on','current-on','current-off'):
    values=row[engine]['raw_values']
    bands[engine]={}
    for name,predicate in (('fast_lt_34_5',lambda x:x<34.5),('slow_34_5_to_38',lambda x:34.5<=x<=38),('above_38',lambda x:x>38)):
        subset=[x for x in values if predicate(x)]
        bands[engine][name]=dict(count=len(subset),mean=statistics.mean(subset) if subset else None)
result=dict(experiment_status=env['status'],rounds=n,samples_per_arm=n,
    ordinary_hash_update=row,comparisons=comparisons,descriptive_bands=bands,
    outliers_discarded=0,frequency_checks=len(env['clock_samples']),
    conclusion='The prior +3.9% full-run difference was not reproduced as a stable version slowdown; both binaries and both flag settings show the same fast/slow regimes. Exact source of inter-process variation remains unproven.')
(root/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
lines=['# Ordinary hash update regression recheck','',
    '12 balanced A/B/C blocks (36 fresh processes/maps), original full-suite BPF object and victim, 10,000 invocations per process. Each invocation performs 1,000 map operations. All 17 cases were retained in each process. CPU 5 locked at 1,728 MHz; frozen binaries checked before/after. No source or runtime binary changed.','',
    '| Arm | Mean ns/op | Median ns/op | Mean vs previous |',
    '|---|---:|---:|---:|']
for engine in ('previous-on','current-on','current-off'):
    values=row[engine]
    delta='—' if engine=='previous-on' else f'{comparisons[engine]["mean_change_percent"]:+.2f}%'
    lines.append(f'| {engine} | {values["mean"]:.4f} | {values["median"]:.4f} | {delta} |')
lines.extend(['','All samples, including the >40 ns current-off sample, are retained in means and bootstrap calculations. Fast/slow bands are descriptive only, not exclusions.','',
    '| Arm | <34.5 ns count / mean | 34.5–38 ns count / mean | >38 ns count |','|---|---:|---:|---:|'])
for engine,b in bands.items():
    fmt=lambda x:f'{x["count"]} / {x["mean"]:.4f}' if x['count'] else '0 / —'
    lines.append(f'| {engine} | {fmt(b["fast_lt_34_5"])} | {fmt(b["slow_34_5_to_38"])} | {b["above_38"]["count"]} |')
lines.extend(['','## Interpretation','',result['conclusion'],'',
    'Address-normalized instructions match for both the 64-instruction public hash-update helper and the 192-instruction lower-level hash-table update routine. This does not prove identical JIT placement, cache behavior, memory layout or scheduling. Those mechanisms have not been isolated.','',
    'The current-on mean-change paired bootstrap interval is '+str(comparisons['current-on']['paired_bootstrap_95_percent_interval'])+' percent. This is an estimate under the independent-block assumption, not proof against every small regression.','',
    'Do not replace the historical full-ten-round table with these recheck numbers or recalculate a kernel ratio: this recheck did not rerun kernel. Its purpose is comparing the two frozen versions under the same original workload.'])
(root/'RESULT.md').write_text('\n'.join(lines)+'\n')
print(json.dumps(dict(comparisons=comparisons,bands=bands),indent=2))
