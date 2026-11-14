#!/usr/bin/env python3
"""e4-publish analysis: violations per variant and the verdicts of check 4.

Usage: python3 -I analyze.py RUN_DIR
Reads RUN_DIR/*.json (one pub4 result each); writes RUN_DIR/summary.csv,
RUN_DIR/verdicts.json and RUN_DIR/summary.md. The rules are the README's
"Decision rules", fixed before the real run.
"""
import csv
import glob
import json
import os
import sys


def load(run):
    res = {}
    for p in sorted(glob.glob(os.path.join(run, "*.json"))):
        name = os.path.basename(p)[:-5]
        if name in ("verdicts",):
            continue
        try:
            res[name] = json.loads(open(p).read())
        except Exception as e:  # a killed run leaves no or partial JSON
            res[name] = {"error": str(e)}
    return res


def t1_verdicts(res, mode):
    # stress: payload over 16 lines, reverse reads, poison, a launch per 4
    # rounds; normal: same without the payload stress; long: ~1 s launches
    # (no E run of its own: E of the normal mode shows the checker works)
    sfx = {"stress": "_stress", "normal": "", "long": "_long"}[mode]
    g = lambda v: res.get("t1_%s%s" % (v, sfx))
    out = {"mode": mode}
    E = res.get("t1_E" + ("_stress" if mode == "stress" else ""))
    A, C, B, Cd, Am = g("A"), g("C"), g("B"), g("Cd"), g("Am")
    problems = []
    for v in ("A", "B", "C", "Cd", "D1", "D2", "E", "Am", "Cd15"):  # per mode
        r = E if v == "E" else g(v)
        if not r or "error" in r:
            continue
        if r["persistent"]:
            problems.append("%s: %d violations never healed (test bug)" % (v, r["persistent"]))
        if r["flag_anomalies"] or r["future_words"] or r["other_words"]:
            problems.append("%s: anomalies %d, future words %d, other words %d" % (
                v, r["flag_anomalies"], r["future_words"], r["other_words"]))
        if r["verified"] < r["published"]:
            problems.append("%s: %d published rounds not verified" % (v, r["published"] - r["verified"]))
    out["problems"] = problems
    if not E or "error" in E or not A or "error" in A:
        out["verdict"] = "missing runs"
        return out
    out["checker_detects"] = E["violating_rounds"] > 0
    if not out["checker_detects"]:
        out["verdict"] = "INVALID: the flag-first control E showed no violation, the checker cannot detect stale data"
        return out
    if problems:
        out["verdict"] = "INVALID: " + "; ".join(problems)
        return out
    NA, VA = A["verified"], A["violating_rounds"]
    out["A_rounds"], out["A_violations"] = NA, VA
    if C and "error" not in C:
        NC, VC = C["verified"], C["violating_rounds"]
        out["C_rounds"], out["C_violations"] = NC, VC
        pC = VC / NC if NC else 0
        out["C_rate"] = pC
        out["expected_A_violations_if_like_C"] = pC * NA
    else:
        VC = None
    if VA > 0:
        out["verdict"] = "one leader fence is NOT sufficient: %d violating rounds in %d" % (VA, NA)
    elif VC:
        power = out["expected_A_violations_if_like_C"] >= 3
        out["verdict"] = ("no violation observed in %d rounds of A; C failed %d times in %d rounds of the same setup: "
                          "%s (95%% upper bound on A's rate: %.2e per round)" % (
                              NA, VC, out["C_rounds"],
                              "evidence that one leader fence after __syncwarp suffices in this layout (the publisher's "
                              "own payload stores share instructions with the other lanes'; whether the fence orders "
                              "stores of lanes that did not fence is decided by A0, lane 0 idle)" if power else
                              "consistent with one fence per warp, but A's run had too few rounds to expect 3 C-like failures",
                              3.0 / NA))
    else:
        out["verdict"] = ("inconclusive-but-consistent: no violation in %d rounds of A, and the no-fence control C also "
                          "showed none, so this setup cannot discriminate" % NA)
    # Am: A with its fence timed (same mode). Its median fence duration in
    # the first round of a launch (where the races are) sets Cd's delay.
    fence_p50 = None
    if Am and "error" not in Am and Am.get("fence_ns"):
        fr = Am["fence_ns"]["first_round"]
        out["A_fence_ns_first_round"] = fr
        out["A_fence_ns_other_rounds"] = Am["fence_ns"]["other_rounds"]
        out["Am_rounds"], out["Am_violations"] = Am["verified"], Am["violating_rounds"]
        if Am["violating_rounds"]:
            out["Am_note"] = ("Am (A with a timed fence) showed %d violating rounds in %d: an A-like publish failed"
                              % (Am["violating_rounds"], Am["verified"]))
        if fr.get("p50", -1) > 0:
            fence_p50 = fr["p50"]
    # Cd: C plus a fenceless wait as long as A's fence (calibrated by Am).
    # Decides whether A's zero is attributable to the fence's ordering or
    # to the time the fence takes. Rules: README "Decision rules".
    if Cd and "error" not in Cd and VA == 0 and VC:
        NCd, VCd = Cd["verified"], Cd["violating_rounds"]
        out["Cd_rounds"], out["Cd_violations"] = NCd, VCd
        out["Cd_mean_delay_ns"] = Cd["mean_delay_ns"]
        calibrated = fence_p50 is not None and Cd["mean_delay_ns"] >= 0.9 * fence_p50
        if not calibrated:
            out["attribution"] = ("undecided (uncalibrated): Cd's mean delay %.0f ns is not at least 0.9 x A's median "
                                  "fence duration in launch first rounds (%s ns)" % (
                                      Cd["mean_delay_ns"], fence_p50 if fence_p50 is not None else "not measured"))
        elif VCd >= 3 and (VCd / NCd) * NA >= 3:
            out["attribution"] = ("the fence's ordering, not only its duration: Cd (a fenceless wait of %.0f ns, A's "
                                  "median fence %d ns) failed %d times in %d rounds, so A would have expected %.1f "
                                  "failures if its fence were only a delay" % (
                                      Cd["mean_delay_ns"], fence_p50, VCd, NCd, VCd / NCd * NA))
        elif VCd > 0:
            out["attribution"] = ("undecided: Cd failed %d times in %d rounds, too few to separate A's zero from a "
                                  "delay (needs >= 3 Cd failures and an expectation of >= 3 in A's rounds)" % (VCd, NCd))
        elif out["C_rate"] * NCd >= 3:
            out["attribution"] = ("the fence's duration: Cd (a fenceless wait of %.0f ns, A's median fence %d ns) "
                                  "showed 0 in %d rounds where C-like failures would be %.1f, so A is 'safe in "
                                  "practice here, not attributable to fence semantics'" % (
                                      Cd["mean_delay_ns"], fence_p50, NCd, out["C_rate"] * NCd))
        else:
            out["attribution"] = "undecided: Cd showed 0 but had too few rounds to expect 3 C-like failures"
    Cd15 = g("Cd15")
    if Cd15 and "error" not in Cd15:
        out["Cd15_info"] = ("C plus a fixed 1.5 us fenceless wait (informational, not a rule): %d violating rounds "
                            "in %d" % (Cd15["violating_rounds"], Cd15["verified"]))
    if B and "error" not in B and B["violating_rounds"]:
        out["B_note"] = "B (per-lane fence) showed %d violating rounds: investigate" % B["violating_rounds"]
    for v in ("A", "B", "C", "Cd", "D1", "D2", "Am"):
        r = g(v)
        if r and "error" not in r:
            out["rate_%s" % v] = r["rate_rounds_per_s"]
    return out


