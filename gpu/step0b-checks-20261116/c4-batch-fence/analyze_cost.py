#!/usr/bin/env python3
"""C4 cost analysis: raw.csv + items.json of one bfc run -> summary.csv,
derived.json, summary.md (decision rules in README.md, "Decision rules").

Usage: python3 -I analyze_cost.py RUN_DIR
"""
import csv
import json
import os
import statistics
import sys

STEP0_REC_W32 = 1.61  # step 0 e2, anchor, rec_lane0 reg f0, ns per warp
STEP0_RF_SCSYS = 10.06  # step 0 e2, anchor, rec_lane0 reg f1
FIXED_T = 2 * STEP0_REC_W32  # 3.22 ns
TEN_PCT = 3.40  # ns per record: 10% of 204.6 ms over 6.02M records
PROJ_RECORDS = 6.02e6
PROJ_NS = 204.6e6
PROTOCOLS = ["tf_sc", "ar_sc", "rel_sc", "ar_ar", "rel_ar", "ar_aa", "rmw_ar",
             "ar32_sc", "ar32_ar", "ar32_aa"]
# every link a literal §8.8 pattern, the completer's acquire and its system
# release in separate instructions (no reading needed)
LITERAL = {"ar_aa", "rmw_ar", "ar32_aa"}
# literal patterns, but one completer fence closes the acquire pattern and
# opens the release pattern (needs the endpoint reading, README)
SHARED = {"ar_ar", "rel_ar", "ar32_ar"}


def tag(v):
    return " (literal, GPU side)" if v in LITERAL else " (literal, shared fence)" if v in SHARED else ""


def rank_key(r):
    """Pre-registered ranking: reached variants first, by the smallest G*,
    then by ns at G*; unreached variants after them, by their lowest ns."""
    if r.get("G_star") is not None:
        return (0, r["G_star"], r["ns"])
    return (1, 0, r.get("asymptote_ns", 1e9))
CONTROLS = ["nofence", "nosys", "sysonly"]


def proj(ns):
    return 100.0 * PROJ_RECORDS * ns / PROJ_NS


def med(v):
    return statistics.median(v) if v else float("nan")


