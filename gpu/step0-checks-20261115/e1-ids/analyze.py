#!/usr/bin/env python3
"""analyze.py OUT  |  analyze.py --selftest

Reads the CSVs that run_all.sh leaves in OUT/{a,b,c,d,e_ctl,e_hog}/ and writes
OUT/summary.json and OUT/summary.txt. Decision rules: README.md, section "Decision rules";
the thresholds below are those rules and were fixed before any real run.
Standard library only (run with python3 -I).
"""
import csv
import collections
import json
import os
import sys

UNWRITTEN = 0xFFFFFFFF
# ---- thresholds from README "Decision rules" (do not retune to data)
GAP_NS = 50_000            # a gap: two consecutive %globaltimer reads of one warp > 50 us apart
MIN_EPISODES = 10          # preemption episodes needed before "ids stable" can be yes
MIN_WARP_GAPS = 1000       # warp-gaps needed before "ids stable" can be yes
EPISODE_WARP_FRAC = 0.5    # an episode = merged gap intervals touching >= 50% of the launch's warps
ATTRIB_MIN = 0.5           # hog-running coverage of gap time below this: gaps not attributed to the hog


def rd(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def ints(r, *keys):
    return [int(r[k]) for k in keys]


def collisions(keys):
    c = collections.Counter(keys)
    return sum(v - 1 for v in c.values() if v > 1)


# ---------------------------------------------------------------- cases a, b
def launch_stats(recs, caphits):
    ent = [r for r in recs if int(r["kind"]) == 0]
    ex = [r for r in recs if int(r["kind"]) == 1]
    missing = sum(1 for r in recs if int(r["tag"]) == UNWRITTEN)
    ent = [r for r in ent if int(r["tag"]) != UNWRITTEN]
    ex = [r for r in ex if int(r["tag"]) != UNWRITTEN]
    flagged = sum(1 for r in ex if int(r["flags"]))
    proven = caphits == 0 and missing == 0 and flagged == 0 and len(ent) > 0
    k_ent = [(int(r["smid"]), int(r["warpid"])) for r in ent]
    k_ex = [(int(r["smid"]), int(r["warpid"])) for r in ex]
    k_cta = [(int(r["smid"]), int(r["warp_in_cta"])) for r in ent]
    by_w = {int(r["warp_lin"]): r for r in ent}
    changes = 0
    for r in ex:
        e = by_w.get(int(r["warp_lin"]))
        if e is not None and (e["smid"], e["warpid"]) != (r["smid"], r["warpid"]):
            changes += 1
    ctas_per_sm = collections.defaultdict(set)
    grids_per_sm = collections.defaultdict(set)
    for r in ent:
        ctas_per_sm[int(r["smid"])].add((int(r["tag"]), int(r["ctax"]), int(r["ctay"]), int(r["ctaz"])))
        grids_per_sm[int(r["smid"])].add(int(r["tag"]))
    gridids_by_tag = collections.defaultdict(set)
    for r in ent + ex:
        gridids_by_tag[int(r["tag"])].add(int(r["gridid"]))
    g32_mismatch = sum(1 for r in ent + ex if int(r["gridid32"]) != (int(r["gridid"]) & 0xFFFFFFFF))
    g_hi = sum(1 for r in ent + ex if int(r["gridid"]) >> 32)
    return {
        "warps": len(ent),
        "missing_records": missing,
        "caphits": caphits,
        "proven_coresident": proven,
        "collisions_smid_warpid_entry": collisions(k_ent),
        "collisions_smid_warpid_exit": collisions(k_ex),
        "control_collisions_smid_tid32": collisions(k_cta),
        "warpid_eq_tid32": sum(1 for r in ent if r["warpid"] == r["warp_in_cta"]),
        "entry_exit_id_changes": changes,
        "max_smid": max((int(r["smid"]) for r in ent), default=-1),
        "max_warpid": max((int(r["warpid"]) for r in ent), default=-1),
        "nsmid": sorted({int(r["nsmid"]) for r in ent}),
        "nwarpid": sorted({int(r["nwarpid"]) for r in ent}),
        "laneid": sorted({int(r["laneid"]) for r in ent}),
        "max_ctas_on_one_sm": max((len(v) for v in ctas_per_sm.values()), default=0),
        "sms_with_two_grids": sum(1 for v in grids_per_sm.values() if len(v) >= 2),
        "gridid_by_tag": {str(k): sorted(v) for k, v in gridids_by_tag.items()},
        "gridid_values_per_launch_max": max((len(v) for v in gridids_by_tag.values()), default=0),
        "gridid32_ne_low_half": g32_mismatch,
        "gridid_high_half_nonzero": g_hi,
    }


def analyze_ab(d, case):
    recs = rd(os.path.join(d, f"{case}_records.csv"))
    launches = rd(os.path.join(d, f"{case}_launches.csv"))
    by_l = collections.defaultdict(list)
    for r in recs:
        by_l[int(r["launch"])].append(r)
    out = []
    for l in launches:
        L = int(l["launch"])
        s = launch_stats(by_l[L], int(l["caphits"]))
        s.update({k: l[k] for k in l if k not in s})
        out.append(s)
    return out


# ---------------------------------------------------------------- gridid series
def gridid_series(recs, order_key):
    """recs of one part; returns per-launch gridid (must be one per launch) in issue order."""
    per = collections.defaultdict(set)
    missing = 0
    for r in recs:
        if int(r["tag"]) == UNWRITTEN:
            missing += 1
            continue
        per[order_key(r)].add(int(r["gridid"]))
    multi = sum(1 for v in per.values() if len(v) != 1)
    seq = [min(per[k]) for k in sorted(per)]
    diffs = [b - a for a, b in zip(seq, seq[1:])]
    return {
        "launches": len(seq),
        "missing_records": missing,
        "launches_with_several_gridids": multi,
        "distinct_gridids": len(set(seq)),
        "duplicates": len(seq) - len(set(seq)),
        "strictly_increasing_in_issue_order": all(x > 0 for x in diffs),
        "nondecreasing_in_issue_order": all(x >= 0 for x in diffs),
        "step_histogram": dict(collections.Counter(diffs).most_common(8)),
        "first": seq[:5],
        "last": seq[-3:],
        "gridid32_ne_low_half": sum(1 for r in recs if int(r["tag"]) != UNWRITTEN
                                    and int(r["gridid32"]) != (int(r["gridid"]) & 0xFFFFFFFF)),
        "gridid_high_half_nonzero": sum(1 for r in recs if int(r["tag"]) != UNWRITTEN and int(r["gridid"]) >> 32),
    }


def analyze_c(d):
    recs = rd(os.path.join(d, "c_records.csv"))
    meta = {(r["part"], r["key"]): r["value"] for r in rd(os.path.join(d, "c_meta.csv"))}
    parts = collections.defaultdict(list)
    for r in recs:
        parts[r["case"]].append(r)
    res = {}
    for p in ("c1", "c2"):
        res[p] = gridid_series(parts[p], lambda r: int(r["launch"]))
    for p in ("c3", "c4"):
        rs = parts[p]
        # order = (replay, node); a launch = one node in one replay
        res[p] = gridid_series(rs, lambda r: (int(r["aux"]), int(r["tag"])))
        per_node = collections.defaultdict(set)
        for r in rs:
            per_node[int(r["tag"])].add(int(r["gridid"]))
        res[p]["distinct_gridids_per_node"] = {str(k): len(v) for k, v in sorted(per_node.items())}
        res[p]["gridids_per_node"] = {str(k): sorted(v)[:4] for k, v in sorted(per_node.items())}
        res[p]["replay_counters"] = [int(meta.get((p, f"ctr_node{i}"), -1)) for i in range(3)]
        res[p]["replays_requested"] = int(meta.get((p, "replays"), -1))
        if p == "c4":
            res[p]["update_ok"] = int(meta.get(("c4", "update_ok"), -1))
            k40 = {int(r["gridid"]) for r in rs if int(r["kind"]) == 40}
            k41 = {int(r["gridid"]) for r in rs if int(r["kind"]) == 41}
            res[p]["gridids_before_update"] = sorted(k40)
            res[p]["gridids_after_update"] = sorted(k41)
    res["graph_node_overflow"] = int(meta.get(("c", "overflow"), -1))
    res["c1_launches_requested"] = int(meta.get(("c1", "launches"), -1))
    res["c2_launches_requested"] = int(meta.get(("c2", "launches"), -1))
    return res


def analyze_d(d):
    recs = rd(os.path.join(d, "d_records.csv"))
    res = gridid_series(recs, lambda r: int(r["launch"]))
    per_mod = collections.defaultdict(set)
    nw = collections.defaultdict(set)
    for r in recs:
        if int(r["tag"]) == UNWRITTEN:
            continue
        per_mod[int(r["kind"])].add(int(r["gridid"]))
        nw[int(r["kind"])].add(int(r["nwarpid"]))
    res["modules"] = {str(k): {"launches": len(v), "nwarpid": sorted(nw[k])} for k, v in sorted(per_mod.items())}
    res["cross_module_duplicates"] = sum(
        len(per_mod[a] & per_mod[b]) for a in per_mod for b in per_mod if a < b)
    return res


# ---------------------------------------------------------------- case e
def merge(ivs):
    out = []
    for a, b, *rest in sorted(ivs):
        if out and a <= out[-1][1]:
            out[-1][1] = max(out[-1][1], b)
            if rest:
                out[-1][2] |= rest[0]
        else:
            out.append([a, b, set(rest[0]) if rest else set()])
    return out


def overlap(a, b, ivs):
    s = 0
    for x, y, *_ in ivs:
        lo, hi = max(a, x), min(b, y)
        if hi > lo:
            s += hi - lo
    return s


def analyze_e(d, hogdir=None):
    warps = rd(os.path.join(d, "e_warps.csv"))
    events = rd(os.path.join(d, "e_events.csv"))
    recs = rd(os.path.join(d, "e_records.csv"))
    launches = rd(os.path.join(d, "e_launches.csv"))
    res = {"launches": []}
    hog_run = None
    if hogdir and os.path.exists(os.path.join(hogdir, "hog_kernels.csv")):
        hk = rd(os.path.join(hogdir, "hog_kernels.csv"))
        hg = rd(os.path.join(hogdir, "hog_gaps.csv"))
        gaps_by_k = collections.defaultdict(list)
        for g in hg:
            gaps_by_k[int(g["kernel"])].append((int(g["t_prev"]), int(g["t_now"])))
        hog_run = []
        for k in hk:
            t0, t1 = int(k["t0"]), int(k["t1"])
            if t1 <= t0:
                continue
            cur = t0
            for a, b in sorted(gaps_by_k[int(k["kernel"])]):
                if a > cur:
                    hog_run.append((cur, a))
                cur = max(cur, b)
            if t1 > cur:
                hog_run.append((cur, t1))
        res["hog"] = {
            "kernels": len(hk),
            "warp0_gaps": sum(int(k["gaps_w0"]) for k in hk),
            "all_warp_gaps": sum(int(k["gaps_all"]) for k in hk),
            "gap_log_overflow": sum(int(k["gap_overflow"]) for k in hk),
            "capped_warps": sum(int(k["capped_warps"]) for k in hk),
        }
    for l in launches:
        L = int(l["launch"])
        ws = [w for w in warps if int(w["launch"]) == L]
        nw = len(ws)
        evs = [e for e in events if int(e["launch"]) == L]
        rs = [r for r in recs if int(r["launch"]) == L]
        gap_evs = [e for e in evs if int(e["kind"]) & 1 and not int(e["kind"]) & 4]
        eps = merge([(int(e["t_prev"]), int(e["t_now"]), {int(e["warp"])}) for e in gap_evs])
        big = [e for e in eps if len(e[2]) >= EPISODE_WARP_FRAC * nw]
        s = {
            "launch": L,
            "warps": nw,
            "iters_min": min(int(w["iters"]) for w in ws),
            "iters_max": max(int(w["iters"]) for w in ws),
            "ns_per_iter_median": sorted((int(w["t_loop1"]) - int(w["t_loop0"])) / max(1, int(w["iters"]))
                                         for w in ws)[nw // 2],
            "max_dt_ns_excl_injected": max(int(w["max_dt"]) for w in ws),
            "min_nonzero_dt_ns": min(int(w["min_nz_dt"]) for w in ws),
            "warp_gaps": sum(int(w["gaps"]) for w in ws),
            "injected_detected": sum(int(w["inj_detected"]) for w in ws),
            "injected_done": sum(int(w["inj_done"]) for w in ws),
            "id_changes_with_gap": sum(int(w["changes_gap"]) for w in ws),
            "id_changes_without_gap": sum(int(w["changes_nogap"]) for w in ws),
            "negative_dt": sum(int(w["neg_dt"]) for w in ws),
            "event_log_overflow": sum(int(w["ev_overflow"]) for w in ws),
            "sample_overflow": sum(int(w["smp_overflow"]) for w in ws),
            "cap_flags": sum(1 for w in ws if int(w["flags"])),
            "gap_episodes": len(eps),
            "gap_episodes_ge_half_warps": len(big),
            "episode_ms_max": max(((b - a) / 1e6 for a, b, _ in eps), default=0.0),
            "episode_ms_total": sum((b - a) / 1e6 for a, b, _ in eps),
            "wall_ms": float(l["wall_ms"]),
        }
        # ids: post-barrier (kind 1) vs exit (kind 2); exit exclusivity
        k1 = {int(r["warp_lin"]): r for r in rs if int(r["kind"]) == 1 and int(r["tag"]) != UNWRITTEN}
        k2 = [r for r in rs if int(r["kind"]) == 2 and int(r["tag"]) != UNWRITTEN]
        s["entry_exit_id_changes"] = sum(1 for r in k2 if int(r["warp_lin"]) in k1 and
                                         (k1[int(r["warp_lin"])]["smid"], k1[int(r["warp_lin"])]["warpid"]) !=
                                         (r["smid"], r["warpid"]))
        s["exit_barrier_proven"] = all(int(r["flags"]) == 0 for r in k2) and len(k2) == nw
        # kind-1 records are taken right after the entry barrier: co-resident when it passed
        s["entry_barrier_proven"] = all(int(r["flags"]) == 0 for r in k1.values()) and len(k1) == nw
        s["collisions_smid_warpid_exit"] = collisions((int(r["smid"]), int(r["warpid"])) for r in k2)
        s["collisions_smid_warpid_entry"] = collisions((int(r["smid"]), int(r["warpid"])) for r in k1.values())
        allrec = [r for r in rs if int(r["tag"]) != UNWRITTEN]
        s["max_smid"] = max((int(r["smid"]) for r in allrec), default=-1)
        s["max_warpid"] = max((int(r["warpid"]) for r in allrec), default=-1)
        s["nsmid"] = sorted({int(r["nsmid"]) for r in allrec})
        s["nwarpid"] = sorted({int(r["nwarpid"]) for r in allrec})
        ip = os.path.join(d, "e_intruders.csv")
        if os.path.exists(ip):
            lo = min(int(w["t_loop0"]) for w in ws)
            hi = max(int(w["t_loop1"]) for w in ws)
            its = [r for r in rd(ip) if int(r["launch"]) == L]
            s["intruders"] = len(its)
            s["intruders_started_inside_long_kernel"] = sum(1 for r in its if lo <= int(r["t0"]) < hi)
            # issued (host clock, from the long kernel's launch) before the long kernel's loop could
            # have ended: only these test whether a high-priority kernel can start inside it
            s["intruders_issued_within_long_dur"] = sum(
                1 for r in its if "host_issue_ms" in r and float(r["host_issue_ms"]) < float(r["long_dur_ms"]))
            s["intruders_unwritten"] = sum(1 for r in its if int(r["t0"]) == 0)
            s["intruder_capped_warps"] = sum(int(r["capped_warps"]) for r in its)
        if hog_run is not None:
            tot = sum(b - a for a, b, _ in eps)
            cov = sum(overlap(a, b, hog_run) for a, b, _ in eps)
            s["gap_time_covered_by_hog_running"] = (cov / tot) if tot else None
        res["launches"].append(s)
    logp = os.path.join(d, "ids_e.log")
    if hogdir and os.path.exists(logp):
        res["hog_alive_at_end"] = "hog_alive_at_end=1" in open(logp).read()
    # sample check: every warp's sampled ids constant (redundant with the change counters)
    smp_path = os.path.join(d, "e_samples.csv")
    if os.path.exists(smp_path):
        per = collections.defaultdict(set)
        with open(smp_path, newline="") as f:
            for r in csv.DictReader(f):
                per[(r["launch"], r["warp"])].add((r["smid"], r["warpid"]))
        res["warps_with_sampled_id_change"] = sum(1 for v in per.values() if len(v) > 1)
    return res


# ---------------------------------------------------------------- decisions
def decide(S):
    D = {}
    a = S.get("a", [])
    b = S.get("b", [])
    allr = [x for x in a + b]
    # S and W
    smax = max([x["max_smid"] for x in allr] + [-1])
    wmax = max([x["max_warpid"] for x in allr] + [-1])
    nsm = sorted({v for x in allr for v in x["nsmid"]})
    nwp = sorted({v for x in allr for v in x["nwarpid"]})
    if "d" in S:
        nwp = sorted(set(nwp) | {v for m in S["d"]["modules"].values() for v in m["nwarpid"]})
    for e in ("e_ctl", "e_hog", "e_prio"):
        for x in S.get(e, {}).get("launches", []):
            smax = max(smax, x["max_smid"])
            wmax = max(wmax, x["max_warpid"])
            nsm = sorted(set(nsm) | set(x["nsmid"]))
            nwp = sorted(set(nwp) | set(x["nwarpid"]))
    D["S"] = {"max_smid_plus_1": smax + 1, "nsmid": nsm}
    D["W"] = {"max_warpid_plus_1": wmax + 1, "nwarpid": nwp}
    # controls
    over = [x for x in a if x.get("control") == "1"]
    reg_a = [x for x in a if x.get("control") != "1"]
    over_ok = bool(over) and all(x["caphits"] > 0 for x in over)
    proven = [x for x in reg_a + b if x["proven_coresident"]]
    multi_cta = [x for x in proven if x["max_ctas_on_one_sm"] >= 2]
    ctl_can_fail = any(x["control_collisions_smid_tid32"] > 0 for x in multi_cta)
    b_cross = [x for x in b if x["proven_coresident"] and x["sms_with_two_grids"] > 0]
    coll = sum(x["collisions_smid_warpid_entry"] + x["collisions_smid_warpid_exit"] for x in proven)
    # e: the post-entry-barrier records of a launch whose entry barrier passed, and the exit
    # records of a launch whose exit barrier passed, are co-resident too
    e_all = [x for e in ("e_ctl", "e_hog", "e_prio") for x in S.get(e, {}).get("launches", [])]
    e_proven = [x for x in e_all if x["exit_barrier_proven"]]
    coll += sum(x["collisions_smid_warpid_exit"] for x in e_proven)
    coll += sum(x["collisions_smid_warpid_entry"] for x in e_all if x.get("entry_barrier_proven"))
    notproven = [x for x in reg_a + b if not x["proven_coresident"]]
    if coll > 0:
        excl = "no"
    elif proven and over_ok and ctl_can_fail and b_cross and not notproven:
        excl = "yes"
    else:
        excl = "inconclusive"
    D["exclusive_smid_warpid"] = {
        "answer": excl,
        "collisions_in_proven_launches": coll,
        "proven_launches": len(proven),
        "not_proven_launches": len(notproven),
        "oversubscribed_control_capped": over_ok,
        "control_key_smid_tid32_collides": ctl_can_fail,
        "b_pairs_proven_with_shared_sm": len(b_cross),
        "e_launches_with_proven_exit_barrier": len(e_proven),
    }
    eq = sum(x["warpid_eq_tid32"] for x in reg_a + b)
    tot = sum(x["warps"] for x in reg_a + b)
    if tot and eq == tot:
        cta = "CTA-relative (warpid == tid/32 in every warp)"
    elif excl == "yes" and eq < tot:
        cta = "hardware slot (differs from tid/32 and exclusive per SM)"
    else:
        cta = "unclear"
    D["warpid_kind"] = {"answer": cta, "warps_with_warpid_eq_tid32": eq, "warps": tot}
    D["entry_exit_id_changes_ab"] = sum(x["entry_exit_id_changes"] for x in reg_a + b)
    # gridid
    g = {}
    dup = 0
    parts_seen = []
    for p in ("c1", "c2", "c3", "c4"):
        if "c" in S and p in S["c"]:
            g[p] = {k: S["c"][p][k] for k in ("duplicates", "strictly_increasing_in_issue_order",
                                              "launches_with_several_gridids", "launches", "missing_records")}
            dup += S["c"][p]["duplicates"]
            parts_seen.append(p)
    if "d" in S:
        g["d"] = {k: S["d"][k] for k in ("duplicates", "strictly_increasing_in_issue_order",
                                         "launches_with_several_gridids", "launches", "missing_records")}
        dup += S["d"]["duplicates"]
        parts_seen.append("d")
    ab_multi = sum(1 for x in allr if x["gridid_values_per_launch_max"] != 1)
    c_complete = "c" in S and all(
        S["c"][p]["replay_counters"] == [S["c"][p]["replays_requested"]] * 3 for p in ("c3", "c4"))
    # every launch must have left its records: a dropped launch could hide a duplicate.
    # c1, c2, d dump every record slot (unwritten ones count in missing_records); c3, c4 dump
    # written records only, so their launch count must be 3 nodes x replays requested
    g_missing = sum(g[p]["missing_records"] for p in g)
    requested = {}
    if "c" in S:
        requested = {"c1": S["c"].get("c1_launches_requested"), "c2": S["c"].get("c2_launches_requested"),
                     "c3": 3 * S["c"]["c3"]["replays_requested"], "c4": 3 * S["c"]["c4"]["replays_requested"]}
    g_short = [p for p in g if p in requested and g[p]["launches"] != requested[p]]
    g_overflow = S["c"].get("graph_node_overflow", -1) if "c" in S else -1
    if dup > 0:
        gu = "no"
    elif len(parts_seen) == 5 and c_complete and ab_multi == 0 and g_missing == 0 and not g_short \
            and g_overflow == 0:
        gu = "yes"
    else:
        gu = "inconclusive"
    D["gridid_unique_per_launch"] = {"answer": gu, "duplicates": dup, "parts": g,
                                     "graph_replay_counters_complete": c_complete,
                                     "ab_launches_with_several_gridids": ab_multi,
                                     "missing_records": g_missing, "parts_with_fewer_launches": g_short,
                                     "graph_node_overflow": g_overflow}
    mono = {p: g[p]["strictly_increasing_in_issue_order"] for p in g}
    D["gridid_monotonic"] = mono
    w32 = 0
    hi = 0
    for x in allr:
        w32 += x["gridid32_ne_low_half"]
        hi += x["gridid_high_half_nonzero"]
    if "c" in S:
        for p in ("c1", "c2", "c3", "c4"):
            w32 += S["c"][p]["gridid32_ne_low_half"]
            hi += S["c"][p]["gridid_high_half_nonzero"]
    D["gridid_width"] = {"u32_read_ne_low_half_of_u64": w32, "records_with_high_half_nonzero": hi}
    # preemption
    ctl = S.get("e_ctl")
    hog = S.get("e_hog")
    pe = {}
    if ctl and hog:
        cl = ctl["launches"]
        hl = hog["launches"]
        det_ok = all(x["injected_detected"] == x["injected_done"] == x["warps"] for x in cl + hl)
        ctl_quiet = all(x["max_dt_ns_excl_injected"] < GAP_NS for x in cl)
        pl = S["e_prio"]["launches"] if "e_prio" in S else []
        # any id change anywhere (e per iteration, e samples, e entry->exit, a/b entry->exit) is a "no"
        changes = sum(x["id_changes_with_gap"] + x["id_changes_without_gap"] + x["entry_exit_id_changes"]
                      for x in cl + hl + pl) + ctl.get("warps_with_sampled_id_change", 0) + \
            hog.get("warps_with_sampled_id_change", 0) + S.get("e_prio", {}).get("warps_with_sampled_id_change", 0) + \
            D["entry_exit_id_changes_ab"]
        eps = sum(x["gap_episodes_ge_half_warps"] for x in hl)
        wg = sum(x["warp_gaps"] for x in hl)
        cov = [x.get("gap_time_covered_by_hog_running") for x in hl]
        # the gaps must be the hog's doing: every hog launch has gaps, mostly while the hog ran,
        # and the hog was still running when the last launch ended
        attributed = bool(cov) and all(c is not None and c >= ATTRIB_MIN for c in cov) and \
            hog.get("hog_alive_at_end") is True
        if changes > 0:
            ans = "no"
        elif det_ok and ctl_quiet and eps >= MIN_EPISODES and wg >= MIN_WARP_GAPS and attributed:
            ans = "yes"
        else:
            ans = "inconclusive"
        pe = {
            "answer": ans,
            "detector_positive_control_ok": det_ok,
            "control_max_dt_below_threshold": ctl_quiet,
            "control_warp_gaps": sum(x["warp_gaps"] for x in cl),
            "hog_episodes_ge_half_warps": eps,
            "hog_warp_gaps": wg,
            "id_changes_total": changes,
            "id_changes_without_gap": sum(x["id_changes_without_gap"] for x in cl + hl),
            "exit_collisions": sum(x["collisions_smid_warpid_exit"] for x in cl + hl),
            "gap_time_covered_by_hog_running": cov,
            "gaps_attributed_to_hog": attributed,
            "event_log_overflow": sum(x["event_log_overflow"] for x in cl + hl),
            "warps_with_cap_flags": sum(x["cap_flags"] for x in cl + hl),
            "hog_alive_at_end": hog.get("hog_alive_at_end"),
        }
    D["ids_stable_after_preemption"] = pe or {"answer": "not run"}
    pr = S.get("e_prio")
    if pr:
        ins = sum(x.get("intruders_started_inside_long_kernel", 0) for x in pr["launches"])
        issued = sum(x.get("intruders_issued_within_long_dur", 0) for x in pr["launches"])
        unwritten = sum(x.get("intruders_unwritten", 0) for x in pr["launches"])
        D["same_context_priority_preemption"] = {
            "intruders_started_inside_long_kernel": ins,
            "intruders": sum(x.get("intruders", 0) for x in pr["launches"]),
            "intruders_issued_within_long_dur": issued,
            "intruders_unwritten": unwritten,
            "warp_gaps": sum(x["warp_gaps"] for x in pr["launches"]),
            "id_changes": sum(x["id_changes_with_gap"] + x["id_changes_without_gap"] + x["entry_exit_id_changes"]
                              for x in pr["launches"]),
            # "no" needs intruders that were issued while the long kernel ran and left timestamps
            "observed": "yes" if ins > 0 else ("no" if issued > 0 and unwritten == 0 else "not tested"),
            "intruder_capped_warps": sum(x.get("intruder_capped_warps", 0) for x in pr["launches"]),
        }
    return D


def text_summary(S, D):
    L = []
    L.append("check 1: one full-occupancy id log")
    L.append(f"S: max smid+1 = {D['S']['max_smid_plus_1']}, %nsmid = {D['S']['nsmid']}")
    L.append(f"W: max warpid+1 = {D['W']['max_warpid_plus_1']}, %nwarpid = {D['W']['nwarpid']}")
    x = D["exclusive_smid_warpid"]
    L.append(f"(smid, warpid) exclusive among co-resident warps: {x['answer']} "
             f"({x['collisions_in_proven_launches']} collisions in {x['proven_launches']} proven launches; "
             f"{x['not_proven_launches']} not proven; oversubscribed control capped: {x['oversubscribed_control_capped']}; "
             f"(smid, tid/32) control collides: {x['control_key_smid_tid32_collides']}; "
             f"b pairs sharing an SM: {x['b_pairs_proven_with_shared_sm']})")
    L.append(f"%warpid: {D['warpid_kind']['answer']} ({D['warpid_kind']['warps_with_warpid_eq_tid32']} of "
             f"{D['warpid_kind']['warps']} warps have warpid == tid/32)")
    L.append(f"entry->exit id changes in a/b: {D['entry_exit_id_changes_ab']}")
    g = D["gridid_unique_per_launch"]
    L.append(f"%gridid unique per launch (replays included): {g['answer']} ({g['duplicates']} duplicates; "
             f"replay counters complete: {g['graph_replay_counters_complete']}; missing records "
             f"{g['missing_records']}; parts with fewer launches {g['parts_with_fewer_launches']}; graph node "
             f"overflow {g['graph_node_overflow']})")
    for p, v in g["parts"].items():
        L.append(f"  {p}: launches {v['launches']}, duplicates {v['duplicates']}, strictly increasing "
                 f"{v['strictly_increasing_in_issue_order']}")
    L.append(f"%gridid width: {D['gridid_width']}")
    p = D["ids_stable_after_preemption"]
    L.append(f"ids stable after preemption: {p.get('answer')}")
    for k, v in p.items():
        if k != "answer":
            L.append(f"  {k}: {v}")
    if "same_context_priority_preemption" in D:
        L.append(f"same-context priority preemption (extra): {D['same_context_priority_preemption']}")
    if "a" in S:
        L.append("case a launches (bs, blocks, proven, collisions entry/exit, control (smid,tid/32) collisions, "
                 "max CTAs on one SM, max warpid, gridid):")
        for x in S["a"]:
            L.append(f"  {x.get('case')} L{x.get('launch')} bs={x.get('bs')} blocks={x.get('blocks')} proven="
                     f"{x['proven_coresident']} coll={x['collisions_smid_warpid_entry']}/"
                     f"{x['collisions_smid_warpid_exit']} ctl={x['control_collisions_smid_tid32']} "
                     f"ctas/sm={x['max_ctas_on_one_sm']} maxwarpid={x['max_warpid']} "
                     f"gridid={list(x['gridid_by_tag'].values())[:1]}")
    if "b" in S:
        for x in S["b"]:
            L.append(f"  b L{x.get('launch')} bs={x.get('bs')} proven={x['proven_coresident']} "
                     f"coll={x['collisions_smid_warpid_entry']}/{x['collisions_smid_warpid_exit']} "
                     f"sms_with_both_grids={x['sms_with_two_grids']} gridids={x['gridid_by_tag']}")
    for e in ("e_ctl", "e_hog", "e_prio"):
        if e in S:
            for x in S[e]["launches"]:
                L.append(f"  {e} L{x['launch']}: warp-gaps {x['warp_gaps']}, episodes>=half "
                         f"{x['gap_episodes_ge_half_warps']} (all {x['gap_episodes']}), max dt "
                         f"{x['max_dt_ns_excl_injected']} ns, id changes {x['id_changes_with_gap']}+"
                         f"{x['id_changes_without_gap']}, injected {x['injected_detected']}/{x['injected_done']}, "
                         f"ns/iter {x['ns_per_iter_median']:.0f}, hog coverage "
                         f"{x.get('gap_time_covered_by_hog_running')}")
    return "\n".join(L) + "\n"


def run(out):
    S = {}
    if os.path.exists(os.path.join(out, "a", "a_records.csv")):
        S["a"] = analyze_ab(os.path.join(out, "a"), "a")
    if os.path.exists(os.path.join(out, "b", "b_records.csv")):
        S["b"] = analyze_ab(os.path.join(out, "b"), "b")
    if os.path.exists(os.path.join(out, "c", "c_records.csv")):
        S["c"] = analyze_c(os.path.join(out, "c"))
    if os.path.exists(os.path.join(out, "d", "d_records.csv")):
        S["d"] = analyze_d(os.path.join(out, "d"))
    if os.path.exists(os.path.join(out, "e_ctl", "e_warps.csv")):
        S["e_ctl"] = analyze_e(os.path.join(out, "e_ctl"))
    if os.path.exists(os.path.join(out, "e_prio", "e_warps.csv")):
        S["e_prio"] = analyze_e(os.path.join(out, "e_prio"))
    if os.path.exists(os.path.join(out, "e_hog", "e_warps.csv")):
        S["e_hog"] = analyze_e(os.path.join(out, "e_hog"), os.path.join(out, "e_hog"))
    D = decide(S)
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump({"decisions": D, "details": S}, f, indent=1)
    txt = text_summary(S, D)
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write(txt)
    sys.stdout.write(txt)


# ---------------------------------------------------------------- self-test
def selftest():
    """Synthetic inputs where the right answer is 'no': the analysis must say so."""
    def rec(**kw):
        base = dict(case="a", launch=0, kind=0, tag=0, warp_lin=0, warp_in_cta=0, smid=0, warpid=0, laneid=0,
                    nsmid=8, nwarpid=48, ctax=0, ctay=0, ctaz=0, gridid=5, gridid32=5, gtime=0, aux=0, flags=0)
        base.update(kw)
        return {k: str(v) for k, v in base.items()}
    # two co-resident warps sharing (smid 0, warpid 3) in different CTAs
    recs = [rec(warp_lin=0, ctax=0, warpid=3), rec(warp_lin=1, ctax=1, warpid=3),
            rec(kind=1, warp_lin=0, ctax=0, warpid=3), rec(kind=1, warp_lin=1, ctax=1, warpid=4)]
    s = launch_stats(recs, 0)
    assert s["proven_coresident"] and s["collisions_smid_warpid_entry"] == 1, s
    assert s["entry_exit_id_changes"] == 1, s
    s2 = launch_stats(recs, 1)
    assert not s2["proven_coresident"]
    # gridid duplicates across launches
    rs = [rec(launch=i, gridid=7 if i < 2 else 8 + i, gridid32=7 if i < 2 else 8 + i) for i in range(4)]
    g = gridid_series(rs, lambda r: int(r["launch"]))
    assert g["duplicates"] == 1 and not g["strictly_increasing_in_issue_order"], g
    S = {"a": [dict(s, control="0"), dict(launch_stats(recs, 5), control="1")], "b": []}
    D = decide(S)
    assert D["exclusive_smid_warpid"]["answer"] == "no", D

    # ---- full decide() on a synthetic run where every answer is "yes", then one defect at a time
    import copy

    def good():
        # a: 2 CTAs on SM 0, warpids 0 and 1, both tid/32 == 0 (the control key collides)
        ra = [rec(warp_lin=0, ctax=0, warpid=0), rec(warp_lin=1, ctax=1, warpid=1),
              rec(kind=1, warp_lin=0, ctax=0, warpid=0), rec(kind=1, warp_lin=1, ctax=1, warpid=1)]
        rb = [rec(case="b", tag=0, warp_lin=0, warpid=0), rec(case="b", tag=1, warp_lin=1, warpid=1),
              rec(case="b", tag=0, kind=1, warp_lin=0, warpid=0), rec(case="b", tag=1, kind=1, warp_lin=1, warpid=1)]
        rb = [dict(r, gridid=str(5 + int(r["tag"]))) for r in rb]
        A = [dict(launch_stats(ra, 0), control="0"), dict(launch_stats(ra, 3), control="1")]
        B = [launch_stats(rb, 0)]

        def series(n, start):
            return gridid_series([rec(launch=i, gridid=start + i, gridid32=start + i) for i in range(n)],
                                 lambda r: int(r["launch"]))
        C = {"c1": series(4, 10), "c2": series(4, 20), "c3": series(6, 30), "c4": series(6, 40),
             "graph_node_overflow": 0, "c1_launches_requested": 4, "c2_launches_requested": 4}
        for p in ("c3", "c4"):
            C[p].update(replay_counters=[2, 2, 2], replays_requested=2)
        Dd = dict(series(6, 50), modules={"50": {"launches": 2, "nwarpid": [48]}})
        el = dict(max_smid=7, max_warpid=47, nsmid=[8], nwarpid=[48], exit_barrier_proven=True,
                  entry_barrier_proven=True, collisions_smid_warpid_exit=0, collisions_smid_warpid_entry=0,
                  injected_detected=4, injected_done=4, warps=4, max_dt_ns_excl_injected=5000,
                  id_changes_with_gap=0, id_changes_without_gap=0, entry_exit_id_changes=0,
                  gap_episodes_ge_half_warps=0, warp_gaps=0, gap_time_covered_by_hog_running=None,
                  event_log_overflow=0, cap_flags=0)
        hl = dict(el, max_dt_ns_excl_injected=3_000_000, gap_episodes_ge_half_warps=20, warp_gaps=2000,
                  gap_time_covered_by_hog_running=0.95)
        return {"a": A, "b": B, "c": C, "d": Dd,
                "e_ctl": {"launches": [dict(el)], "warps_with_sampled_id_change": 0},
                "e_hog": {"launches": [hl], "warps_with_sampled_id_change": 0, "hog_alive_at_end": True}}

    def ans(S):
        D = decide(S)
        return (D["exclusive_smid_warpid"]["answer"], D["gridid_unique_per_launch"]["answer"],
                D["ids_stable_after_preemption"]["answer"])
    assert ans(good()) == ("yes", "yes", "yes"), decide(good())
    S = good(); S["c"]["c3"] = gridid_series([rec(launch=i, gridid=30 + i % 3) for i in range(6)],
                                             lambda r: int(r["launch"]))
    S["c"]["c3"].update(replay_counters=[2, 2, 2], replays_requested=2)
    assert ans(S)[1] == "no", ans(S)                       # a graph node keeps its gridid
    S = good(); S["c"]["c1"]["missing_records"] = 4
    assert ans(S)[1] == "inconclusive", ans(S)             # a launch left no record
    S = good(); S["c"]["c4"]["replay_counters"] = [2, 2, 1]
    assert ans(S)[1] == "inconclusive", ans(S)             # a replay did not run
    S = good(); S["e_hog"]["launches"][0]["id_changes_with_gap"] = 1
    assert ans(S)[2] == "no", ans(S)                       # an id moved after a gap
    S = good(); S["a"][0]["entry_exit_id_changes"] = 1
    assert ans(S)[2] == "no", ans(S)                       # an id moved between entry and exit in a
    S = good(); S["e_hog"]["launches"][0]["gap_episodes_ge_half_warps"] = 3
    assert ans(S)[2] == "inconclusive", ans(S)             # too few preemptions
    S = good(); S["e_ctl"]["launches"][0]["max_dt_ns_excl_injected"] = 60_000
    assert ans(S)[2] == "inconclusive", ans(S)             # control not quiet
    S = good(); S["e_hog"]["launches"][0]["gap_time_covered_by_hog_running"] = 0.4
    assert ans(S)[2] == "inconclusive", ans(S)             # gaps not the hog's
    S = good(); S["e_hog"]["launches"][0]["injected_detected"] = 3
    assert ans(S)[2] == "inconclusive", ans(S)             # detector missed its own stall
    S = good(); S["e_hog"]["launches"][0]["collisions_smid_warpid_exit"] = 1
    assert ans(S)[0] == "no", ans(S)                       # collision among e's exit records
    S = good(); S["a"][1]["caphits"] = 0
    assert ans(S)[0] == "inconclusive", ans(S)             # oversubscribed control did not cap
    print("selftest ok")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest()
    elif len(sys.argv) == 2:
        run(sys.argv[1])
    else:
        sys.stderr.write(__doc__)
        sys.exit(1)
