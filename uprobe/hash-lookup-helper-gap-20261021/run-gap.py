#!/usr/bin/env python3
"""Hash lookup helper gap: kernel and bdd9835 on three hash consuming variants.

v1-hash-only     data+result maps hash            -> typed hash helper
v2-hash-globals  v1 + .rodata/.bss global data    -> generic helper
v3-hash-mixed    data hash, result array          -> generic helper, bound program
All six (engine, variant) arms alternate within each round (order reversed on
odd rounds), fresh loader/victim processes, CPU 5 at 1.728 GHz, checksums and
hit/miss counts verified like the array consuming runners.
"""
import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

OUT = Path(__file__).resolve().parent
BUILD = Path(os.environ.get('GAP_BUILD', '/home/jetson/src/results/uprobe/array-jit-global-data-20261021/formal/artifacts'))
VARIANTS = ('v1-hash-only', 'v2-hash-globals', 'v3-hash-mixed')
CASES = ('control_hit', 'hit', 'control_mixed', 'mixed', 'unused')
TYPED = 'Using specialized hash lookup helper for program'


def checksum(seed, mask):
    state = seed
    total = hits = misses = 0
    for _ in range(1000):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        key = (state >> 16) & mask
        if key < 1024:
            total += key * 17 + 5
            hits += 1
        else:
            total += 0x9e3779b9
            misses += 1
    return total, hits, misses


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def freq():
    p = Path('/sys/devices/system/cpu/cpu5/cpufreq')
    values = {k: int((p / f'scaling_{k}_freq').read_text()) for k in ('min', 'max', 'cur')}
    assert values == dict(min=1728000, max=1728000, cur=1728000), values
    return values


