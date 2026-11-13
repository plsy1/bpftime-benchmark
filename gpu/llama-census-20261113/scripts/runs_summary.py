"""Summarise the bpftime runs on llama-bench (run.sh output dirs copied under runs/).

Usage: python3 -I runs_summary.py <runs-dir> > runs/summary.md
Per run: llama-bench tokens/s (mean, sd, samples), the process wall time, and the threadhist
loader's last line (slots written, sum of counts).
"""
import json, os, re, sys

D = sys.argv[1]
print('| run | prompt | gen | tokens/s mean | sd | samples | victim wall ms | threadhist last line |')
print('|---|---:|---:|---:|---:|---|---:|---|')
for name in sorted(os.listdir(D)):
    p = os.path.join(D, name)
    if not os.path.isdir(p):
        continue
    s = open(os.path.join(p, 'client.log')).read()
    # the JSON array can be interleaved with agent log lines; decode the first result object
    i = s.find('{\n    "build_commit"')
    x, _ = json.JSONDecoder().raw_decode(s[i:])
    meta = open(os.path.join(p, 'meta.txt')).read()
    ms = re.search(r'client_ms=(\d+)', meta).group(1)
    last = [l for l in open(os.path.join(p, 'server.log')) if 'lookup=' in l]
    last = last[-1].strip() if last else ''
    last = re.sub(r'^\S+\s+', '', last)
    print(f"| {name} | {x['n_prompt']} | {x['n_gen']} | {x['avg_ts']:.1f} | {x['stddev_ts']:.1f} | {', '.join(f'{v:.1f}' for v in x['samples_ts'])} | {ms} | {last} |")
