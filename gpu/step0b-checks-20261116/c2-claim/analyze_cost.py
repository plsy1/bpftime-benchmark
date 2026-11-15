#!/usr/bin/env python3
"""analyze_cost.py BENCH_DIR [CHK_SUMMARY_JSON]: check C2's cost numbers (rules in README.md).

From BENCH_DIR/raw.csv and items.json (c2bench), per item and round:
  delta      = t(item) - t(empty kernel of the same module, shape and round)   [us per launch]
  ns/thread  = delta / threads, ns/warp = delta / warps
  incr_wid   = t(item) - t(c_wid), incr_probe = t(item) - t(c_probe), paired per round, per thread
  [projection] % of one pp512 = ns/thread x 192.56e6 / 204.6e6 ms x 100
Medians with [min-max] over rounds. Rows with hold_capped are dropped (counted); rows whose pool
was not clean afterwards are counted per item. Writes BENCH_DIR/cost_summary.{json,md}.
Standard library only.
"""
import csv
import json
import os
import statistics
import sys

PP512_THREADS = 192.56e6
PP512_MS = 204.6
SMALL_NS = 0.053   # 5% of one pp512 [projection]
MODERATE_NS = 0.106  # 10%
MISS_RATE_MAX = 0.01
TODAY_1512_NS = 0.433  # step 0, check 2 (pass 1): today's 1512 lookup-and-increment, anchor

DECISION = {  # group -> decision kernels (documented-ordering variants with per-lane accounting)
    "warp_activemask": ["c_bmsm_o_lane", "c_bmglob_o_lane", "c_stksm_o_lane", "c_stkglob_o_lane"],
    "warp_constructed_mask": ["c_bmsm_oc_lane", "c_bmglob_oc_lane", "c_stksm_oc_lane", "c_stkglob_oc_lane"],
    "lane": ["c_lnsm_o_lane", "c_lnglob_o_lane", "c_lfsm_o_lane", "c_lfglob_o_lane"],
}


def proj(ns):
    return ns * PP512_THREADS / (PP512_MS * 1e6) * 100.0


def mmm(v):
    if not v:
        return None
    return {"med": statistics.median(v), "min": min(v), "max": max(v), "n": len(v)}


