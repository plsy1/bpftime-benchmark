#!/usr/bin/env python3
"""analyze_chk.py OUT: verdicts of check C2's correctness runs (decision rules in README.md).

Reads OUT/native/results.jsonl, OUT/jit/results.jsonl, OUT/hold_hog/results.jsonl (+
hog_alive.txt), OUT/hold_ctl/results.jsonl and the meta files; writes OUT/chk_summary.json and
OUT/chk_summary.md. `--selftest` runs the rules on synthetic runs that must give each verdict.
Standard library only.
"""
import json
import os
import sys

WARP_POOLS = ["bmsm", "bmglob", "stksm", "stkglob"]
LANE_POOLS = ["lnsm", "lnglob", "lfsm", "lfglob"]
DIV_EXIT_CASES = ["early", "lowlanes", "nested", "loops", "entrydiv", "churnsync"]  # exits diverge
DIV_ALL = ["early", "lowlanes", "nested", "loops", "split", "entrydiv", "churn", "churnsync"]
OC_SKIP = {"split", "entrydiv", "churn"}
# thresholds, fixed before the real run (README "Decision rules")
CLAIM_FRACTION = 0.9      # each div run reaches >= 90% of its claim target
SPLIT_MIN = 10000         # warp claims: first and last exit in different calls, per div run
REUSE_MIN = 100           # claims per slot, per div run
OVERLAP_MIN = 10          # streams: overlapping launch pairs across the two streams
HOG_GAPS_MIN = 100        # hold under the hog: warp-gaps > 200 us
HOG_GAPMAX_MIN_NS = 1000000
HOLD_MIN_NS = 1000000
ARBITRATION_MIN = 1000    # retries + claims served away from the hint, native div runs at factor 0.5
FULL48 = {96, 128, 256, 512}  # block sizes that reach 48 warps per SM


def load(path):
    out = []
    if os.path.exists(path):
        for line in open(path):
            line = line.strip()
            if line:
                out.append(json.loads(line))
    return out


def accounting_problems(r):
    c, e = r["ctr"], r["end"]
    p = []
    if r["exit"].endswith("_first") or r["case"] == "mask":
        return p
    if r["pool"] in ("stksmnv", "stkglobnv"):
        return p
    if c["exit_lanes"] != c["claim_lanes"]:
        p.append("exit_lanes %d != claim_lanes %d" % (c["exit_lanes"], c["claim_lanes"]))
    if c["releases"] != c["claims"]:
        p.append("releases %d != claims %d" % (c["releases"], c["claims"]))
    for k in ("cap_push", "cap_spin"):
        if c[k]:
            p.append("%s %d" % (k, c[k]))
    if not e["clean"]:
        p.append("end state not clean: %s" % json.dumps(e))
    return p


