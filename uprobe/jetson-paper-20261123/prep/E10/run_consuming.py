#!/usr/bin/env python3
"""E10 consuming array lookup (paper campaign). One file for both runners of
results/uprobe/consuming-official-vs-optimized-20261021 (run-ordinary.py, run-percpu.py; they differ
only in DIAG, the object name and the per-CPU RESULT_CPU check, selected here with --kind).

Changes against the 10-21 runners (logic, checks, order and output format otherwise the same):
- engines kernel / O / D (10-21: kernel / previous = official 177a1dc / inline = dev 5466f0d);
  builds are the frozen flat directories builds/<arm>/ (agent, syscall server, bpftimetool);
- workloads from workloads/E10 (E10-build.sh; the 10-21 tree is gone), used as cwd (ROOT) because
  the .bpf.o's uprobe path is relative;
- BPFTIME_VM_NAME is no longer set (plan 1.5; the default VM is llvm in both builds);
- the D markers ("[array-jit-inline] inlined", "program=") are looked for in the loader's and the
  victim's logs together: with the AOT cache (D default) the syscall server compiles, and inlines,
  at BPF_PROG_LOAD, so the line may be in the loader's log only. O must print no "[array-jit" in either;
- poc/off engines and --dump-jit removed; bpftimetool of the arm just measured.
"""
import argparse, csv, datetime, hashlib, json, os, re, statistics, subprocess, time
from pathlib import Path

R = Path.home() / 'src/paper-runs-20261124'
ROOT = R / 'workloads/E10'
BUILDS = R / 'builds'
CASES = ('control_hit', 'hit', 'control_mixed', 'mixed', 'unused')
KIND = {'ordinary': ('array-jit-poc', 'array_jit_poc.bpf.o'),
        'percpu': ('percpu-array-jit', 'percpu_array_jit.bpf.o')}


def checksum(seed, mask):
    state = seed; total = hits = misses = 0
    for _ in range(1000):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        key = (state >> 16) & mask
        if key < 1024: total += key * 17 + 5; hits += 1
        else: total += 0x9e3779b9; misses += 1
    return total, hits, misses


def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()


