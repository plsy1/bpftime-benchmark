#!/usr/bin/env python3
"""analyze_b.py RUNDIR [--quick] | --selftest

Part (b) of check C1: applies the decision rules of README.md ("Decision
rules", part b) to RUNDIR/{basic,graph,streams,spin_ctl,spin_hog}/ written by
build/c1check (and run_spin.sh). Writes RUNDIR/b_summary.json and
RUNDIR/b_summary.txt; the last line of the text is the verdict.
--quick (QUICK=1 run_all.sh, development only): the parameter minimums of
the completeness rule are relaxed to QUICK's; such a verdict is not a result.
"""
import csv
import json
import os
import sys

CARRY = ("carry", "carry_direct")
MUST_FAIL = ("shared", "lane")
GAP_NS = 50000
MIN_EPISODES = 10
# Completeness (README, rule 0): the trials every verdict needs, with the
# parameters run_all.sh uses. (run, kernel, variant, shape or None).
MINS = {
    "full": {"basic_shapes": ("full512", "w48", "anchor"), "basic": 3, "graph": 1000,
             "streams": 3, "spin": 2, "spin_ms": 300},
    "quick": {"basic_shapes": ("full512", "w48"), "basic": 1, "graph": 20,
              "streams": 1, "spin": 1, "spin_ms": 100},
}


def required_trials(mode):
    m = MINS[mode]
    req = []
    for fam in ("kmr", "kdiv"):
        for sh in m["basic_shapes"]:
            for v in ("carry", "shared", "lane"):
                req.append(("basic", fam, v, sh, m["basic"]))
            req.append(("basic", fam + "_d", "carry_direct", sh, m["basic"]))
        for v in ("carry", "shared"):
            req.append(("graph", fam, v, None, m["graph"]))
        req.append(("graph", fam + "_d", "carry_direct", None, m["graph"]))
    for k in ("ks2a", "ks2b"):
        for v in ("carry", "shared"):
            req.append(("streams", k, v, None, m["streams"]))
        req.append(("streams", k + "_d", "carry_direct", None, m["streams"]))
    for run in ("spin_ctl", "spin_hog"):
        for v in ("carry", "shared"):
            req.append((run, "kspin", v, None, m["spin"]))
        req.append((run, "kspin_d", "carry_direct", None, m["spin"]))
    return req


def extra_val(t, key):
    for part in (t.get("extra") or "").split():
        if part.startswith(key + "="):
            return I(part.split("=", 1)[1])
    return None


def missing_trials(trials, mode):
    """The required trials that are absent or ran with fewer launches (or a
    shorter spin) than the minimum."""
    miss = []
    for run, k, v, sh, n in required_trials(mode):
        hit = [t for t in trials if t.get("run") == run and t.get("kernel") == k and
               t.get("variant") == v and (sh is None or t.get("shape") == sh) and
               I(t.get("launches")) >= n]
        if run.startswith("spin_"):
            hit = [t for t in hit if (extra_val(t, "spin_ms") or 0) >= MINS[mode]["spin_ms"]]
        if not hit:
            miss.append("%s/%s/%s%s>=%d" % (run, k, v, "/" + sh if sh else "", n))
    return miss


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path) as f:
        return list(csv.DictReader(f))


def I(x):
    return int(x) if x not in (None, "") else 0


def trial_ok_counts(t):
    """entries == exits == launches x threads, no NULL storage at entry, no
    NULL rec. NULL storage at exit is the slot+tag design's counted fallback
    and allowed there only."""
    exp = I(t["expect"])
    ok = I(t["entries"]) == exp and I(t["exits"]) == exp and I(t["threads_count_err"]) == 0
    ok = ok and I(t["entry_null"]) == 0 and I(t["rec_null"]) == 0
    if t["variant"] != "slottag":
        ok = ok and I(t["exit_null"]) == 0
    return ok


