#!/usr/bin/env python3
"""e2-cost analysis: per-case costs and the three derived outputs of check 2.

Usage: python3 -I analyze.py OUT_DIR
Reads OUT_DIR/raw.csv and OUT_DIR/items.json; writes OUT_DIR/summary.csv,
OUT_DIR/derived.json and OUT_DIR/summary.md.

Definitions (fixed before the real run; README "Decision rules"):
- t[r, i]: per-launch time of item i in round r (us), from CUDA events
  around N back-to-back launches.
- delta[r, i] = t[r, i] - t[r, empty kernel of the same module and shape]
  (paired per round; ref_empty for ref1512 and ref1512_loop, empty for the
  rest).
- ops: thread-ops = threads x R; warp-ops = warps x R; for atom_sm/cas_sm/
  comp_rec the counted operations of the batch (ops_batch / N).
- ns per thread-op = delta / thread-ops; ns per warp-op = delta / warp-ops;
  ns per counted op = delta / counted ops. Median, min, max over rounds.
- slope (per warp-op) = (t[r, R=Rbig] - t[r, R=1]) / ((Rbig - 1) x warps),
  paired per round.
"""
import csv
import json
import os
import statistics
import sys
from collections import defaultdict

LLAMA_PER_THREAD_NS = 7e6 / 11273216  # census README:93, 7 ms / 11.27M
PP512_WARP_RECORDS = 6.02e6  # design note section 1 [arith, sqlite]
PP512_GPU_NS = 204.6e6  # census README section 1
PP512_THREAD_EXECS = 192560640
PRIMARY_SHAPE = "anchor"  # 512 x 512: rms_norm's shape, median pp512 launch
SECONDARY_SHAPES = ["large", "full"]
GATE_LO, GATE_HI = 0.5, 2.0
THRESH_PCT = 10.0
RECORD_CASES = ["rec_lane0", "rec_lane4", "comp_rec"]
PER_WARP_CASES = {"st_lane1", "rec_lane0", "rec_lane4", "atom_warp_leader",
                  "atom_warp_leader_sys", "red_warp_leader", "comp_rec"}
COUNTED = {"atom_sm", "cas_sm", "comp_rec"}


def stats(v):
    v = [x for x in v if x is not None]
    if not v:
        return None
    return {"median": statistics.median(v), "min": min(v), "max": max(v), "n": len(v)}