def freq():
    p = Path('/sys/devices/system/cpu/cpu5/cpufreq')
    values = {k: int((p / f'scaling_{k}_freq').read_text()) for k in ('min', 'max', 'cur')}
    assert values == dict(min=1728000, max=1728000, cur=1728000), values
    return values


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--kind', choices=tuple(KIND), required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--runs', type=int, default=6)
    ap.add_argument('--iterations', type=int, default=50000)
    ap.add_argument('--engines', nargs='+', choices=('kernel', 'O', 'D'), default=['kernel', 'O', 'D'])
    args = ap.parse_args()
    dirname, objname = KIND[args.kind]
    DIAG = ROOT / 'benchmark/uprobe/.output' / dirname
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    assert not Path('/dev/shm/bpftime_maps_shm').exists()
    artifacts = [DIAG / name for name in ('loader', 'victim', objname)]
    artifacts += [BUILDS / e / n for e in args.engines if e != 'kernel'
                  for n in ('libbpftime-agent.so', 'libbpftime-syscall-server.so', 'bpftimetool')]
    hashes = {str(p): digest(p) for p in artifacts}
    meta = dict(status='running', started=datetime.datetime.now().astimezone().isoformat(), kind=args.kind,
                runs=args.runs, iterations=args.iterations, binary_sha256=hashes, cpu=5, frequency_samples=[],
                workload='runtime LCG keys; value checksum; live maps; 1000 elements/invocation')
    (out / 'environment.json').write_text(json.dumps(meta, indent=2) + '\n')
    rows = []
    base_env = {k: v for k, v in os.environ.items() if not k.startswith('BPFTIME_') and k != 'LD_PRELOAD'}
    base_env.update(BPFTIME_LOG_OUTPUT='console', SPDLOG_LEVEL='info')
    prefix = ['sudo', '-n', 'taskset', '-c', '5']
    def command(env, path, *rest):
        return prefix + ['env', *[f'{k}={v}' for k, v in env.items()
                                  if k in ('BPFTIME_LOG_OUTPUT', 'SPDLOG_LEVEL', 'LD_PRELOAD')], str(path), *map(str, rest)]
    def cleanup(proc):
        if proc.poll() is None:
            subprocess.run(['sudo', '-n', 'kill', '-TERM', '--', f'-{proc.pid}'], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try: proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                subprocess.run(['sudo', '-n', 'kill', '-KILL', '--', f'-{proc.pid}'], check=False)
                proc.wait(timeout=10)
    def measure(engine, round_no):
        meta['frequency_samples'].append(freq())
        dest = out / f'{engine}-{round_no:02d}'; dest.mkdir()
        loader_env = dict(base_env); victim_env = dict(base_env)
        if engine != 'kernel':
            loader_env['LD_PRELOAD'] = str(BUILDS / engine / 'libbpftime-syscall-server.so')
            victim_env['LD_PRELOAD'] = str(BUILDS / engine / 'libbpftime-agent.so')
        proc = None
        try:
            with (dest / 'loader.log').open('w') as log:
                proc = subprocess.Popen(command(loader_env, DIAG / 'loader'), cwd=ROOT, env=base_env,
                                        stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                deadline = time.monotonic() + 40
                while 'Successfully started!' not in (dest / 'loader.log').read_text():
                    if proc.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError(f'Loader startup failed: {dest}')
                    time.sleep(.1)
                time.sleep(1)
                with (dest / 'victim.log').open('w') as target_log:
                    target = subprocess.Popen(command(victim_env, DIAG / 'victim', args.iterations, round_no % 5),
                                              cwd=ROOT, env=base_env, stdout=target_log, stderr=subprocess.STDOUT,
                                              start_new_session=True)
                    try: status = target.wait(timeout=180)
                    finally: cleanup(target)
                    assert status == 0, (dest, status)
                assert proc.poll() is None, f'Loader exited during target: {dest}'
                cleanup(proc)
            text = (dest / 'victim.log').read_text()
            both = (dest / 'loader.log').read_text() + text
            if engine != 'kernel': assert 'shm_open_type 1' in text, dest
            if engine == 'O': assert '[array-jit' not in both, dest       # official has no array JIT
            if engine == 'D':
                assert '[array-jit-inline] inlined ' in both, dest
                assert '[array-jit-inline] program=' in both, dest
            times = {}
            for line in text.splitlines():
                fields = line.split(',')
                if fields[0] in CASES and len(fields) == 5:
                    assert fields[0] not in times
                    assert int(fields[1]) == args.iterations
                    times[fields[0]] = (float(fields[3]), int(fields[4]))
            assert set(times) == set(CASES), times
            expected_seed = (0x12345678 + (args.iterations - 1) * 0x9e3779b9) & 0xffffffff
            assert all(seed == expected_seed for _, seed in times.values())
            observed = {}
            ltext = (dest / 'loader.log').read_text()
            for line in ltext.splitlines():
                m = re.fullmatch(r'RESULT slot=(\d+) seed=(\d+) checksum=(\d+) hits=(\d+) misses=(\d+)', line)
                if m:
                    slot, seed, total, hits, misses = map(int, m.groups())
                    observed[slot] = (seed, total, hits, misses)
            assert set(observed) == {0, 1, 2, 3}, (dest, observed)
            for slot in range(4):
                assert observed[slot] == (expected_seed, *checksum(expected_seed, 1023 if slot < 2 else 2047)), \
                    (dest, slot, observed[slot])
            if args.kind == 'percpu':
                cpu_rows = re.findall(r'RESULT_CPU slot=(\d+) cpu=(\d+)', ltext)
                assert sorted(cpu_rows) == [(str(i), '5') for i in range(4)], (dest, cpu_rows)
            (dest / 'checksum-check.json').write_text(json.dumps(dict(status='passed', observed=observed), indent=2) + '\n')
            for case, (ns, seed) in times.items():
                assert ns > 0
                rows.append(dict(engine=engine, round=round_no, case=case, raw_ns_per_element=ns, last_seed=seed))
            meta['frequency_samples'].append(freq())
            print(f'Completed {engine} round {round_no}/{args.runs}; all four checksums/counts passed', flush=True)
        finally:
            if proc is not None: cleanup(proc)
            if engine != 'kernel':
                subprocess.run(['sudo', '-n', str(BUILDS / engine / 'bpftimetool'), 'remove'],
                               cwd=ROOT, env=base_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    try:
        for i in range(args.runs):
            order = ['kernel', 'O', 'D'] if i % 2 == 0 else ['D', 'O', 'kernel']
            for engine in order:
                if engine in args.engines: measure(engine, i + 1)
        assert hashes == {str(p): digest(p) for p in artifacts}
        with (out / 'raw.csv').open('w') as stream:
            w = csv.DictWriter(stream, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
        summary = []
        for case in CASES:
            item = dict(case=case, not_a_gain_metric=(case == 'unused'))
            for engine in args.engines:
                values = [r['raw_ns_per_element'] for r in rows if r['case'] == case and r['engine'] == engine]
                item[engine] = dict(median=statistics.median(values), mean=statistics.mean(values), raw_values=values)
            if 'D' in item and 'O' in item:
                item['D_vs_O_percent'] = 100 * (item['D']['median'] / item['O']['median'] - 1)
                item['paired_wins_D_vs_O'] = sum(a < b for a, b in zip(item['D']['raw_values'], item['O']['raw_values']))
            for e in ('O', 'D'):
                if e in item and 'kernel' in item:
                    item[f'{e}_vs_kernel'] = item[e]['median'] / item['kernel']['median']
            summary.append(item)
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(json.dumps(summary, indent=2), flush=True)
        meta['status'] = 'success'
    except BaseException as error:
        meta.update(status='failed', error=repr(error)); raise
    finally:
        meta['finished'] = datetime.datetime.now().astimezone().isoformat()
        (out / 'environment.json').write_text(json.dumps(meta, indent=2) + '\n')


if __name__ == '__main__': main()