def merge_intervals(iv):
    """[(start, end, warp)] -> [(start, end, set(warps))] of overlapping groups."""
    out = []
    for s, e, w in sorted(iv):
        if out and s <= out[-1][1]:
            out[-1][1] = max(out[-1][1], e)
            out[-1][2].add(w)
        else:
            out.append([s, e, {w}])
    return out


def spin_analysis(d):
    """Per launch of a spin run: warps, non-injected gap episodes (merged
    overlapping lane-0 gap intervals touching >= half the warps), the
    detector control, and (with a hog) the share of our gap time during which
    the hog's warp 0 was running."""
    launches = read_csv(os.path.join(d, "spin_launches.csv"))
    warps = {}
    inj = {}
    for r in read_csv(os.path.join(d, "spin_warps.csv")):
        warps.setdefault(r["tag"], 0)
        warps[r["tag"]] += 1
        inj[(r["tag"], r["warp"])] = (I(r["inj0"]), I(r["inj1"]))
    gaps = {}
    for r in read_csv(os.path.join(d, "spin_gaps.csv")):
        p, q = I(r["t_prev"]), I(r["t_now"])
        i0, i1 = inj.get((r["tag"], r["warp"]), (0, 0))
        injected = i0 and p <= i0 + 1 and q >= i1
        if not injected:
            gaps.setdefault(r["tag"], []).append((p, q, r["warp"]))
    # hog running intervals: kernel windows minus warp-0 gaps
    hog_k = read_csv(os.path.join(d, "hog_kernels.csv"))
    hog_g = read_csv(os.path.join(d, "hog_gaps.csv"))
    running = []
    if hog_k:
        holes = {}
        for g in hog_g:
            holes.setdefault(g["kernel"], []).append((I(g["t_prev"]), I(g["t_now"])))
        for k in hog_k:
            t0, t1 = I(k["t0"]), I(k["t1"])
            cur = t0
            for a, b in sorted(holes.get(k["kernel"], [])):
                if a > cur:
                    running.append((cur, min(a, t1)))
                cur = max(cur, b)
            if cur < t1:
                running.append((cur, t1))

    def covered(a, b):
        c = 0
        for x, y in running:
            lo, hi = max(a, x), min(b, y)
            if hi > lo:
                c += hi - lo
        return c

    res = []
    for L in launches:
        tag = L["tag"]
        nw = warps.get(tag, 0)
        groups = merge_intervals(gaps.get(tag, []))
        episodes = [g for g in groups if len(g[2]) * 2 >= nw and nw > 0]
        gap_time = sum(g[1] - g[0] for g in groups)
        cov = sum(covered(g[0], g[1]) for g in groups) if running else None
        res.append({
            "tag": tag, "kernel": L["kernel"], "variant": L["variant"], "warps": nw,
            "warp_gaps_total": I(L["warp_gaps"]), "max_gap_ns": I(L["max_gap_ns"]),
            "capped_warps": I(L["capped_warps"]), "inj_seen_warps": I(L["inj_seen_warps"]),
            "id_changes": I(L["id_changes"]),
            "noninjected_logged_gaps": len(gaps.get(tag, [])),
            "max_noninjected_gap_ns": max([q - p for p, q, _ in gaps.get(tag, [])] or [0]),
            "episodes": len(episodes),
            "gap_time_ns": gap_time,
            "share_hog_running": (cov / gap_time) if (running and gap_time) else None,
        })
    return res