def main():
    run = sys.argv[1]
    for f in ("raw.csv", "items.json"):
        if not os.path.exists(os.path.join(run, f)):
            print("missing %s in %s: bfc did not get past validation; no analysis" % (f, run))
            sys.exit(2)
    rows = list(csv.DictReader(open(os.path.join(run, "raw.csv"))))
    meta = json.load(open(os.path.join(run, "items.json")))
    items = {it["id"]: it for it in meta["items"]}
    rounds_done = int(meta.get("rounds_done", 0))
    rounds_req = int(meta.get("rounds_requested", 0))
    # review: the rules decide on medians over the requested rounds; a run cut
    # short (max-wall or timeout) is reported as PARTIAL, with its round count
    partial = rounds_done < rounds_req or not meta.get("final", True)
    dropped_cap = sum(1 for r in rows if r["hold_capped"] != "0")
    dropped_post = sum(1 for r in rows if r["post_ok"] != "1")
    good = [r for r in rows if r["hold_capped"] == "0" and r["post_ok"] == "1"]
    by = {}
    for r in good:
        by.setdefault(r["id"], {})[int(r["round"])] = float(r["per_launch_us"])
    empty = {}
    for iid, d in by.items():
        it = items[iid]
        if it["variant"] == "empty":
            empty[it["shape"]] = d
    out = {}
    for iid, d in by.items():
        it = items[iid]
        e = empty.get(it["shape"], {})
        deltas = [d[k] - e[k] for k in d if k in e]
        if not deltas:
            continue
        w = it["warps"]
        thr = w * 32
        per_warp = [x * 1000.0 / w for x in deltas]
        out[iid] = {
            "id": iid, "shape": it["shape"], "layout": it["layout"], "variant": it["variant"],
            "G": it["G"], "cnt": it["cnt"], "warps": w, "chunks": it["chunks"],
            "valid": it["valid"], "rounds": len(deltas),
            "us": med(list(d.values())), "delta_us": med(deltas),
            "delta_min": min(deltas), "delta_max": max(deltas),
            "resolved": all(x > 0 for x in deltas),
            "ns_warp": med(per_warp), "ns_warp_min": min(per_warp), "ns_warp_max": max(per_warp),
            "ns_lane": med(deltas) * 1000.0 / thr,
            "proj_pct": proj(med(per_warp)),
        }
    # summary.csv
    keys = ["id", "shape", "layout", "variant", "G", "cnt", "warps", "chunks", "valid", "rounds",
            "us", "delta_us", "delta_min", "delta_max", "resolved", "ns_warp", "ns_warp_min",
            "ns_warp_max", "ns_lane", "proj_pct"]
    with open(os.path.join(run, "summary.csv"), "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=keys)
        wr.writeheader()
        for k in sorted(out):
            wr.writerow(out[k])

    def get(shape, layout, variant, G=0, cnt=None):
        c = cnt if cnt is not None else ("dev" if G else "-")
        return out.get("%s|%s|%s|G%d|%s" % (shape, layout, variant, G, c))

    shapes = [s for s in ("anchor", "large", "full") if any(v["shape"] == s for v in out.values())]
    gs = sorted({v["G"] for v in out.values() if v["G"]})
    short = sorted(k for k, v in out.items() if v["rounds"] < rounds_done)
    derived = {"dropped_hold_capped": dropped_cap, "dropped_post_check": dropped_post,
               "invalid_items": sorted(i for i, it in items.items() if it["valid"] != "ok"),
               "rounds_done": rounds_done, "rounds_requested": rounds_req, "partial": partial,
               "items_with_fewer_rounds": short,
               "shapes": {}}
    md = ["# C4 cost summary (bfc)", "",
          "Medians over rounds of (item - empty kernel of the same shape), paired per round; "
          "[min-max] over rounds. ns per warp record = delta / warps. "
          "[projection] = 6.02M warp records x ns / 204.6 ms (step 0's pp512 figure, empty kernels). "
          "A smoke or a loaded board is not a measurement.", "",
          "- rows dropped: %d (gate capped), %d (post-check failed); invalid items: %s"
          % (dropped_cap, dropped_post, derived["invalid_items"] or "none"),
          "- rounds: %d of %d requested%s; items with fewer usable rounds than that: %s"
          % (rounds_done, rounds_req,
             " -- **PARTIAL run: the decision below rests on fewer rounds than the rules require**" if partial else "",
             ", ".join(short[:20]) + (" ..." if len(short) > 20 else "") if short else "none"), ""]
    for shape in shapes:
        sd = {"gate": {}, "thresholds": {}, "protocols": {}, "floors": {}}
        derived["shapes"][shape] = sd
        md += ["## Shape %s%s" % (shape, " (decides)" if shape == "anchor" else " (secondary)"), ""]
        # gate (anchor): comparable to step 0
        rec = get(shape, "w32", "rec")
        rf = get(shape, "w32", "rf_scsys")
        if shape == "anchor" and rec and rf and rec["valid"] == "ok" and rf["valid"] == "ok":
            r1, r2 = rec["ns_warp"] / STEP0_REC_W32, rf["ns_warp"] / STEP0_RF_SCSYS
            comp = 0.75 <= r1 <= 1.33 and 0.75 <= r2 <= 1.33
            sd["gate"] = {"rec_w32": rec["ns_warp"], "ratio_rec": r1, "rf_scsys_w32": rf["ns_warp"],
                          "ratio_rf": r2, "comparable": comp}
            md += ["- gate: record only %.2f ns (step 0: 1.61, ratio %.2f); record + fence.sc.sys %.2f ns "
                   "(step 0: 10.06, ratio %.2f) -> %s" % (rec["ns_warp"], r1, rf["ns_warp"], r2,
                   "**comparable to step 0**" if comp else "**NOT comparable to step 0** (report only)"), ""]
        # references
        md += ["### References (ns per warp)", "", "| item | w32 | l8 | l8 per lane |", "|---|---:|---:|---:|"]
        for v in ["rec", "rf_scgpu", "rf_argpu", "rf_scsys", "rf_arsys", "pw_sc", "pw_ar"]:
            a, b = get(shape, "w32", v), get(shape, "l8", v)
            md.append("| %s | %s | %s | %s |" % (
                v, "%.2f [%.2f-%.2f]" % (a["ns_warp"], a["ns_warp_min"], a["ns_warp_max"]) if a else "-",
                "%.2f [%.2f-%.2f]" % (b["ns_warp"], b["ns_warp_min"], b["ns_warp_max"]) if b else "-",
                "%.3f" % b["ns_lane"] if b else "-"))
        for f in ["fonly_scgpu", "fonly_argpu", "fonly_scsys", "fonly_arsys"]:
            a = get(shape, "-", f)
            if a:
                md.append("| %s (lane 0, no store) | %.2f [%.2f-%.2f] | | |" % (f, a["ns_warp"], a["ns_warp_min"], a["ns_warp_max"]))
        md.append("")
        for layout in ("w32", "l8"):
            recl = get(shape, layout, "rec")
            if not recl:
                continue
            if recl["valid"] != "ok":
                md += ["### %s: record-only item INVALID, so no threshold T and no decision for this layout" % layout, ""]
                sd["thresholds"][layout] = {"T": None, "reason": "rec invalid"}
                continue
            T = 2 * recl["ns_warp"]
            sd["thresholds"][layout] = {"T": T, "fixed": FIXED_T, "ten_pct": TEN_PCT}
            variants = CONTROLS + PROTOCOLS + (["ar_sc|reg"] if layout == "w32" else [])
            md += ["### %s: ns per warp record vs warps per system fence (G)" % layout, "",
                   "T = 2 x this run's record-only cost = %.2f ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%%)." % T, "",
                   "| variant | " + " | ".join("G=%d" % g for g in gs) + " |",
                   "|---|" + "---:|" * len(gs)]
            for v in variants:
                name, cnt = (v.split("|") + ["dev"])[:2]
                cells, any_ = [], False
                for g in gs:
                    x = get(shape, layout, name, g, cnt)
                    if x:
                        any_ = True
                        mark = "" if x["valid"] == "ok" else " INVALID"
                        cells.append("%.2f%s%s" % (x["ns_warp"], "*" if x["ns_warp_max"] <= T else "", mark))
                    else:
                        cells.append("-")
                if any_:
                    md.append("| %s%s | %s |" % (name, " (counter in registered memory)" if cnt == "reg" else "", " | ".join(cells)))
            md += ["", "(* = round max <= T)", ""]
            # floors: the counter alone
            nf = [(g, get(shape, layout, "nofence", g)) for g in gs]
            nf = [(g, x) for g, x in nf if x]
            if nf:
                gbest, xbest = min(nf, key=lambda t: t[1]["ns_warp"])
                sd["floors"][layout] = {"nofence_min": xbest["ns_warp"], "at_G": gbest,
                                        "above_T": xbest["ns_warp"] > T}
            # decision per protocol
            md += ["Decision (%s, %s):" % (shape, layout), ""]
            res = {}
            for v in PROTOCOLS:
                pts = [(g, get(shape, layout, v, g)) for g in gs]
                pts = [(g, x) for g, x in pts if x and x["valid"] == "ok"]
                if not pts:
                    continue
                under = [(g, x) for g, x in pts if x["ns_warp"] <= T]
                r = {"literal": v in LITERAL, "literal_shared_fence": v in SHARED,
                     "points": {g: x["ns_warp"] for g, x in pts}}
                if under:
                    g0, x0 = under[0]
                    r.update({"G_star": g0, "ns": x0["ns_warp"], "stable": x0["ns_warp_max"] <= T,
                              "proj_pct": x0["proj_pct"],
                              "above_T_at_larger_G": [g for g, x in pts if g > g0 and x["ns_warp"] > T]})
                    md.append("- %s%s: **G* = %d**, %.2f ns per warp record (%s), [projection] %.1f%%%s"
                              % (v, tag(v), g0, x0["ns_warp"],
                                 "stable" if r["stable"] else "marginal: round max %.2f > T" % x0["ns_warp_max"],
                                 x0["proj_pct"],
                                 "; non-monotone: above T again at G = %s" % r["above_T_at_larger_G"] if r["above_T_at_larger_G"] else ""))
                else:
                    gb, xb = min(pts, key=lambda t: t[1]["ns_warp"])
                    fl = sd["floors"].get(layout, {})
                    r.update({"G_star": None, "asymptote_ns": xb["ns_warp"], "at_G": gb, "proj_pct": xb["proj_pct"]})
                    md.append("- %s%s: **not under T in the sweep**; lowest %.2f ns at G = %d, [projection] %.1f%%%s"
                              % (v, tag(v), xb["ns_warp"], gb, xb["proj_pct"],
                                 "; the counter alone (nofence) is already %.2f ns > T" % fl["nofence_min"] if fl.get("above_T") else ""))
                for thr_name, thr in (("fixed_3.22", FIXED_T), ("ten_pct_3.40", TEN_PCT)):
                    u = [g for g, x in pts if x["ns_warp"] <= thr]
                    r["G_" + thr_name] = u[0] if u else None
                res[v] = r
            sd["protocols"][layout] = res
            if res:
                order = sorted(res.items(), key=lambda kv: rank_key(kv[1]))
                lit = [kv for kv in order if kv[0] in LITERAL]
                shr = [kv for kv in order if kv[0] in SHARED]
                sd["ranking"] = sd.get("ranking", {})
                sd["ranking"][layout] = {"all": [k for k, _ in order], "literal": [k for k, _ in lit],
                                         "literal_shared_fence": [k for k, _ in shr]}
                desc = lambda kv: ("%s (G* = %s, %.2f ns, %.1f%%)" % (kv[0], kv[1]["G_star"], kv[1]["ns"], kv[1]["proj_pct"])
                                   if kv[1].get("G_star") is not None else
                                   "%s (not reached; lowest %.2f ns at G = %d, %.1f%%)" % (kv[0], kv[1]["asymptote_ns"], kv[1]["at_G"], kv[1]["proj_pct"]))
                rmw = [kv for kv in order if kv[0] == "rmw_ar"]
                md += ["", "- ranking (smallest G*, then ns at G*; unreached last): " + "; ".join(k for k, _ in order),
                       "- **decided output (%s, %s): the best literal protocol (GPU side) is %s**; best with a shared completer fence: %s; best overall: %s"
                       % (shape, layout, desc(lit[0]) if lit else "-", desc(shr[0]) if shr else "-", desc(order[0])),
                       "- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the "
                       "'acquire memory fence')%s: %s" % (
                           "; in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings"
                           if layout == "l8" else "", desc(rmw[0]) if rmw else "-"),
                       "- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking",
                       "- secondary thresholds: " + "; ".join(
                           "%s: G(3.22)=%s G(3.40)=%s" % (k, v["G_fixed_3.22"], v["G_ten_pct_3.40"]) for k, v in res.items()), ""]
            # fence share at each G
            md += ["Fence share = protocol - nofence at the same G (ns per warp record):", "",
                   "| variant | " + " | ".join("G=%d" % g for g in gs) + " |", "|---|" + "---:|" * len(gs)]
            for v in PROTOCOLS + ["nosys", "sysonly"]:
                cells, any_ = [], False
                for g in gs:
                    x, n = get(shape, layout, v, g), get(shape, layout, "nofence", g)
                    if x and n:
                        any_ = True
                        cells.append("%.2f" % (x["ns_warp"] - n["ns_warp"]))
                    else:
                        cells.append("-")
                if any_:
                    md.append("| %s | %s |" % (v, " | ".join(cells)))
            md.append("")
    json.dump(derived, open(os.path.join(run, "derived.json"), "w"), indent=1)
    open(os.path.join(run, "summary.md"), "w").write("\n".join(md) + "\n")
    print("\n".join(md))


if __name__ == "__main__":
    main()
