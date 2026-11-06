#!/usr/bin/env python3
"""Where bpftime's ring buffer output spends its time with more producers (finding 1):
out-rbres-8 (8-byte records, reserve + submit), burstloop back to back (bursts of 10,000, no
pause), consumer in ring_buffer__poll. Configurations by CPU list: 1 thread (5), 2 threads on
one cluster (5,4), 2 threads across clusters (5,3), 4 (5,4,3,2), 6 (5..0); this Orin's clusters
are CPUs 0-3 and 4-5. For each: a run without perf for ns per call, then one under
`perf record -F 4999` (flat profile by DSO and symbol into prof/*.txt; perf.data stays in
scratch). kernel bpf and fix-final."""
import json, os, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'more-dims-20261106'))
import common  # noqa: E402

PERF = '/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf'
SCRATCH = Path(os.environ.get('SCRATCH', '/tmp'))
OBJ = HERE / 'src' / 'out-rbres-8.bpf.o'
BL = HERE / 'src' / 'burstloop'
CONFIGS = [('1t', '5'), ('2t-same', '5,4'), ('2t-cross', '5,3'), ('4t', '5,4,3,2'),
           ('6t', '5,4,3,2,1,0')]


def run(backend, name, cpus, perf):
    common.cleanup(backend, ('outattach', 'burstloop'))
    threads = len(cpus.split(','))
    res = dict(backend=backend, config=name, cpus=cpus, perf=perf)
    tool = common.Tool(backend, OBJ, BL, ['on_count=count_event'], shm_mb=512)
    try:
        wl = [str(BL), str(threads), '4', '10000', '0']
        envs = common.agent_env(backend) + [f'CPUS={cpus}']
        if perf:
            data = SCRATCH / f'perf-{backend}-{name}.data'
            cmd = ['sudo', '-n', 'timeout', '120', PERF, 'record', '-q', '-F', '4999', '-o',
                   str(data), '--', 'env', *envs] + wl
        else:
            cmd = ['sudo', '-n', 'timeout', '120', 'env', *envs] + wl
        w = common.sh(cmd)
        res['total_ops'] = common.num(r'^total_ops (\d+)$', w.stdout)
        if res['total_ops']:
            res['ns_per_call'] = round(4e9 * threads / res['total_ops'], 1)
        text = tool.stop()
        s = tool.sums(text)
        res.update(hits=s['sum0'], failed=s['sum1'],
                   received=common.num(r'^received (\d+)$', text))
        res['ok'] = res['hits'] == res['total_ops']
        if perf:
            rep = common.sh(['sudo', '-n', PERF, 'report', '-i', str(data), '--stdio',
                             '--no-children', '--sort', 'dso,sym', '--percent-limit', '0.3'])
            (HERE / 'prof' / f'{backend}-{name}.txt').write_text(rep.stdout)
    finally:
        tool.kill()
        common.cleanup(backend, ('outattach', 'burstloop'))
    return res


def main():
    out = HERE / 'prof' / 'runs.jsonl'
    for rep in range(2):
        for name, cpus in CONFIGS:
            for backend in ('kernel', 'fix-final'):
                for perf in (False, True) if rep == 0 else (False,):
                    r = run(backend, name, cpus, perf)
                    r['rep'] = rep
                    with open(out, 'a') as f:
                        f.write(json.dumps(r) + '\n')
                    print(json.dumps(r), flush=True)


if __name__ == '__main__':
    main()
