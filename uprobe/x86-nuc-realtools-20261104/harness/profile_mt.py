#!/usr/bin/env python3
"""perf profiles of real tools on bpftime at a thread count (2026-11-04): starts the tool as
audit.py does, runs the multi-thread workload under `perf record -e cycles:u` (perf itself
without the agent: LD_PRELOAD goes to the workload only), writes the top symbols.
Usage: profile_mt.py --tool T --backend B --threads N --out DIR [--secs S]
"""
import argparse, subprocess, time
from pathlib import Path
import audit
import measure_mt

PERF = '/usr/bin/perf'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tool', required=True)
    ap.add_argument('--backend', required=True)
    ap.add_argument('--threads', type=int, required=True)
    ap.add_argument('--secs', type=int, default=6)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    audit.WL = measure_mt.MT / f't{a.threads}'
    stem = f'{a.tool}.{a.backend}.t{a.threads}'
    data = out / f'{stem}.perf.data'
    cmd = measure_mt.BASE[a.tool](a.secs)
    agent = audit.BUILDS / a.backend / 'libbpftime-agent.so'
    perf_cmd = ['sudo', '-n', PERF, 'record', '-e', 'cpu_core/cycles/u', '-o', str(data), '--',
                'env', f'LD_PRELOAD={agent}', 'BPFTIME_LOG_OUTPUT=console',
                str(audit.WL / cmd[0])] + cmd[1:]
    audit.cleanup(a.backend)
    if a.tool == 'memleak':
        raise SystemExit('memleak forks its workload from the tool; profile it separately')
    tool_cmd = {
        'malloc-example': [str(audit.EX / 'malloc-core' / 'malloc')],
        'funclatency-libbpf': [str(audit.LT / 'funclatency'), '-u', '-d', str(a.secs + 6),
                               f'{audit.WL}/callloop:target_func'],
        'funclatency-bpftime-example': [str(audit.EX / 'tracing' / 'funclatency-core' / 'funclatency'),
                                        '-u', '-d', str(a.secs + 6), f'{audit.WL}/callloop:target_func'],
    }[a.tool]
    p = audit.start_tool(tool_cmd, a.backend)
    time.sleep(3)
    r = subprocess.run(perf_cmd, capture_output=True, text=True, timeout=a.secs + 60)
    (out / f'{stem}.workload.txt').write_text(r.stdout + r.stderr)
    subprocess.run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
    audit.stop_tool(p, wait=5)
    audit.cleanup(a.backend)
    subprocess.run(['sudo', '-n', 'chown', 'y:y', str(data)])
    rep = subprocess.run([PERF, 'report', '-i', str(data), '--stdio', '--sort', 'symbol',
                          '-F', 'overhead,sample,symbol'], capture_output=True, text=True)
    lines = [l for l in rep.stdout.splitlines() if l.strip() and not l.startswith('#')]
    (out / f'{stem}.symbols.txt').write_text('\n'.join(l[:160] for l in lines[:40]) + '\n')
    print(stem, audit.ops(r.stdout))

if __name__ == '__main__':
    main()