def lane0_idle_verdict(res):
    """A0: the protocol with a publisher that writes no payload word (lanes
    1-31 write, lane 0 fences and stores the flag), stress mode. A fence that
    covered only the issuing lane's own accesses would order nothing of the
    payload here, so C0 (same layout, no fence) is A0's power reference.
    Am0mix (one run: even warps as A0, odd warps with no payload at all)
    gives the fence's duration with and without stores of its own warp under
    the same load; if they are about equal, A0's zero cannot be separated
    from the fence's duration (README "Decision rules")."""
    A0, C0, Mx = res.get("t1_A0_stress"), res.get("t1_C0_stress"), res.get("t1_Am0mix_stress")
    E = res.get("t1_E_stress")
    out = {"mode": "stress, lane 0 idle"}
    problems = []
    for name, r in (("A0", A0), ("C0", C0), ("Am0mix", Mx)):
        if not r or "error" in r:
            continue
        if r["persistent"]:
            problems.append("%s: %d violations never healed (test bug)" % (name, r["persistent"]))
        if r["flag_anomalies"] or r["future_words"] or r["other_words"]:
            problems.append("%s: anomalies %d, future words %d, other words %d" % (
                name, r["flag_anomalies"], r["future_words"], r["other_words"]))
        if r["verified"] < r["published"]:
            problems.append("%s: %d published rounds not verified" % (name, r["published"] - r["verified"]))
    out["problems"] = problems
    if not A0 or "error" in A0 or not C0 or "error" in C0 or not E or "error" in E:
        out["verdict"] = "missing runs"
        return out
    if E["violating_rounds"] == 0:
        out["verdict"] = "INVALID: the flag-first control E (stress) showed no violation"
        return out
    if problems:
        out["verdict"] = "INVALID: " + "; ".join(problems)
        return out
    NA, VA = A0["verified"], A0["violating_rounds"]
    NC, VC = C0["verified"], C0["violating_rounds"]
    pC = VC / NC if NC else 0
    out.update({"A0_rounds": NA, "A0_violations": VA, "C0_rounds": NC, "C0_violations": VC,
                "expected_A0_violations_if_like_C0": pC * NA})
    ratio = None
    if Mx and "error" not in Mx and Mx.get("fence_ns"):
        fw = Mx["fence_ns"]["first_round"].get("p50", -1)
        fi = Mx["fence_ns"]["first_round_idle_warps"].get("p50", -1)
        out["fence_ns_first_round_p50_lane0_idle_warps"] = fw
        out["fence_ns_first_round_p50_warps_without_payload"] = fi
        if fw > 0 and fi > 0:
            ratio = fi / fw
            out["fence_ratio_without_over_with_own_stores"] = ratio
        if Mx["violating_rounds"]:
            out["Am0mix_note"] = "Am0mix (A0 layout, fence timed) failed %d times in %d" % (
                Mx["violating_rounds"], Mx["verified"])
    if VA > 0:
        out["verdict"] = ("a leader fence does NOT order the other lanes' stores: A0 failed %d times in %d rounds"
                          % (VA, NA))
    elif VC > 0 and pC * NA >= 3:
        base = ("no violation observed in %d rounds of A0 (the publishing lane wrote no payload) while C0 failed "
                "%d times in %d (95%% upper bound %.2e per round)" % (NA, VC, NC, 3.0 / NA))
        if ratio is not None and ratio <= 0.5:
            out["verdict"] = (base + ": evidence that one leader fence orders the stores of lanes that did not "
                              "fence, on this GPU; the leader's fence waited for its warp's stores (a fence in a "
                              "warp without payload took %.2fx as long under the same load)" % ratio)
        elif ratio is not None:
            out["verdict"] = (base + ", but not separable from the fence's duration: a fence in a warp without "
                              "payload took %.2fx as long under the same load, so a fence that ordered nothing of "
                              "the payload would have hidden the race too" % ratio)
        else:
            out["verdict"] = base + "; the fence baseline (Am0mix) is missing, so not separable from the fence's duration"
    elif VC > 0:
        out["verdict"] = "consistent but under-powered: A0 0 in %d rounds, C0 rate %.2e" % (NA, pC)
    else:
        out["verdict"] = "inconclusive-but-consistent: C0 showed no violation either"
    return out


