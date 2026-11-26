#!/usr/bin/env python3
"""J1 (ATC week 1): opensnoop gap on Orin. The E12 run-3 opensnoop cell, unchanged
(audit.py = byte copy of prep/E12/audit.py: libbpf-tools v0.37 opensnoop, workload openloop 4 s,
100 opens then 10 ms pause, /etc/hostname, unpinned, tool settle 3 s, 2 s after), plus counters:
 - K: kernel.bpf_stats_enabled=1 (set by the run script); run_cnt delta of every program the tool
   loaded (new prog ids after the tool starts), read before and after the workload; ringbuf
   max_entries of the tool's new maps.
 - all: perf stat -a -e syscalls:sys_exit_openat,syscalls:sys_exit_openat2 over the workload
   (system-wide; arm64 has no open(2)); openloop's own total_ops is the exact count of its opens.
Variants: K (as E12), Kpin (tool on cpu 3, workload on cpus 0,1,4,5), D (9ccda7e8, as E12).
Order per round: K, Kpin, D for rounds 0-4, then K only for rounds 5-9.
"""
import json, re, subprocess, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import audit as A

BT = '/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/bpftool'
PERF = '/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf'
SECS = 4

def bt(*args):
    r = A.run(['sudo', '-n', BT, '-j', *args])
    return json.loads(r.stdout) if r.stdout.strip() else []

def one(variant, out_dir, idx):
    backend = 'D' if variant == 'D' else 'kernel'
    A.cleanup(backend)
    before_p = {p['id'] for p in bt('prog', 'show')} if backend == 'kernel' else set()
    before_m = {m['id'] for m in bt('map', 'show')} if backend == 'kernel' else set()
    tool = ([ 'taskset', '-c', '3'] if variant == 'Kpin' else []) + [str(A.LT / 'opensnoop')]
    p = A.start_tool(tool, backend)
    time.sleep(3)
    progs, rb = {}, []
    if backend == 'kernel':
        progs = {x['id']: x for x in bt('prog', 'show') if x['id'] not in before_p}
        rb = [dict(id=m['id'], name=m.get('name'), max_entries=m.get('max_entries'))
              for m in bt('map', 'show') if m['id'] not in before_m and m.get('type') == 'ringbuf']
    rc0 = {i: progs[i].get('run_cnt', 0) for i in progs}
    pf = out_dir / f'{idx:02d}-{variant}.perf.txt'
    perf = subprocess.Popen(['sudo', '-n', PERF, 'stat', '-a', '-x,', '-o', str(pf), '-e',
                             'syscalls:sys_exit_openat,syscalls:sys_exit_openat2'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    time.sleep(0.5)
    wl = ([ 'taskset', '-c', '0,1,4,5'] if variant == 'Kpin' else []) + [str(A.WL / 'openloop'), str(SECS), '10000']
    t0 = time.time()
    rc, wout = A.workload(wl, backend, timeout=60, role='workload-syscall')
    wl_s = time.time() - t0
    time.sleep(0.5)
    A.run(['sudo', '-n', 'kill', '-INT', '--', f'-{perf.pid}'])
    perf.wait(timeout=10)
    time.sleep(1.5)                      # E12: 2 s after the workload before SIGINT
    rc1 = {}
    if backend == 'kernel':
        now = {x['id']: x for x in bt('prog', 'show')}
        rc1 = {i: now.get(i, {}).get('run_cnt', 0) for i in progs}
    A.run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
    tout = A.stop_tool(p, wait=10)
    A.cleanup(backend)
    counts = {}
    for line in Path(pf).read_text(errors='replace').splitlines() if Path(pf).exists() else []:
        f = line.split(',')
        if len(f) > 2 and f[2].startswith('syscalls:'):
            counts[f[2]] = int(f[0]) if f[0].isdigit() else None
    run_cnt = {f"{progs[i].get('name')}#{i}": rc1.get(i, 0) - rc0[i] for i in progs}
    row = dict(exp='J1', variant=variant, backend=backend, idx=idx, workload_rc=rc, workload_s=round(wl_s, 2),
               expected=A.expected_calls(wout, None, SECS),
               reported=len(re.findall(r'\bopenloop\b.*/etc/hostname', tout)),
               reported_openloop_any=len(re.findall(r'\bopenloop\b', tout)),
               lost=A.lost_events(tout), run_cnt=run_cnt, ringbuf=rb,
               sys_exit_openat_all=counts.get('syscalls:sys_exit_openat'),
               sys_exit_openat2_all=counts.get('syscalls:sys_exit_openat2'),
               t_end=time.time())
    (out_dir / f'{idx:02d}-{variant}.tool.txt').write_text(tout)
    (out_dir / f'{idx:02d}-{variant}.workload.txt').write_text(wout)
    return row

def main():
    out = Path(sys.argv[1]); raw = out.with_suffix(''); raw.mkdir(parents=True, exist_ok=True)
    order = []
    for r in range(10):
        order += ['K', 'Kpin', 'D'] if r < 5 else ['K']
    for idx, v in enumerate(order):
        try:
            row = one(v, raw, idx)
        except Exception as e:
            row = dict(exp='J1', variant=v, idx=idx, error=repr(e))
        with out.open('a') as f:
            f.write(json.dumps(row) + '\n')
        print(json.dumps(row), flush=True)

if __name__ == '__main__':
    main()
