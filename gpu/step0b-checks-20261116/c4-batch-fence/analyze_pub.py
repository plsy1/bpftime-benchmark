#!/usr/bin/env python3
"""C4 correctness analysis: the pubc JSON files of one run -> verdicts.json,
summary.md (decision rules in README.md, "Decision rules", part B).

Usage: python3 -I analyze_pub.py RUN_DIR/pubc
"""
import glob
import json
import os
import sys

PROTOCOLS = ["ar_aa", "rmw_ar", "ar32_aa", "ar_sc", "tf_sc", "rel_sc", "ar_ar", "rel_ar",
             "ar32_sc", "ar32_ar"]
LITERAL = {"ar_aa", "rmw_ar", "ar32_aa"}
SHARED = {"ar_ar", "rel_ar", "ar32_ar"}


def tag(v):
    return " (literal, GPU side)" if v in LITERAL else " (literal, shared fence)" if v in SHARED else ""
CONTROLS = ["none", "sysonly", "nosys", "early"]


def harness_ok(d):
    bad = []
    for k in ("persistent", "flag_anomalies", "future_words", "other_words", "dead_warps",
              "cap_hits", "deadline_hits"):
        if d.get(k, 0):
            bad.append("%s=%s" % (k, d[k]))
    if d["chunk_rounds_verified"] != d["completions"]:
        bad.append("verified %d != published %d" % (d["chunk_rounds_verified"], d["completions"]))
    if d["chunk_rounds_verified"] == 0:
        bad.append("nothing verified")
    return bad