def t2_verdicts(res):
    out = {}
    ctl = [res.get(k) for k in ("t2_control_cas", "t2_control_casfence")]
    ctl = [c for c in ctl if c and "error" not in c]
    out["control_stale_rounds"] = {c["variant"]: c["stale_rounds"] for c in ctl}
    detects = any(c["stale_rounds"] > 0 for c in ctl)
    out["checker_detects"] = detects
    for k in ("t2_cas", "t2_casfence", "t2_casacq", "t2_cas_stress"):
        r = res.get(k)
        if not r or "error" in r:
            continue
        v = {}
        v["rounds"] = r["cpu_full_seen"]
        v["stale_rounds"] = r["stale_rounds"]
        v["double_winners"] = r["double_winners_gpu"]
        v["winners_bad_cpu"] = r["winners_bad_cpu"]
        v["cpu_bad_gpu_payload_rounds"] = r["cpu_bad_gpu_payload_rounds"]
        v["iter_cap_hits"] = r["iter_cap_hits"]
        v["rate_rounds_per_s"] = r["rate_rounds_per_s"]
        if v["double_winners"]:
            v["verdict"] = "CAS state machine broken: %d double winners" % v["double_winners"]
        elif v["winners_bad_cpu"]:
            # the GPU's atomicAdd on winners landed before the CPU's reset of
            # winners (a plain store before the release of FREE) became
            # visible, or vice versa: the CPU-to-GPU ordering failed
            v["verdict"] = ("the GPU acquire is insufficient: the CPU found winners != 1 at FULL in %d rounds "
                            "(its reset of the winners word, released with FREE, was not seen by the winner's add)"
                            % v["winners_bad_cpu"])
        elif v["stale_rounds"]:
            v["verdict"] = "the GPU acquire is insufficient: %d rounds saw the CPU's reset late" % v["stale_rounds"]
        elif not detects:
            v["verdict"] = "no stale read in %d rounds, but the control showed none either: inconclusive" % v["rounds"]
        else:
            v["verdict"] = ("no stale read and no double winner in %d rounds (95%% bound %.2e per round); the control "
                            "shows the check can detect stale data" % (v["rounds"], 3.0 / max(v["rounds"], 1)))
        if v["cpu_bad_gpu_payload_rounds"]:
            v["verdict"] += "; GPU->CPU payload check failed in %d rounds" % v["cpu_bad_gpu_payload_rounds"]
        out[k] = v
    return out


