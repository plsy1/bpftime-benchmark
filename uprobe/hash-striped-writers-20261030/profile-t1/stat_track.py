#!/usr/bin/env python3
"""Single-thread tracking table (1,000 live entries): instructions per op on each build.
The loader (../../map-table-20261028/src/attach) attaches track.bpf.o to trackloop's
alloc_event/free_event; trackloop runs 1 thread for 12 s (it pins to cpu5); after 3 s
`perf stat` counts it for 4 s. Outputs <backend>.track.stat.txt and stat-track.jsonl."""
import json, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = Path('/home/jetson/src/results/uprobe/map-table-20261028/src')
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')
PERF = '/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf'
SECS = 12


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(b):
    for name in ('attach', 'trackloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    sh(['sudo', '-n', str(BUILDS / b / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


for b in ('c98951a4', 'stripe-view', 'stripe-line'):
    cleanup(b)
    res = dict(backend=b)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / b / "libbpftime-syscall-server.so"}',
                             'BPFTIME_LOG_OUTPUT=console', 'BPFTIME_SHM_MEMORY_MB=1024',
                             str(SRC / 'attach'), str(SRC / 'track.bpf.o'), str(SRC / 'trackloop'),
                             'on_alloc=alloc_event', 'on_free=free_event'],
                            stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.time() + 30
        while time.time() < deadline:
            out.seek(0)
            if 'attached' in out.read():
                break
            time.sleep(0.2)
        w = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / b / "libbpftime-agent.so"}',
                              'BPFTIME_LOG_OUTPUT=console', str(SRC / 'trackloop'), '1', str(SECS), '1000'],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        time.sleep(3)
        pid = sh(['pgrep', '-n', '-x', 'trackloop']).stdout.strip()
        st = sh(['sudo', '-n', PERF, 'stat', '-e', 'cycles,instructions,l1d_tlb_refill,l1i_cache_refill,stall_backend,stall_frontend',
                 '-p', pid, '--', 'sleep', '4'], timeout=60)
        (HERE / f'{b}.track.stat.txt').write_text(st.stderr)
        wout, _ = w.communicate(timeout=SECS + 60)
        m = re.search(r'ops_per_sec_median ([\d.]+)', wout)
        res['us_per_op'] = 1e6 / float(m.group(1)) if m else None
        ins = re.search(r'([\d,]+)\s+instructions', st.stderr)
        cyc = re.search(r'([\d,]+)\s+cycles', st.stderr)
        if m and ins:
            ops = float(m.group(1)) * 4
            res['instructions_per_op'] = int(ins.group(1).replace(',', '')) / ops
            res['cycles_per_op'] = int(cyc.group(1).replace(',', '')) / ops
    finally:
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
        try:
            tool.wait(timeout=20)
        except Exception:
            pass
        cleanup(b)
    with open(HERE / 'stat-track.jsonl', 'a') as f:
        f.write(json.dumps(res) + '\n')
    print(json.dumps(res), flush=True)