def decide(trials, spin_ctl, spin_hog, streams, hog_alive, mode="full"):
    reasons = []
    by = lambda pred: [t for t in trials if pred(t)]
    carry_trials = by(lambda t: t["variant"] in CARRY)
    fails = [t for t in carry_trials if I(t["mismatches"]) != 0 or not trial_ok_counts(t)]
    cases_seen = sorted({t["case"] for t in carry_trials})
    # must-fail controls
    mf = by(lambda t: t["variant"] in MUST_FAIL)
    mf_pass = [t for t in mf if I(t["mismatches"]) == 0]
    # slot+tag (fallback design): never a wrong value
    st_bad = by(lambda t: t["variant"] == "slottag" and (I(t["mismatches"]) != 0 or not trial_ok_counts(t)))
    slot = by(lambda t: t["variant"] == "slot")
    frame = by(lambda t: t["variant"] == "frame")
    # preemption evidence
    pre = {}
    for v in CARRY:
        hl = [x for x in spin_hog if x["variant"] == v]
        cl = [x for x in spin_ctl if x["variant"] == v]
        pre[v] = {
            "hog_launches": len(hl),
            "episodes_total": sum(x["episodes"] for x in hl),
            "warp_gaps_total": sum(x["noninjected_logged_gaps"] for x in hl),
            "share_hog_running_min": min([x["share_hog_running"] for x in hl
                                          if x["share_hog_running"] is not None] or [0]),
            "detector_ok": all(x["inj_seen_warps"] == x["warps"] for x in hl + cl) and bool(hl) and bool(cl),
            "ctl_max_noninjected_gap_ns": max([x["max_noninjected_gap_ns"] for x in cl] or [0]),
            "capped": sum(x["capped_warps"] for x in hl + cl),
        }
    # the control alone must show no gap above the threshold (else the
    # threshold is not valid: the GPU was busy), as step0/e1-ids
    ctl_quiet = all(p["ctl_max_noninjected_gap_ns"] == 0 for p in pre.values())
    pre_ok = all(p["episodes_total"] >= MIN_EPISODES and p["share_hog_running_min"] >= 0.5
                 and p["detector_ok"] and p["capped"] == 0 for p in pre.values()) and hog_alive and ctl_quiet
    conc_ok = bool(streams) and all(I(s["overlap_ns"]) > 0 and I(s["sms_both"]) >= 1 for s in streams)
    # streams.csv: one row per rep and variant; the carry reps must all be there
    conc_reps = {v: sum(1 for s in streams if s.get("variant") == v) for v in CARRY}
    conc_ok = conc_ok and all(n >= MINS[mode]["streams"] for n in conc_reps.values())
    missing = missing_trials(trials, mode)
    # spin-cap hits of kmr/kdiv (basic, graph): a stuck timer, never expected
    spin_caps = sum(extra_val(t, "spin_caps") or 0 for t in trials if t.get("case") in ("basic", "graph"))
    if fails:
        verdict = "FAILS"
        reasons.append("carry mismatches or count errors in %d trials" % len(fails))
    elif missing:
        verdict = "INCONCLUSIVE"
        reasons.append("required trials missing or short (%d): %s" % (len(missing), missing[:12]))
    elif mf_pass or not mf:
        verdict = "INCONCLUSIVE"
        reasons.append("a must-fail control showed 0 mismatches in %d trials" % len(mf_pass))
    elif not pre_ok:
        verdict = "INCONCLUSIVE"
        reasons.append("preemption evidence not met: %s, hog_alive_at_end=%s" % (json.dumps(pre), hog_alive))
    elif not conc_ok:
        verdict = "INCONCLUSIVE"
        reasons.append("two-stream concurrency not shown")
    elif spin_caps:
        verdict = "INCONCLUSIVE"
        reasons.append("%d spin-cap hits in basic/graph (stuck timer?)" % spin_caps)
    else:
        verdict = "WORKS"
    return {
        "verdict": verdict, "reasons": reasons,
        "carry_trials": len(carry_trials), "carry_failed_trials": len(fails),
        "carry_cases": cases_seen,
        "carry_thread_checks": sum(I(t["exits"]) for t in carry_trials),
        "must_fail_trials": len(mf), "must_fail_trials_with_0_mismatches": len(mf_pass),
        "slottag_bad_trials": len(st_bad),
        "slottag_fallbacks": sum(I(t["exit_null"]) for t in trials if t["variant"] == "slottag"),
        "slot_untagged_mismatch_trials": sum(1 for t in slot if I(t["mismatches"]) != 0),
        "slot_untagged_trials": len(slot),
        "frame_trials_with_mismatches": sum(1 for t in frame if I(t["mismatches"]) != 0),
        "frame_trials": len(frame),
        "preemption": pre, "preemption_ok": pre_ok, "ctl_quiet": ctl_quiet,
        "streams_concurrent": conc_ok,
        "carry_addr_distinct_max": max([I(t["addr_distinct"]) for t in carry_trials] or [0]),
        "mode": mode, "missing_trials": missing, "spin_caps_basic_graph": spin_caps,
        "streams_carry_reps": conc_reps,
        # warps whose (smid, warpid) differed between the start and the end of
        # the spin: 0 means preemption never moved a warp here, so the carry
        # was not exercised across a move (documented as possible, 10.4/10.8)
        "id_changes_hog": sum(x.get("id_changes", 0) for x in spin_hog),
        "id_changes_ctl": sum(x.get("id_changes", 0) for x in spin_ctl),
    }


