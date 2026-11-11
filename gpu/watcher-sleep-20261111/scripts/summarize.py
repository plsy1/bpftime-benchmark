#!/usr/bin/env python3
"""Summarize the short-sleep rounds: summarize.py LOG_DIR [ROUNDS...]

Per cell and set: victim CPU s per wall s (getrusage, includes the watcher), per-launch mean
and median (us; median of the per-run values), load throughput vs P of the same round (load on),
power VDD_CPU_GPU_CV and VDD_IN (mW, tegrastats samples of whole seconds inside the window,
load off), host-call latency pooled over rounds (GPU-timed, warm-up call dropped; single-lane
cells only): mean, median, p90, p99, max, share > 1 ms; burst cells: (per-launch mean - P's) /
32 lanes = cost per host call (us). Validity per run in verify.csv.
"""
import csv, datetime, glob, os, re, statistics as st, sys

D = sys.argv[1]
rounds = sys.argv[2:] or sorted(os.path.basename(p)[1:] for p in glob.glob(f"{D}/r*") if os.path.isdir(p))
SETS = ["P", "N", "W", "S10", "S20", "S50", "S100", "M100"]
CELLS = [("host", "on", "2000", "1"), ("host", "off", "2000", "1"), ("host", "on", "20000", "1"),
         ("host", "off", "20000", "1"), ("none", "on", "2000", "1"), ("none", "on", "20000", "1"),
         ("host", "on", "2000", "32"), ("host", "off", "2000", "32")]
LAUNCHES = {"1": {"2000": int(os.environ.get("L2000", 3200)), "20000": 400}, "32": {"2000": 2400}}


def victim(p):
    if not os.path.exists(p):
        return None
    t = open(p).read()
    w = re.search(r"window_ns=(\d+) (\d+)", t)
    rw = re.search(r"realtime_ns=(\d+) (\d+)", t)
    pl = re.search(r"per_launch_us mean=([\d.]+) median=([\d.]+)", t)
    c = re.search(r"cpu_s=([\d.]+) wall_s=([\d.]+)", t)
    if not (w and pl and c and rw):
        return None
    return dict(s=int(w[1]), e=int(w[2]), rs=int(rw[1]), re=int(rw[2]), mean=float(pl[1]),
                median=float(pl[2]), cpu=float(c[1]), wall=float(c[2]))


def load_rate(p, s, e):
    pts = [(int(a), int(b)) for a, b in (l.split() for l in open(p) if len(l.split()) == 2) if s <= int(a) <= e]
    return (pts[-1][1] - pts[0][1]) / ((pts[-1][0] - pts[0][0]) / 1e9) if len(pts) >= 2 else None


def power(p, rs, re_):
    if not os.path.exists(p):
        return None
    lo = rs // 10**9 + 1; hi = re_ // 10**9 - 1  # whole seconds strictly inside
    cv, vin = [], []
    for l in open(p):
        m = re.match(r"(\d\d-\d\d-\d{4} \d\d:\d\d:\d\d)", l)
        if not m:
            continue
        ts = datetime.datetime.strptime(m[1], "%m-%d-%Y %H:%M:%S").timestamp()
        if lo <= ts <= hi:
            a = re.search(r"VDD_CPU_GPU_CV (\d+)mW", l); b = re.search(r"VDD_IN (\d+)mW", l)
            if a and b:
                cv.append(int(a[1])); vin.append(int(b[1]))
    return (st.mean(cv), st.mean(vin), len(cv)) if cv else None


def lat(p):
    t = open(p).read()
    m = re.search(r"count=(\d+)", t)
    v = t.split("ns=")[1].strip().split(",") if "ns=" in t else []
    return (int(m[1]) if m else -1), [int(x) / 1000 for x in v if x][1:]


def env(p):
    t = open(p).read()
    g = lambda k: (int(m[1]) if (m := re.search(k + r" rc=(\d+)", t)) else None)
    clk = re.findall(r"cpu5=(\d+) cpu0=(\d+) gpu=(\d+)", t)
    temps = [int(x) for m in re.findall(r"temps=([\d,]+)", t) for x in m.split(",") if x]
    return dict(victim=g("victim"), loader=g("loader"), load=g("load"),
                clocks_ok=len(clk) == 2 and all(c == ("1728000", "1728000", "1020000000") for c in clk),
                tmax=max(temps) / 1000 if temps else None, note="not attached" in t)