def main():
    out = sys.argv[1]
    items = {it["id"]: it for it in json.load(open(os.path.join(out, "items.json")))["items"]}
    rows = list(csv.DictReader(open(os.path.join(out, "raw.csv"))))
    t = defaultdict(dict)  # id -> round -> us
    ops = defaultdict(dict)  # id -> round -> counted ops per launch
    capped_rows = []  # batches whose k_hold gate hit its cap: not used
    for r in rows:
        if r.get("hold_capped", "0") not in ("0", ""):
            capped_rows.append((r["round"], r["id"]))
            continue
        rd = int(r["round"])
        t[r["id"]][rd] = float(r["per_launch_us"])
        n = int(r["n_launch"])
        if r["case"] in COUNTED:
            ops[r["id"]][rd] = int(r["ops_batch"]) / n
    rounds = sorted({int(r["round"]) for r in rows})
    # baseline per (module, shape): the reference kernels (ref1512,
    # ref1512_loop) are in their own module, whose empty kernel (ref_empty)
    # launches about 0.5 us slower at small shapes in gated smokes; every
    # other case is in the kernels module (empty)
    empty, ref_empty = {}, {}
    for iid, it in items.items():
        if it["case"] == "empty":
            empty[it["shape"]] = iid
        elif it["case"] == "ref_empty":
            ref_empty[it["shape"]] = iid
    summary = {}
    for iid, it in items.items():
        if iid not in t:
            continue
        sh = it["shape"]
        base = ref_empty if it["case"] in ("ref1512", "ref1512_loop") else empty
        e = t.get(base.get(sh), {})
        T, W, R = it["threads"], it["warps"], it["R"]
        d = {rd: t[iid][rd] - e[rd] for rd in t[iid] if rd in e}
        rec = dict(it)
        rec["t_us"] = stats(list(t[iid].values()))
        rec["delta_us"] = stats(list(d.values()))
        rec["ns_per_thread_op"] = stats([v * 1e3 / (T * R) for v in d.values()])
        rec["ns_per_warp_op"] = stats([v * 1e3 / (W * R) for v in d.values()])
        if it["case"] in COUNTED and ops[iid]:
            rec["ops_per_launch"] = stats(list(ops[iid].values()))
            rec["ns_per_counted_op"] = stats(
                [d[rd] * 1e3 / ops[iid][rd] for rd in d if ops[iid].get(rd)])
        # resolved: the paired delta is positive in every round
        rec["resolved"] = bool(d) and min(d.values()) > 0
        summary[iid] = rec

    # slopes between R = 1 and R = Rbig
    def key(it):
        return (it["shape"], it["case"], it["mem"], it["fence"], it["k"])
    by_key = defaultdict(dict)
    for iid, it in summary.items():
        by_key[key(it)][it["R"]] = iid
    for k_, rs in by_key.items():
        if 1 in rs and len(rs) == 2:
            rb = max(rs)
            a, b = rs[1], rs[rb]
            W = summary[a]["warps"]
            T = summary[a]["threads"]
            sl = [(t[b][rd] - t[a][rd]) * 1e3 / ((rb - 1) * W) for rd in t[a] if rd in t[b]]
            summary[b]["slope_ns_per_warp_op"] = stats(sl)
            summary[b]["slope_ns_per_thread_op"] = stats([x * W / T for x in sl])
            if k_[1] in COUNTED and ops[a] and ops[b]:
                slc = [(t[b][rd] - t[a][rd]) * 1e3 / (ops[b][rd] - ops[a][rd])
                       for rd in t[a] if rd in t[b] and ops[b].get(rd, 0) > ops[a].get(rd, 0)]
                summary[b]["slope_ns_per_counted_op"] = stats(slc)

    def get(shape, case, mem, fence, R=1, k=0):
        iid = "%s|%s|%s|f%d|R%d|k%d" % (shape, case, mem, fence, R, k)
        return summary.get(iid)

    def med(rec, field):
        if rec is None or rec.get(field) is None:
            return None
        return rec[field]["median"]

    derived = {"llama_per_thread_ns": LLAMA_PER_THREAD_NS, "primary_shape": PRIMARY_SHAPE,
               "rounds": len(rounds)}

    # (1) which reading of the 0.6 ns figure
    def reading(shape, fence, refcase="ref1512"):
        ref = get(shape, refcase, "reg", 1)
        recq = get(shape, "rec_lane0", "reg", fence)
        g = med(ref, "ns_per_thread_op")
        P = med(ref, "ns_per_warp_op")
        Q = med(recq, "ns_per_warp_op")
        res = {"shape": shape, "record": "rec_lane0 reg f%d" % fence, "reference": refcase,
               "ref_ns_per_thread": g, "ref_ns_per_warp": P, "record_ns_per_warp": Q}
        if g is None or Q is None:
            res["verdict"] = "missing data"
            return res
        res["gate_ratio_to_llama"] = g / LLAMA_PER_THREAD_NS
        res["gate_ok"] = GATE_LO <= res["gate_ratio_to_llama"] <= GATE_HI
        gs = ref["ns_per_thread_op"]
        res["ref_ns_per_thread_min_max"] = [gs["min"], gs["max"]]
        res["gate_unstable"] = not (GATE_LO <= gs["min"] / LLAMA_PER_THREAD_NS <= GATE_HI and
                                    GATE_LO <= gs["max"] / LLAMA_PER_THREAD_NS <= GATE_HI)
        loop = get(shape, "ref1512_loop", "reg", 1)
        if loop is not None and med(ref, "delta_us"):
            res["ref_loop_over_exit"] = med(loop, "delta_us") / med(ref, "delta_us")
        res["Q_over_g"] = Q / g
        res["Q_over_P"] = Q / P
        if Q <= 2 * g:
            v = "reading 1 (a record per warp costs about one thread's share)"
        elif Q >= 0.5 * P:
            v = "reading 2 (a record per warp costs about one warp memory instruction of today's op)"
        else:
            v = "in between"
        qr = recq["ns_per_warp_op"]
        if qr["min"] <= 2 * g < qr["max"] or qr["min"] < 0.5 * P <= qr["max"]:
            v += " [unstable: the round spread crosses a threshold]"
        if res["gate_unstable"]:
            v += " [gate unstable: the reference's round spread leaves [0.5, 2] x the llama figure]"
        if not res["gate_ok"]:
            v = "inconclusive (gate: the reference is %.2fx the llama figure) / data say: %s" % (
                res["gate_ratio_to_llama"], v)
        res["verdict"] = v
        return res

    derived["reading"] = {
        "primary": reading(PRIMARY_SHAPE, 1),
        "secondary": [reading(PRIMARY_SHAPE, 1, "ref1512_loop"), reading(PRIMARY_SHAPE, 0)] +
                     [reading(s, f) for s in SECONDARY_SHAPES for f in (1, 0)],
    }

    # (2) projection [projection]: one record per warp on every kernel of a pp512
    proj = []
    for shape in [PRIMARY_SHAPE] + SECONDARY_SHAPES:
        for case in RECORD_CASES:
            for mem in ("reg", "dev"):
                for fence in (1, 0):
                    rec = get(shape, case, mem, fence)
                    q = med(rec, "ns_per_warp_op")
                    if q is None:
                        continue
                    span = rec["ns_per_warp_op"]
                    proj.append({
                        "shape": shape, "case": case, "mem": mem, "fence": fence,
                        "ns_per_record": q,
                        "overhead_pct": PP512_WARP_RECORDS * q / PP512_GPU_NS * 100,
                        "overhead_pct_min": PP512_WARP_RECORDS * span["min"] / PP512_GPU_NS * 100,
                        "overhead_pct_max": PP512_WARP_RECORDS * span["max"] / PP512_GPU_NS * 100,
                    })
        ref = get(shape, "ref1512", "reg", 1)
        g = med(ref, "ns_per_thread_op")
        if g is not None:
            proj.append({"shape": shape, "case": "ref1512 (one op per thread execution)", "mem": "reg",
                         "fence": 1, "ns_per_thread": g,
                         "overhead_pct": PP512_THREAD_EXECS * g / PP512_GPU_NS * 100})
    derived["projection"] = proj

    def decide(rows):
        regf = [p for p in rows if p["mem"] == "reg" and p["fence"] == 1 and p["case"] in RECORD_CASES]
        regu = [p for p in rows if p["mem"] == "reg" and p["fence"] == 0 and p["case"] in RECORD_CASES]
        devall = [p for p in rows if p["mem"] == "dev" and p["case"] in RECORD_CASES]
        devf = [p for p in devall if p["fence"] == 1]
        below = lambda s: [p for p in s if p["overhead_pct"] < THRESH_PCT]
        out = {}
        if below(regf):
            out["single_digit"] = "reachable with per-record publication (a fenced registered variant < 10%%: %s)" % ", ".join(
                "%s %.1f%%" % (p["case"], p["overhead_pct"]) for p in below(regf))
        elif below(regu):
            out["single_digit"] = "reachable only with batched publication (only unfenced registered variants < 10%%: %s)" % ", ".join(
                "%s %.1f%%" % (p["case"], p["overhead_pct"]) for p in below(regu))
        else:
            out["single_digit"] = "not reachable in registered memory (every registered record variant >= 10%)"
        if not below(regf + regu) and below(devall):
            out["copy_out"] = "required: every registered variant >= 10% while a device-memory variant < 10%"
        elif not below(regf) and below(devf):
            out["copy_out"] = "required for per-record publication only: fenced registered >= 10%, fenced device < 10%"
        else:
            out["copy_out"] = "not required by this rule"
        unstable = [p for p in rows if p["case"] in RECORD_CASES and
                    p["overhead_pct_min"] < THRESH_PCT <= p["overhead_pct_max"]]
        if unstable:
            out["unstable"] = ["%s %s f%d (%.1f-%.1f%%)" % (p["case"], p["mem"], p["fence"], p["overhead_pct_min"],
                                                            p["overhead_pct_max"]) for p in unstable]
        return out

    derived["decision"] = {s: decide([p for p in proj if p["shape"] == s])
                           for s in [PRIMARY_SHAPE] + SECONDARY_SHAPES}

    # (3) registered / device ratio per case
    ratios = []
    for iid, rec in summary.items():
        if rec["mem"] != "reg":
            continue
        other = summary.get(iid.replace("|reg|", "|dev|"))
        if other is None:
            continue
        a, b = med(rec, "delta_us"), med(other, "delta_us")
        ratios.append({"id": iid.replace("|reg|", "|*|"), "reg_delta_us": a, "dev_delta_us": b,
                       "ratio": (a / b) if (b is not None and b > 0.5 and other["resolved"]) else None})
    derived["reg_dev_ratio"] = sorted(ratios, key=lambda x: x["id"])
    # sanity: ref_empty vs empty, ref1512 loop form vs exit form
    san = {}
    for sh in sorted({it["shape"] for it in items.values()}):
        a = summary.get("%s|ref_empty|none|f0|R1|k0" % sh)
        san[sh] = {"ref_empty_minus_empty_us": med(a, "delta_us")}
        for mem in ("reg", "dev"):
            x, y = get(sh, "ref1512", mem, 1), get(sh, "ref1512_loop", mem, 1)
            if x and y:
                san[sh]["ref_loop_over_exit_%s" % mem] = med(y, "delta_us") / med(x, "delta_us")
    san["hold_capped_rows_excluded"] = len(capped_rows)
    san["hold_capped_rows_first"] = capped_rows[:10]
    derived["sanity"] = san

    json.dump(derived, open(os.path.join(out, "derived.json"), "w"), indent=1)
    fields = ["id", "case", "mem", "fence", "shape", "threads", "R", "k", "n", "valid", "resolved",
              "t_us", "delta_us", "ns_per_thread_op", "ns_per_warp_op", "ns_per_counted_op",
              "ops_per_launch", "slope_ns_per_warp_op", "slope_ns_per_thread_op", "slope_ns_per_counted_op",
              "occ_blocks_per_sm", "regs", "cap_hits", "k_max_concurrent"]
    with open(os.path.join(out, "summary.csv"), "w") as f:
        w = csv.writer(f)
        hdr = []
        for x in fields:
            if x in ("t_us", "delta_us", "ns_per_thread_op", "ns_per_warp_op", "ns_per_counted_op",
                     "ops_per_launch", "slope_ns_per_warp_op", "slope_ns_per_thread_op",
                     "slope_ns_per_counted_op"):
                hdr += [x + "_med", x + "_min", x + "_max"]
            else:
                hdr.append(x)
        w.writerow(hdr)
        for iid in sorted(summary):
            rec = summary[iid]
            row = []
            for x in fields:
                v = rec.get(x)
                if x in ("t_us", "delta_us", "ns_per_thread_op", "ns_per_warp_op", "ns_per_counted_op",
                         "ops_per_launch", "slope_ns_per_warp_op", "slope_ns_per_thread_op",
                         "slope_ns_per_counted_op"):
                    row += ["%.4g" % v[s] if v else "" for s in ("median", "min", "max")]
                else:
                    row.append(v)
            w.writerow(row)

    # human-readable summary
    L = []
    L.append("# e2-cost summary (%d rounds; medians, [min-max] over rounds)\n" % len(rounds))
    L.append("Smoke or loaded-board numbers are not measurements; only a run on a quiet board counts.\n")
    r1 = derived["reading"]["primary"]
    L.append("## (1) Reading of the 0.6 ns figure (shape %s, R = 1, registered)\n" % PRIMARY_SHAPE)
    if r1.get("ref_ns_per_thread") is not None:
        L.append("- today's 1512 op: %.3f ns per thread [%.3f-%.3f], %.2f ns per warp; llama figure %.3f ns per thread (ratio %.2f, gate %s%s)" % (
            r1["ref_ns_per_thread"], r1["ref_ns_per_thread_min_max"][0], r1["ref_ns_per_thread_min_max"][1],
            r1["ref_ns_per_warp"], LLAMA_PER_THREAD_NS, r1["gate_ratio_to_llama"],
            "ok" if r1["gate_ok"] else "FAILED", ", unstable" if r1["gate_unstable"] else ""))
        if r1.get("ref_loop_over_exit") is not None:
            L.append("- loop form / exit form of the reference at R = 1: %.2f (1.00 expected)" % r1["ref_loop_over_exit"])
        L.append("- one 32-byte record per warp by lane 0, with membar.sys: %.2f ns per warp (%.2fx the per-thread op, %.2fx the per-warp op)" % (
            r1["record_ns_per_warp"], r1["Q_over_g"], r1["Q_over_P"]))
    L.append("- verdict: **%s**" % r1["verdict"])
    for s in derived["reading"]["secondary"]:
        L.append("  - secondary %s, %s vs %s: %s" % (s["shape"], s["record"], s["reference"], s["verdict"]))
    L.append("\n## (2) [projection] one record per warp on every kernel of a pp512 (6.02M records, 204.6 ms)\n")
    L.append("| shape | case | memory | fence | ns/record | overhead % [min-max] |")
    L.append("|---|---|---|---|---:|---:|")
    for p in proj:
        if "ns_per_record" in p:
            L.append("| %s | %s | %s | %s | %.2f | %.1f [%.1f-%.1f] |" % (
                p["shape"], p["case"], p["mem"], "membar.sys" if p["fence"] else "none", p["ns_per_record"],
                p["overhead_pct"], p["overhead_pct_min"], p["overhead_pct_max"]))
        else:
            L.append("| %s | %s | %s | membar.sys | %.3f per thread | %.1f |" % (
                p["shape"], p["case"], p["mem"], p["ns_per_thread"], p["overhead_pct"]))
    for s, d in derived["decision"].items():
        L.append("\n- %s: single-digit percent: **%s**; copy-out from device memory: **%s**%s" % (
            s, d["single_digit"], d["copy_out"], ("; unstable: " + "; ".join(d["unstable"])) if d.get("unstable") else ""))
    L.append("\n## (3) registered / device ratio (median deltas; NA when the device delta is < 0.5 us or unresolved)\n")
    L.append("| case | reg delta us | dev delta us | ratio |")
    L.append("|---|---:|---:|---:|")
    for x in derived["reg_dev_ratio"]:
        if "|R1|" in x["id"]:
            L.append("| %s | %s | %s | %s |" % (x["id"], "%.1f" % x["reg_delta_us"] if x["reg_delta_us"] is not None else "",
                                            "%.1f" % x["dev_delta_us"] if x["dev_delta_us"] is not None else "",
                                            "%.1f" % x["ratio"] if x["ratio"] else "NA"))
    L.append("\n## Atomic contention, full occupancy (24 x 512), R = 1 and R = Rbig, ns per counted op\n")
    L.append("| case | mem | fence | k | R | ns/op [min-max] | slope ns/op |")
    L.append("|---|---|---|---:|---:|---:|---:|")
    for iid in sorted(summary, key=lambda i: (summary[i]["case"], summary[i]["mem"], summary[i]["fence"],
                                              summary[i]["R"], summary[i]["k"])):
        rec = summary[iid]
        if rec["shape"] != "full" or rec["case"] not in ("atom_sm", "cas_sm"):
            continue
        c = rec.get("ns_per_counted_op")
        sl = rec.get("slope_ns_per_counted_op")
        L.append("| %s | %s | %d | %d | %d | %s | %s |" % (
            rec["case"], rec["mem"], rec["fence"], rec["k"], rec["R"],
            "%.1f [%.1f-%.1f]" % (c["median"], c["min"], c["max"]) if c else "",
            "%.1f" % sl["median"] if sl else ""))
    L.append("\n## Sanity\n")
    L.append("```\n" + json.dumps(san, indent=1) + "\n```")
    bad = [i for i, it in items.items() if it["valid"] != "ok"]
    L.append("\nvalidation failures: %d %s" % (len(bad), bad[:10]))
    open(os.path.join(out, "summary.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L[:30]))


if __name__ == "__main__":
    main()
