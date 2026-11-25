#!/usr/bin/env python3
"""Spin the given CPUs at once for 4 s, then 3 short bursts after 1 s idle; read each
CPU's real busy MHz from APERF/MPERF (root, msr module). Exit 1 if any reading is more
than 3% from its expected kHz. Usage: busy-check.py <tsc-mhz> <cpu>:<khz>...
"""
import os, struct, sys, time
tsc = float(sys.argv[1])
want = {int(a.split(':')[0]): int(a.split(':')[1]) // 1000 for a in sys.argv[2:]}
def rd(c, reg):
    with open(f'/dev/cpu/{c}/msr', 'rb') as f:
        f.seek(reg); return struct.unpack('<Q', f.read(8))[0]
def spin(secs):
    kids = []
    for c in want:
        pid = os.fork()
        if pid == 0:
            os.sched_setaffinity(0, {c}); e = time.monotonic() + secs
            while time.monotonic() < e: pass
            os._exit(0)
        kids.append(pid)
    return kids
def measure(secs, settle):
    kids = spin(secs)
    time.sleep(settle)
    a0 = {c: (rd(c, 0xe8), rd(c, 0xe7)) for c in want}
    time.sleep(secs - settle - 0.02)
    out = {c: tsc * (rd(c, 0xe8) - a0[c][0]) / (rd(c, 0xe7) - a0[c][1]) for c in want}
    for k in kids: os.waitpid(k, 0)
    return out
rows = [measure(4.0, 1.0)]
for _ in range(3):
    time.sleep(1.0)
    rows.append(measure(0.25, 0.05))
ok = all(abs(r[c] - want[c]) <= 0.03 * want[c] for r in rows for c in want)
for r in rows:
    print(' '.join(f'cpu{c}={r[c]:.0f}MHz' for c in want))
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