def variant_verdict(pool, exit_s, runs, ctl_runs, min_claims, hog, hog_alive, oversub_ok):
    """runs: every run of (pool, exit_s) outside the hold-under-hog file; hog: its hold runs there."""
    reasons_bad, reasons_inc, notes = [], [], []
    viol = sum(r["violations"] for r in runs + hog)
    for r in runs + hog:
        if r["violations"]:
            reasons_bad.append("%s/%s f=%g: %d violations (%s)" % (
                r["path"], r["case"], r["factor"], r["violations"],
                ", ".join("%s=%d" % (k, r["ctr"][k]) for k in
                          ("dbl_claim", "foreign", "freed_live", "lanetag_bad", "bad_rel", "bad_clear", "tag_bad")
                          if r["ctr"][k])))
        for a in accounting_problems(r):
            reasons_bad.append("%s/%s f=%g: %s" % (r["path"], r["case"], r["factor"], a))
    lane = pool in LANE_POOLS
    is_oc = exit_s.startswith("oc")
    # power floors on the native div runs
    for case in DIV_ALL:
        if is_oc and case in OC_SKIP:
            continue
        for f in (0.5, 1.0):
            rs = [r for r in runs if r["path"] == "native" and r["case"] == case and abs(r["factor"] - f) < 1e-9]
            if not rs:
                reasons_inc.append("no native %s run at factor %g" % (case, f))
                continue
            r = rs[0]
            c = r["ctr"]
            target = max(min_claims, REUSE_MIN * r["nslots"])
            if c["claims"] < CLAIM_FRACTION * target:
                reasons_inc.append("%s f=%g: %d claims < %d" % (case, f, c["claims"], CLAIM_FRACTION * target))
            if c["claims"] < REUSE_MIN * r["nslots"]:
                reasons_inc.append("%s f=%g: %.0f claims per slot < %d" % (case, f, c["claims"] / r["nslots"], REUSE_MIN))
            if not lane and case in DIV_EXIT_CASES and c["split_exit"] < SPLIT_MIN:
                reasons_inc.append("%s f=%g: split exits %d < %d" % (case, f, c["split_exit"], SPLIT_MIN))
    # full occupancy
    full = [r for r in runs if r["case"] == "full"]
    if not full:
        reasons_inc.append("no full-occupancy runs")
    for r in full:
        c, x = r["ctr"], r["extra"]
        if c["bar_cap"]:
            reasons_inc.append("full bs=%d: barrier cap hit (co-residency not proven)" % r["bs"])
        empty = c["misses"] - c["cap_claim"]
        if empty:
            reasons_bad.append("full bs=%d: %d empty-pool misses with a pool of the resident count" % (r["bs"], empty))
        if c["cap_claim"]:
            notes.append("full bs=%d: %d cap misses (contention)" % (r["bs"], c["cap_claim"]))
        if r["bs"] in FULL48 and x.get("resident_warps_per_sm") != 48:
            reasons_inc.append("full bs=%d: %s warps per SM, not 48" % (r["bs"], x.get("resident_warps_per_sm")))
    if not oversub_ok:
        reasons_inc.append("the oversubscribed full launch did not hit the barrier cap")
    # streams and graph
    st = [r for r in runs if r["case"] == "streams"]
    if not st:
        reasons_inc.append("no streams run")
    for r in st:
        if r["extra"]["overlap_pairs"] < OVERLAP_MIN:
            reasons_inc.append("streams: %d overlapping pairs < %d" % (r["extra"]["overlap_pairs"], OVERLAP_MIN))
        if r["ctr"]["launches"] != r["launches_issued"] or r["extra"]["launches_without_times"]:
            reasons_inc.append("streams: launches counted %d of %d" % (r["ctr"]["launches"], r["launches_issued"]))
    gr = [r for r in runs if r["case"] == "graph"]
    if not gr:
        reasons_inc.append("no graph run")
    for r in gr:
        if r["ctr"]["launches"] != r["extra"]["expected_launches"]:
            reasons_inc.append("graph: launches counted %d of %d" % (r["ctr"]["launches"], r["extra"]["expected_launches"]))
    # time-slicing (hold next to the hog)
    if not hog:
        reasons_inc.append("no hold run under the hog")
    for r in hog:
        c = r["ctr"]
        if not (c["gaps"] >= HOG_GAPS_MIN and c["gap_max_ns"] >= HOG_GAPMAX_MIN_NS and hog_alive):
            reasons_inc.append("hold under hog: gaps %d, max gap %.2f ms, hog alive %s (time-slicing not shown)" % (
                c["gaps"], c["gap_max_ns"] / 1e6, hog_alive))
        if c["hold_ns_min"] < HOLD_MIN_NS:
            reasons_inc.append("hold under hog: shortest hold %.2f ms < 1 ms" % (c["hold_ns_min"] / 1e6))
        if c["hold_cap"]:
            reasons_inc.append("hold under hog: iteration cap hit")
    # the control of this pool must show double ownership (not only its own released ledger) in
    # every case where exits diverge
    for case in DIV_EXIT_CASES:
        if is_oc and case in OC_SKIP:
            continue
        cr = [r for r in ctl_runs if r["path"] == "native" and r["case"] == case and abs(r["factor"] - 0.5) < 1e-9]
        if not cr:
            reasons_inc.append("control: no %s run at factor 0.5" % case)
        else:
            c = cr[0]["ctr"]
            if c["dbl_claim"] + c["foreign"] + c["lanetag_bad"] == 0:
                reasons_inc.append("control: %s at factor 0.5 showed no double ownership (no power)" % case)
    # the atomic must actually have arbitrated: retries or claims away from the hinted slot
    arb = sum(r["ctr"]["retry"] + r["ctr"]["steal"] for r in runs
              if r["path"] == "native" and r["case"] in DIV_ALL and abs(r["factor"] - 0.5) < 1e-9)
    notes.append("arbitration at factor 0.5 (retries + claims away from the hint): %d" % arb)
    if arb < ARBITRATION_MIN:
        reasons_inc.append("only %d retries/steals at factor 0.5: the claim atomic was hardly arbitrated" % arb)
    if reasons_bad:
        verdict = "incorrect"
    elif reasons_inc:
        verdict = "inconclusive"
    else:
        verdict = "correct"
    claims = sum(r["ctr"]["claims"] for r in runs + hog)
    misses = sum(r["ctr"]["misses"] for r in runs + hog)
    capm = sum(r["ctr"]["cap_claim"] for r in runs + hog)
    notes.append("claims %d, misses %d (cap %d, empty pool %d), violations %d" % (claims, misses, capm, misses - capm, viol))
    return {"pool": pool, "exit": exit_s, "verdict": verdict, "violations": viol,
            "incorrect_because": reasons_bad[:40], "inconclusive_because": reasons_inc[:40],
            "n_runs": len(runs) + len(hog), "notes": notes}


