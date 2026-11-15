#!/usr/bin/env python3
"""analyze_d.py BUILD_D OCC_CSV OUTDIR

Part (d): the carry in llama.cpp's rms_norm module. Combines the compile
results (BUILD_D/d_compile.json, tools/build_d.py) with the occupancy the
driver reports for the same cubins (OCC_CSV, build/occ). Writes
OUTDIR/d_summary.json and d_summary.txt; the last line is the verdict.

Rules (README, part d):
  - PASS if ptxas accepted every variant and the driver loaded every cubin
    and found the kernel (one occ row per variant).
  - FAIL if ptxas rejected a variant, or occ reported a driver error while
    loading a cubin or looking up the kernel (cuModuleLoad /
    cuModuleGetFunction).
  - INCONCLUSIVE otherwise: rows missing without such an error (the occ
    step was skipped, timed out, or failed for another reason).
  - The bound is the change from the same probes without routing to the
    routed ones: N -> H16 (helper form), N -> D16/D64/D256 (direct form), and
    P0e -> P0c/P0cd (into the kept patched module, which also adds the
    entry program's own work, so it is an upper bound): registers per thread,
    cumulative stack, blocks per SM at the kernel's 512 threads per block.
    An occupancy drop is reported as an "occupancy cost", not a failure.
  - Memory: cumulative stack x resident threads (SMs x max threads per SM),
    split into the carry (carry bytes + alignment padding) and the rest; and
    whether the kernel stays within the context's default stack limit
    (CU_LIMIT_STACK_SIZE), below which the driver needs no larger per-thread
    stack [inferred: the driver's sizing of local memory is not documented].
"""
import csv
import json
import os
import re
import sys

PAIRS = [("N", "H16"), ("N", "D16"), ("N", "D64"), ("N", "D256"), ("P0e", "P0c"), ("P0e", "P0cd"),
         ("P0", "P0c"), ("U", "N")]


def main():
    bd, occ_csv, out = sys.argv[1:4]
    comp = json.load(open(os.path.join(bd, "d_compile.json")))
    occ = {}
    occ_errors = []
    if os.path.exists(occ_csv):
        lines = open(occ_csv).read().split("\n")
        # occ's CK() messages: "occ.cpp:LINE: <call> -> <rc> (<text>)"
        occ_errors = [l for l in lines if re.match(r"^\S*occ\.cpp:\d+: .* -> \d+ \(", l)]
        data = [l for l in lines if l and l not in occ_errors]
        for r in csv.DictReader(data):
            if r.get("cubin") and r.get("blocks_per_sm_at_512"):
                occ[os.path.basename(r["cubin"]).rsplit(".", 1)[0]] = r
    load_errors = [l for l in occ_errors if "cuModuleLoad" in l or "cuModuleGetFunction" in l]
    V = comp["variants"]
    rows = {}
    ok = True
    ptxas_fail = False
    for name, r in V.items():
        k = r["kernel"] or {}
        o = occ.get(name)
        if r["ptxas_rc"] != 0 or not k:
            ptxas_fail = True
        if r["ptxas_rc"] != 0 or not k or o is None:
            ok = False
        resident = int(o["sm_count"]) * int(o["max_threads_per_sm"]) if o else None
        rows[name] = {
            "ptxas": r["ptxas_rc"] == 0, "regs": k.get("regs"), "cumulative_stack": k.get("cumulative_stack"),
            "spills": (k.get("spill_stores") or 0) + (k.get("spill_loads") or 0),
            "occ_arith_512": r.get("occ_blocks_512_arith"),
            "occ_driver_512": int(o["blocks_per_sm_at_512"]) if o else None,
            "driver_regs": int(o["regs"]) if o else None,
            "driver_local_bytes": int(o["local_bytes"]) if o else None,
            "stack_limit": int(o["stack_limit_after"]) if o else None,
            "resident_threads": resident,
            "stack_MiB_resident": (k.get("cumulative_stack", 0) * resident / 2**20) if (o and k) else None,
            "carry_bytes": r.get("carry_bytes"),
        }
    bounds = []
    occ_cost = []
    for a, b in PAIRS:
        if a in rows and b in rows and rows[a]["regs"] is not None and rows[b]["regs"] is not None:
            x, y = rows[a], rows[b]
            d = {"from": a, "to": b, "d_regs": y["regs"] - x["regs"],
                 "d_stack": y["cumulative_stack"] - x["cumulative_stack"],
                 "occ_from": x["occ_driver_512"], "occ_to": y["occ_driver_512"]}
            if y["resident_threads"]:
                d["d_stack_KiB_resident"] = d["d_stack"] * y["resident_threads"] / 1024
            bounds.append(d)
            if x["occ_driver_512"] is not None and y["occ_driver_512"] is not None and \
                    y["occ_driver_512"] < x["occ_driver_512"]:
                occ_cost.append("%s->%s" % (a, b))
    within = {n: (r["cumulative_stack"] is not None and r["stack_limit"] is not None
                  and r["cumulative_stack"] <= r["stack_limit"]) for n, r in rows.items()}
    verdict = "PASS" if ok else ("FAIL" if (ptxas_fail or load_errors) else "INCONCLUSIVE")
    res = {"kernel": comp["kernel"], "rows": rows, "bounds": bounds, "occupancy_cost": occ_cost,
           "within_default_stack_limit": within, "occ_errors": occ_errors,
           "occ_rows": len(occ), "verdict": verdict}
    json.dump(res, open(os.path.join(out, "d_summary.json"), "w"), indent=1)
    L = ["kernel: " + comp["kernel"],
         "variant  ptxas regs(ptxas/driver) cum.stack local(driver) spills blocks/SM@512(arith/driver) "
         "stack x resident (MiB) <= stack limit"]
    for n, r in rows.items():
        L.append("%-8s %-5s %4s/%-4s %9s %12s %6s %8s/%-8s %10s %s" % (
            n, "ok" if r["ptxas"] else "FAIL", r["regs"], r["driver_regs"], r["cumulative_stack"],
            r["driver_local_bytes"], r["spills"], r["occ_arith_512"], r["occ_driver_512"],
            "%.2f" % r["stack_MiB_resident"] if r["stack_MiB_resident"] is not None else "NA",
            within[n]))
    L.append("bounds (from -> to: registers, cumulative stack bytes, KiB over resident threads, blocks/SM):")
    for d in bounds:
        L.append("  %-4s -> %-5s %+d regs, %+d B stack (%+.0f KiB), %s -> %s blocks/SM" % (
            d["from"], d["to"], d["d_regs"], d["d_stack"], d.get("d_stack_KiB_resident", 0),
            d["occ_from"], d["occ_to"]))
    L.append("occupancy cost: %s" % (", ".join(occ_cost) if occ_cost else "none"))
    L.append("occ rows: %d of %d variants; occ errors: %s" % (len(occ), len(V), occ_errors or "none"))
    L.append("C1 part d verdict: %s" % res["verdict"])
    open(os.path.join(out, "d_summary.txt"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