def stop(proc):
    if proc is None or proc.poll() is not None:
        return
    subprocess.run(['sudo', '-n', 'kill', '-TERM', '--', f'-{proc.pid}'], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        subprocess.run(['sudo', '-n', 'kill', '-KILL', '--', f'-{proc.pid}'], check=False)
        proc.wait(timeout=10)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--runs', type=int, default=6)
    ap.add_argument('--iterations', type=int, default=50000)
    args = ap.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    assert not Path('/dev/shm/bpftime_maps_shm').exists()
    artifacts = [OUT / 'workloads' / v / n for v in VARIANTS for n in ('loader', 'victim', 'array_jit_poc.bpf.o')]
    artifacts += [BUILD / 'runtime/agent/libbpftime-agent.so', BUILD / 'runtime/syscall-server/libbpftime-syscall-server.so']
    hashes = {str(p): digest(p) for p in artifacts}
    meta = dict(status='running', started=datetime.datetime.now().astimezone().isoformat(), runs=args.runs,
                iterations=args.iterations, binary_sha256=hashes, cpu=5, frequency_samples=[])
    (out / 'environment.json').write_text(json.dumps(meta, indent=2) + '\n')
    base_env = {k: v for k, v in os.environ.items() if not k.startswith('BPFTIME_') and k != 'LD_PRELOAD'}
    selected = dict(BPFTIME_VM_NAME='llvm', BPFTIME_LOG_OUTPUT='console', SPDLOG_LEVEL='info')
    prefix = ['sudo', '-n', 'taskset', '-c', '5', 'env']
    rows, evidence = [], {}

    def command(env, path, *rest):
        return prefix + [f'{k}={v}' for k, v in env.items()] + [str(path), *map(str, rest)]

    def measure(engine, variant, round_no):
        meta['frequency_samples'].append(freq())
        wl = OUT / 'workloads' / variant
        dest = out / f'{round_no:02d}-{engine}-{variant}'
        dest.mkdir()
        loader_env, victim_env = dict(selected), dict(selected)
        if engine == 'bpftime':
            loader_env['LD_PRELOAD'] = str(BUILD / 'runtime/syscall-server/libbpftime-syscall-server.so')
            victim_env['LD_PRELOAD'] = str(BUILD / 'runtime/agent/libbpftime-agent.so')
        proc = target = None
        try:
            with (dest / 'loader.log').open('w') as log:
                proc = subprocess.Popen(command(loader_env if engine == 'bpftime' else {}, wl / 'loader'),
                                        cwd=OUT, env=base_env, stdout=log, stderr=subprocess.STDOUT,
                                        start_new_session=True)
                deadline = time.monotonic() + 40
                while 'Successfully started!' not in (dest / 'loader.log').read_text():
                    if proc.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError(f'Loader startup failed: {dest}')
                    time.sleep(.1)
                time.sleep(1)
                with (dest / 'victim.log').open('w') as tlog:
                    target = subprocess.Popen(command(victim_env if engine == 'bpftime' else {}, wl / 'victim',
                                                      args.iterations, round_no % 5),
                                              cwd=OUT, env=base_env, stdout=tlog, stderr=subprocess.STDOUT,
                                              start_new_session=True)
                    status = target.wait(timeout=180)
                    assert status == 0, (dest, status)
                assert proc.poll() is None, f'Loader exited during target: {dest}'
                stop(proc)
            text = (dest / 'victim.log').read_text()
            if engine == 'bpftime':
                assert 'shm_open_type 1' in text, dest
                typed = set(re.findall(TYPED + r' (\w+)', text))
                bound = re.findall(r'\[array-jit-inline\] program=(\w+) retained', text)
                # The typed hash helper needs every map in a program to be a hash
                # and no direct map-value LDDW. Only hit/mixed are compared; v3
                # binds them because their result map is an array.
                if variant == 'v1-hash-only':
                    assert {'hit', 'mixed'} <= typed and not bound, (dest, typed, bound)
                else:
                    assert not ({'hit', 'mixed'} & typed), (dest, typed)
                if variant == 'v3-hash-mixed':
                    assert {'hit', 'mixed'} <= set(bound), (dest, bound)
                elif variant == 'v2-hash-globals':
                    assert not ({'hit', 'mixed'} & set(bound)), (dest, bound)
                typed = sorted(typed)
                evidence[f'{round_no:02d}-{variant}'] = dict(typed_helper_programs=typed, bound_programs=bound)
            times = {}
            for line in text.splitlines():
                fields = line.split(',')
                if fields[0] in CASES and len(fields) == 5:
                    assert fields[0] not in times and int(fields[1]) == args.iterations
                    times[fields[0]] = (float(fields[3]), int(fields[4]))
            assert set(times) == set(CASES), (dest, times)
            expected_seed = (0x12345678 + (args.iterations - 1) * 0x9e3779b9) & 0xffffffff
            assert all(seed == expected_seed for _, seed in times.values()), dest
            observed = {}
            for line in (dest / 'loader.log').read_text().splitlines():
                m = re.fullmatch(r'RESULT slot=(\d+) seed=(\d+) checksum=(\d+) hits=(\d+) misses=(\d+)', line)
                if m:
                    slot, seed, total, hits, misses = map(int, m.groups())
                    observed[slot] = (seed, total, hits, misses)
            assert set(observed) == {0, 1, 2, 3}, (dest, observed)
            for slot in range(4):
                assert observed[slot] == (expected_seed, *checksum(expected_seed, 1023 if slot < 2 else 2047)), (dest, slot)
            for case, (ns, _) in times.items():
                assert ns > 0
                rows.append(dict(engine=engine, variant=variant, round=round_no, case=case, raw_ns_per_element=ns))
            meta['frequency_samples'].append(freq())
            print(f'Completed {engine} {variant} round {round_no}/{args.runs}; checksums passed', flush=True)
        finally:
            stop(target)
            stop(proc)
            if engine == 'bpftime':
                subprocess.run(['sudo', '-n', str(BUILD / 'tools/bpftimetool/bpftimetool'), 'remove'], cwd=OUT,
                               env=base_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

    try:
        arms = [(e, v) for v in VARIANTS for e in ('kernel', 'bpftime')]
        for i in range(args.runs):
            for engine, variant in (arms if i % 2 == 0 else list(reversed(arms))):
                measure(engine, variant, i + 1)
        assert hashes == {str(p): digest(p) for p in artifacts}, 'artifact changed'
        with (out / 'raw.csv').open('w') as stream:
            w = csv.DictWriter(stream, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)
        summary = []
        for variant in VARIANTS:
            for case in CASES:
                item = dict(variant=variant, case=case)
                for engine in ('kernel', 'bpftime'):
                    values = [r['raw_ns_per_element'] for r in rows
                              if r['variant'] == variant and r['case'] == case and r['engine'] == engine]
                    item[engine] = dict(median=statistics.median(values), mean=statistics.mean(values), raw_values=values)
                item['bpftime_vs_kernel'] = item['bpftime']['median'] / item['kernel']['median']
                summary.append(item)
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        (out / 'helper-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
        meta['status'] = 'success'
    except BaseException as error:
        meta.update(status='failed', error=repr(error))
        raise
    finally:
        meta['finished'] = datetime.datetime.now().astimezone().isoformat()
        (out / 'environment.json').write_text(json.dumps(meta, indent=2) + '\n')


if __name__ == '__main__':
    main()
