#!/usr/bin/env python3
"""Summarize the watcher-policy rounds: summarize.py LOG_DIR [ROUNDS...]

Per cell and set (pooled over the processes of a run, then over rounds):
- victim CPU s per wall s (getrusage of every victim, summed; includes the watchers), and the
  same minus P's of the same round (= what bpftime's process-side work costs)
- per-launch mean and median (us), median over runs of the per-process values
- load throughput vs P (same round), median and min-max (load cells)
- power VDD_CPU_GPU_CV / VDD_IN (mW), tegrastats samples of whole seconds inside the window
  (no-load cells)
- host-call latency (GPU-timed, warm-up dropped, single-lane cells): mean, median, p90, p99,
  max, share > 1 ms; burst cells: (per-launch mean - P's) / 32
- watcher counters (policies A..ABC) summed per run, median over runs
Validity per run in verify.csv.
"""
import csv, datetime, glob, os, re, statistics as st, sys

D = sys.argv[1]
rounds = sys.argv[2:] or sorted(os.path.basename(p)[1:] for p in glob.glob(f"{D}/r*") if os.path.isdir(p))
SETS = ["P", "W", "M100", "A", "AC", "AB", "ABC"]
CELLS = {}
for line in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "rounds.sh")):
    m = re.match(r'\s*"(c\d\d) (\w+) (\w+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d) (\d)"\s*(?:#\s*(.*))?', line)
    if m:
        CELLS[m[1]] = dict(probe=m[2], load=m[3], spin=int(m[4]), launches=int(m[5]), threads=int(m[6]),
                          jitter=int(m[7]), gap=int(m[8]), graph=int(m[9]), procs=int(m[10]), note=(m[11] or "").strip())


def victim(p):
    if not os.path.exists(p):
        return None
    t = open(p).read()
    w = re.search(r"window_ns=(\d+) (\d+)", t); rw = re.search(r"realtime_ns=(\d+) (\d+)", t)
    pl = re.search(r"per_launch_us mean=([\d.]+) median=([\d.]+)", t); c = re.search(r"cpu_s=([\d.]+) wall_s=([\d.]+)", t)
    if not (w and rw and pl and c):
        return None
    return dict(s=int(w[1]), e=int(w[2]), rs=int(rw[1]), re=int(rw[2]), mean=float(pl[1]), median=float(pl[2]),
                cpu=float(c[1]), wall=float(c[2]))


def load_rate(p, s, e):
    if not os.path.exists(p):
        return None
    pts = [(int(a), int(b)) for a, b in (l.split() for l in open(p) if len(l.split()) == 2) if s <= int(a) <= e]
    return (pts[-1][1] - pts[0][1]) / ((pts[-1][0] - pts[0][0]) / 1e9) if len(pts) >= 2 else None


def power(p, rs, re_):
    if not os.path.exists(p):
        return None
    lo, hi = rs // 10**9 + 1, re_ // 10**9 - 1
    cv, vin = [], []
    for l in open(p):
        m = re.match(r"(\d\d-\d\d-\d{4} \d\d:\d\d:\d\d)", l)
        if m and lo <= datetime.datetime.strptime(m[1], "%m-%d-%Y %H:%M:%S").timestamp() <= hi:
            a = re.search(r"VDD_CPU_GPU_CV (\d+)mW", l); b = re.search(r"VDD_IN (\d+)mW", l)
            if a and b:
                cv.append(int(a[1])); vin.append(int(b[1]))
    return (st.mean(cv), st.mean(vin), len(cv)) if cv else None


def lat(p):
    if not os.path.exists(p):
        return -1, []
    t = open(p).read()
    m = re.search(r"count=(\d+)", t)
    v = t.split("ns=")[1].strip().split(",") if "ns=" in t else []
    return (int(m[1]) if m else -1), [int(x) / 1000 for x in v if x][1:]


def stats(p):
    out = []
    if os.path.exists(p):
        for l in open(p):
            out.append({k: float(v) for k, v in re.findall(r"(\w+)=([\d.]+)", l) if k not in ("pid",)})
    return out


def env(p, np_):
    t = open(p).read()
    rcs = [int(x) for x in re.findall(r"victim \d+ rc=(\d+)", t)]
    lrc = [int(x) for x in re.findall(r"loader \d+ rc=(\d+)", t)]
    ldrc = re.search(r"load rc=(\d+)", t)
    clk = re.findall(r"cpu5=(\d+) cpu0=(\d+) gpu=(\d+)", t)
    temps = [int(x) for m in re.findall(r"temps=([\d,]+)", t) for x in m.split(",") if x]
    return dict(victims_ok=len(rcs) == np_ and all(r == 0 for r in rcs), loader_rcs=lrc,
                load_rc=int(ldrc[1]) if ldrc else None,
                clocks_ok=len(clk) == 2 and all(c == ("1728000", "1728000", "1020000000") for c in clk),
                tmax=max(temps) / 1000 if temps else None, note=("not attached" in t or "in use" in t))


