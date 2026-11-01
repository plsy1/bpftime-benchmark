#!/usr/bin/env python3
"""Spin on the given CPU for 2 s, then do 3 short bursts after 1 s idle each,
and report scaling_cur_freq. With an expected kHz, exit 1 if any reading is
more than 2% away (the bursts catch slow ramps after deep idle states)."""
import os, sys, time, threading
cpu = int(sys.argv[1])
os.sched_setaffinity(0, {cpu})
seen = []
def watch():
    for _ in range(8):
        time.sleep(0.2)
        seen.append(int(open(f'/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_cur_freq').read()))
t = threading.Thread(target=watch); t.start()
end = time.monotonic() + 2.0
while time.monotonic() < end:
    pass
t.join()
bursts = []
for _ in range(3):
    time.sleep(1.0)
    t0 = time.monotonic()
    while time.monotonic() - t0 < 0.2:
        pass
    bursts.append(int(open(f'/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_cur_freq').read()))
print(f'cpu{cpu} busy scaling_cur_freq kHz: {seen}; after-idle bursts: {bursts}')
seen += bursts
if len(sys.argv) > 2:
    want = int(sys.argv[2])
    sys.exit(0 if all(abs(f - want) <= want * 0.02 for f in seen) else 1)
