#!/usr/bin/env python3
"""analyze.py OUT  |  analyze.py --selftest

Reads OUT/<mode>/{results,hist,leader}.csv (mode = native, jit) and OUT/check_sass_*.txt, writes
OUT/summary.json and OUT/summary.txt. Decision rules: README.md, "Decision rules"; the thresholds
below were fixed before any real run. Standard library only (python3 -I).
"""
import collections
import csv
import json
import math
import os
import sys

MIN_CALLS = 1_000_000          # per case, correct helper, per mode
CASES = ["early", "lowlanes", "nested", "loops", "memcap"]
CHUNKS = [1, 2, 4, 8, 16, 32]  # chunk sizes C for the waste estimate of approach C


def rd(p):
    with open(p, newline="") as f:
        return list(csv.DictReader(f))


def mode_summary(d):
    res = rd(os.path.join(d, "results.csv"))
    hist = rd(os.path.join(d, "hist.csv"))
    lead = rd(os.path.join(d, "leader.csv"))
    sp = os.path.join(d, "sites.csv")
    sites = rd(sp) if os.path.exists(sp) else []
    out = {}
    for case in CASES:
        for var in ("ok", "bad_rank", "bad_leader0"):
            rows = [r for r in res if r["case"] == case and r["variant"] == var]
            if not rows:
                continue
            S = {k: sum(int(r.get(k) or 0) for r in rows) for k in
                 ("calls", "reserved", "count_mismatch", "missing", "double", "oob", "bad_tag", "lost_call",
                  "dup_tag", "hist_mismatch", "violations", "warp_calls", "partial_warp_calls",
                  "lane0_absent_warp_calls", "reservations", "mixed_site_reservations", "resv_tiling_errors")}
            # lanes from different call sites in one reservation (correct helper only; caller-side
            # bookkeeping). Valid only if the reservations tile [0, total) and match the warp calls.
            if var == "ok":
                valid = S["resv_tiling_errors"] == 0 and S["reservations"] == S["warp_calls"] > 0
                S["mixed_site_share"] = (S["mixed_site_reservations"] / S["reservations"]) if valid else None
                S["distinct_sites_histogram"] = {r["distinct_sites"]: int(r["reservations"]) for r in sites
                                                 if r["case"] == case and r["variant"] == var}
            S["launches"] = len(rows)
            S["blocks"] = sorted({int(r["blocks"]) for r in rows})
            S["occ_blocks_per_sm"] = sorted({int(r["occ_blocks_per_sm"]) for r in rows})
            h = collections.Counter()
            per_site = collections.defaultdict(collections.Counter)
            for r in hist:
                if r["case"] == case and r["variant"] == var:
                    h[int(r["popc"])] += int(r["warp_calls"])
                    per_site[int(r["site"])][int(r["popc"])] += int(r["warp_calls"])
            n = sum(h.values())
            if n:
                recs = sum(p * c for p, c in h.items())
                S["records_per_call_mean"] = recs / n
                acc, med = 0, None
                for p in sorted(h):
                    acc += h[p]
                    if med is None and acc >= n / 2:
                        med = p
                S["records_per_call_median"] = med
                S["frac_calls_popc1"] = h[1] / n
                S["frac_calls_popc32"] = h[32] / n
                S["popc_histogram"] = {str(p): h[p] for p in sorted(h)}
                # by the LEADER's call site: other lanes of the same call may come from other sites
                S["records_per_call_mean_by_leader_site"] = {
                    str(s): sum(p * c for p, c in v.items()) / max(1, sum(v.values()))
                    for s, v in sorted(per_site.items())}
                # approach C: a call claims ceil(n / C) chunks of C slots
                S["chunk_waste"] = {str(C): 1 - recs / sum(math.ceil(p / C) * C * c for p, c in h.items())
                                    for C in CHUNKS}
            ld = collections.Counter()
            for r in lead:
                if r["case"] == case and r["variant"] == var:
                    ld[int(r["leader_lane"])] += int(r["warp_calls"])
            S["leader_lane_histogram"] = {str(k): ld[k] for k in sorted(ld)}
            out[f"{case}/{var}"] = S
    return out


