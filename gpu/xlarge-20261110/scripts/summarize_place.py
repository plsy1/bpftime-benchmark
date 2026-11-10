#!/usr/bin/env python3
"""Placement sweep: per run, the counter's segment offset (agent log) and the update means."""
import re, glob, statistics
R = '/home/jetson/src/results/gpu/xlarge-20261110/runs'
rows = []
for p in sorted(glob.glob(R + '/[OD]-t*-r*/micro_vec_add_bench.log')):
    run = p.split('/')[-2]; s, t, r = run.split('-')
    log = open(p, errors='replace').read()
    parts = re.split(r"^.*Running test: (.+)$", log, flags=re.M)
    for i in range(1, len(parts), 2):
        sec = parts[i + 1]; case = parts[i].strip()
        m = re.search(r"Mean:\s+([\d.]+).*?Min:\s+([\d.]+)", sec, re.S)
        base = re.search(r"Registered shared memory with CUDA: addr=0x([0-9a-f]+)", sec)
        ctr = re.search(r"map_type = 1513, gpu_buffer = ([0-9a-f]+)", sec)
        segbase_gpu = 0x1021c4000
        off = int(ctr.group(1), 16) - segbase_gpu if ctr else None
        rows.append((s, int(t[1:]), r, case.split('(')[1][:-1], off, float(m.group(1)), float(m.group(2))))
for size in ('large', 'xlarge'):
    print(f'## {size}')
    for s in 'OD':
        v = [x for x in rows if x[0] == s and x[3] == size]
        for x in sorted(v, key=lambda x: (x[1], x[2])):
            print(f'{s} t{x[1]} {x[2]} off=0x{x[4]:x} page_off=0x{x[4] & 0xfff:03x} mean={x[5]:.1f} min={x[6]:.1f}')
        print(s, 'median', statistics.median([x[5] for x in v]), 'range', min(x[5] for x in v), max(x[5] for x in v), 'n', len(v))