def main():
    bd = sys.argv[1]
    chk = json.load(open(sys.argv[2])) if len(sys.argv) > 2 and os.path.exists(sys.argv[2]) else None
    items = json.load(open(os.path.join(bd, "items.json")))
    rows = list(csv.DictReader(open(os.path.join(bd, "raw.csv"))))
    dropped = sum(1 for r in rows if r["hold_capped"] == "1")
    rows = [r for r in rows if r["hold_capped"] != "1"]
    t = {}  # (round, id) -> us per launch
    meta = {it["id"]: it for it in items["items"]}
    agg = {}
    for r in rows:
        t[(r["round"], r["id"])] = float(r["per_launch_us"])
        a = agg.setdefault(r["id"], {"misses": 0, "miss_lanes": 0, "lanes": 0, "cap_claim": 0, "cap_spin": 0,
                                     "tag_bad": 0, "unclean": 0, "spin_extra": 0})
        a["misses"] += int(r["misses"])
        a["miss_lanes"] += int(r["miss_lanes"])
        a["lanes"] += int(r["threads"]) * int(r["n_launch"])
        a["cap_claim"] += int(r["cap_claim"])
        a["cap_spin"] += int(r["cap_spin"])
        a["tag_bad"] += int(r["tag_bad"])
        a["spin_extra"] += int(r["spin_extra"])
        a["unclean"] += r["end_clean"] != "1"
    rounds = sorted({r["round"] for r in rows}, key=int)
    res = {}
    for iid, it in meta.items():
        shape, k, f = iid.split("|")
        base = "%s|%s|f1" % (shape, "c_empty" if it["module"] == "k" else "rp_empty")
        T, W = it["threads"], it["warps"]
        d, iw, ip = [], [], []
        for rd in rounds:
            if (rd, iid) not in t or (rd, base) not in t:
                continue
            d.append(t[(rd, iid)] - t[(rd, base)])
            w = (rd, "%s|c_wid|f1" % shape)
            p = (rd, "%s|c_probe|f1" % shape)
            if it["module"] == "k" and w in t:
                iw.append(t[(rd, iid)] - t[w])
            if it["module"] == "k" and p in t:
                ip.append(t[(rd, iid)] - t[p])
        a = agg.get(iid, {})
        x = {"id": iid, "shape": shape, "kernel": k, "factor": f, "pool": it["pool"], "valid": it["valid"],
             "valid_detail": it["valid_detail"], "regs": it["regs"], "occ_blocks_per_sm": it["occ_blocks_per_sm"],
             "delta_us": mmm(d),
             "ns_per_thread": mmm([v * 1e3 / T for v in d]), "ns_per_warp": mmm([v * 1e3 / W for v in d]),
             "incr_wid_ns_per_thread": mmm([v * 1e3 / T for v in iw]),
             "incr_probe_ns_per_thread": mmm([v * 1e3 / T for v in ip]),
             "resolved": bool(d) and all(v > 0 for v in d),
             "miss_rate": (a.get("miss_lanes", 0) / a["lanes"]) if a.get("lanes") else 0.0,
             "cap_claim": a.get("cap_claim", 0), "cap_spin": a.get("cap_spin", 0), "tag_bad": a.get("tag_bad", 0),
             "unclean_rows": a.get("unclean", 0)}
        if x["ns_per_thread"]:
            x["projection_pct"] = proj(x["ns_per_thread"]["med"])
        if x["incr_wid_ns_per_thread"]:
            x["incr_wid_projection_pct"] = proj(x["incr_wid_ns_per_thread"]["med"])
        res[iid] = x
    # decision at the anchor shape
    dec = {}
    correct = {}
    if chk:
        for v in chk.get("variants", []):
            correct["c_%s_%s" % (v["pool"], v["exit"])] = v["verdict"]
    def row_problem(x):
        """A row cannot carry the cost decision if its own accounting failed, or if its claims
        missed (a missed claim skips the work, so the row would look cheaper than it is)."""
        p = []
        if x["valid"] != "ok":
            p.append("validation %s" % x["valid"])
        if x["unclean_rows"] or x["cap_spin"] or x["tag_bad"]:
            p.append("unclean rows %d, cap_spin %d, tag_bad %d" % (x["unclean_rows"], x["cap_spin"], x["tag_bad"]))
        if x["miss_rate"] > MISS_RATE_MAX:
            p.append("miss rate %.3f > %.2f" % (x["miss_rate"], MISS_RATE_MAX))
        return p

    excluded = {}
    for g, ks in DECISION.items():
        cand = []
        excluded[g] = []
        for k in ks:
            x = res.get("anchor|%s|f1" % k)
            if not x or not x["incr_wid_ns_per_thread"]:
                continue
            row = x
            note = ""
            prob = row_problem(x)
            if prob and x["miss_rate"] > MISS_RATE_MAX and len(prob) == 1:
                x2 = res.get("anchor|%s|f2" % k)
                note = "miss rate %.3f at the resident-count pool" % x["miss_rate"]
                if x2 and x2["incr_wid_ns_per_thread"] and not row_problem(x2):
                    row, prob, note = x2, [], note + "; the 2x pool row is used"
                elif x2:
                    prob = prob + ["2x row: %s" % ("; ".join(row_problem(x2)) or "no paired data")]
                else:
                    prob = prob + ["no 2x row"]
            if prob:
                excluded[g].append("%s: %s" % (k, "; ".join(prob)))
                continue
            v = correct.get(k, "unknown (no chk summary)")
            cand.append((row["incr_wid_ns_per_thread"]["med"], k, row["id"], v, note))
        ok = [c for c in cand if c[3] == "correct"]
        use = sorted(ok or cand)
        if not use:
            dec[g] = {"verdict": "no usable row", "excluded": excluded[g]}
            continue
        inc, k, rid, v, note = use[0]
        if inc <= SMALL_NS:
            word = "small"
        elif inc <= MODERATE_NS:
            word = "moderate"
        else:
            word = "large"
        dec[g] = {"kernel": k, "row": rid, "chk_verdict": v, "incr_over_wid_ns_per_thread": inc,
                  "incr_projection_pct": proj(inc), "class": word, "note": note,
                  "ns_per_thread": res[rid]["ns_per_thread"]["med"], "projection_pct": res[rid].get("projection_pct"),
                  "chosen_from": "chk-correct variants" if ok else "all variants (none chk-correct or no chk summary)",
                  "excluded": excluded[g]}
    out = {"rounds": len(rounds), "dropped_hold_capped": dropped, "validation_failures": items["validation_failures"],
           "unclean_rows": items.get("unclean_rows"), "items": res, "decision": dec,
           "thresholds": {"small_ns": SMALL_NS, "moderate_ns": MODERATE_NS, "miss_rate_max": MISS_RATE_MAX}}
    json.dump(out, open(os.path.join(bd, "cost_summary.json"), "w"), indent=1)
    L = ["# C2 cost summary", "",
         "rounds %d, hold-capped rows dropped %d, validation failures %d, unclean rows %s" % (
             len(rounds), dropped, items["validation_failures"], items.get("unclean_rows")), "",
         "## Decision (anchor; increment over the %%warpid-addressed slot; small <= %.3f ns = 5%%, moderate <= %.3f ns = 10%%)" % (SMALL_NS, MODERATE_NS), ""]
    for g, x in dec.items():
        if "kernel" not in x:
            L.append("- %s: %s%s" % (g, x["verdict"], ("; excluded: " + " | ".join(x["excluded"])) if x.get("excluded") else ""))
            continue
        L.append("- %s: **%s** (%s, chk %s): +%.4f ns per thread execution over c_wid [projection %.1f%%]; "
                 "whole mechanism %.4f ns [projection %.1f%%]; chosen from %s%s" % (
                     g, x["class"], x["row"], x["chk_verdict"], x["incr_over_wid_ns_per_thread"], x["incr_projection_pct"],
                     x["ns_per_thread"], x["projection_pct"], x["chosen_from"], ("; " + x["note"]) if x["note"] else ""))
        if x.get("excluded"):
            L.append("  - rows that could not carry the decision: " + " | ".join(x["excluded"]))
    L += ["", "Reference: today's 1512 op %.3f ns per thread execution at anchor (step 0, check 2)." % TODAY_1512_NS, ""]
    for shape in ("anchor", "large", "full"):
        L += ["## %s" % shape, "",
              "| item | ns/thread [min-max] | ns/warp | [projection] | incr over c_wid | incr over c_probe | miss rate | caps | valid |",
              "|---|---:|---:|---:|---:|---:|---:|---:|---|"]
        for iid in sorted(k for k in res if k.startswith(shape + "|")):
            x = res[iid]
            if not x["ns_per_thread"]:
                continue
            n = x["ns_per_thread"]
            iw = x["incr_wid_ns_per_thread"]
            ipr = x["incr_probe_ns_per_thread"]
            L.append("| %s f%s | %.4f [%.4f-%.4f]%s | %.2f | %.1f%% | %s | %s | %.4f | %d/%d | %s |" % (
                x["kernel"], x["factor"][1:], n["med"], n["min"], n["max"], "" if x["resolved"] else " (unresolved)",
                x["ns_per_warp"]["med"], x.get("projection_pct", 0),
                "%.4f" % iw["med"] if iw else "-", "%.4f" % ipr["med"] if ipr else "-", x["miss_rate"],
                x["cap_claim"], x["cap_spin"], x["valid"]))
        L.append("")
    open(os.path.join(bd, "cost_summary.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L[:20]))


if __name__ == "__main__":
    main()
