#!/usr/bin/env python3
"""Standalone supporting measurements, not an additive production cost model."""
from pathlib import Path
import csv
import os
import re
import subprocess
import time

root = Path(__file__).resolve().parent
while not (root/'resume-exit-status.txt').exists() or (root/'resume-exit-status.txt').read_text().strip() != '0':
    # Launched once by tmux after production sampling; bounded prerequisite wait.
    if time.monotonic() - globals().setdefault('started', time.monotonic()) > 180:
        raise RuntimeError('Production captures did not complete successfully')
    time.sleep(2)
subprocess.run(['python3', str(root/'build-layers.py')], check=True)
env = dict(os.environ, SPDLOG_LEVEL='off', BPFTIME_VM_NAME='llvm')
cases = {
    'array': ('array-map-path-layers', ['control', 'l0', 'l1', 'l2', 'l3', 'l4']),
    'percpu_array': ('per-cpu-array-path-layers', ['control', 'fixed', 'sched', 'callable', 'l0', 'l1', 'l2', 'l3', 'l4']),
}
logs = root/'layers/logs'
logs.mkdir(exist_ok=True)
with (root/'layers/measurements.csv').open('w') as f:
    writer = csv.DictWriter(f, fieldnames=['case', 'round', 'position', 'layer', 'operations', 'ns_per_op'])
    writer.writeheader()
    for round_no in range(1, 7):
        case_order = list(cases) if round_no % 2 else list(reversed(cases))
        for case in case_order:
            target, layers = cases[case]
            offset = (round_no-1) % len(layers)
            order = layers[offset:] + layers[:offset]
            if round_no % 2 == 0:
                order.reverse()
            for position, layer in enumerate(order):
                freq = Path('/sys/devices/system/cpu/cpu5/cpufreq')
                assert all(int((freq/f'scaling_{k}_freq').read_text()) == 1728000 for k in ['min', 'max', 'cur'])
                cmd = [str(root/'layers/bin'/target), layer, 'lookup', '20000000', '1', '5']
                p = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
                (logs/f'{case}-{round_no}-{layer}.log').write_text(p.stdout)
                p.check_returncode()
                matches = re.findall(r'round=1 elapsed_ns=\d+ ns_per_op=([0-9.]+)', p.stdout)
                assert len(matches) == 1, p.stdout
                writer.writerow(dict(case=case, round=round_no, position=position, layer=layer, operations=20000000, ns_per_op=matches[0]))
                f.flush()
        print(f'Layer round {round_no}/6 completed', flush=True)
(root/'layers/exit-status.txt').write_text('0\n')