def main():
    rd = sys.argv[1]
    runs = []
    for p in sorted(glob.glob(os.path.join(rd, "*.json"))):
        try:
            d = json.load(open(p))
        except (ValueError, OSError):
            continue
        if "variant" not in d:
            continue
        d["_file"] = os.path.basename(p)
        d["_timed"] = bool(d.get("fence_ns"))
        runs.append(d)
    keys = sorted({(d["layout"], d["G"]) for d in runs}, key=lambda t: (t[0], t[1]))
    out = {"groups": [], "fence_ns": []}
    md = ["# C4 correctness summary (pubc)", "",
          "Counts are chunk-rounds (one chunk-round = G warp records behind one system fence). "
          "Rates include the CPU readers (4 threads, one chunk per reader at a time), so they are rates "
          "of the whole loop, not GPU costs. A smoke is not a measurement.", ""]
    for layout, G in keys:
        grp = [d for d in runs if d["layout"] == layout and d["G"] == G and not d["_timed"]
               and d.get("interleaved", 1) == 1]
        if not grp:
            continue
        by = {}
        for d in grp:
            by.setdefault(d["variant"], []).append(d)
        # pool repeated runs of one variant
        pool = {}
        for v, ds in by.items():
            pool[v] = {
                "N": sum(x["chunk_rounds_verified"] for x in ds),
                "records": sum(x["records_verified"] for x in ds),
                "V": sum(x["violating_rounds"] for x in ds),
                "V_first": sum(x["violations_in_first_round_of_launch"] for x in ds),
                "bad_completer": sum(x["bad_words_completer_warp"] for x in ds),
                "bad_others": sum(x["bad_words_other_warps"] for x in ds),
                "stale": sum(x["stale_words"] for x in ds),
                "poison": sum(x["poison_words"] for x in ds),
                "healed": sum(x["healed"] for x in ds),
                "harness": [b for x in ds for b in harness_ok(x)],
                "rate_records": sum(x["rate_records_per_s"] for x in ds) / len(ds),
                "files": [x["_file"] for x in ds],
                "budget_hit": any(x["budget_hit"] for x in ds),
            }
        g = {"layout": layout, "G": G, "variants": pool, "verdicts": {}}
        e = pool.get("early")
        valid = bool(e) and e["V"] > 0 and not e["harness"]
        g["valid"] = valid
        g["validity_reason"] = ("early (flag before records) failed %d of %d" % (e["V"], e["N"]) if e else "no early run")
        nn = pool.get("none")
        # review: a power reference only from a harness-clean none run
        p_none = (nn["V"] / nn["N"]) if nn and nn["N"] and not nn["harness"] else None
        g["p_none"] = p_none
        md += ["## %s, G = %d warps per system fence (%d chunks)" % (layout, G, grp[0]["chunks"]), "",
               "- validity: %s -> %s" % (g["validity_reason"], "**valid**" if valid else "**INVALID** (the checker did not show it can fail, or the harness misbehaved)"),
               "- power reference: no fence at all (none): %s" % (
                   "%d of %d chunk-rounds violated (p = %.3g)" % (nn["V"], nn["N"], p_none) if nn else "not run"), "",
               "| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |",
               "|---|---:|---:|---:|---|---|---|---:|"]
        for v in PROTOCOLS + CONTROLS:
            x = pool.get(v)
            if not x:
                continue
            md.append("| %s%s | %d | %d | %d | %d / %d | %d / %d | %s | %.3gM |" % (
                v, tag(v), x["N"], x["V"], x["V_first"],
                x["bad_completer"], x["bad_others"], x["stale"], x["poison"],
                "ok" if not x["harness"] else "BAD: " + ", ".join(x["harness"]),
                x["rate_records"] / 1e6))
        md.append("")
        for v in PROTOCOLS:
            x = pool.get(v)
            if not x:
                continue
            if x["harness"]:
                verdict = "INVALID run (%s)" % ", ".join(x["harness"])
            elif not valid:
                verdict = "mode INVALID (%s)" % g["validity_reason"]
            elif x["V"] > 0:
                verdict = ("VIOLATION observed: %d of %d chunk-rounds. The model forbids it for this "
                           "protocol, so either the compiled protocol or the harness is wrong: investigate "
                           "before any design uses it." % (x["V"], x["N"]))
            else:
                exp = p_none * x["N"] if p_none is not None else 0.0
                power = ("with power (the no-fence control would have given about %.3g failures in as many rounds)" % exp
                         if exp >= 3 else "under-powered (the no-fence control would have given %.3g)" % exp)
                verdict = ("consistent with the model: no violation in %d chunk-rounds (%d warp records); "
                           "95%% upper bound 3/N = %.2g per chunk-round; %s; not attributable to the fences' "
                           "ordering (a system fence lasts microseconds, see the fence durations)"
                           % (x["N"], x["records"], 3.0 / x["N"], power))
            g["verdicts"][v] = verdict
            md.append("- **%s**%s: %s" % (v, tag(v), verdict))
        for v, what in (("sysonly", "the contributors' gpu-scope release"),
                        ("nosys", "the completer's system-scope fence")):
            x = pool.get(v)
            if not x:
                continue
            if x["harness"]:
                verdict = "INVALID run (%s)" % ", ".join(x["harness"])
            elif not valid:
                verdict = "mode INVALID (%s)" % g["validity_reason"]
            elif x["V"] > 0:
                verdict = "%s is needed in practice too (observed: %d of %d violated)" % (what, x["V"], x["N"])
            else:
                exp = p_none * x["N"] if p_none is not None else 0.0
                if exp >= 3:
                    verdict = ("0 of %d without %s although no fence at all would have failed about %.3g times: "
                               "on this GPU the remaining fences (or their duration) hide the race. Its necessity "
                               "is not shown by data and rests on the model; a design keeps it." % (x["N"], what, exp))
                else:
                    verdict = "0 of %d without %s, under-powered (expected %.3g)" % (x["N"], what, exp)
            g["verdicts"][v] = verdict
            md.append("- control %s: %s" % (v, verdict))
        if nn:
            g["verdicts"]["none"] = ("INVALID run (%s): no power reference" % ", ".join(nn["harness"]) if nn["harness"] else
                                     "the check can fail at this G (power reference)" if nn["V"] > 0 else
                                     "inconclusive-but-consistent: no fence at all also showed 0, the setup cannot discriminate at this G")
            md.append("- control none: " + g["verdicts"]["none"])
        md.append("")
        out["groups"].append(g)
    # review: the timed and the contiguous runs carry protocol variants too;
    # a violation or a harness fault there must reach the verdicts
    extra = []
    for d in runs:
        if not (d["_timed"] or d.get("interleaved", 1) == 0):
            continue
        bad = harness_ok(d)
        v = d["variant"]
        if bad:
            vd = "INVALID run (%s)" % ", ".join(bad)
        elif v in PROTOCOLS and d["violating_rounds"] > 0:
            vd = "VIOLATION observed: %d of %d chunk-rounds: investigate" % (d["violating_rounds"], d["chunk_rounds_verified"])
        elif v in PROTOCOLS:
            vd = "no violation in %d chunk-rounds" % d["chunk_rounds_verified"]
        else:
            vd = "%d of %d violating (control)" % (d["violating_rounds"], d["chunk_rounds_verified"])
        extra.append({"file": d["_file"], "layout": d["layout"], "variant": v, "G": d["G"],
                      "timed": d["_timed"], "contiguous": d.get("interleaved", 1) == 0, "verdict": vd})
    out["extra_runs"] = extra
    out["any_protocol_violation"] = (
        any("VIOLATION" in vd for g in out["groups"] for k, vd in g["verdicts"].items() if k in PROTOCOLS)
        or any("VIOLATION" in e["verdict"] for e in extra))
    failed = sorted(os.path.basename(p)[:-len(".stderr")] for p in glob.glob(os.path.join(rd, "*.stderr")))
    no_json = [f for f in failed if not os.path.exists(os.path.join(rd, f + ".json"))]
    out["calls_with_stderr"] = failed
    out["calls_without_result"] = no_json
    md += ["## Calls", "",
           "- calls that wrote to stderr: %s" % (", ".join(failed) or "none"),
           "- calls without a result file (their verdicts are missing above): %s" % (", ".join(no_json) or "none"),
           "- any protocol violation anywhere in this run: %s" % ("**YES**" if out["any_protocol_violation"] else "no"), ""]
    if extra:
        md += ["## Timed and contiguous runs: verdicts", ""]
        md += ["- %s (%s %s G=%d%s): %s" % (e["file"], e["layout"], e["variant"], e["G"],
                                          ", timed" if e["timed"] else ", contiguous", e["verdict"]) for e in extra]
        md.append("")
    timed = [d for d in runs if d["_timed"]]
    if timed:
        md += ["## System-fence durations (completer, --time-fence runs)", "",
               "| layout | variant | G | first round of a launch p50 / p90 / p99 (ns) | other rounds p50 / p90 / p99 (ns) | mean | max | violating |",
               "|---|---|---:|---|---|---:|---:|---:|"]
        for d in timed:
            f = d["fence_ns"]
            fr, ot = f["first_round"], f["other_rounds"]
            md.append("| %s | %s | %d | %d / %d / %d | %d / %d / %d | %.0f | %d | %d of %d |" % (
                d["layout"], d["variant"], d["G"], fr["p50"], fr["p90"], fr["p99"], ot["p50"], ot["p90"],
                ot["p99"], f["mean"], f["max"], d["violating_rounds"], d["chunk_rounds_verified"]))
            out["fence_ns"].append({"layout": d["layout"], "variant": d["variant"], "G": d["G"], **f})
        md.append("")
    contig = [d for d in runs if d.get("interleaved", 1) == 0]
    if contig:
        md += ["## Contiguous chunks (a chunk = consecutive warps)", ""]
        for d in contig:
            md.append("- %s %s G=%d: %d of %d violating" % (d["layout"], d["variant"], d["G"],
                                                            d["violating_rounds"], d["chunk_rounds_verified"]))
        md.append("")
    json.dump(out, open(os.path.join(rd, "verdicts.json"), "w"), indent=1)
    open(os.path.join(rd, "summary.md"), "w").write("\n".join(md) + "\n")
    print("\n".join(md))


if __name__ == "__main__":
    main()