def main():
    run = sys.argv[1]
    res = load(run)
    ver = {"t1_stress": t1_verdicts(res, "stress"), "t1_normal": t1_verdicts(res, "normal"),
           "t1_long": t1_verdicts(res, "long"), "t1_lane0_idle": lane0_idle_verdict(res),
           "t2": t2_verdicts(res)}
    json.dump(ver, open(os.path.join(run, "verdicts.json"), "w"), indent=1)
    cols = ["name", "test", "variant", "blocks", "stress", "poison", "lane0_idle", "chunk_rounds", "launches", "mean_delay_ns",
            "rounds_requested",
            "published", "verified", "violating_rounds", "violations_in_first_round_of_launch", "bad_words", "stale_words", "poison_words", "future_words", "healed",
            "persistent", "heal_ms_max", "flag_anomalies", "dead_warps", "cap_hits", "deadline_hits",
            "cpu_full_seen", "stale_rounds", "double_winners_gpu", "winners_bad_cpu",
            "cpu_bad_gpu_payload_rounds", "iter_cap_hits", "budget_hit", "gpu_wall_s", "rate_rounds_per_s"]
    with open(os.path.join(run, "summary.csv"), "w") as f:
        w = csv.writer(f)
        w.writerow(cols)
        for n, r in sorted(res.items()):
            w.writerow([n] + [r.get(c, "") for c in cols[1:]])
    L = ["# e4-publish summary\n"]
    L.append("| run | rounds verified | violating rounds (in a launch's first round) | stale / poison / future words | healed / persistent | rate (rounds/s) | budget hit |")
    L.append("|---|---:|---:|---|---|---:|---|")
    for n, r in sorted(res.items()):
        if "error" in r:
            L.append("| %s | error: %s | | | | | |" % (n, r["error"]))
        elif r.get("test") == 1:
            L.append("| %s | %d | %d (%d) | %d / %d / %d | %d / %d | %.0f | %s |" % (
                n, r["verified"], r["violating_rounds"], r.get("violations_in_first_round_of_launch", 0),
                r["stale_words"], r.get("poison_words", 0),
                r["future_words"], r["healed"], r["persistent"], r["rate_rounds_per_s"], r["budget_hit"]))
        else:
            L.append("| %s | %d | stale rounds %d, double winners %d | GPU payload bad rounds %d | - | %.0f | %s |" % (
                n, r["cpu_full_seen"], r["stale_rounds"], r["double_winners_gpu"] + r["winners_bad_cpu"],
                r["cpu_bad_gpu_payload_rounds"], r["rate_rounds_per_s"], r["budget_hit"]))
    for k in ("t1_stress", "t1_normal", "t1_long"):
        L.append("\n## Test 1, %s\n" % k[3:])
        L.append("- verdict: **%s**" % ver[k].get("verdict"))
        for x in ("attribution", "A_rounds", "A_violations", "C_rounds", "C_violations",
                  "expected_A_violations_if_like_C", "Cd_rounds", "Cd_violations", "Cd_mean_delay_ns",
                  "A_fence_ns_first_round", "A_fence_ns_other_rounds", "Am_rounds", "Am_violations", "Am_note",
                  "Cd15_info", "rate_A", "rate_B", "B_note"):
            if x in ver[k]:
                L.append("- %s: %s" % (x, ver[k][x]))
        if ver[k].get("problems"):
            L.append("- problems: %s" % "; ".join(ver[k]["problems"]))
    v0 = ver["t1_lane0_idle"]
    L.append("\n## Test 1, stress, publishing lane writes no payload (A0, C0, Am0mix)\n")
    L.append("- verdict: **%s**" % v0.get("verdict"))
    for x in ("A0_rounds", "A0_violations", "C0_rounds", "C0_violations", "expected_A0_violations_if_like_C0",
              "fence_ns_first_round_p50_lane0_idle_warps", "fence_ns_first_round_p50_warps_without_payload",
              "fence_ratio_without_over_with_own_stores", "Am0mix_note"):
        if x in v0:
            L.append("- %s: %s" % (x, v0[x]))
    if v0.get("problems"):
        L.append("- problems: %s" % "; ".join(v0["problems"]))
    L.append("\n## Test 2\n")
    L.append("- control stale rounds: %s (checker detects: %s)" % (ver["t2"]["control_stale_rounds"],
                                                                 ver["t2"]["checker_detects"]))
    for k, v in ver["t2"].items():
        if isinstance(v, dict) and "verdict" in v:
            L.append("- %s: **%s**" % (k, v["verdict"]))
    open(os.path.join(run, "summary.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L))


if __name__ == "__main__":
    main()
