"""summarize.py OUTDIR: apply check 3's decision rules (README.md) to a run.

Reads OUTDIR/partA, partB0, partB-llama, partB-nsys, partC (any may be
missing) and writes OUTDIR/summary.md and OUTDIR/decisions.json.
Standard library only; run with python3 -I.
"""
import csv
import glob
import json
import os
import statistics
import sys

# ---- thresholds, fixed before any real run (README "Decision rules") ----
A_MIN_FLAGGED = 1_000_000      # flagged chunk observations per layout
A_MIN_MIXED = 10_000           # chunks caught mid-write per layout
A_MIN_CHANGE_SHARE = 0.9       # share of copies in which some flag changed
B_IDLE_MAX_LOSS = 0.01         # or the no-shim noise, whichever is larger
B_BOUNDED_MAX_LOSS = 0.05      # 1 MiB every 1 ms, pp512 and tg128
B_MAX_NOISE = 0.03             # no-shim spread above this: inconclusive
B_MIN_RATE_SHARE = 0.5         # c1m1ms must reach >= 50% of 1000 copies/s
OFFERED_GBPS = (0.9, 6.2)      # design note: one record per warp, offered rate
C_MIN_SEEN_SHARE = 0.5         # distinct generations seen / updates made
# added in the review of 2026-11-15, before any real run (README "Revisions")
B0_MIN_ITERS = 100             # capture iterations per relaxed safety case
B0_MIN_COLLISIONS = 1000       # copies that overlapped an open capture, per case
TIMEOUT_RCS = (124, 137)       # rc of a run ended by timeout(1)


def jl(path):
    out = []
    if not os.path.exists(path):
        return out
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                try:
                    out.append(json.loads(line))
                except ValueError:
                    out.append({"_bad": line[:200]})
    return out


