#!/usr/bin/env python3
"""Where does bpftime's probe time go as threads are added? (problem 2: the probe path without
map operations slows from 0.2 us at 1 thread to 2-4.5 us at 4-6 threads.)

Per configuration: the bpftime loader attaches count-control.bpf.o (no map operation) from
../map-table-20261028 to count_event of countloop; countloop runs THREADS threads on CPUS with
one key; after 3 s warm-up `perf record` samples the workload for 4 s (cycles, user and kernel,
frame-pointer call graphs), then `perf stat` counts for 3 s. The loader's hit count is checked
against the workload's calls. Outputs per configuration: <name>.perf.data, <name>.report.txt
(flat, by symbol), <name>.callers.txt (callers of the hottest symbols), <name>.stat.txt, and one
JSON line in results.jsonl.
"""
import json, re, subprocess, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
TABLE = HERE.parent / 'map-table-20261028' / 'src'
BUILD = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/09421773')
PERF = '/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf'
CONFIGS = [('t1', 1, '5'), ('t2', 2, '5,4'), ('t4-cross', 4, '5,4,3,2'), ('t4-cluster0', 4, '0,1,2,3'),
           ('t6', 6, '5,4,3,2,1,0')]
SECS = 12


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup():
    for name in ('attach', 'countloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    sh(['sudo', '-n', str(BUILD / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def run(name, threads, cpus):
    cleanup()
    res = dict(name=name, threads=threads, cpus=cpus)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={BUILD / "libbpftime-syscall-server.so"}',
                             'BPFTIME_LOG_OUTPUT=console', 'BPFTIME_SHM_MEMORY_MB=256',
                             str(TABLE / 'attach'), str(TABLE / 'count-control.bpf.o'),
                             str(TABLE / 'countloop'), 'on_count=count_event'],
                            stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.time() + 30
        while time.time() < deadline:
            out.seek(0)
            if 'attached' in out.read():
                break
            time.sleep(0.2)
        else:
            raise RuntimeError('loader did not attach')
        w = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={BUILD / "libbpftime-agent.so"}',
                              'BPFTIME_LOG_OUTPUT=console', str(TABLE / 'countloop'), str(threads),
                              str(SECS), '1', cpus], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors='replace')
        time.sleep(3)
        pid = sh(['pgrep', '-n', '-x', 'countloop']).stdout.strip()
        data = HERE / f'{name}.perf.data'
        sh(['sudo', '-n', PERF, 'record', '-F', '1999', '-g', '-p', pid, '-o', str(data), '--', 'sleep', '4'],
           timeout=60)
        st = sh(['sudo', '-n', PERF, 'stat', '-e',
                 'cycles,instructions,cache-references,cache-misses,stalled-cycles-backend',
                 '-p', pid, '--', 'sleep', '3'], timeout=60)
        (HERE / f'{name}.stat.txt').write_text(st.stderr)
        wout, _ = w.communicate(timeout=SECS + 60)
        m = re.search(r'ops_per_sec_median ([\d.]+)', wout)
        res['calls_per_sec'] = float(m.group(1)) if m else None
        res['us_per_call_per_thread'] = threads / res['calls_per_sec'] * 1e6 if m else None
        total = re.search(r'total_ops (\d+)', wout)
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
        tool.wait(timeout=20)
        out.seek(0)
        hits = re.search(r'sum0 (\d+)', out.read())
        res['ok'] = bool(total and hits and total.group(1) == hits.group(1))
        sh(['sudo', '-n', 'chown', 'jetson:jetson', str(data)])
        rep = sh([PERF, 'report', '-i', str(data), '--no-children', '--sort', 'dso,sym', '--stdio',
                  '-g', 'none', '--percent-limit', '0.5'], timeout=300)
        (HERE / f'{name}.report.txt').write_text(rep.stdout)
        cal = sh([PERF, 'report', '-i', str(data), '--no-children', '--sort', 'sym', '--stdio',
                  '-g', 'caller,0.5,callee', '--percent-limit', '5'], timeout=300)
        (HERE / f'{name}.callers.txt').write_text(cal.stdout)
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        cleanup()
    return res


if __name__ == '__main__':
    only = set(sys.argv[1:])
    for name, threads, cpus in CONFIGS:
        if only and name not in only:
            continue
        r = run(name, threads, cpus)
        with open(HERE / 'results.jsonl', 'a') as f:
            f.write(json.dumps(r) + '\n')
        print(json.dumps(r), flush=True)