def mask_verdict(rs):
    by = {r["exit"]: r["ctr"] for r in rs}
    if not all(k in by for k in ("loop_ni", "one_ni", "loop_inl")):
        return {"verdict": "not run", "counts": by}
    d = {k: by[k]["mask_notin"] for k in by}
    calls = {k: by[k]["mask_calls"] for k in by}
    if d["loop_ni"] > 0 and d["loop_inl"] > 0 and d["one_ni"] == 0:
        v = "reproduced: lanes outside their leader's __activemask() at its __shfl_sync, with a loop in the leader-only region (inlined or not); 0 with one atomic"
    elif d["loop_ni"] > 0 and d["one_ni"] > 0:
        v = "reproduced in every noinline shape"
    elif all(x == 0 for x in d.values()):
        v = "not reproduced"
    else:
        v = "reproduced in some shapes: %s" % d
    return {"verdict": v, "lane_not_in_leader_mask": d, "calls": calls}


def analyze(out):
    native = load(os.path.join(out, "native", "results.jsonl"))
    jit = load(os.path.join(out, "jit", "results.jsonl"))
    hog = load(os.path.join(out, "hold_hog", "results.jsonl"))
    ctl = load(os.path.join(out, "hold_ctl", "results.jsonl"))
    meta = {}
    mp = os.path.join(out, "native", "meta_native.json")
    if os.path.exists(mp):
        meta = json.load(open(mp))
    min_claims = meta.get("min_claims", 100000)
    hog_alive = False
    ap = os.path.join(out, "hold_hog", "hog_alive.txt")
    if os.path.exists(ap):
        hog_alive = open(ap).read().strip() == "hog_alive_at_end=1"
    allr = native + jit
    oversub = [r for r in native if r["case"] == "full_oversub"]
    oversub_ok = bool(oversub) and all(r["ctr"]["bar_cap"] > 0 for r in oversub)
    S = {"variants": [], "info_variants": [], "controls": {}, "aba": {}, "mask": {}, "hold_ctl": [], "jit": {}}
    groups = [("warp_activemask", WARP_POOLS, "o_lane"), ("warp_constructed_mask", WARP_POOLS, "oc_lane"),
              ("lane", LANE_POOLS, "o_lane")]
    # each group's control: the same claim with release at the first exit call (lane pools: right
    # after the claim); the constructed-mask group has its own matched control oc_first
    ctl_exit = {"warp_activemask": "o_first", "warp_constructed_mask": "oc_first", "lane": "o_first"}
    for gname, pools, ex in groups:
        for p in pools:
            runs = [r for r in allr if r["pool"] == p and r["exit"] == ex and r["case"] != "aba"]
            runs += [r for r in native if r["pool"] == p and r["exit"] == ex and r["case"] == "aba"]
            runs += [r for r in ctl if r["pool"] == p and r["exit"] == ex]  # hold without the hog
            ctl_runs = [r for r in allr if r["pool"] == p and r["exit"] == ctl_exit[gname]]
            hr = [r for r in hog if r["pool"] == p and r["exit"] == ex]
            v = variant_verdict(p, ex, runs, ctl_runs, min_claims, hr, hog_alive, oversub_ok)
            v["group"] = gname
            S["variants"].append(v)
    # informational: relaxed and aggregated-exit variants (reported, not decided)
    for p in WARP_POOLS + LANE_POOLS:
        for ex in ("r_lane", "o_agg"):
            runs = [r for r in allr + hog if r["pool"] == p and r["exit"] == ex]
            if runs:
                S["info_variants"].append({
                    "pool": p, "exit": ex, "runs": len(runs),
                    "violations": sum(r["violations"] for r in runs),
                    "failing_cases": sorted({"%s/%s f=%g" % (r["path"], r["case"], r["factor"]) for r in runs if r["violations"]}),
                    "accounting_problems": sum(len(accounting_problems(r)) for r in runs)})
    for p in WARP_POOLS + LANE_POOLS:
        for cx in ("o_first", "oc_first"):
            rs = [r for r in allr if r["pool"] == p and r["exit"] == cx]
            if rs:
                S["controls"]["%s %s" % (p, cx)] = {"%s/%s f=%g" % (r["path"], r["case"], r["factor"]): r["violations"]
                                                    for r in rs}
    for r in native:
        if r["case"] == "aba":
            S["aba"][r["pool"]] = {"violations": r["violations"], "claims": r["ctr"]["claims"],
                                   "end_clean": r["end"]["clean"]}
    aba_v = S["aba"]
    if aba_v:
        nv = [aba_v.get(p, {}).get("violations", 0) for p in ("stksmnv", "stkglobnv")]
        ver = [aba_v.get(p, {}).get("violations", 0) for p in ("stksm", "stkglob")]
        if all(nv) and not any(ver):
            S["aba_verdict"] = "ABA provoked without the version tag (%s); versioned stacks 0" % nv
        elif not any(ver) and not all(nv):
            S["aba_verdict"] = "ABA not provoked in both no-version stacks (%s): the version tag's necessity is not shown" % nv
        else:
            S["aba_verdict"] = "versioned stacks failed in the ABA case: %s" % ver
    S["mask"]["native"] = mask_verdict([r for r in native if r["case"] == "mask"])
    S["mask"]["jit"] = mask_verdict([r for r in jit if r["case"] == "mask"])
    for r in ctl:
        S["hold_ctl"].append({"pool": r["pool"], "exit": r["exit"], "violations": r["violations"],
                              "gaps": r["ctr"]["gaps"], "gap_max_ms": r["ctr"]["gap_max_ns"] / 1e6})
    S["oversub_control_hit_cap"] = oversub_ok
    gpw = {}
    for r in native:
        if r["case"] in DIV_ALL and r["exit"] in ("o_lane", "oc_lane") and r["pool"] in WARP_POOLS:
            warps = r["launches_issued"] * r["blocks"] * r["bs"] // 32
            if warps:
                gpw.setdefault(r["case"], []).append((r["ctr"]["claims"] + r["ctr"]["misses"]) / warps)
    S["groups_per_warp"] = {k: [min(v), max(v)] for k, v in gpw.items()}
    # group verdicts
    S["groups"] = {}
    for gname, pools, ex in groups:
        vs = [v for v in S["variants"] if v["group"] == gname]
        verdicts = {v["pool"]: v["verdict"] for v in vs}
        if all(x == "correct" for x in verdicts.values()):
            g = "correct (every pool)"
        elif any(x == "correct" for x in verdicts.values()):
            g = "correct for %s" % ", ".join(p for p, x in verdicts.items() if x == "correct")
        elif any(x == "incorrect" for x in verdicts.values()):
            g = "incorrect"
        else:
            g = "inconclusive"
        S["groups"][gname] = {"verdict": g, "per_pool": verdicts}
    return S


