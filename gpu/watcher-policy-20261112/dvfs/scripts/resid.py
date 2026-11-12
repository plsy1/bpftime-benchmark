#!/usr/bin/env python3
"""Residency of one run from OUT_PREFIX.resid (run_one.sh, before/after the victims):
resid.py PREFIX...  -> one line per run: CPU policy mean frequency (MHz) and share of time at
1728 MHz, CPU idle (c7 entries per s, share of CPU time in c7 and in WFI, all CPUs), GPU and EMC
mean frequency (MHz). As a module: resid(prefix) -> dict.

The window is the victims' whole life (agent and CUDA start-up included), not the timed loop.
The EMC value is bwmgr's devfreq trans_stat, which books time to the floor written to min_freq
(its governor never changes state); the BPMP sets the actual EMC rate, not recorded here."""
import re, sys


def parse(block):
    out = {"cpufreq": {}, "idle": {}, "gpu": {}, "emc": {}}
    sec = None
    for l in block.splitlines():
        m = re.match(r"## cpufreq (\S+)", l)
        if m:
            sec = ("cpufreq", m[1]); out["cpufreq"][m[1]] = {}; continue
        if l.startswith("## gpu"):
            sec = ("gpu",); continue
        if l.startswith("## emc"):
            sec = ("emc",); continue
        m = re.match(r"idle (cpu\d+) (\S+) (\d+) (\d+)", l)
        if m:
            out["idle"][(m[1], m[2])] = (int(m[3]), int(m[4])); continue
        if sec and sec[0] == "cpufreq":
            m = re.match(r"(\d+) (\d+)$", l)
            if m:
                out["cpufreq"][sec[1]][int(m[1])] = int(m[2]) * 10  # 10 ms units -> ms
        elif sec and sec[0] in ("gpu", "emc") and ":" in l and not l.strip().startswith("From") and not l.strip().startswith(":"):
            head, rest = l.split(":", 1)
            f = head.replace("*", "").strip()
            if not f.isdigit():
                continue
            chunks = [rest[i:i + 10] for i in range(0, len(rest.rstrip("\n")), 10)]
            out[sec[0]][int(f)] = int(chunks[-1])  # time (ms), the last 10-wide field
    return out


def resid(prefix):
    t = open(prefix + ".resid").read()
    parts = re.split(r"^== (before|after) (\d+)$", t, flags=re.M)
    blocks = {parts[i]: (int(parts[i + 1]), parse(parts[i + 2])) for i in range(1, len(parts), 3)}
    (t0, b), (t1, a) = blocks["before"], blocks["after"]
    wall_ms = (t1 - t0) / 1e6
    r = {"wall_s": wall_ms / 1000}
    for p in a["cpufreq"]:
        d = {f: a["cpufreq"][p][f] - b["cpufreq"][p].get(f, 0) for f in a["cpufreq"][p]}
        tot = sum(d.values()) or 1
        r[p + "_mhz"] = sum(f * v for f, v in d.items()) / tot / 1000
        r[p + "_atmax"] = d.get(max(d), 0) / tot
    c7n = c7t = wfit = 0
    ncpu = len({c for c, _ in a["idle"]})
    for k, (u, tm) in a["idle"].items():
        du, dt = u - b["idle"][k][0], tm - b["idle"][k][1]
        if k[1] == "c7":
            c7n += du; c7t += dt
        elif k[1] == "WFI":
            wfit += dt
    r["c7_per_s"] = c7n / (wall_ms / 1000)
    r["c7_share"] = c7t / 1000 / (wall_ms * ncpu)
    r["wfi_share"] = wfit / 1000 / (wall_ms * ncpu)
    for dev in ("gpu", "emc"):
        d = {f: a[dev][f] - b[dev].get(f, 0) for f in a[dev]}
        tot = sum(d.values())
        r[dev + ("_floor_mhz" if dev == "emc" else "_mhz")] = sum(f * v for f, v in d.items()) / tot / 1e6 if tot > 0 else float("nan")
    return r


if __name__ == "__main__":
    for p in sys.argv[1:]:
        r = resid(p)
        print(p.rsplit("/", 1)[-1], " ".join(f"{k}={v:.3g}" for k, v in r.items()))
