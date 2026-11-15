#!/usr/bin/env python3
"""analyze_c.py OUTDIR

Part (c): the cost table from build/c1bench's OUTDIR/raw.csv and items.json.
Per item, per round: per-launch time; delta against c_none of the same
shape and round (same module), per thread execution = delta / threads. Also
against c_empty (the storage and its probe work without the bare probe
calls). Medians with [min-max] over rounds. A delta is "resolved" if it is
positive in every round.

[projection] as step0/e2-cost: one carry (entry write + exit read) per thread
execution on every pp512 kernel: 192.56M thread executions over 204.6 ms of
GPU time, overhead % = ns per thread x 192.56e6 / 204.6e6 x 100.

Comparability with e2: c_ref is e2's reference (threadhist's exit-only
lookup-and-increment, 1512, registered); e2 measured 0.433 ns (pass 1) and
0.431 (pass 2) per thread at the anchor shape. The ratio is printed; outside
[0.8, 1.25] the harness does not reproduce e2's number and the table is
marked "not comparable with e2".

Writes OUTDIR/summary.json, OUTDIR/summary.md.
"""
import csv
import json
import os
import statistics
import sys

E2_REF_NS = 0.433
PROJ_THREADS = 192.56e6
PROJ_NS = 204.6e6
ORDER = ["c_empty", "c_value", "c_local_d", "c_local", "c_slottag", "c_1512", "c_ref"]
LABEL = {
    "c_empty": "empty probes (both, return 0)",
    "c_value": "entry calls the timer helper, no storage (shared by 1-3)",
    "c_local_d": "(1) carry in .local, direct form",
    "c_local": "(1') carry in .local, via the lookup wrapper",
    "c_slottag": "(2) (SM, warp slot, lane) slot + owner tag, device memory",
    "c_1512": "(3) today's 1512 lookup, registered memory (entry write + exit read)",
    "c_ref": "e2's reference: threadhist exit-only lookup-and-increment (1512)",
}


def med(v):
    return (statistics.median(v), min(v), max(v)) if v else (None, None, None)