def write(out, S):
    json.dump(S, open(os.path.join(out, "chk_summary.json"), "w"), indent=1)
    L = ["# C2 correctness summary", ""]
    L.append("| group | verdict | per pool |")
    L.append("|---|---|---|")
    for g, x in S["groups"].items():
        L.append("| %s | %s | %s |" % (g, x["verdict"], ", ".join("%s: %s" % kv for kv in x["per_pool"].items())))
    L += ["", "## Variants (decision rows)", ""]
    for v in S["variants"]:
        L.append("- **%s %s (%s): %s**. %s" % (v["pool"], v["exit"], v["group"], v["verdict"], "; ".join(v["notes"])))
        for r in v["incorrect_because"][:8]:
            L.append("  - incorrect: " + r)
        for r in v["inconclusive_because"][:8]:
            L.append("  - inconclusive: " + r)
    L += ["", "## Reproducer (__activemask + leader work + __shfl_sync)", ""]
    for k, m in S["mask"].items():
        L.append("- %s: %s; lanes not in their leader's mask %s of calls %s" % (
            k, m["verdict"], m.get("lane_not_in_leader_mask"), m.get("calls")))
    L += ["", "## Claim groups per warp (warp claims, native div; min-max over runs)", ""]
    for k, v in S.get("groups_per_warp", {}).items():
        L.append("- %s: %.2f-%.2f" % (k, v[0], v[1]))
    L += ["", "## Controls", "", "- ABA: %s" % S.get("aba_verdict", "not run"),
          "- oversubscribed full launch hit the barrier cap: %s" % S["oversub_control_hit_cap"]]
    for p, d in S["controls"].items():
        zero = [k for k, v in d.items() if v == 0]
        L.append("- first-exit/early release, %s: %d runs, %d with 0 violations %s" % (p, len(d), len(zero), zero[:6]))
    L += ["", "## Informational variants (relaxed ordering, aggregated exit)", ""]
    for v in S["info_variants"]:
        L.append("- %s %s: %d runs, violations %d %s, accounting problems %d" % (
            v["pool"], v["exit"], v["runs"], v["violations"], v["failing_cases"][:6], v["accounting_problems"]))
    if S["hold_ctl"]:
        L += ["", "## Hold without the hog (control)", ""]
        for h in S["hold_ctl"]:
            L.append("- %s %s: violations %d, gaps %d, max gap %.3f ms" % (h["pool"], h["exit"], h["violations"], h["gaps"], h["gap_max_ms"]))
    open(os.path.join(out, "chk_summary.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))


def selftest():
    def run(case, pool="bmsm", ex="o_lane", f=0.5, viol=0, claims=200000, split=50000, nslots=192, path="native", **kw):
        c = {k: 0 for k in ("claims", "claim_lanes", "misses", "miss_lanes", "exit_lanes", "releases", "cap_claim",
                            "cap_push", "cap_spin", "tag_bad", "dbl_claim", "foreign", "freed_live", "lanetag_bad",
                            "bad_rel", "bad_clear", "split_exit", "checks", "gaps", "gap_max_ns", "hold_cap", "bar_cap",
                            "launches", "hold_ns_min", "mask_calls", "mask_disagree", "mask_notin", "retry",
                            "steal")}
        c.update(claims=claims, claim_lanes=claims * 32, exit_lanes=claims * 32, releases=claims, split_exit=split,
                 foreign=viol, retry=kw.get("retry", 500))
        c.update(kw.get("ctr", {}))
        return {"case": case, "pool": pool, "exit": ex, "factor": f, "violations": viol, "nslots": nslots,
                "path": path, "bs": kw.get("bs", 128), "launches_issued": kw.get("li", 1), "extra": kw.get("extra", {}),
                "ctr": c, "end": {"clean": kw.get("clean", True)}}

    def good(viol_case=None, ctl_zero=None, gaps=500):
        runs = []
        for case in DIV_ALL:
            for f in (0.5, 1.0):
                runs.append(run(case, f=f, viol=(5 if case == viol_case else 0)))
        for bs in (32, 96, 128):
            runs.append(run("full", f=1.0, bs=bs, extra={"resident_warps_per_sm": 48 if bs in FULL48 else 16}))
        runs.append(run("streams", f=1.0, li=60, extra={"overlap_pairs": 30, "launches_without_times": 0},
                        ctr={"launches": 60}))
        runs.append(run("graph", f=1.0, extra={"expected_launches": 600}, ctr={"launches": 600}))
        ctl = [run(c, ex="o_first", viol=(0 if c == ctl_zero else 9)) for c in DIV_EXIT_CASES]
        hog = [run("hold", f=1.0, ctr={"gaps": gaps, "gap_max_ns": 2500000, "hold_ns_min": 8000000})]
        return runs, ctl, hog

    ok = True
    r, c, h = good()
    v = variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)
    ok &= v["verdict"] == "correct" or print("selftest: clean set ->", v)
    r, c, h = good(viol_case="churn")
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "incorrect"
    r, c, h = good(ctl_zero="nested")
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "inconclusive"
    r, c, h = good(gaps=0)
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "inconclusive"
    r, c, h = good()
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, False, True)["verdict"] == "inconclusive"
    r, c, h = good()
    r[0]["ctr"]["exit_lanes"] += 1
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "incorrect"
    r, c, h = good()
    r[0]["ctr"]["split_exit"] = 10
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "inconclusive"
    r, c, h = good()
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, False)["verdict"] == "inconclusive"
    r, c, h = good()
    for x in r:
        x["ctr"]["retry"] = 0
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "inconclusive"
    r, c, h = good()
    for x in c:
        x["ctr"]["freed_live"], x["ctr"]["foreign"] = x["ctr"]["foreign"], 0
    ok &= variant_verdict("bmsm", "o_lane", r, c, 100000, h, True, True)["verdict"] == "inconclusive"
    # constructed mask: no split/entrydiv/churn runs and no entrydiv control are needed ...
    r, c, h = good()
    r = [x for x in r if x["case"] not in OC_SKIP]
    c = [x for x in c if x["case"] not in OC_SKIP]
    v = variant_verdict("bmsm", "oc_lane", r, c, 100000, h, True, True)
    ok &= v["verdict"] == "correct" or print("selftest: oc clean set ->", v)
    # ... but its matched control must still fail in every other diverging case
    r, c, h = good(ctl_zero="loops")
    r = [x for x in r if x["case"] not in OC_SKIP]
    ok &= variant_verdict("bmsm", "oc_lane", r, c, 100000, h, True, True)["verdict"] == "inconclusive"
    m = mask_verdict([{"exit": "loop_ni", "ctr": {"mask_notin": 5, "mask_calls": 9}},
                      {"exit": "one_ni", "ctr": {"mask_notin": 0, "mask_calls": 9}},
                      {"exit": "loop_inl", "ctr": {"mask_notin": 3, "mask_calls": 9}}])
    ok &= m["verdict"].startswith("reproduced: lanes outside")
    m = mask_verdict([{"exit": k, "ctr": {"mask_notin": 0, "mask_calls": 9}} for k in ("loop_ni", "one_ni", "loop_inl")])
    ok &= m["verdict"] == "not reproduced"
    print("selftest ok" if ok else "selftest FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    out = sys.argv[1]
    write(out, analyze(out))
