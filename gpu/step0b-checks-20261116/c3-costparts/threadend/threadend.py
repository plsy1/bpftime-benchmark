#!/usr/bin/env python3
"""C3 (c): thread-end merge and write-out costs from step 0's e2-cost data.
No new runs: reads the two real-run summary.csv files of step 0 (pass 1 and
pass 2) and writes only into OUT_DIR.

Usage: python3 -I threadend.py OUT_DIR [PASS1_SUMMARY_CSV PASS2_SUMMARY_CSV]
Writes OUT_DIR/threadend.csv, OUT_DIR/threadend.md, OUT_DIR/threadend.json.

Operations (step 0's e2-cost cases; f0 = no fence, f1 = membar.sys after it):
  red_warp_leader       lane 0 of each warp, red.global.add.u64 (result unused)
                        on its warp's word: a per-warp merge, fire-and-forget
  atom_warp_leader      the same with atom.global.add.u64 (result used)
  atom_warp_leader_sys  the same with atom.sys
  atom_lane             every lane atom.global.add.u64 on its own word
  comp_rec              design D's record write-out: lane 0 fetch-adds the
                        SM's cursor, lanes 0-3 write 32 B into a per-SM ring,
                        __syncwarp, lane 0 (f1: membar.sys) st.relaxed.sys
  atom_sm k             lane 0 of each warp with %warpid < k adds to one word
                        per SM (k = 48: every warp; full occupancy k-sweep,
                        large shape k = 48 only)

Normalisation (what each case counts):
  per-warp cases (red/atom_warp_leader*, comp_rec): ns per warp-op = delta /
    (warps x R); [projection] per warp record = 6.02M x ns / 204.6 ms;
    ns per thread execution = ns per warp-op / 32 (step 0's ns_per_thread_op)
    and [projection] 192.56M x that / 204.6 ms (the two projections agree to
    0.04% because 192.56M / 32 = 6.0175M).
  atom_lane: ns per thread-op; [projection] 192.56M x ns / 204.6 ms.
  atom_sm: ns per counted op (delta / every counted atomic on all 8 SMs) and
    the slope from R = 1 to Rbig per counted op. Only k = 48 has every warp
    taking part, so only k = 48 rows get a per-thread figure and a
    projection; at k < 48 step 0's ns_per_thread_op divides by threads that
    never took part (reported as n/a).
"""
import csv
import json
import os
import sys

STEP0 = "/home/jetson/src/bpftime-gpu-work/step0/e2-cost/results"
PASS1 = STEP0 + "/run-20261115-050452/summary.csv"
PASS2 = STEP0 + "/run-20261115-060441/summary.csv"
PP512_THREAD_EXECS = 192560640
PP512_WARP_RECORDS = 6.02e6
PP512_GPU_NS = 204.6e6
CASES = ["red_warp_leader", "atom_warp_leader", "atom_warp_leader_sys", "atom_lane", "comp_rec", "atom_sm"]
PER_WARP = {"red_warp_leader", "atom_warp_leader", "atom_warp_leader_sys", "comp_rec"}
SHAPE_ORDER = {"tiny": 0, "full": 1, "anchor": 2, "large": 3}


def fnum(x):
    return None if x in (None, "") else float(x)


def load(path):
    rows = {}
    for r in csv.DictReader(open(path)):
        rows[r["id"]] = r
    return rows


def pct_thread(ns):
    return None if ns is None else PP512_THREAD_EXECS * ns / PP512_GPU_NS * 100


def pct_warp(ns):
    return None if ns is None else PP512_WARP_RECORDS * ns / PP512_GPU_NS * 100


def triple(r, field):
    return [fnum(r.get(field + s)) for s in ("_med", "_min", "_max")]