rows, ver = {}, []
for r in rounds:
    for cn, c in CELLS.items():
        for s in SETS:
            base = f"{D}/r{r}/{cn}_{s}"
            if not os.path.exists(base + ".env"):
                continue
            e = env(base + ".env", c["procs"])
            vs = [victim(f"{base}.{i}.victim") for i in range(1, c["procs"] + 1)]
            ls = [lat(f"{base}.{i}.loader") for i in range(1, c["procs"] + 1)] if s != "P" else []
            ok_v = all(vs) and e["victims_ok"]
            s0 = min(v["s"] for v in vs) if ok_v else 0; e0 = max(v["e"] for v in vs) if ok_v else 0
            rate = load_rate(base + ".load", s0, e0) if (ok_v and c["load"] != "off") else None
            pw = power(base + ".power", max(v["rs"] for v in vs), min(v["re"] for v in vs)) if (ok_v and c["load"] == "off") else None
            if s == "P":
                rec_ok = True
            elif c["threads"] == 32:
                rec_ok = all(n > 0 for n, _ in ls)
            else:
                rec_ok = all(n == c["launches"] + 1 + c["graph"] for n, _ in ls)  # graph mode: one more warm-up through the graph
            ok = bool(ok_v and e["clocks_ok"] and not e["note"] and rec_ok
                      and (s == "P" or (len(e["loader_rcs"]) == c["procs"] and all(x == 0 for x in e["loader_rcs"])))
                      and (c["load"] == "off" or (e["load_rc"] == 0 and rate)))
            ver.append(dict(round=r, cell=cn, set=s, ok=ok, victims_ok=e["victims_ok"], loader_rcs=e["loader_rcs"],
                            load_rc=e["load_rc"], records=[n for n, _ in ls], clocks_ok=e["clocks_ok"], tmax_c=e["tmax"]))
            if ok:
                rows.setdefault((cn, s), []).append(dict(
                    r=r, cpu=sum(v["cpu"] for v in vs) / max(v["wall"] for v in vs),
                    mean=st.mean(v["mean"] for v in vs), median=st.mean(v["median"] for v in vs),
                    rate=rate, pw=pw, lat=[x for _, l in ls for x in l], st=stats(base + ".stats")))

with open(f"{D}/verify.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(ver[0].keys())); w.writeheader(); w.writerows(ver)
q = lambda v, p: v[min(len(v) - 1, int(len(v) * p))]
out = [f"Runs: {len(ver)}, valid {sum(x['ok'] for x in ver)}; rounds {','.join(rounds)}; max temperature "
       f"{max(x['tmax_c'] for x in ver if x['tmax_c']):.1f} C\n"]
STK = ["req", "wfe_waits", "slow_sleeps", "defer_sleeps", "hot_iters", "parks", "park_timeouts", "unseen_latch",
       "gate_opens", "latch_trips", "cap_forced", "guard_skips", "win0", "win20", "win80", "win300"]
for cn, c in CELLS.items():
    out.append(f"\n### {cn}: {c['note']} (probe {c['probe']}, load {c['load']}, spin {c['spin']} us + 0-{c['jitter']} us, "
               f"{c['launches']} launches, {c['threads']} lane(s), gap {c['gap']} us, graph {c['graph']}, {c['procs']} process(es))\n")
    hdr = "| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) |"
    sep = "|---|---:|---:|---:|---:|---:|"
    if c["load"] != "off":
        hdr += " load vs P |"; sep += "---:|"
    else:
        hdr += " VDD_CPU_GPU_CV (mW) | VDD_IN (mW) |"; sep += "---:|---:|"
    if c["probe"] == "host" and c["threads"] == 1:
        hdr += " host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |"; sep += "---:|---:|---:|"
    if c["threads"] == 32:
        hdr += " cost per call (us) |"; sep += "---:|"
    out += [hdr, sep]
    Pm = {x["r"]: x for x in rows.get((cn, "P"), [])}
    for s in SETS:
        R = rows.get((cn, s), [])
        if not R:
            out.append(f"| {s} | 0 |"); continue
        dcpu = [x["cpu"] - Pm[x["r"]]["cpu"] for x in R if x["r"] in Pm]
        line = (f"| {s} | {len(R)} | {st.median(x['cpu'] for x in R):.3f} | {st.median(dcpu):+.3f} | "
                f"{st.median(x['mean'] for x in R):,.1f} | {st.median(x['median'] for x in R):,.1f} |") if dcpu else f"| {s} | {len(R)} | - | - | - | - |"
        if c["load"] != "off":
            rat = sorted(x["rate"] / Pm[x["r"]]["rate"] for x in R if x["r"] in Pm and Pm[x["r"]]["rate"])
            line += f" {st.median(rat):.3f} ({rat[0]:.3f}-{rat[-1]:.3f}) |" if rat else " - |"
        else:
            P_ = [x["pw"] for x in R if x["pw"]]
            line += f" {st.median(p[0] for p in P_):,.0f} | {st.median(p[1] for p in P_):,.0f} |" if P_ else " - | - |"
        if c["probe"] == "host" and c["threads"] == 1:
            L = sorted(v for x in R for v in x["lat"])
            line += (f" {st.mean(L):,.1f} | {st.median(L):,.1f} / {q(L, .9):,.1f} / {q(L, .99):,.1f} / {L[-1]:,.1f} |"
                     f" {100 * sum(v > 1000 for v in L) / len(L):.2f}% |") if L else " - | - | - |"
        if c["threads"] == 32:
            per = [(x["mean"] - Pm[x["r"]]["mean"]) / 32 for x in R if x["r"] in Pm]
            line += f" {st.median(per):,.1f} |" if per and s != "P" else " - |"
        out.append(line)
    cnt = [(s, rows.get((cn, s), [])) for s in SETS if s in ("A", "AC", "AB", "ABC")]
    if any(R and R[0]["st"] for _, R in cnt):
        out.append("\nWatcher counters (sum over the run's processes, median over runs): " + ", ".join(STK))
        for s, R in cnt:
            if R and R[0]["st"]:
                vals = [st.median(sum(d.get(k, 0) for d in x["st"]) for x in R) for k in STK]
                out.append(f"- {s}: " + " ".join(f"{k}={v:.0f}" for k, v in zip(STK, vals)))
open(f"{D}/summary.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))