def decide(M, sass_ok):
    D = {"modes": {}}
    overall = []
    for mode, S in M.items():
        why = []
        ok_viol = sum(S[f"{c}/ok"]["violations"] for c in CASES if f"{c}/ok" in S)
        missing_cases = [c for c in CASES if f"{c}/ok" not in S]
        low_volume = [c for c in CASES if f"{c}/ok" in S and S[f"{c}/ok"]["calls"] < MIN_CALLS]
        no_partial = [c for c in CASES if f"{c}/ok" in S and S[f"{c}/ok"]["partial_warp_calls"] == 0]
        no_lane0_absent = [c for c in CASES if f"{c}/ok" in S and S[f"{c}/ok"]["lane0_absent_warp_calls"] == 0]
        low = S.get("lowlanes/ok")
        lowlanes_all_absent = bool(low) and low["lane0_absent_warp_calls"] == low["warp_calls"] > 0
        ctl_silent = [c for c in CASES if f"{c}/bad_rank" not in S or S[f"{c}/bad_rank"]["violations"] == 0]
        l0 = S.get("lowlanes/bad_leader0")
        if ok_viol > 0:
            ans = "no"
            why.append(f"{ok_viol} violations with the correct helper")
        else:
            if missing_cases:
                why.append(f"cases not run: {missing_cases}")
            if low_volume:
                why.append(f"fewer than {MIN_CALLS} calls: {low_volume}")
            if no_partial:
                why.append(f"no partial-mask calls: {no_partial}")
            if no_lane0_absent:
                why.append(f"lane 0 never absent: {no_lane0_absent}")
            if not lowlanes_all_absent:
                why.append("lowlanes: lane 0 not absent in every call")
            if ctl_silent:
                why.append(f"bad_rank control without violations: {ctl_silent}")
            if not sass_ok.get(mode, False):
                why.append("SASS check failed or missing")
            ans = "yes" if not why else "inconclusive"
        D["modes"][mode] = {
            "answer": ans,
            "reasons": why,
            "violations_correct_helper": ok_viol,
            "bad_rank_violations": {c: S[f"{c}/bad_rank"]["violations"] for c in CASES if f"{c}/bad_rank" in S},
            "bad_leader0_violations": {c: S[f"{c}/bad_leader0"]["violations"] for c in CASES
                                       if f"{c}/bad_leader0" in S},
            "bad_leader0_lowlanes_detected": bool(l0) and l0["violations"] > 0,
        }
        overall.append(ans)
    if "no" in overall:
        D["answer"] = "no"
    elif overall and all(a == "yes" for a in overall):
        D["answer"] = "yes"
    else:
        D["answer"] = "inconclusive"
    return D


def text(M, D):
    L = ["check 7 (standalone): warp-level reservation in a __noinline__ helper under partial masks",
         f"correct under partial masks: {D['answer']}"]
    for mode, x in D["modes"].items():
        L.append(f"  {mode}: {x['answer']}; violations (correct helper) {x['violations_correct_helper']}; "
                 f"bad_rank violations {x['bad_rank_violations']}; bad_leader0 {x['bad_leader0_violations']}"
                 + (f"; reasons: {x['reasons']}" if x["reasons"] else ""))
    for mode, S in M.items():
        L.append(f"[{mode}] case: calls, launches, records per call mean/median, popc==1, popc==32, "
                 "lane0-absent share, chunk waste C=8/C=32, share of calls with lanes from >1 call site")
        for c in CASES:
            s = S.get(f"{c}/ok")
            if not s:
                continue
            L.append(f"  {c:9s} {s['calls']:>9d} {s['launches']:>3d}  {s.get('records_per_call_mean', 0):5.2f}/"
                     f"{s.get('records_per_call_median')}  {s.get('frac_calls_popc1', 0):.3f}  "
                     f"{s.get('frac_calls_popc32', 0):.3f}  "
                     f"{s['lane0_absent_warp_calls'] / max(1, s['warp_calls']):.3f}  "
                     f"{s.get('chunk_waste', {}).get('8', 0):.3f}/{s.get('chunk_waste', {}).get('32', 0):.3f}  "
                     + ("n/a" if s.get("mixed_site_share") is None else f"{s['mixed_site_share']:.3f}"))
    return "\n".join(L) + "\n"


def run(out):
    M = {}
    sass_ok = {}
    for mode in ("native", "jit"):
        d = os.path.join(out, mode)
        if os.path.exists(os.path.join(d, "results.csv")):
            M[mode] = mode_summary(d)
        p = os.path.join(out, f"check_sass_{mode}.txt")
        if os.path.exists(p):
            sass_ok[mode] = open(p).read().strip().endswith("RESULT PASS")
    D = decide(M, sass_ok)
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump({"decision": D, "details": M}, f, indent=1)
    t = text(M, D)
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write(t)
    sys.stdout.write(t)


def selftest():
    base = {"calls": 2_000_000, "reserved": 0, "count_mismatch": 0, "missing": 0, "double": 0, "oob": 0,
            "bad_tag": 0, "lost_call": 0, "dup_tag": 0, "hist_mismatch": 0, "violations": 0, "warp_calls": 10,
            "partial_warp_calls": 5, "lane0_absent_warp_calls": 10, "launches": 1}
    S = {}
    for c in CASES:
        S[f"{c}/ok"] = dict(base)
        S[f"{c}/bad_rank"] = dict(base, violations=7)
    assert decide({"native": S}, {"native": True})["answer"] == "yes"
    S["nested/ok"] = dict(base, violations=1)
    assert decide({"native": S}, {"native": True})["answer"] == "no"
    S["nested/ok"] = dict(base)
    S["loops/bad_rank"] = dict(base)  # silent control
    assert decide({"native": S}, {"native": True})["answer"] == "inconclusive"
    print("selftest ok")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest()
    elif len(sys.argv) == 2:
        run(sys.argv[1])
    else:
        sys.stderr.write(__doc__)
        sys.exit(1)