def self_check(p1):
    """Reproduce three numbers of step 0's README from pass 1."""
    def get(i, f):
        return fnum(p1[i][f + "_med"])
    st = get("anchor|st_lane32|dev|f0|R1|k0", "ns_per_thread_op")
    ref = get("anchor|ref1512|reg|f1|R1|k0", "ns_per_thread_op")
    rec = get("anchor|rec_lane0|reg|f1|R1|k0", "ns_per_warp_op")
    got = {"8-B store dev, ns per thread exec": (st, pct_thread(st), 0.032, 3.0),
           "today's 1512 reg, ns per thread exec": (ref, pct_thread(ref), 0.433, 40.7),
           "record per warp reg fenced, ns per warp": (rec, pct_warp(rec), 10.06, 29.6)}
    ok = all(abs(v - want) <= 0.0006 + 0.005 * want and abs(p - wp) <= 0.06 for v, p, want, wp in got.values())
    return ok, got


def main():
    out = sys.argv[1]
    p1path = sys.argv[2] if len(sys.argv) > 2 else PASS1
    p2path = sys.argv[3] if len(sys.argv) > 3 else PASS2
    os.makedirs(out, exist_ok=True)
    p1, p2 = load(p1path), load(p2path)
    ok, chk = self_check(p1)
    rows = []
    for iid, r in p1.items():
        if r["case"] not in CASES:
            continue
        q = p2.get(iid)
        case, k = r["case"], int(r["k"])
        rec = {"id": iid, "case": case, "mem": r["mem"], "fence": int(r["fence"]), "shape": r["shape"],
               "R": int(r["R"]), "k": k, "valid": r["valid"], "resolved_p1": r["resolved"],
               "resolved_p2": q["resolved"] if q else None}
        for tag, src in (("p1", r), ("p2", q)):
            if src is None:
                continue
            th = triple(src, "ns_per_thread_op")
            wa = triple(src, "ns_per_warp_op")
            co = triple(src, "ns_per_counted_op")
            sl_w = fnum(src.get("slope_ns_per_warp_op_med"))
            sl_t = fnum(src.get("slope_ns_per_thread_op_med"))
            sl_c = fnum(src.get("slope_ns_per_counted_op_med"))
            if case == "atom_sm":
                full_part = k == 48
                rec[tag] = {
                    "ns_per_counted_op": co,
                    "slope_ns_per_counted_op": sl_c,
                    "ns_per_thread_exec": th if full_part else None,
                    "ns_per_warp": wa if full_part else None,
                    "pct_per_thread_exec": pct_thread(th[0]) if full_part else None,
                    "pct_per_warp_record": pct_warp(co[0]) if full_part else None,
                    "slope_pct_per_warp_record": pct_warp(sl_c) if (full_part and sl_c is not None) else None,
                }
            else:
                rec[tag] = {
                    "ns_per_thread_exec": th,
                    "ns_per_warp": wa,
                    "pct_per_thread_exec": pct_thread(th[0]),
                    "pct_per_warp_record": pct_warp(wa[0]),
                    "slope_ns_per_thread_exec": sl_t,
                    "slope_ns_per_warp": sl_w,
                    "slope_pct_per_thread_exec": pct_thread(sl_t),
                }
        if "p1" in rec and "p2" in rec:
            a = rec["p1"]["ns_per_counted_op"][0] if case == "atom_sm" else rec["p1"]["ns_per_thread_exec"][0]
            b = rec["p2"]["ns_per_counted_op"][0] if case == "atom_sm" else rec["p2"]["ns_per_thread_exec"][0]
            rec["p2_over_p1"] = (b / a) if (a and b is not None) else None
        rows.append(rec)
    rows.sort(key=lambda x: (CASES.index(x["case"]), x["mem"], x["fence"], SHAPE_ORDER.get(x["shape"], 9),
                             x["k"], x["R"]))
    json.dump({"inputs": [p1path, p2path], "self_check_ok": ok, "self_check": chk, "rows": rows},
              open(os.path.join(out, "threadend.json"), "w"), indent=1)

    def g(x, i=0):
        return "" if x is None else ("%.4g" % (x[i] if isinstance(x, list) else x))

    with open(os.path.join(out, "threadend.csv"), "w") as f:
        w = csv.writer(f)
        w.writerow(["id", "case", "mem", "fence", "shape", "R", "k", "resolved_p1", "resolved_p2",
                    "p1_ns_per_thread_exec_med", "p1_min", "p1_max", "p1_pct_per_thread_exec",
                    "p1_ns_per_warp_med", "p1_pct_per_warp_record", "p1_ns_per_counted_op_med",
                    "p1_slope_ns_per_thread_exec", "p1_slope_ns_per_counted_op",
                    "p2_ns_per_thread_exec_med", "p2_pct_per_thread_exec", "p2_ns_per_warp_med",
                    "p2_pct_per_warp_record", "p2_ns_per_counted_op_med", "p2_over_p1"])
        for x in rows:
            a, b = x.get("p1", {}), x.get("p2", {})
            w.writerow([x["id"], x["case"], x["mem"], x["fence"], x["shape"], x["R"], x["k"],
                        x["resolved_p1"], x["resolved_p2"],
                        g(a.get("ns_per_thread_exec")), g(a.get("ns_per_thread_exec"), 1),
                        g(a.get("ns_per_thread_exec"), 2), g(a.get("pct_per_thread_exec")),
                        g(a.get("ns_per_warp")), g(a.get("pct_per_warp_record")), g(a.get("ns_per_counted_op")),
                        g(a.get("slope_ns_per_thread_exec")), g(a.get("slope_ns_per_counted_op")),
                        g(b.get("ns_per_thread_exec")), g(b.get("pct_per_thread_exec")), g(b.get("ns_per_warp")),
                        g(b.get("pct_per_warp_record")), g(b.get("ns_per_counted_op")), g(x.get("p2_over_p1"))])

    def f(x, d=3):
        return "" if x is None else ("%." + str(d) + "f") % x

    L = ["# Thread-end merge and write-out costs from step 0's e2-cost data (C3 c)\n",
         "Source: step 0 check 2, pass 1 (`%s`) and pass 2 (`%s`); 10 rounds each, medians. No new runs." % (
             p1path, p2path),
         "Empty kernels: the cost is fully exposed (no overlap with real work). [projection] = the shape's cost "
         "applied to every pp512 kernel: 192.56M thread executions or 6.02M warp records over 204.6 ms.\n",
         "Self-check against step 0's README: %s" % ("ok" if ok else "FAILED"),
         ""]
    for n, (v, p, want, wp) in chk.items():
        L.append("- %s: %.4f (README %.3f), %.1f%% (README %.1f%%)" % (n, v, want, p, wp))
    L.append("")
    # headline: R = 1, llama shapes, per warp record (= per thread execution x 32)
    L.append("## Headline: [projection] % of a pp512 if every warp did it once per kernel (R = 1, pass 1; "
             "ns per warp in parentheses)\n")
    L.append("tiny (2 warps) is left out of this table: one launch's latency, not a llama shape.\n")
    hl = [("per-warp merge, red (result unused)", "red_warp_leader", 0, 0),
          ("per-warp merge, red + membar.sys", "red_warp_leader", 1, 0),
          ("per-warp merge, atom (result used)", "atom_warp_leader", 0, 0),
          ("per-warp merge, atom.sys", "atom_warp_leader_sys", 0, 0),
          ("per-lane atom on its own word (x32 per warp)", "atom_lane", 0, 0),
          ("per-SM merge word, every warp (atom_sm k = 48)", "atom_sm", 0, 48),
          ("per-SM merge word + membar.sys (k = 48)", "atom_sm", 1, 48),
          ("record write-out, per-SM ring, no fence (comp_rec)", "comp_rec", 0, 0),
          ("record write-out, per-SM ring, membar.sys (comp_rec)", "comp_rec", 1, 0)]
    cols = [(m, sh) for m in ("dev", "reg") for sh in ("anchor", "large", "full")]
    L.append("| operation | " + " | ".join("%s %s" % c for c in cols) + " |")
    L.append("|---|" + "---:|" * len(cols))
    for label, case, fence, k in hl:
        cells = []
        for mem, sh in cols:
            x = [r for r in rows if r["case"] == case and r["fence"] == fence and r["k"] == k and
                 r["mem"] == mem and r["shape"] == sh and r["R"] == 1]
            if not x:
                cells.append("-")
                continue
            a = x[0]["p1"]
            if case == "atom_sm":
                ns_w = a["ns_per_counted_op"][0]
            else:
                ns_w = a["ns_per_warp"][0]
            cells.append("%.1f%% (%.2f)" % (pct_warp(ns_w), ns_w))
        L.append("| %s | %s |" % (label, " | ".join(cells)))
    L.append("")
    L.append("Per-SM merge (atom_sm): ns per counted op = per warp at k = 48. The per-lane atom is per thread; "
             "its per-warp figure is 32 lanes' worth.\n")
    # all rows: R = 1 and Rbig, all shapes, both memories, per case
    for case in CASES:
        sel = [x for x in rows if x["case"] == case]
        if not sel:
            continue
        L.append("## %s\n" % case)
        if case == "atom_sm":
            L.append("| mem | fence | shape | k | R | ns per counted op p1 [min-max] | p2 | p2/p1 | slope ns/op | "
                     "ns per thread exec | % per thread exec | % per warp record | resolved |")
            L.append("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|")
            for x in sel:
                a, b = x["p1"], x.get("p2", {})
                L.append("| %s | f%d | %s | %d | %d | %s [%s-%s] | %s | %s | %s | %s | %s | %s | %s/%s |" % (
                    x["mem"], x["fence"], x["shape"], x["k"], x["R"], f(a["ns_per_counted_op"][0], 2),
                    f(a["ns_per_counted_op"][1], 2), f(a["ns_per_counted_op"][2], 2),
                    f(b.get("ns_per_counted_op", [None])[0], 2), f(x.get("p2_over_p1"), 3),
                    f(a["slope_ns_per_counted_op"], 2),
                    f(a["ns_per_thread_exec"][0], 4) if a["ns_per_thread_exec"] else "n/a",
                    f(a["pct_per_thread_exec"], 1) if a["pct_per_thread_exec"] is not None else "n/a",
                    f(a["pct_per_warp_record"], 1) if a["pct_per_warp_record"] is not None else "n/a",
                    x["resolved_p1"], x["resolved_p2"]))
        else:
            L.append("| mem | fence | shape | R | ns per thread exec p1 [min-max] | p2 | p2/p1 | % per thread exec | "
                     "ns per warp | % per warp record | slope ns/thread exec | resolved |")
            L.append("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|")
            for x in sel:
                a, b = x["p1"], x.get("p2", {})
                L.append("| %s | f%d | %s | %d | %s [%s-%s] | %s | %s | %s | %s | %s | %s | %s/%s |" % (
                    x["mem"], x["fence"], x["shape"], x["R"], f(a["ns_per_thread_exec"][0], 4),
                    f(a["ns_per_thread_exec"][1], 4), f(a["ns_per_thread_exec"][2], 4),
                    f(b.get("ns_per_thread_exec", [None])[0], 4), f(x.get("p2_over_p1"), 3),
                    f(a["pct_per_thread_exec"], 1), f(a["ns_per_warp"][0], 3), f(a["pct_per_warp_record"], 1),
                    f(a["slope_ns_per_thread_exec"], 4), x["resolved_p1"], x["resolved_p2"]))
        L.append("")
    open(os.path.join(out, "threadend.md"), "w").write("\n".join(L) + "\n")
    print("self-check:", "ok" if ok else "FAILED")
    for n, (v, p, want, wp) in chk.items():
        print("  %s: %.4f (README %.3f), %.2f%% (README %.1f%%)" % (n, v, want, p, wp))
    print("rows:", len(rows))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