def main():
    out = sys.argv[1]
    items = json.load(open(os.path.join(out, "items.json")))
    rows = list(csv.DictReader(open(os.path.join(out, "raw.csv"))))
    rows = [r for r in rows if r["hold_capped"] == "0"]
    capped = items.get("hold_capped_rows", 0)
    t = {}
    for r in rows:
        t[(int(r["round"]), r["shape"], r["kernel"])] = float(r["per_launch_us"])
    shapes = sorted({r["shape"] for r in rows}, key=lambda s: ["anchor", "large", "full"].index(s))
    threads = {r["shape"]: int(r["threads"]) for r in rows}
    rounds = sorted({int(r["round"]) for r in rows})
    res = {"rounds": len(rounds), "hold_capped_rows_dropped": capped,
           "validation_failures": items.get("validation_failures"), "shapes": {}}
    for s in shapes:
        T = threads[s]
        sres = {}
        for k in ["c_none"] + ORDER:
            lat = [t[(r, s, k)] for r in rounds if (r, s, k) in t]
            dn = [(t[(r, s, k)] - t[(r, s, "c_none")]) * 1000.0 / T for r in rounds
                  if (r, s, k) in t and (r, s, "c_none") in t]
            de = [(t[(r, s, k)] - t[(r, s, "c_empty")]) * 1000.0 / T for r in rounds
                  if (r, s, k) in t and (r, s, "c_empty") in t]
            m, lo, hi = med(dn)
            sres[k] = {
                "launch_us": med(lat), "ns_per_thread_vs_none": (m, lo, hi),
                "resolved": bool(dn) and min(dn) > 0,
                "ns_per_thread_vs_empty": med(de),
                "projection_pct": (m * PROJ_THREADS / PROJ_NS * 100) if m is not None else None,
            }
        sres["ratio_ref_to_e2"] = (sres["c_ref"]["ns_per_thread_vs_none"][0] / E2_REF_NS
                                   if sres["c_ref"]["ns_per_thread_vs_none"][0] else None)
        res["shapes"][s] = sres
    reg = {it["kernel"]: (it["regs"], it["local_bytes"], it["occ_blocks_per_sm"])
           for it in items["items"] if it["shape"] == "anchor"}
    res["kernel_resources_anchor"] = reg
    # validation-only kernels: sabotaged twins, the valid mark and its control
    val = [{"id": it["id"], "valid": it["valid"], "detail": it["valid_detail"]}
           for it in items["items"] if not it.get("timed", True)]
    res["validation_only"] = val
    # the valid mark (README, part c validation): the exit-only kernels with
    # the mark 0 count a mismatch in every thread, and the _mark1 twin (the
    # prologue writes 1) counts none: the exit reads what the prologue wrote,
    # so the check can fail. Vacuous (no such items) is not ok.
    mark0 = [v for v in val if "_noentry" in v["id"] and "_noinit" not in v["id"] and "_mark1" not in v["id"]]
    mark1 = [v for v in val if "_mark1" in v["id"]]
    noinit = [v for v in val if "_noinit" in v["id"]]
    res["valid_mark_ok"] = bool(mark0) and all(v["valid"] == "ok" for v in mark0)
    res["valid_mark_can_fail"] = bool(mark1) and all(v["valid"] == "ok" for v in mark1)
    # reported, not judged: local memory is not cleared between kernels
    # (measured only), which is why a design needs the mark at all
    res["noinit_read_stale_as_valid"] = (bool(noinit) and all(
        "stale bytes read as valid: 0)" not in v["detail"] for v in noinit))
    # the carry kernels' own validation (0 mismatches, 0 NULL in every thread)
    res["carry_validation_failed"] = [it["id"] for it in items["items"]
                                      if it["kernel"] in ("c_local", "c_local_d") and it["valid"] != "ok"]
    res["validation_failed"] = [it["id"] for it in items["items"] if it["valid"] == "FAIL"]
    a = res["shapes"].get("anchor", {})
    comp = a.get("ratio_ref_to_e2")
    res["comparable_with_e2"] = comp is not None and 0.8 <= comp <= 1.25
    json.dump(res, open(os.path.join(out, "summary.json"), "w"), indent=1)

    def f(x, nd=3):
        return "NA" if x is None else ("%.*f" % (nd, x))

    L = ["# C1 part (c): cost per thread execution (entry write + exit read)", "",
         "rounds: %d; batches dropped for a capped gate: %s; validation failures: %s" % (
             len(rounds), capped, items.get("validation_failures")),
         "e2 comparability (anchor, c_ref / 0.433 ns): %s -> %s" % (
             f(comp, 2), "comparable" if res["comparable_with_e2"] else "NOT comparable with e2"), ""]
    for s in shapes:
        L.append("## %s (%d threads)" % (s, threads[s]))
        L.append("")
        L.append("| storage | ns per thread vs none, median [min-max] | vs empty probes | [projection] pp512 | resolved | regs, local B, blocks/SM (anchor) |")
        L.append("|---|---|---|---:|---|---|")
        for k in ORDER:
            x = res["shapes"][s][k]
            m, lo, hi = x["ns_per_thread_vs_none"]
            e = x["ns_per_thread_vs_empty"][0]
            L.append("| %s | %s [%s-%s] | %s | %s%% | %s | %s |" % (
                LABEL[k], f(m), f(lo), f(hi), f(e), f(x["projection_pct"], 1),
                "yes" if x["resolved"] else "no", reg.get(k)))
        L.append("")
        L.append("c_none launch: %s us (median)" % f(res["shapes"][s]["c_none"]["launch_us"][0], 2))
        L.append("")
    L.append("## Validation-only kernels")
    L.append("")
    L.append("valid mark (exit without entry finds 0 in every thread): %s; the mark-1 twin read its "
             "prologue's 1 as valid in every thread (so the check can fail): %s; the no-mark control "
             "read stale bytes from the previous kernel as valid (reported): %s" % (
                 res["valid_mark_ok"], res["valid_mark_can_fail"], res["noinit_read_stale_as_valid"]))
    L.append("carry kernels whose validation failed: %s; all failed validations: %s" % (
        res["carry_validation_failed"] or "none", res["validation_failed"] or "none"))
    L.append("")
    for v in val:
        L.append("- %s: %s %s" % (v["id"], v["valid"], v["detail"]))
    L.append("")
    open(os.path.join(out, "summary.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
