"""Project the direction-1 and direction-2 operation volume of GPU eBPF tools on llama.cpp.

Usage: python3 -I projection.py <census-dir> > projection.md

Inputs:
- census/<run>-nograph.csv from parse_census.py (one llama-bench run under nsys, no bpftime):
  per kernel launches, threads per launch, thread executions, GPU time.
- The per-firing operation table (per-firing-ops.json, two independent derivations that agree on
  every GPU program): which threads run each program and how many D1 / D2 operations one firing
  executes. The numbers are copied into TOOLS below with the key they come from.

Nothing here is measured with bpftime. Host-call time = D2 operations x the measured cost per call
on this Orin (9.8-15.6 us with the shipped per-module lock, results gpu/fixes-20261109/protocol),
and the calls of different threads run one after another.
"""
import csv, math, os, sys

D = sys.argv[1]
RUNS = [('pp32', 'one 32-token prefill'), ('pp128', 'one 128-token prefill'),
        ('pp512', 'one 512-token prefill'), ('tg128', '128 decode tokens')]
COST_LO, COST_HI = 9.8e-6, 15.6e-6
DEFAULT_THREADS = 1024          # bpftime_shm.hpp:45, used when BPFTIME_MAP_GPU_THREAD_COUNT is unset
SHM_BYTES = 128 << 20           # default segment, BPFTIME_SHM_MEMORY_MB

# tool -> (who runs it, D1 ops per firing, D2 ops per firing, per-thread storage bytes, source key)
# who: 'thread' = every thread, 'warp' = one lane per warp, 'launch' = thread 0 of the grid.
TOOLS = [
    ('threadhist (bpftime)', 'thread', 1, 0, 8, 'gpu/threadhist/threadhist.bpf.c'),
    ('kernelretsnoop (bpftime)', 'thread', 1, 0, 16 * (1024 + 8), 'gpu/kernelretsnoop/kernelretsnoop.bpf.c'),
    ('kernel_trace (bpftime)', 'launch', 1, 0, 16 * (56 + 8), 'gpu/kernel_trace/kernel_trace.bpf.c'),
    ('kernelretsnoop (gpu_ext, warp leader)', 'warp', 1, 0, None, 'gpubpf_tool_build/kernelretsnoop'),
    ('launchlate (gpu_ext, thread 0)', 'launch', 0, 0, None, 'gpubpf_tool_build/launchlate'),
    ('launchlate (bpftime, printk per thread)', 'thread', 0, 1, None, 'gpu/launchlate/launchlate.bpf.c'),
    ('threadscheduling (bpftime, CPU maps)', 'thread', 0, 3, None, 'gpu/threadscheduling/threadscheduling.bpf.c'),
    ('cuda-counter (bpftime, CPU maps + printk)', 'thread', 0, 8, None, 'gpu/cuda-counter/cuda_probe.bpf.c (3 entry + 5 exit)'),
]

SCOPES = [
    ('every kernel', lambda k: True),
    ('rope_norm, K (gpu_ext target)', lambda k: k.startswith('_Z9rope_normILb1ELb0Ef6__half')),
    ('rms_norm_f32 (smoke target)', lambda k: k.startswith('_Z12rms_norm_f32')),
]


def load(run):
    p = os.path.join(D, f'{run}-nograph.csv')
    return list(csv.DictReader(open(p)))


def fmt(x):
    if x >= 1e9:
        return f'{x/1e9:.2f}G'
    if x >= 1e6:
        return f'{x/1e6:.2f}M'
    if x >= 1e3:
        return f'{x/1e3:.1f}k'
    return f'{x:.0f}'


def secs(x):
    if x >= 3600:
        return f'{x/3600:.1f} h'
    if x >= 60:
        return f'{x/60:.1f} min'
    if x >= 1:
        return f'{x:.1f} s'
    return f'{x*1e3:.1f} ms'


print('# Projected direction-1 and direction-2 volume on llama.cpp (TinyLlama Q4_K_M, Orin)\n')
print('Census runs: llama-bench --no-warmup -r 1, CUDA graphs off, nsys 2026.3.1; nothing measured with bpftime.\n')

print('## Census totals\n')
print('| run | launches | thread executions | max threads in one launch | min | GPU kernel time |')
print('|---|---:|---:|---:|---:|---:|')
for run, what in RUNS:
    rows = load(run)
    n = sum(int(r['launches']) for r in rows)
    te = sum(int(r['thread_execs']) for r in rows)
    tmax = max(int(r['threads_max']) for r in rows)
    tmin = min(int(r['threads_min']) for r in rows)
    ns = sum(int(r['gpu_ns']) for r in rows)
    print(f'| {what} | {n:,} | {te:,} | {tmax:,} | {tmin:,} | {ns/1e6:.1f} ms |')
print()

for scope, sel in SCOPES:
    print(f'## Probe on: {scope}\n')
    print('| run | tool | D1 ops | D2 ops (host calls) | host-call time, serialized | x GPU time of the run | D2 if aggregated per warp |')
    print('|---|---|---:|---:|---:|---:|---:|')
    for run, what in RUNS:
        rows = [r for r in load(run) if sel(r['kernel'])]
        if not rows:
            continue
        te = sum(int(r['thread_execs']) for r in rows)
        warps = te / 32  # every launch here has a block size that is a multiple of 32
        launches = sum(int(r['launches']) for r in rows)
        gpu_s = sum(int(r['gpu_ns']) for r in load(run)) / 1e9
        for name, who, d1, d2, _, _ in TOOLS:
            firings = {'thread': te, 'warp': warps, 'launch': launches}[who]
            o1, o2 = d1 * firings, d2 * firings
            t_lo, t_hi = o2 * COST_LO, o2 * COST_HI
            ratio = f'{t_lo/gpu_s:,.0f}-{t_hi/gpu_s:,.0f}x' if o2 else '0'
            agg = f'{fmt(o2/32)} ({secs(o2/32*COST_LO)}-{secs(o2/32*COST_HI)})' if (o2 and who == 'thread') else '-'
            tm = f'{secs(t_lo)}-{secs(t_hi)}' if o2 else '0'
            print(f'| {what} | {name} | {fmt(o1)} | {fmt(o2)} | {tm} | {ratio} | {agg} |')
    print()

    print(f'Per-thread storage (direction 1), probe on {scope}:\n')
    print('| run | slots a launch needs (max threads per launch) | slots written per launch, mean, as share of that | records lost with the default 1,024 slots | threadhist array at max | kernelretsnoop rings at max (16 x 1,032 B per thread) |')
    print('|---|---:|---:|---:|---:|---:|')
    for run, what in RUNS:
        rows = [r for r in load(run) if sel(r['kernel'])]
        if not rows:
            continue
        tmax = max(int(r['threads_max']) for r in rows)
        launches = sum(int(r['launches']) for r in rows)
        te = sum(int(r['thread_execs']) for r in rows)
        used = te / launches / tmax
        # loss with 1,024 slots: a launch of T threads keeps min(T, 1024) per-thread records.
        # Exact per launch needs per-launch T; with only min/max per kernel, bound it:
        kept_hi = sum(min(int(r['threads_max']), DEFAULT_THREADS) * int(r['launches']) for r in rows)
        lost = 1 - kept_hi / te
        ring = tmax * (16 * (1024 + 8) + 64)
        print(f'| {what} | {tmax:,} | {used*100:.1f}% | at least {lost*100:.1f}% | {tmax*8/2**20:.1f} MiB | {ring/2**30:.1f} GiB (segment {SHM_BYTES>>20} MiB) |')
    print()
