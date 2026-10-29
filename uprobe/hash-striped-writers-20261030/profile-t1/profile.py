#!/usr/bin/env python3
"""Single-thread churn: where the striped build's extra time goes. Per backend, the loader
(../../map-threads-20261028/src/churn) attaches churn-k4-v8.bpf.o to churnloop's target_func;
churnloop runs 1 thread on cpu5 for 12 s; after 3 s `perf record` samples it for 4 s.
Outputs <backend>.perf.data, <backend>.report.txt (flat by symbol), results.jsonl."""
import json, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = Path('/home/jetson/src/results/uprobe/map-threads-20261028/src')
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')
PERF = '/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf'
SECS = 12


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(b):
    for name in ('churn', 'churnloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    sh(['sudo', '-n', str(BUILDS / b / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


for b in ('c98951a4', 'stripe-tokens'):
    cleanup(b)
    res = dict(backend=b)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / b / "libbpftime-syscall-server.so"}',
                             'BPFTIME_LOG_OUTPUT=console', 'BPFTIME_SHM_MEMORY_MB=256',
                             str(SRC / 'churn'), str(SRC / 'churn-k4-v8.bpf.o'), str(SRC / 'churnloop'),
                             'target_func'], stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.time() + 30
        while time.time() < deadline:
            out.seek(0)
            if 'attached' in out.read():
                break
            time.sleep(0.2)
        w = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / b / "libbpftime-agent.so"}',
                              'BPFTIME_LOG_OUTPUT=console', str(SRC / 'churnloop'), '1', str(SECS), '5'],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        time.sleep(3)
        pid = sh(['pgrep', '-n', '-x', 'churnloop']).stdout.strip()
        data = HERE / f'{b}.perf.data'
        sh(['sudo', '-n', PERF, 'record', '-F', '4999', '-p', pid, '-o', str(data), '--', 'sleep', '4'],
           timeout=60)
        wout, _ = w.communicate(timeout=SECS + 60)
        m = re.search(r'ops_per_sec_median ([\d.]+)', wout)
        res['us_per_call'] = 1e6 / float(m.group(1)) if m else None
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
        tool.wait(timeout=20)
        out.seek(0)
        h = re.search(r'hits (\d+)', out.read())
        n = re.search(r'total_ops (\d+)', wout)
        res['ok'] = bool(h and n and h.group(1) == n.group(1))
        sh(['sudo', '-n', 'chown', 'jetson:jetson', str(data)])
        rep = sh([PERF, 'report', '-i', str(data), '--no-children', '--sort', 'dso,sym', '--stdio',
                  '--percent-limit', '0.3'], timeout=300)
        (HERE / f'{b}.report.txt').write_text(rep.stdout)
    finally:
        cleanup(b)
    with open(HERE / 'results.jsonl', 'a') as f:
        f.write(json.dumps(res) + '\n')
    print(json.dumps(res), flush=True)
