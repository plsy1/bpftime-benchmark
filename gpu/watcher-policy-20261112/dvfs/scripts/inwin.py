# In-window CPU frequency from tegrastats (.power, 100 ms samples), the window summarize.py uses for
# power (whole seconds inside the victim's realtime window); no-load cells only. Run from the policy
# work directory.
import re, datetime, statistics as st, sys
sys.path.insert(0, ".")
SETS = "P W D AC3 M100 AC2 AC4".split()
def victim(p):
    t = open(p).read(); rw = re.search(r"realtime_ns=(\d+) (\d+)", t); return int(rw[1]), int(rw[2])
def inwin(base):
    rs, re_ = victim(base + ".1.victim")
    lo, hi = rs // 10**9 + 1, re_ // 10**9 - 1
    f0, f4, u = [], [], []
    for l in open(base + ".power"):
        m = re.match(r"(\d\d-\d\d-\d{4} \d\d:\d\d:\d\d)", l)
        if not m or not (lo <= datetime.datetime.strptime(m[1], "%m-%d-%Y %H:%M:%S").timestamp() <= hi): continue
        c = re.search(r"CPU \[([^\]]+)\]", l)
        if not c: continue
        cores = [x.split("@") for x in c[1].split(",")]
        f0.append(int(cores[0][1])); f4.append(int(cores[4][1]))
        u.append(sum(int(x[0].rstrip("%")) for x in cores))
    return st.mean(f0), st.mean(f4), st.mean(u), len(f0)
for k in ("on", "off"):
    print("==", k)
    for c in ("c02", "c06", "c11", "c25"):
        for s in SETS:
            v = [inwin(f"logs/dvfs/{k}/r{r}/{c}_{s}") for r in range(1, 5)]
            print(f"  {c} {s:5} policy0 {min(x[0] for x in v):.0f}-{max(x[0] for x in v):.0f} policy4 {min(x[1] for x in v):.0f}-{max(x[1] for x in v):.0f} busy%sum {st.median(x[2] for x in v):.0f} n {min(x[3] for x in v)}")