def selftest():
    def T(case, v, m=0, exp=100, ex=None, en=None, run=None, kernel="x", shape="s", launches=1,
          extra=""):
        return {"case": case, "variant": v, "mismatches": str(m), "expect": str(exp),
                "entries": str(exp if en is None else en), "exits": str(exp if ex is None else ex),
                "threads_count_err": "0", "entry_null": "0", "exit_null": "0", "rec_null": "0",
                "addr_distinct": "1", "run": run or case, "kernel": kernel, "shape": shape,
                "launches": str(launches), "extra": extra}

    def full_set():
        out = []
        for run, k, v, sh, n in required_trials("full"):
            case = "spin" if run.startswith("spin_") else run
            m = 50 if v in MUST_FAIL else 0
            out.append(T(case, v, m=m, run=run, kernel=k, shape=sh or "s", launches=n,
                         extra="spin_ms=300" if case == "spin" else "spin_caps=0"))
        return out
    good = full_set()
    sp = lambda v, ep: {"variant": v, "episodes": ep, "noninjected_logged_gaps": 1000, "share_hog_running": 0.9,
                        "inj_seen_warps": 8, "warps": 8, "max_noninjected_gap_ns": 0, "capped_warps": 0}
    hog = [sp("carry", 20), sp("carry_direct", 20)]
    ctl = [sp("carry", 0), sp("carry_direct", 0)]
    st = [{"overlap_ns": "5", "sms_both": "8", "variant": v} for v in CARRY for _ in range(3)]
    assert decide(good, ctl, hog, st, True)["verdict"] == "WORKS"
    # completeness: a missing shape, a short graph, a short spin -> INCONCLUSIVE
    assert decide([t for t in good if t["shape"] != "anchor"], ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    short = [dict(t, launches="999") if t["case"] == "graph" else t for t in good]
    assert decide(short, ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    shortspin = [dict(t, extra="spin_ms=100") if t["case"] == "spin" else t for t in good]
    assert decide(shortspin, ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    assert decide(shortspin, ctl, hog, st, True, mode="quick")["verdict"] == "WORKS"
    nolane = [t for t in good if t["variant"] != "lane"]
    assert decide(nolane, ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    assert decide(good, ctl, hog, st[:-1], True)["verdict"] == "INCONCLUSIVE"
    capped = [dict(t, extra="spin_caps=1") if t["case"] == "graph" else t for t in good]
    assert decide(capped, ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    # a carry mismatch wins over missing trials
    assert decide([t for t in good if t["case"] != "graph"] + [T("basic", "carry", m=1)], ctl, hog, st,
                  True)["verdict"] == "FAILS"
    assert decide(good + [T("spin", "carry", m=1)], ctl, hog, st, True)["verdict"] == "FAILS"
    assert decide(good + [T("basic", "carry_direct", ex=99)], ctl, hog, st, True)["verdict"] == "FAILS"
    assert decide(good + [T("basic", "lane", m=0)], ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    assert decide(good, ctl, [sp("carry", 3), sp("carry_direct", 20)], st, True)["verdict"] == "INCONCLUSIVE"
    assert decide(good, ctl, hog, st, False)["verdict"] == "INCONCLUSIVE"
    noisy = [dict(sp("carry", 0), max_noninjected_gap_ns=90000), sp("carry_direct", 0)]
    assert decide(good, noisy, hog, st, True)["verdict"] == "INCONCLUSIVE"
    assert decide(good, ctl, hog, [dict(x, overlap_ns="-5") for x in st], True)["verdict"] == "INCONCLUSIVE"
    assert decide([t for t in good if t["case"] != "graph"], ctl, hog, st, True)["verdict"] == "INCONCLUSIVE"
    bad_det = [dict(sp("carry", 20), inj_seen_warps=7), sp("carry_direct", 20)]
    assert decide(good, ctl, bad_det, st, True)["verdict"] == "INCONCLUSIVE"
    g = merge_intervals([(0, 10, "a"), (5, 12, "b"), (20, 30, "a")])
    assert len(g) == 2 and g[0][2] == {"a", "b"}
    print("selftest ok")


def main():
    if "--selftest" in sys.argv:
        selftest()
        return
    run = sys.argv[1]
    mode = "quick" if "--quick" in sys.argv else "full"
    trials = []
    for sub in ("basic", "graph", "streams", "spin_ctl", "spin_hog"):
        for t in read_csv(os.path.join(run, sub, "trials.csv")):
            t["run"] = sub
            trials.append(t)
    spin_ctl = spin_analysis(os.path.join(run, "spin_ctl"))
    spin_hog = spin_analysis(os.path.join(run, "spin_hog"))
    streams = read_csv(os.path.join(run, "streams", "streams.csv"))
    log = os.path.join(run, "spin_hog", "c1check.log")
    hog_alive = os.path.exists(log) and "hog_alive_at_end=1" in open(log).read()
    res = decide(trials, spin_ctl, spin_hog, streams, hog_alive, mode)
    res["spin_ctl"] = spin_ctl
    res["spin_hog"] = spin_hog
    res["streams"] = streams
    json.dump(res, open(os.path.join(run, "b_summary.json"), "w"), indent=1)
    lines = []
    lines.append("trials (case, kernel, variant, shape: threads x launches, mismatches, NULL at exit)")
    for t in trials:
        lines.append("  %-7s %-8s %-12s %-8s %8s x %-5s mism %-8s null %s%s" % (
            t["case"], t["kernel"], t["variant"], t["shape"], t["threads"], t["launches"],
            t["mismatches"], t["exit_null"], "" if trial_ok_counts(t) else "  COUNT ERROR"))
    lines.append("spin (hog): tag, warps, episodes, logged non-injected gaps, max gap ms, share hog running")
    for x in spin_hog:
        lines.append("  %-28s %4d %4d %7d %7.2f %s" % (
            x["tag"], x["warps"], x["episodes"], x["noninjected_logged_gaps"], x["max_gap_ns"] / 1e6,
            "%.2f" % x["share_hog_running"] if x["share_hog_running"] is not None else "NA"))
    lines.append("spin (ctl): tag, max non-injected gap us, detector warps")
    for x in spin_ctl:
        lines.append("  %-28s %8.1f %d/%d" % (x["tag"], x["max_noninjected_gap_ns"] / 1e3,
                                              x["inj_seen_warps"], x["warps"]))
    for k in ("carry_trials", "carry_thread_checks", "carry_failed_trials", "must_fail_trials",
              "must_fail_trials_with_0_mismatches", "slottag_bad_trials", "slottag_fallbacks",
              "slot_untagged_trials", "slot_untagged_mismatch_trials", "frame_trials",
              "frame_trials_with_mismatches", "preemption_ok", "ctl_quiet", "streams_concurrent",
              "carry_addr_distinct_max", "spin_caps_basic_graph", "id_changes_hog", "id_changes_ctl",
              "mode"):
        lines.append("%s: %s" % (k, res[k]))
    lines.append("preemption: %s" % json.dumps(res["preemption"]))
    for r in res["reasons"]:
        lines.append("reason: " + r)
    lines.append("C1 part b verdict: carry %s" % res["verdict"])
    open(os.path.join(run, "b_summary.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines[-9:]))


if __name__ == "__main__":
    main()