def jload(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def pct(x):
    return "%.1f%%" % (100 * x) if x is not None else "-"


# ------------------------------------------------------------------ part A
def part_a(o, md):
    d = os.path.join(o, "partA")
    runs = [r for r in jl(os.path.join(d, "order.jsonl")) if r.get("test") == "order"]
    res = {"present": bool(runs)}
    if not runs:
        md.append("## Part A: not run\n")
        return res
    md.append("## Part A: ordering of a concurrent DtoH copy\n")
    md.append("| layout | writer | area | chunk B | copies | flagged | viol chunks | older words | mixed | copies w/ change | quiet ok | writer exits stop/cap/deadline | valid |")
    md.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|")
    key = lambda r: (r["layout"], r["area_payload_bytes"], r["wpl"])
    rev = {key(r): r for r in runs if r["writer"] == "reversed"}
    nrm = {key(r): r for r in runs if r["writer"] == "normal"}
    # the same data read in the opposite order; when it violates with the
    # normal writer, it shows the copy could catch this layout's race too
    mirror = {"before": "after", "after": "before", "farbefore": "farafter",
              "farafter": "farbefore", "sepff": "seppf", "seppf": "sepff"}
    per = {}
    for r in runs:
        quiet = (r["quiet_viol_chunks"] == 0 and r["quiet_mixed_chunks"] == 0
                 and r["quiet_flag_mismatch"] == 0 and r["quiet_unflagged"] == 0)
        overlap = (r["mixed_chunks"] > 0 and r["copies"] > 0
                   and r["copies_with_change"] >= A_MIN_CHANGE_SHARE * r["copies"])
        writer_ok = r["exit_deadline"] == 0 and r["exit_seqcap"] == 0
        mk = (mirror.get(r["layout"]), r["area_payload_bytes"], r["wpl"])
        ctrl = (rev.get(key(r), {}).get("viol_chunks", 0) > 0
                or nrm.get(mk, {}).get("viol_chunks", 0) > 0)
        valid = quiet and overlap and writer_ok and (ctrl or r["writer"] == "reversed")
        r["_valid"], r["_quiet"] = valid, quiet
        md.append("| %s | %s | %d | %d | %d | %d | %d | %d | %d | %d | %s | %d/%d/%d | %s |" % (
            r["layout"], r["writer"], r["area_payload_bytes"], r["chunk_payload_bytes"],
            r["copies"], r["flagged"], r["viol_chunks"], r["older_words"], r["mixed_chunks"],
            r["copies_with_change"], "yes" if quiet else "NO", r["exit_stop"], r["exit_seqcap"],
            r["exit_deadline"], "yes" if valid else "no"))
        if r["writer"] == "normal":
            per.setdefault(r["layout"], []).append(r)
    verdict = {}
    for lay, rs in per.items():
        viol = sum(r["viol_chunks"] for r in rs if r["_quiet"])
        flagged = sum(r["flagged"] for r in rs if r["_valid"])
        mixed = sum(r["mixed_chunks"] for r in rs if r["_valid"])
        if viol > 0:
            v = "violates"
        elif all(r["_valid"] for r in rs) and flagged >= A_MIN_FLAGGED and mixed >= A_MIN_MIXED:
            v = "consistent"
        else:
            v = "inconclusive"
        verdict[lay] = {"verdict": v, "viol_chunks": viol, "flagged_valid": flagged,
                        "mixed_valid": mixed, "runs": len(rs)}
    # the copy order that must race (payload copied before flags) must show it
    seppf_ctrl = verdict.get("seppf", {}).get("verdict") == "violates"
    if not seppf_ctrl:
        for lay in ("after", "before", "farafter", "farbefore", "sepff"):
            if verdict.get(lay, {}).get("verdict") == "consistent":
                verdict[lay]["verdict"] = "inconclusive"
                verdict[lay]["why"] = "seppf control showed no violation"
    single = ("after", "before", "farafter", "farbefore")
    va = {k: verdict.get(k, {}).get("verdict", "missing") for k in single + ("sepff",)}
    if all(va[k] == "consistent" for k in single):
        ans = "yes"  # one copy suffices, whatever the flag's distance and side
    elif va["sepff"] == "consistent":
        ans = "fixable"  # flags first, then payload, in stream order
    elif va["sepff"] == "violates":
        ans = "no"
    else:
        ans = "inconclusive"
    single_ok = [k for k in single if va[k] == "consistent"]
    # sub-verdict (review 2026-11-15): one copy with every flag at a lower
    # address than its payload. It rests on the copy engine reading in
    # ascending address order, which is observed, not documented.
    ff = [va[k] for k in ("before", "farbefore")]
    if "violates" in ff:
        flag_first = "violates"
    elif all(x == "consistent" for x in ff):
        flag_first = "consistent"
    else:
        flag_first = "inconclusive"
    res.update({"layouts": verdict, "seppf_control_violates": seppf_ctrl, "answer": ans,
                "single_copy_consistent": single_ok, "flag_first_single_copy": flag_first,
                "ok": ans in ("yes", "fixable")})
    md.append("")
    md.append("Per layout (normal writer): " + ", ".join(
        "%s=%s" % (k, v["verdict"]) for k, v in sorted(verdict.items())))
    md.append("Control seppf (payload copied before flags) violates: %s" % seppf_ctrl)
    md.append("**A answer: %s** (yes: one copy suffices for every flag placement; fixable: two-step copy,"
              " flags first)" % ans)
    if ans != "yes" and single_ok:
        md.append("Single-copy layouts that passed: %s. They are not relied on: the copy engine's read order"
                  " is not documented, and another placement of the same data did not pass." % ", ".join(single_ok))
    md.append("Sub-verdict, one copy with flags placed before their payload (before + farbefore; rests on the"
              " observed, undocumented ascending copy order): %s. after and farafter read each flag after its"
              " payload, a reader-side race that no writer protocol fixes." % flag_first)
    md.append("")
    seq = [r for r in jl(os.path.join(d, "order-seqcap.jsonl")) if r.get("test") == "order"]
    if seq:
        r = seq[0]
        res["seqcap_check"] = {"relaunches": r["relaunches"], "exit_seqcap": r["exit_seqcap"],
                               "viol": r["viol_chunks"], "quiet_flag_mismatch": r["quiet_flag_mismatch"]}
        md.append("Sequence-cap path: %d relaunches, %d cap exits, %d violations, %d quiet flag mismatches\n" % (
            r["relaunches"], r["exit_seqcap"], r["viol_chunks"], r["quiet_flag_mismatch"]))
    bw = [r for r in jl(os.path.join(d, "bw.jsonl")) if r.get("test") == "bw"]
    if bw:
        md.append("| writer | bytes | lat p50 us | lat p99 us | lat max us | event p50 us | issue p50 us | GB/s | writer ran throughout |")
        md.append("|---|---:|---:|---:|---:|---:|---:|---:|---|")
        for r in bw:
            md.append("| %s | %d | %.1f | %.1f | %.1f | %.1f | %.1f | %.2f | %s |" % (
                r["writer"], r["bytes"], r["lat_us_p50"], r["lat_us_p99"], r["lat_us_max"],
                r["event_us_p50"], r["issue_us_p50"], r["tput_GBps"],
                "-" if r["writer"] == "none" else
                ("yes" if r["writer_running_before"] and r["writer_running_after"] else "NO")))
        res["bw"] = bw
        top = [r["tput_GBps"] for r in bw if r["bytes"] == 16 << 20 and r["writer"] == "running"]
        if top:
            md.append("\n16 MiB copies with the writer running: %.2f GB/s against the offered %.1f-%.1f GB/s." % (
                top[0], *OFFERED_GBPS))
        md.append("")
    return res


# ------------------------------------------------------------------ part B0
def part_b0(o, md):
    d = os.path.join(o, "partB0")
    lines = jl(os.path.join(d, "captest.jsonl"))
    res = {"present": bool(lines)}
    if not lines:
        md.append("## Part B0: not run\n")
        return res
    cases = {}
    cur = None
    for x in lines:
        if "case" in x:
            cur = x["case"]
            cases[cur] = None
        elif x.get("test") == "captest" and cur:
            cases[cur] = x
    rcs = {}  # case -> rc; rc is the last column (older runs.csv left shim_env unquoted)
    try:
        with open(os.path.join(d, "runs.csv")) as f:
            for row in list(csv.reader(f))[1:]:
                if len(row) >= 2:
                    try:
                        rcs[row[0]] = int(row[-1])
                    except ValueError:
                        pass
    except OSError:
        pass
    md.append("## Part B0: copier next to a capture loop (captest)\n")
    md.append("| case | mode | iters | capture open | end errors (first) | kernel launch errors | graph launches | shim copies | copies during a capture | shim copy/wait errors (first) | shim counts match |")
    md.append("|---|---|---:|---:|---|---:|---:|---:|---:|---|---|")
    rows = {}
    for name, x in cases.items():
        shim = jload(os.path.join(d, name + ".shim.json"))
        if x is None:
            rc = rcs.get(name)
            md.append("| %s | - | crashed or no output (rc %s) | | | | | | | | |" % (name, rc))
            rows[name] = {"ran": False, "rc": rc, "timed_out": rc in TIMEOUT_RCS}
            continue
        match = None
        if shim:
            g = shim["graph_api"]
            match = (g["cudaStreamBeginCapture"]["calls"] == x["begin"]["calls"]
                     and g["cudaStreamEndCapture"]["calls"] == x["end"]["calls"]
                     and g["cudaGraphLaunch"]["calls"] == x["graph_launch"]["calls"]
                     and g["cudaGraphInstantiate"]["calls"] == x["instantiate"]["calls"]
                     and g["cudaGraphExecUpdate"]["calls"] == x["exec_update"]["calls"])
        cerr = (shim["copies"]["errors"] + shim["copies"]["wait_errors"]) if shim else 0
        cfirst = (shim["copies"]["first_err"] or shim["copies"]["wait_first_err"]) if shim else 0
        errs = sum(x[k]["errors"] for k in ("begin", "kernel_launch", "end", "instantiate",
                                             "exec_update", "graph_launch", "sync"))
        during = shim["copies"].get("during_capture") if shim else None
        rows[name] = {"ran": True, "errors": errs, "end_errors": x["end"]["errors"],
                      "copies": shim["copies"]["n"] if shim else 0, "copier_errors": cerr,
                      "during_capture": during, "shim_log": shim is not None,
                      "unsafe_errors": shim["unsafe"]["errors"] if shim else 0,
                      "match": match, "iters": x["iters"],
                      "capture_open_share": x["capture_open_share"]}
        md.append("| %s | %s/%s | %d | %.2f | %d (%d) | %d | %d | %s | %s | %s | %s |" % (
            name, x["mode"], x["stream"], x["iters"], x["capture_open_share"], x["end"]["errors"],
            x["end"]["first_err"], x["kernel_launch"]["errors"], x["graph_launch"]["calls"],
            shim["copies"]["n"] if shim else "-", "-" if during is None else during,
            ("%d (%d)" % (cerr, cfirst)) if shim else "-",
            "-" if match is None else ("yes" if match else "NO")))
    shim_rows = [v for k, v in rows.items() if k != "noshim" and v.get("ran")]
    interpose_ok = bool(shim_rows) and all(v["match"] for v in shim_rows)
    # can-fail controls: a foreign thread's call invalidates a capture and
    # captest counts it. The relaxed one (legacy-stream copy against a
    # blocking capture stream) is the control for the relaxed-mode claim
    # (review 2026-11-15); the global ones are reported.
    gpos = [rows.get(k, {}) for k in ("memalloc-global", "ctxsync-global")]
    global_ctrl = any(p.get("ran") and p.get("end_errors", 0) > 0 for p in gpos)
    rb = rows.get("legacysync-relaxed-blk", {})
    relaxed_ctrl = bool(rb.get("ran")) and rb.get("end_errors", 0) > 0
    fail_why, unk_why = [], []
    for k in ("async-relaxed", "asyncevent-relaxed"):
        v = rows.get(k)
        if v is None:
            unk_why.append("%s missing" % k)
        elif not v.get("ran"):
            (unk_why if v.get("timed_out") else fail_why).append(
                "%s %s (rc %s)" % (k, "timed out" if v.get("timed_out") else "crashed", v.get("rc")))
        elif v["errors"] or v["copier_errors"]:
            fail_why.append("%s: %d captest errors, %d copier errors" % (k, v["errors"], v["copier_errors"]))
        elif not v["shim_log"] or v["copies"] == 0:
            unk_why.append("%s: no shim log or no copies" % k)
        elif v["iters"] < B0_MIN_ITERS or (v["during_capture"] or 0) < B0_MIN_COLLISIONS:
            unk_why.append("%s: %d iterations, %s copies during a capture (need %d, %d)" % (
                k, v["iters"], v["during_capture"], B0_MIN_ITERS, B0_MIN_COLLISIONS))
    if not interpose_ok:
        unk_why.append("interposer counts differ from captest's")
    if not relaxed_ctrl:
        unk_why.append("relaxed control legacysync-relaxed-blk showed no invalidation")
    # baseline: without copies (no shim, idle shim) the loop must run clean,
    # or no failure can be put on the copier (review 2026-11-15)
    base_ok = all(rows.get(k, {}).get("ran") and rows[k]["errors"] == 0
                  for k in ("noshim", "idle-relaxed"))
    if not base_ok:
        unk_why.append("baseline noshim/idle-relaxed missing, crashed or with errors")
    if fail_why and base_ok:
        verdict = "fail"
    elif fail_why or unk_why:
        verdict = "inconclusive"
    else:
        verdict = "pass"
    gl = rows.get("async-global", {})
    res.update({"cases": rows, "interposition_ok": interpose_ok, "positive_control_relaxed": relaxed_ctrl,
                "positive_control_global": global_ctrl, "verdict": verdict,
                "fail_why": fail_why, "inconclusive_why": unk_why,
                "global_safe": bool(gl.get("ran")) and gl.get("errors", 1) == 0
                and gl.get("copier_errors", 1) == 0,
                "ok": verdict == "pass"})
    md.append("")
    md.append("Interposer counts equal captest's own: %s. Relaxed-mode control (legacy-stream copy, blocking capture"
              " stream) invalidates captures: %s. Global-mode controls (memalloc or ctxsync): %s. In global mode the"
              " async copier is safe: %s." % (interpose_ok, relaxed_ctrl, global_ctrl, res["global_safe"]))
    md.append("**B0 verdict (async copier safe next to relaxed-mode captures): %s**%s\n" % (
        verdict, ("; " + "; ".join(fail_why + unk_why)) if (fail_why or unk_why) else ""))
    return res


# ------------------------------------------------------------------ part B llama
def part_b(o, md):
    d = os.path.join(o, "partB-llama")
    path = os.path.join(d, "runs.csv")
    res = {"present": os.path.exists(path)}
    if not res["present"]:
        md.append("## Part B: not run\n")
        return res
    with open(path) as f:
        runs = list(csv.DictReader(f))
    md.append("## Part B: copier shim inside llama-bench, graphs on\n")
    ts = {}   # (config, test) -> [avg_ts per round]
    gate = {}  # tag -> dict
    launches = {}  # (round, test, config) -> cudaGraphLaunch calls
    for r in runs:
        tag = "r%s-%s-%s" % (r["round"], r["config"], r["test"])
        if r["graphs"] == "off":
            tag += "-graphsoff"
        out = jload(os.path.join(d, tag + ".json")) or []
        err = ""
        try:
            with open(os.path.join(d, tag + ".err"), errors="replace") as f:
                err = f.read()
        except OSError:
            pass
        shim = jload(os.path.join(d, tag + ".shim.json"))
        avg = out[0]["avg_ts"] if out and isinstance(out, list) and "avg_ts" in out[0] else None
        g = {"rc": int(r["rc"]), "cuda_error_in_stderr": "CUDA error" in err, "avg_ts": avg,
             "shim": shim is not None}
        if shim:
            ga = shim["graph_api"]
            g["graph_errors"] = sum(v["errors"] for k, v in ga.items() if k != "begin_modes")
            g["launches"] = ga["cudaGraphLaunch"]["calls"]
            g["captures"] = ga["cudaStreamBeginCapture"]["calls"]
            g["copies"] = shim["copies"]["n"]
            g["copy_errors"] = shim["copies"]["errors"] + shim["copies"]["wait_errors"]
            g["achieved_GBps"] = shim["copies"]["achieved_GBps"]
            g["copies_per_s"] = shim["copies"]["copies_per_s"]
            g["missed_periods"] = shim["copies"]["missed_periods"]
            g["during_capture"] = shim["copies"].get("during_capture")
            g["lat_p50"] = shim["latency_us"]["p50"]
            g["lat_p99"] = shim["latency_us"]["p99"]
            g["lat_max"] = shim["latency_us"]["max"]
            g["ctx_wait_s"] = shim["times_s"]["ctx_active"]
            g["setup"] = shim["setup"]
            launches[(r["round"], r["test"], r["config"])] = g["launches"]
        gate[tag] = g
        if r["graphs"] == "on" and avg is not None and int(r["rc"]) == 0:
            ts.setdefault((r["config"], r["test"]), []).append(avg)
    configs = ["noshim", "idle", "c1m1ms", "c16m1ms", "cont"]
    tests = ["pp512", "tg128"]
    base = {t: ts.get(("noshim", t), []) for t in tests}
    noise = {t: ((max(v) - min(v)) / statistics.mean(v)) if len(v) > 1 else None
             for t, v in base.items()}
    md.append("| config | test | tokens/s per round | mean | loss vs noshim |")
    md.append("|---|---|---|---:|---:|")
    loss = {}
    for c in configs:
        for t in tests:
            v = ts.get((c, t), [])
            if not v:
                continue
            m = statistics.mean(v)
            b = statistics.mean(base[t]) if base[t] else None
            loss[(c, t)] = (b - m) / b if b else None
            md.append("| %s | %s | %s | %.2f | %s |" % (
                c, t, ", ".join("%.2f" % x for x in v), m,
                pct(loss[(c, t)]) if c != "noshim" else "noise " + pct(noise[t])))
    # graph gate for every graphs-on shim run. A failure (the run broke, or
    # graph calls failed or changed) gives "no"; a gap in the evidence (no shim
    # log, copier never copied, a timeout, no idle run to compare with) gives
    # "inconclusive" (review 2026-11-15: failed controls never give "no").
    gate_fail, gate_unk = [], []
    for tag, g in gate.items():
        if tag.endswith("-graphsoff") or "-noshim-" in tag:
            continue
        rnd, rest = tag[1:].split("-", 1)
        c, t = rest.rsplit("-", 1)
        idle_l = launches.get((rnd, t, "idle"))
        bad, unk = [], []
        if g["rc"] in TIMEOUT_RCS:
            unk.append("timed out (rc %d)" % g["rc"])
        elif g["rc"] != 0:
            bad.append("rc %d" % g["rc"])
        if g["cuda_error_in_stderr"]:
            bad.append("CUDA error")
        if not g["shim"]:
            unk.append("no shim log")
        else:
            if g["graph_errors"]:
                bad.append("graph API errors")
            if idle_l is None:
                unk.append("no idle run to compare launches with")
            elif g["launches"] != idle_l:
                bad.append("launches %s vs idle %s" % (g["launches"], idle_l))
            if c != "idle":
                if g["copy_errors"]:
                    bad.append("copy errors %d" % g["copy_errors"])
                elif g["copies"] == 0:
                    unk.append("copier made no copies")
        if bad:
            gate_fail.append((tag, bad))
        if unk:
            gate_unk.append((tag, unk))
    # baseline: the no-shim runs must end cleanly, or a failure in a shim run
    # cannot be put on the shim (review 2026-11-15)
    noshim_bad = [t for t, g in gate.items() if "-noshim-" in t
                  and (g["rc"] != 0 or g["cuda_error_in_stderr"])]
    ctrl = gate.get("r0-idle-tg128-graphsoff", {})
    ctrl_ok = ctrl.get("shim") and ctrl.get("launches") == 0 and ctrl.get("captures") == 0 \
        and ctrl.get("rc") == 0
    tg_launch = {c: launches.get(("1", "tg128", c)) for c in configs[1:]}
    tg_ok = all((v or 0) > 0 for v in tg_launch.values())
    # nsys cross-check of the detector (README: nsys's cudaGraphLaunch rows
    # must equal the shim's count; a mismatch makes the graph verdict
    # inconclusive, review 2026-11-15)
    # states: None = not run (no partB-nsys/); True = both tags parsed and
    # match; False = a mismatch, or a tag missing or unparsable
    nd = os.path.join(o, "partB-nsys")
    nsys_match = None if not os.path.isdir(nd) else True
    for tag in ("llama-idle", "llama-cont"):
        p = jload(os.path.join(nd, tag + ".parse.json"))
        s = jload(os.path.join(nd, tag + ".shim.json"))
        if not p or "runtime_api" not in p:
            if nsys_match is not None:
                nsys_match = False
                res.setdefault("nsys", {})[tag] = {"missing_or_unparsable": True}
            continue
        api_l = sum(v for k, v in p["runtime_api"].items() if k.startswith("cudaGraphLaunch"))
        shim_l = s["graph_api"]["cudaGraphLaunch"]["calls"] if s else None
        m = shim_l is not None and api_l == shim_l and api_l > 0
        nsys_match = bool(nsys_match) and m
        res.setdefault("nsys", {})[tag] = {
            "kernels": p["kernels"], "kernels_in_graphs": p["kernels_in_graphs"],
            "nsys_graph_launch_calls": api_l, "shim_graph_launch_calls": shim_l, "match": m,
            "shim_copies_seen_by_cupti": p["shim_copies"],
            "copy_overlap_share": p["shim_copy_overlap_share"]}
    if gate_fail and not noshim_bad:
        graphs = "no"
    elif gate_fail or gate_unk or noshim_bad or not ctrl_ok or not tg_ok or nsys_match is False:
        graphs = "inconclusive"
    else:
        graphs = "yes"
    graphs_ok = graphs == "yes"
    md.append("")
    md.append("Graph launches in round 1, tg128 (shim count): " + ", ".join(
        "%s=%s" % (k, v) for k, v in tg_launch.items()))
    md.append("Graphs-off control: launches=%s captures=%s rc=%s -> %s" % (
        ctrl.get("launches"), ctrl.get("captures"), ctrl.get("rc"), "ok" if ctrl_ok else "FAIL"))
    if gate_fail:
        md.append("Gate failures: " + "; ".join("%s: %s" % (t, ",".join(b)) for t, b in gate_fail))
    if gate_unk:
        md.append("Gate gaps (inconclusive): " + "; ".join("%s: %s" % (t, ",".join(b)) for t, b in gate_unk))
    if noshim_bad:
        md.append("No-shim baseline runs that failed (graph verdict inconclusive): " + ", ".join(noshim_bad))
    md.append("nsys cross-check (cudaGraphLaunch rows equal the shim's count): %s" % (
        "not run" if nsys_match is None else nsys_match))
    md.append("")
    md.append("| tag | copies | during a capture | copies/s | GB/s | missed periods | lat p50/p99/max us | ctx wait s |")
    md.append("|---|---:|---:|---:|---:|---:|---|---:|")
    for tag, g in sorted(gate.items()):
        if g.get("shim") and g.get("copies"):
            md.append("| %s | %d | %s | %.0f | %.3f | %d | %s/%s/%.0f | %.3f |" % (
                tag, g["copies"], g.get("during_capture"), g["copies_per_s"], g["achieved_GBps"],
                g["missed_periods"], g["lat_p50"], g["lat_p99"], g["lat_max"], g["ctx_wait_s"]))
    # verdicts
    nz = [n for n in noise.values() if n is not None]
    max_noise = max(nz) if nz else None
    idle_ok = all(loss.get(("idle", t)) is not None and loss[("idle", t)] <= max(
        B_IDLE_MAX_LOSS, noise[t] or 0) for t in tests)
    rate = [g["copies_per_s"] for tag, g in gate.items() if "-c1m1ms-" in tag and g.get("shim")]
    rate_ok = bool(rate) and min(rate) >= B_MIN_RATE_SHARE * 1000
    l1 = [loss.get(("c1m1ms", t)) for t in tests]
    if max_noise is None or max_noise > B_MAX_NOISE or None in l1 or not rate_ok:
        bounded = "inconclusive"
    else:
        bounded = "yes" if all(x <= B_BOUNDED_MAX_LOSS for x in l1) else "no"
    res.update({"graphs": graphs, "graphs_ok": graphs_ok, "graphs_off_control_ok": bool(ctrl_ok),
                "gate_failures": gate_fail, "gate_gaps": gate_unk, "noshim_failed": noshim_bad,
                "nsys_match": nsys_match,
                "noise": noise, "idle_ok": idle_ok,
                "loss": {"%s/%s" % k: v for k, v in loss.items()},
                "c1m1ms_rate_ok": rate_ok, "bounded": bounded,
                "ok": graphs_ok and bounded == "yes"})
    md.append("")
    md.append("**B: graphs keep working with the copier: %s; idle shim within noise: %s; 1 MiB/1 ms loss bounded"
              " (<= %s, both tests): %s.**\n" % (graphs, idle_ok, pct(B_BOUNDED_MAX_LOSS), bounded))
    for tag, v in sorted(res.get("nsys", {}).items()):
        if v.get("missing_or_unparsable"):
            md.append("nsys %s: parse output missing or unparsable" % tag)
            continue
        md.append("nsys %s: kernels %d, in graphs %d (%s); cudaGraphLaunch nsys %d vs shim %s; shim copies seen %d,"
                  " copy time overlapping kernels %s" % (
                      tag, v["kernels"], v["kernels_in_graphs"],
                      pct(v["kernels_in_graphs"] / v["kernels"]) if v["kernels"] else "-",
                      v["nsys_graph_launch_calls"], v["shim_graph_launch_calls"],
                      v["shim_copies_seen_by_cupti"], pct(v["copy_overlap_share"])))
    md.append("")
    return res


# ------------------------------------------------------------------ part C
def part_c(o, md):
    d = os.path.join(o, "partC")
    lines = jl(os.path.join(d, "vmm.jsonl"))
    res = {"present": bool(lines)}
    if not lines:
        md.append("## Part C: not run\n")
        return res
    var = {}
    cur = None
    for x in lines:
        if "variant" in x:
            cur = x["variant"]
            var[cur] = {}
        elif cur and x.get("test") == "vmmexport":
            var[cur][x["role"]] = x
    md.append("## Part C: POSIX-FD VMM export to a second process\n")
    rc = lambda x, k: x.get(k, {}).get("rc") if isinstance(x.get(k), dict) else None
    tiers = {}
    for name, v in var.items():
        e, i = v.get("exporter", {}), v.get("importer", {})
        upd = e.get("updates", 0)
        calls = {k: x["name"] for side in (e, i) for k, x in side.items()
                 if isinstance(x, dict) and "rc" in x}
        md.append("**%s**: %s" % (name, ", ".join("%s=%s" % kv for kv in calls.items())))
        t2 = (rc(i, "cuMemImportFromShareableHandle") == 0 and rc(i, "cuMemMap") == 0
              and rc(i, "cuMemSetAccess_device") == 0 and i.get("kernel_bad_words_gen1") == 0
              and i.get("dtoh_bad_words_gen1") == 0 and upd > 0
              and i.get("dtoh_distinct_gens", 0) >= C_MIN_SEEN_SHARE * upd
              and i.get("dtoh_backwards", 1) == 0 and i.get("kernel_backwards", 1) == 0
              and i.get("final_dtoh_stale", 1) == 0)
        t1 = (i.get("cpu_map_used") == 1 and i.get("cpu_gen1_stale_plain", 1) == 0
              and i.get("cpu_final_stale_plain_0ms", 1) == 0
              and i.get("cpu_final_stale_plain_10ms", 1) == 0 and upd > 0
              and i.get("cpu_distinct_gens", 0) >= C_MIN_SEEN_SHARE * upd
              and i.get("cpu_backwards", 1) == 0)
        # a variant whose processes did not both write their record (crash,
        # alarm, timeout) decides nothing (review 2026-11-15)
        complete = bool(e) and bool(i) and "parent" in v
        tiers[name] = {"tier1_cpu_mapping": t1, "tier2_import_and_copy": t2, "complete": complete}
        md.append("")
        md.append("- fd: %s; mmap before CUDA init: %s; updates by process 1: %d; distinct generations seen:"
                  " DtoH %s, kernel %s, CPU mapping %s" % (
                      i.get("p2_fd_link"), i.get("mmap_before_init_ok"), upd, i.get("dtoh_distinct_gens"),
                      i.get("kernel_distinct_gens"), i.get("cpu_distinct_gens")))
        md.append("- stale CPU-mapped words: gen 1 %s (civac %s); after the writer stopped 0/10/100/1000 ms: %s;"
                  " after process 1 exited: %s; final DtoH stale %s" % (
                      i.get("cpu_gen1_stale_plain"), i.get("cpu_gen1_stale_civac"),
                      "/".join(str(i.get("cpu_final_stale_plain_%dms" % t)) for t in (0, 10, 100, 1000)),
                      i.get("cpu_stale_after_p1_exit_plain"), i.get("final_dtoh_stale")))
        md.append("- DtoH bandwidth, imported %s GB/s vs local cuMemAlloc %s GB/s; fill us mean %s" % (
            i.get("dtoh_imported_GBps"), i.get("dtoh_local_GBps"), e.get("fill_us_mean")))
        md.append("- tier 1 (CPU mapping usable): %s; tier 2 (import + reads in own context): %s;"
                  " both processes reported: %s\n" % (t1, t2, complete))
    any_t1 = any(t["tier1_cpu_mapping"] for t in tiers.values())
    any_t2 = any(t["tier2_import_and_copy"] for t in tiers.values())
    if any_t1 or any_t2:
        verdict = "works"
    elif tiers and all(t["complete"] for t in tiers.values()):
        verdict = "no"
    else:
        verdict = "inconclusive"
    res.update({"variants": tiers, "tier1": any_t1, "tier2": any_t2, "verdict": verdict,
                "ok": verdict == "works"})
    md.append("**C: tier 1 (direct CPU mapping) works: %s; tier 2 (import, kernel and DtoH reads) works: %s;"
              " verdict: %s.**\n" % (any_t1, any_t2, verdict))
    return res


def main():
    o = sys.argv[1]
    md = ["# Check 3 summary: %s\n" % o,
          "Decision rules and thresholds: README.md, written before any real run.\n"]
    pre = os.path.join(o, "preflight.txt")
    if os.path.exists(pre):
        with open(pre) as f:
            md.append("Preflight: " + " | ".join(l.strip() for l in f.readlines()[:5]) + "\n")
    a = part_a(o, md)
    b0 = part_b0(o, md)
    b = part_b(o, md)
    c = part_c(o, md)
    # path 1, agent copy-out: A yes/fixable AND B passes (graphs, bounded
    # loss, B0). It fails on a verdict (A no, or B/B0 fail), never on a
    # failed control or a missing part (review 2026-11-15).
    a_ok, c_ok = a.get("ok"), c.get("ok")
    b_verdict_fail = b.get("graphs") == "no" or b.get("bounded") == "no" or b0.get("verdict") == "fail"
    b_ok = bool(b.get("ok")) and b0.get("verdict") == "pass"
    if a_ok and b_ok:
        path1 = "pass"
    elif a.get("answer") == "no" or b_verdict_fail:
        path1 = "fail"
    else:
        path1 = "inconclusive"
    if path1 == "pass" or c_ok:
        overall = "yes"
    elif path1 == "fail" and c.get("verdict") == "no":
        overall = "no"
    else:
        overall = "inconclusive"
    paths = []
    if path1 == "pass":
        paths.append("agent copy-out into the segment (%s)" % (
            "one copy" if a.get("answer") == "yes" else "two-step copy, flags first"))
    if c.get("tier1"):
        paths.append("direct CPU mapping of the exported fd")
    if c.get("tier2"):
        paths.append("reader imports the fd and copies in its own CUDA context (needs CUDA in the reader;"
                     " a copy concurrent with the writer, so ordering is part A's answer, here %s; cost on a"
                     " real application not measured: process 1 is a toy writer)" % a.get("answer", "not run"))
    md.append("## Overall\n")
    md.append("Can storage the CPU reads live in device memory on the Orin at all? **%s**" % overall)
    md.append("Paths that passed: %s" % ("; ".join(paths) if paths else "none"))
    md.append("Path 1 (agent copy-out: A %s, B graphs %s, B bounded %s, B0 %s): %s. Part C: %s.\n" % (
        a.get("answer", "not run"), b.get("graphs", "not run"), b.get("bounded", "not run"),
        b0.get("verdict", "not run"), path1, c.get("verdict", "not run")))
    dec = {"overall": overall, "path1_copy_out": path1, "paths": paths, "A": a, "B0": b0, "B": b, "C": c}
    with open(os.path.join(o, "decisions.json"), "w") as f:
        json.dump(dec, f, indent=1, default=str)
    with open(os.path.join(o, "summary.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))


if __name__ == "__main__":
    main()