rows, ver = {}, []
for r in rounds:
    for cell in CELLS:
        pr, ld, d, th = cell
        for s in SETS:
            base = f"{D}/r{r}/{pr}_{ld}_{d}_t{th}_{s}"
            if not os.path.exists(base + ".env"):
                continue
            L = LAUNCHES[th][d]
            v = victim(base + ".victim"); e = env(base + ".env")
            rate = load_rate(base + ".load", v["s"], v["e"]) if (v and ld == "on") else None
            pw = power(base + ".power", v["rs"], v["re"]) if (v and ld == "off") else None
            cnt, ls = lat(base + ".loader") if s != "P" else (None, [])
            rec_ok = s == "P" or (th == "32" and cnt > 0) or cnt == L + 1
            ok = bool(v is not None and e["victim"] == 0 and e["clocks_ok"] and not e["note"]
                      and (s == "P" or e["loader"] == 0) and rec_ok
                      and (ld == "off" or (e["load"] == 0 and rate)))
            ver.append(dict(round=r, probe=pr, load=ld, spin_us=d, threads=th, set=s, ok=ok,
                            victim_rc=e["victim"], loader_rc=e["loader"], load_rc=e["load"], records=cnt,
                            clocks_ok=e["clocks_ok"], tmax_c=e["tmax"], power_samples=pw[2] if pw else None))
            if ok:
                rows.setdefault(cell + (s,), []).append(dict(r=r, v=v, rate=rate, pw=pw, lat=ls))

with open(f"{D}/verify.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(ver[0].keys())); w.writeheader(); w.writerows(ver)
q = lambda v, p: v[min(len(v) - 1, int(len(v) * p))]
out = [f"Runs: {len(ver)}, valid {sum(x['ok'] for x in ver)}; rounds {','.join(rounds)}; max temperature "
       f"{max(x['tmax_c'] for x in ver if x['tmax_c']):.1f} C\n"]
for cell in CELLS:
    pr, ld, d, th = cell
    out.append(f"\n### probe {'host call' if pr == 'host' else 'no host call'}, load {ld}, spin {d} us + 0-999 us, {th} lane(s)\n")
    hdr = "| set | runs | victim CPU s per wall s | per-launch mean (us) | per-launch median (us) |"
    sep = "|---|---:|---:|---:|---:|"
    if ld == "on":
        hdr += " load vs P (median, min-max) |"; sep += "---:|"
    else:
        hdr += " VDD_CPU_GPU_CV (mW) | VDD_IN (mW) |"; sep += "---:|---:|"
    if pr == "host" and th == "1":
        hdr += " host call mean (us) | median / p90 / p99 / max (us) | calls > 1 ms |"; sep += "---:|---:|---:|"
    if th == "32":
        hdr += " cost per host call (us) |"; sep += "---:|"
    out += [hdr, sep]
    Pm = {x["r"]: x for x in rows.get(cell + ("P",), [])}
    for s in SETS:
        R = rows.get(cell + (s,), [])
        if not R:
            out.append(f"| {s} | 0 |"); continue
        line = (f"| {s} | {len(R)} | {st.median(x['v']['cpu'] / x['v']['wall'] for x in R):.3f} | "
                f"{st.median(x['v']['mean'] for x in R):,.1f} | {st.median(x['v']['median'] for x in R):,.1f} |")
        if ld == "on":
            rat = sorted(x["rate"] / Pm[x["r"]]["rate"] for x in R if x["r"] in Pm)
            line += f" {st.median(rat):.3f} ({rat[0]:.3f}-{rat[-1]:.3f}) |" if rat else " - |"
        else:
            P = [x["pw"] for x in R if x["pw"]]
            line += (f" {st.median(p[0] for p in P):,.0f} | {st.median(p[1] for p in P):,.0f} |" if P else " - | - |")
        if pr == "host" and th == "1":
            if s == "P":
                line += " - | - | - |"
            else:
                L = sorted(v for x in R for v in x["lat"])
                line += (f" {st.mean(L):,.1f} | {st.median(L):,.1f} / {q(L, .9):,.1f} / {q(L, .99):,.1f} / {L[-1]:,.1f} |"
                         f" {100 * sum(v > 1000 for v in L) / len(L):.2f}% |")
        if th == "32":
            per = [(x["v"]["mean"] - Pm[x["r"]]["v"]["mean"]) / 32 for x in R if x["r"] in Pm]
            line += f" {st.median(per):,.1f} |" if per and s != "P" else " - |"
        out.append(line)
open(f"{D}/summary.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))
