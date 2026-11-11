#!/usr/bin/env python3
"""Summarize the WFE-under-load rounds: summarize.py LOG_DIR [ROUNDS...]

Per cell (load, spin_us, probe) and set (P N W W0):
- load: worker iterations/s inside the victim's timed window (from the 10 ms samples), and the
  ratio to P of the same cell and round (median over rounds, with min-max)
- victim process CPU seconds per wall second in the window (getrusage; includes the watcher)
- per-launch time (launch + sync), us: median of the per-run medians, and of the per-run means
- host-call latency (GPU-timed, us), all timed calls of all rounds pooled (the warm-up launch's
  call dropped): mean, median, p90, p99, max, share above 3 ms
Validity per run goes to verify.csv: rcs, record count = launches + 1, clocks unchanged.
"""
import csv, glob, os, re, statistics as st, sys

D = sys.argv[1]
rounds = sys.argv[2:] or sorted(os.path.basename(p)[1:] for p in glob.glob(f"{D}/r*") if os.path.isdir(p))
SETS = ["P", "N", "W", "W0"]
cells = [(ld, d, pr) for ld in ("on", "off") for d in ("2000", "20000") for pr in ("host", "none")]


def victim(p):
    t = open(p).read()
    w = re.search(r"window_ns=(\d+) (\d+)", t)
    pl = re.search(r"per_launch_us mean=([\d.]+) median=([\d.]+) p90=([\d.]+) max=([\d.]+)", t)
    c = re.search(r"cpu_s=([\d.]+) wall_s=([\d.]+)", t)
    fl = re.search(r"device_flags=(0x[0-9a-f]+)", t)
    if not (w and pl and c):
        return None
    return dict(s=int(w[1]), e=int(w[2]), mean=float(pl[1]), median=float(pl[2]),
                cpu=float(c[1]), wall=float(c[2]), flags=fl[1] if fl else "?")


def load_rate(p, s, e):
    pts = []
    for line in open(p):
        a = line.split()
        if len(a) == 2:
            t, n = int(a[0]), int(a[1])
            if s <= t <= e:
                pts.append((t, n))
    if len(pts) < 2:
        return None
    return (pts[-1][1] - pts[0][1]) / ((pts[-1][0] - pts[0][0]) / 1e9)


def lat(p):
    t = open(p).read()
    m = re.search(r"count=(\d+)", t)
    v = t.split("ns=")[1].strip().split(",") if "ns=" in t else []
    # record 1 is the warm-up launch, outside the timed window: dropped
    return (int(m[1]) if m else -1), [int(x) / 1000 for x in v if x][1:]


def env(p):
    t = open(p).read()
    def g(k):
        m = re.search(k + r" rc=(\d+)", t)
        return int(m[1]) if m else None
    clk = re.findall(r"cpu5=(\d+) gpu=(\d+)", t)
    temps = [int(x) for m in re.findall(r"temps=([\d,]+)", t) for x in m.split(",") if x]
    return dict(victim=g("victim"), loader=g("loader"), load=g("load"),
                clocks_ok=all(c == ("1728000", "1020000000") for c in clk) and len(clk) == 2,
                tmax=max(temps) / 1000 if temps else None, note="not attached" in t)


rows, ver = {}, []
for r in rounds:
    for (ld, d, pr) in cells:
        for s in SETS:
            base = f"{D}/r{r}/{ld}_{d}_{pr}_{s}"
            if not os.path.exists(base + ".env"):
                continue
            launches = 4000 if d == "2000" else 400
            v = victim(base + ".victim")
            e = env(base + ".env")
            rate = load_rate(base + ".load", v["s"], v["e"]) if (v and ld == "on") else None
            cnt, ls = lat(base + ".loader") if s != "P" else (None, [])
            ok = bool(v is not None and e["victim"] == 0 and e["clocks_ok"] and not e["note"]
                  and (s == "P" or (e["loader"] == 0 and cnt == launches + 1))
                  and (ld == "off" or (e["load"] == 0 and rate)))
            ver.append(dict(round=r, load=ld, spin_us=d, probe=pr, set=s, ok=ok,
                            victim_rc=e["victim"], loader_rc=e["loader"], load_rc=e["load"],
                            records=cnt, clocks_ok=e["clocks_ok"], tmax_c=e["tmax"],
                            flags=v["flags"] if v else None))
            if ok:
                rows.setdefault((ld, d, pr, s), []).append(dict(r=r, v=v, rate=rate, lat=ls))

with open(f"{D}/verify.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(ver[0].keys()))
    w.writeheader(); w.writerows(ver)


def q(v, p):
    return v[min(len(v) - 1, int(len(v) * p))]


out = []
out.append(f"Runs: {len(ver)}, valid {sum(x['ok'] for x in ver)}; rounds {','.join(rounds)}; "
           f"max temperature {max(x['tmax_c'] for x in ver if x['tmax_c']):.1f} C; "
           f"device flags seen {sorted(set(x['flags'] for x in ver if x['flags']))}\n")
for (ld, d, pr) in cells:
    out.append(f"\n### load {ld}, spin {d} us per launch, probe {'host call at exit' if pr == 'host' else 'no host call'}\n")
    hdr = "| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) |"
    sep = "|---|---:|---:|---:|---:|"
    if ld == "on":
        hdr += " load (M iter/s) | load vs P (median, min-max) |"; sep += "---:|---:|"
    if pr == "host":
        hdr += " host call mean (us) | host call median / p90 / p99 / max (us) | calls > 3 ms |"; sep += "---:|---:|---:|"
    out += [hdr, sep]
    for s in SETS:
        R = rows.get((ld, d, pr, s), [])
        if not R:
            out.append(f"| {s} | 0 |"); continue
        cpu = st.median(x["v"]["cpu"] / x["v"]["wall"] for x in R)
        med = st.median(x["v"]["median"] for x in R)
        mean = st.median(x["v"]["mean"] for x in R)
        line = f"| {s} | {len(R)} | {cpu:.3f} | {med:,.1f} | {mean:,.1f} |"
        if ld == "on":
            rate = st.median(x["rate"] for x in R) / 1e6
            P = {x["r"]: x["rate"] for x in rows.get((ld, d, pr, "P"), [])}
            ratios = sorted(x["rate"] / P[x["r"]] for x in R if x["r"] in P)
            rr = f"{st.median(ratios):.3f} ({ratios[0]:.3f}-{ratios[-1]:.3f})" if ratios else "-"
            line += f" {rate:,.2f} | {rr} |"
        if pr == "host":
            if s == "P":
                line += " - | - | - |"
            else:
                L = sorted(v for x in R for v in x["lat"])
                line += (f" {st.mean(L):,.1f} | {st.median(L):,.1f} / {q(L, .9):,.1f} / {q(L, .99):,.1f} / {L[-1]:,.1f} |"
                         f" {100 * sum(x > 3000 for x in L) / len(L):.1f}% |")
        out.append(line)
open(f"{D}/summary.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))
