#!/usr/bin/env python3
"""C3 analysis: cost parts of today's per-thread map op (a), slot addressing
(b), fence kinds (d). Part (c) is threadend/threadend.py (no new runs).

Usage: python3 -I analyze.py OUT_DIR
Reads OUT_DIR/raw.csv and OUT_DIR/items.json; writes OUT_DIR/summary.csv,
OUT_DIR/derived.json and OUT_DIR/summary.md. Writes nowhere else.

Definitions (fixed before any real run; README "Decision rules"), the same as
step 0's e2-cost/analyze.py where they overlap:
- t[r, i]: per-launch time of item i in round r (us), CUDA events around N
  back-to-back launches behind the k_hold gate.
- delta[r, i] = t[r, i] - t[r, empty kernel of the same MODULE and shape].
- ns per thread-op = delta / (threads x R); ns per warp-op = delta / (warps x R).
- slope (per thread-op) = (t[r, Rbig] - t[r, R1]) / ((Rbig - 1) x threads).
- A difference between two items (a "term") is taken per round on ns per
  thread-op, then median, min and max over rounds; "resolved" = the paired
  difference has the same sign in every round.
- [projection] % of a pp512 = 192,560,640 thread executions x ns / 204.6 ms
  (per thread execution) or 6.02M x ns per warp-op / 204.6 ms (per warp
  record), as in step 0.
"""
import csv
import json
import os
import statistics
import sys
from collections import defaultdict

PP512_THREAD_EXECS = 192560640
PP512_WARP_RECORDS = 6.02e6
PP512_GPU_NS = 204.6e6
T10 = 0.10 * PP512_GPU_NS / PP512_THREAD_EXECS  # 0.1062 ns per thread exec = 10%
T6 = 0.06 * PP512_GPU_NS / PP512_THREAD_EXECS  # 0.0638 ns = 6% (raw-store band top)
T1 = 0.01 * PP512_GPU_NS / PP512_THREAD_EXECS  # 0.0106 ns = 1%
PRIMARY = ("anchor", "dev")  # shape, memory; R = 1, exit form
SHAPES = ["anchor", "large", "full"]
# step 0 pass 1 values (anchor, R = 1, ns per thread execution) for the gate
STEP0 = {("F", "reg", 1): 0.433, ("F", "dev", 1): 0.369,
         ("st_lane32", "dev", 0): 0.032, ("st_lane32", "dev", 1): 0.259}
GATE_TOL = {("st_lane32", "dev", 0): 0.25}  # small delta (8 us); others 0.10
FENCE_NAME = {0: "none", 1: "membar.sys", 2: "membar.gl", 3: "fence.acq_rel.gpu",
              4: "fence.acq_rel.sys"}


def stats(v):
    v = [x for x in v if x is not None]
    if not v:
        return None
    return {"median": statistics.median(v), "min": min(v), "max": max(v), "n": len(v)}


def pct(ns):
    return None if ns is None else PP512_THREAD_EXECS * ns / PP512_GPU_NS * 100


def pct_warp(ns_warp):
    return None if ns_warp is None else PP512_WARP_RECORDS * ns_warp / PP512_GPU_NS * 100


def main():
    out = sys.argv[1]
    meta = json.load(open(os.path.join(out, "items.json")))
    items = {it["id"]: it for it in meta["items"]}
    rows = list(csv.DictReader(open(os.path.join(out, "raw.csv"))))
    t = defaultdict(dict)
    capped = []
    for r in rows:
        if r.get("hold_capped", "0") not in ("0", ""):
            capped.append((r["round"], r["id"]))
            continue
        t[r["id"]][int(r["round"])] = float(r["per_launch_us"])
    empty = {}
    for iid, it in items.items():
        if it["case"] == "empty":
            empty[(it["module"], it["shape"])] = iid
    summary = {}
    per_round_thread = {}  # id -> round -> ns per thread-op
    for iid, it in items.items():
        if iid not in t:
            continue
        e = t.get(empty.get((it["module"], it["shape"])), {})
        T, W, R = it["threads"], it["warps"], it["R"]
        d = {rd: t[iid][rd] - e[rd] for rd in t[iid] if rd in e}
        rec = dict(it)
        rec["t_us"] = stats(list(t[iid].values()))
        rec["delta_us"] = stats(list(d.values()))
        pt = {rd: v * 1e3 / (T * R) for rd, v in d.items()}
        per_round_thread[iid] = pt
        rec["ns_per_thread_op"] = stats(list(pt.values()))
        rec["ns_per_warp_op"] = stats([v * 1e3 / (W * R) for v in d.values()])
        rec["resolved"] = bool(d) and min(d.values()) > 0
        summary[iid] = rec
    # slopes R = 1 -> Rbig
    by_key = defaultdict(dict)
    for iid, it in summary.items():
        by_key[(it["shape"], it["module"], it["case"], it["mem"], it["fk"])][it["R"]] = iid
    for k_, rs in by_key.items():
        if 1 in rs and len(rs) == 2:
            rb = max(rs)
            a, b = rs[1], rs[rb]
            T = summary[a]["threads"]
            sl = [(t[b][rd] - t[a][rd]) * 1e3 / ((rb - 1) * T) for rd in t[a] if rd in t[b]]
            summary[b]["slope_ns_per_thread_op"] = stats(sl)
            summary[b]["slope_ns_per_warp_op"] = stats([x * 32 for x in sl])

    def find(shape, mod, case, mem, fk, R=1):
        for x in summary:
            it = summary[x]
            if (it["shape"], it["module"], it["case"], it["mem"], it["fk"], it["R"]) == \
                    (shape, mod, case, mem, fk, R):
                return x
        return None

    def rbig(shape):
        rs = [it["R"] for it in summary.values() if it["shape"] == shape and it["R"] != 1]
        return max(rs) if rs else None

    def med(x, field="ns_per_thread_op"):
        if x is None or summary[x].get(field) is None:
            return None
        return summary[x][field]["median"]

    def term(a, b, R1=True, field_slope=False):
        """paired per-round difference of ns per thread-op (a - b)."""
        if a is None or b is None:
            return None
        if field_slope:
            return None
        pa, pb = per_round_thread[a], per_round_thread[b]
        v = [pa[rd] - pb[rd] for rd in pa if rd in pb]
        s = stats(v)
        if s is None:
            return None
        s["resolved"] = (min(v) > 0) or (max(v) < 0)
        s["pct"] = pct(s["median"])
        return s

    derived = {"rounds": len({int(r["round"]) for r in rows}),
               "thresholds_ns_per_thread": {"10pct": T10, "6pct": T6, "1pct": T1},
               "hold_capped_rows_excluded": len(capped),
               "validation_failures": meta.get("validation_failures"),
               "timed_slot_misses": meta.get("timed_slot_misses"),
               "timed_slot_oor": meta.get("timed_slot_oor"),
               "timed_inline_misses": meta.get("timed_inline_misses"),
               "timed_claim_failures": meta.get("timed_claim_failures")}

    # ---- run validity (README "Decision rules", validity) -------------------
    # Every verdict below carries this prefix unless the run is valid: no
    # validation failure, every requested round done, at least 10 rounds,
    # and no timed inline-path miss (an inline variant that took its slow
    # path is not the variant the ladder names).
    problems = []
    if meta.get("validation_failures"):
        problems.append("INVALID RUN: %d validation failures" % meta["validation_failures"])
    if meta.get("rounds_done", 0) < meta.get("rounds_requested", 0):
        problems.append("INCOMPLETE RUN: %s of %s rounds" % (meta.get("rounds_done"), meta.get("rounds_requested")))
    if meta.get("rounds_done", 0) < 10:
        problems.append("NOT A MEASUREMENT: fewer than 10 rounds")
    if meta.get("timed_inline_misses"):
        problems.append("INVALID RUN: %d timed inline-path misses" % meta["timed_inline_misses"])
    derived["run_problems"] = problems
    derived["run_valid"] = not problems
    VPRE = "" if not problems else "[%s] " % "; ".join(problems)

    # ---- gate: comparable with step 0? ------------------------------------
    gate = {}
    for (case, mem, fk), ref in STEP0.items():
        mod = "F" if case == "F" else "k"
        x = find("anchor", mod, case, mem, fk)
        v = med(x)
        tol = GATE_TOL.get((case, mem, fk), 0.10)
        gate["%s %s fk%d" % (case, mem, fk)] = {
            "value": v, "step0": ref, "ratio": None if v is None else v / ref,
            "ok": v is not None and abs(v / ref - 1) <= tol, "tolerance": tol}
    loop = {}
    for m in ("F", "N"):
        for mem in ("reg", "dev"):
            a, b = find("anchor", m, m + "_loop", mem, 1 if m == "F" else 0), find("anchor", m, m, mem, 1 if m == "F" else 0)
            if med(a, "delta_us") and med(b, "delta_us"):
                loop["%s %s" % (m, mem)] = med(a, "delta_us") / med(b, "delta_us")
    # step 0 characterised F's loop/exit ratio only; N's is a sanity line
    gate_ok = all(g["ok"] for g in gate.values()) and \
        all(0.8 <= v <= 1.25 for k_, v in loop.items() if k_.startswith("F "))
    derived["gate"] = {"items": gate, "loop_over_exit_anchor": loop, "comparable_with_step0": gate_ok,
                       "n_loop_over_exit_in_range": all(0.8 <= v <= 1.25 for k_, v in loop.items()
                                                        if k_.startswith("N "))}

    # ---- (a) ladder -------------------------------------------------------
    ladder = {}
    for shape in SHAPES:
        for mem in ("dev", "reg"):
            g = lambda mod, case, fk=0, R=1, m=mem: find(shape, mod, case, m, fk, R)
            F, N = g("F", "F", 1), g("N", "N", 0)
            I, IF, IC, K = g("X", "I"), g("X", "IF", 1), g("X", "IC"), g("X", "K")
            M, M64 = g("X", "M"), g("X", "M64")
            E = find(shape, "X", "E", "none", 0, 1)
            ST0, ST1 = g("k", "st_lane32", 0), g("k", "st_lane32", 1)
            levels = {n: x for n, x in (("F today", F), ("N no fence", N), ("IF inline+fence", IF),
                                        ("IC inline, helper fallback", IC), ("I inline", I),
                                        ("K inline, no probe call", K), ("M memop alone", M),
                                        ("M64 memop u64", M64), ("E empty probe", E),
                                        ("8-B store", ST0), ("8-B store + membar.sys", ST1))}
            lv = {}
            rb = rbig(shape)
            for n, x in levels.items():
                if x is None:
                    continue
                s = summary[x]["ns_per_thread_op"]
                xb = find(shape, summary[x]["module"], summary[x]["case"], summary[x]["mem"], summary[x]["fk"], rb) if rb else None
                slope = summary[xb].get("slope_ns_per_thread_op") if xb else None
                lv[n] = {"id": x, "ns": s and s["median"], "min": s and s["min"], "max": s and s["max"],
                         "pct": pct(s and s["median"]), "resolved": summary[x]["resolved"],
                         "slope_ns": slope and slope["median"], "slope_pct": pct(slope and slope["median"])}
            terms = {
                "fence (F - N)": term(F, N),
                "fence on the inlined path (IF - I)": term(IF, I),
                "helper call + dispatch (N - I)": term(N, I),
                "cold helper fallback present (IC - I)": term(IC, I),
                "probe call (I - K)": term(I, K),
                "lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M)": term(K, M),
                "memory op (M)": summary[M]["ns_per_thread_op"] if M else None,
                "u32 split (M - M64)": term(M, M64),
                "RMW over a plain store (M - 8-B store)": term(M, ST0),
            }
            for k_, v in terms.items():
                if v is not None and "pct" not in v:
                    v = dict(v)
                    v["pct"] = pct(v["median"])
                    terms[k_] = v
            ladder["%s %s" % (shape, mem)] = {"levels": lv, "terms": terms}
    derived["ladder"] = ladder

    def verdict_a(shape, mem):
        L = ladder["%s %s" % (shape, mem)]["levels"]
        T_ = ladder["%s %s" % (shape, mem)]["terms"]

        def p(n):
            return L.get(n, {}).get("pct")

        def crosses(n, thr):
            x = L.get(n)
            return x is not None and x["min"] is not None and x["min"] < thr <= x["max"]
        res = {"N_pct": p("N no fence"), "I_pct": p("I inline"), "IC_pct": p("IC inline, helper fallback"),
               "K_pct": p("K inline, no probe call"), "M_pct": p("M memop alone"), "F_pct": p("F today"),
               "store_pct": p("8-B store"), "E_pct": p("E empty probe")}
        if None in (res["N_pct"], res["I_pct"], res["K_pct"], res["M_pct"]):
            res["verdict"] = "missing data"
            return res
        notes = []
        if res["N_pct"] >= 10:
            notes.append("the fence-free helper path still projects >= 10%% (%.1f%%)" % res["N_pct"])
        if res["N_pct"] <= 6:
            v = ("raw-store band (<= 6%) reachable without inlining: removing the fence suffices (cost only; "
                 "whether the fence may be removed is a separate memory-model question)")
        elif min(res["I_pct"], res["IC_pct"] or 1e9) <= 6:
            v = "raw-store band (<= 6%%) reachable only with an inlined helper path (I %.1f%%, IC %.1f%%)" % (
                res["I_pct"], res["IC_pct"] if res["IC_pct"] is not None else float("nan"))
        elif res["K_pct"] <= 6:
            v = "raw-store band reachable only with the probe body inlined into the kernel as well (K %.1f%%)" % res["K_pct"]
        elif res["M_pct"] <= 6:
            v = ("raw-store band reachable only with the op reduced to its memory access: no probe call, no "
                 "probe switch, eBPF-stack key, 3-D thread id or bounds checks (M %.1f%%, K %.1f%%)" % (
                     res["M_pct"], res["K_pct"]))
        else:
            v = "raw-store band not reachable even by the memory access alone (M %.1f%%)" % res["M_pct"]
            rmw = T_.get("RMW over a plain store (M - 8-B store)")
            if rmw:
                notes.append("the read-modify-write costs %.1f%% more than the 8-B store" % rmw["pct"])
        sd = [n for n, k in (("N", "N no fence"), ("IC", "IC inline, helper fallback"), ("I", "I inline"),
                             ("K", "K inline, no probe call"), ("M", "M memop alone")) if (p(k) or 1e9) < 10]
        res["single_digit_levels"] = sd
        unstable = [n for n in ("N no fence", "I inline", "IC inline, helper fallback", "K inline, no probe call",
                                "M memop alone") if crosses(n, T6) or crosses(n, T10)]
        if unstable:
            notes.append("unstable: the round spread of %s crosses 6%% or 10%%" % ", ".join(unstable))
        fa, fb = T_.get("fence (F - N)"), T_.get("fence on the inlined path (IF - I)")
        if fa and fb and fa["median"] > 0:
            r = fb["median"] / fa["median"]
            res["fence_inline_over_fence_helper"] = r
            if not 0.7 <= r <= 1.3:
                notes.append("fence cost is not additive across paths (IF-I = %.2f x F-N)" % r)
        res["verdict"] = VPRE + v
        res["notes"] = notes
        return res

    derived["decision_a"] = {"primary": verdict_a(*PRIMARY)}
    derived["decision_a"]["secondary"] = {"%s %s" % (s, m): verdict_a(s, m) for s in SHAPES
                                          for m in ("dev", "reg") if (s, m) != PRIMARY}

    # ---- (b) slot addressing ------------------------------------------------
    slots = {}
    for shape in SHAPES:
        g = lambda case, R=1: find(shape, "k", case, "dev", 0, R)
        S = {c: g(c) for c in ("slot_e", "slot_ex", "slot_excg", "tid_e", "tid_ex", "tidw_e", "slot_cx",
                               "slot_cxr")}
        ST0 = find(shape, "k", "st_lane32", "dev", 0)
        rb = rbig(shape)
        lv = {}
        for c, x in S.items():
            if x is None:
                continue
            s = summary[x]["ns_per_thread_op"]
            xb = g(c, rb) if rb else None
            sl = summary[xb].get("slope_ns_per_thread_op") if xb else None
            lv[c] = {"ns": s["median"], "min": s["min"], "max": s["max"], "pct": pct(s["median"]),
                     "slope_ns": sl and sl["median"], "slope_pct": pct(sl and sl["median"]),
                     "regs": summary[x]["regs"]}
        slots[shape] = {"levels": lv, "terms": {
            "special-register addressing (slot_e - tidw_e)": term(S["slot_e"], S["tidw_e"]),
            "exit check (slot_ex - slot_e)": term(S["slot_ex"], S["slot_e"]),
            "exit check from L2 (slot_excg - slot_e)": term(S["slot_excg"], S["slot_e"]),
            "footprint: tid table vs folded (tid_e - tidw_e)": term(S["tid_e"], S["tidw_e"]),
            "slot entry vs 8-B store (slot_e - st_lane32)": term(S["slot_e"], ST0),
            "claim/release over tag store (slot_cx - slot_ex)": term(S["slot_cx"], S["slot_ex"]),
            "acquire/release ordering (slot_cx - slot_cxr)": term(S["slot_cx"], S["slot_cxr"]),
        }}
    derived["slots"] = slots
    pb = slots[PRIMARY[0]]
    dec_b = {}
    # B4: the slot results are void unless both validation controls fired
    bad = [it for it in items.values() if it["vkind"] == "badtag"]
    dec_b["control_badtag_detected_every_mismatch"] = all(it["valid"] == "ok" for it in bad) and bool(bad)
    col = [it for it in items.values() if it["vkind"] == "collide"]
    dec_b["control_collide_counted_claim_failures"] = all(it["valid"] == "ok" for it in col) and bool(col)
    controls_ok = dec_b["control_badtag_detected_every_mismatch"] and dec_b["control_collide_counted_claim_failures"]
    BPRE = VPRE + ("" if controls_ok else "[VOID: a slot control did not fire] ")
    sr = pb["terms"]["special-register addressing (slot_e - tidw_e)"]
    if sr:
        dec_b["special_register_addressing"] = BPRE + ("negligible (< 1%%: %.4f ns, %.2f%%)" if sr["median"] < T1 else
                                                       "visible (>= 1%%: %.4f ns, %.2f%%)") % (sr["median"], sr["pct"])
    se = pb["levels"].get("slot_ex")
    if se:
        dec_b["entry_plus_exit_protocol"] = BPRE + "%s (%.4f ns, %.1f%%)" % (
            "within the raw-store band (<= 6%)" if se["pct"] <= 6 else
            "single digit (< 10%)" if se["pct"] < 10 else ">= 10%", se["ns"], se["pct"])
    sc = pb["levels"].get("slot_cx")
    if sc:
        dec_b["claim_protocol_cost"] = BPRE + "%s (%.4f ns, %.1f%%)" % (
            "within the raw-store band (<= 6%)" if sc["pct"] <= 6 else
            "single digit (< 10%)" if sc["pct"] < 10 else ">= 10%", sc["ns"], sc["pct"])
    dec_b["timed_claim_failures"] = meta.get("timed_claim_failures")
    dec_b["validation_misses"] = sum(it["slot_miss"] for it in items.values() if it["vkind"] == "slot_ex")
    dec_b["timed_misses"] = meta.get("timed_slot_misses")
    dec_b["timed_out_of_range"] = meta.get("timed_slot_oor")
    # B5: counted events in the timed launches are a finding, not a harness
    # failure. The exit side's ids are read together with the entry's (SASS),
    # so a miss means another thread's tag was in the slot (a collision), not
    # that the ids changed between entry and exit.
    ev = [(n, meta.get(k)) for n, k in (("exit-check misses", "timed_slot_misses"),
                                        ("out-of-range indices", "timed_slot_oor"),
                                        ("claim failures", "timed_claim_failures")) if meta.get(k)]
    dec_b["finding_B5"] = ("collision or out-of-range index detected and counted in the timed launches: " +
                           ", ".join("%s %d" % x for x in ev)) if ev else "none counted"
    derived["decision_b"] = dec_b

    # ---- (d) fence kinds per warp -------------------------------------------
    fences = {}
    for shape in SHAPES:
        rows_ = {}
        for case, mems in (("fence_lane32", ["none"]), ("fence_lane1", ["none"]),
                           ("st_lane32", ["dev", "reg"]), ("st_lane1", ["dev", "reg"])):
            for mem in mems:
                for fk in range(5):
                    x = find(shape, "k", case, mem, fk)
                    if x is None:
                        continue
                    w = summary[x]["ns_per_warp_op"]
                    base = find(shape, "k", case, mem, 0) if case.startswith("st") else None
                    fpart = None
                    if base and fk:
                        pa, pb_ = per_round_thread[x], per_round_thread[base]
                        T, W = summary[x]["threads"], summary[x]["warps"]
                        fpart = stats([(pa[rd] - pb_[rd]) * T / W for rd in pa if rd in pb_])
                    rows_["%s %s %s" % (case, mem, FENCE_NAME[fk])] = {
                        "ns_per_warp": w["median"], "min": w["min"], "max": w["max"],
                        "fence_part_ns_per_warp": fpart and fpart["median"],
                        "pct_per_thread_exec": pct(summary[x]["ns_per_thread_op"]["median"]),
                        "pct_per_warp_record": pct_warp(w["median"])}
        fences[shape] = rows_
    derived["fences"] = fences

    def gl_vs_sys(shape, case, mem):
        """D1 / D2 (README): classes on the medians; a class that claims a
        difference also needs the paired per-round difference sys - gl to
        have the same sign in every round, else 'unresolved'."""
        F_ = fences[shape]
        if case.startswith("st"):
            a = F_.get("%s %s membar.gl" % (case, mem), {}).get("fence_part_ns_per_warp")
            b = F_.get("%s %s membar.sys" % (case, mem), {}).get("fence_part_ns_per_warp")
            xg, xs = find(shape, "k", case, mem, 2), find(shape, "k", case, mem, 1)
        else:
            a = F_.get("%s none membar.gl" % case, {}).get("ns_per_warp")
            b = F_.get("%s none membar.sys" % case, {}).get("ns_per_warp")
            xg, xs = find(shape, "k", case, "none", 2), find(shape, "k", case, "none", 1)
        if a is None or b is None or xg is None or xs is None:
            return {"verdict": VPRE + "missing data"}
        if b <= 0:
            return {"gl_ns_per_warp": a, "sys_ns_per_warp": b,
                    "verdict": VPRE + "unresolved (the membar.sys part is not positive)"}
        T, W = summary[xs]["threads"], summary[xs]["warps"]
        ps, pg = per_round_thread[xs], per_round_thread[xg]
        diff = [(ps[rd] - pg[rd]) * T / W for rd in ps if rd in pg]  # sys - gl, ns per warp
        resolved = bool(diff) and (min(diff) > 0 or max(diff) < 0)
        cheaper = (b - a) / b
        if abs(cheaper) <= 0.10:
            v = "about the same (within 10%)"
        elif not resolved:
            v = "unresolved (medians differ by %.0f%%, but the paired sys - gl difference changes sign " \
                "across rounds)" % (cheaper * 100)
        elif cheaper >= 0.25:
            v = "membar.gl materially cheaper (>= 25%)"
        elif cheaper > 0:
            v = "membar.gl somewhat cheaper (10-25%)"
        else:
            v = "membar.gl dearer (by more than 10%)"
        return {"gl_ns_per_warp": a, "sys_ns_per_warp": b, "gl_cheaper_by": cheaper,
                "sys_minus_gl_per_round": stats(diff), "resolved": resolved, "verdict": VPRE + v}

    derived["decision_d"] = {s: {"after store, dev": gl_vs_sys(s, "st_lane32", "dev"),
                                 "after store, reg": gl_vs_sys(s, "st_lane32", "reg"),
                                 "lane 0 after store, dev": gl_vs_sys(s, "st_lane1", "dev"),
                                 "lane 0 after store, reg": gl_vs_sys(s, "st_lane1", "reg"),
                                 "alone, all lanes": gl_vs_sys(s, "fence_lane32", "none"),
                                 "alone, lane 0": gl_vs_sys(s, "fence_lane1", "none")} for s in SHAPES}

    json.dump(derived, open(os.path.join(out, "derived.json"), "w"), indent=1)

    # ---- summary.csv --------------------------------------------------------
    fields = ["id", "module", "case", "mem", "fk", "shape", "threads", "R", "n", "valid", "resolved",
              "t_us", "delta_us", "ns_per_thread_op", "ns_per_warp_op", "slope_ns_per_thread_op",
              "slope_ns_per_warp_op", "occ_blocks_per_sm", "regs", "local_bytes", "slot_miss", "inline_miss"]
    stat_fields = {"t_us", "delta_us", "ns_per_thread_op", "ns_per_warp_op", "slope_ns_per_thread_op",
                   "slope_ns_per_warp_op"}
    with open(os.path.join(out, "summary.csv"), "w") as f:
        w = csv.writer(f)
        hdr = []
        for x in fields:
            hdr += [x + "_med", x + "_min", x + "_max"] if x in stat_fields else [x]
        w.writerow(hdr)
        for x in sorted(summary):
            rec = summary[x]
            row = []
            for fl in fields:
                v = rec.get(fl)
                if fl in stat_fields:
                    row += ["%.4g" % v[s] if v else "" for s in ("median", "min", "max")]
                else:
                    row.append(v)
            w.writerow(row)

    # ---- summary.md ---------------------------------------------------------
    def f3(x):
        return "" if x is None else "%.3f" % x

    def f4(x):
        return "" if x is None else "%.4f" % x

    def f1(x):
        return "" if x is None else "%.1f" % x
    L = ["# C3 summary (%d rounds; medians [min-max] over rounds)\n" % derived["rounds"],
         "Smoke or loaded-board numbers are not measurements; only a run on a quiet board counts.\n",
         "**Run status: %s**\n" % ("valid" if not problems else "; ".join(problems)),
         "[projection] % of a pp512: 192.56M thread executions or 6.02M warp records over 204.6 ms of GPU time.\n",
         "## Gate: comparable with step 0 (anchor, R = 1)\n"]
    for k_, g in gate.items():
        L.append("- %s: %s ns (step 0 %.3f, ratio %s, tolerance %d%%) %s" % (
            k_, f3(g["value"]), g["step0"], f3(g["ratio"]), round(g["tolerance"] * 100), "ok" if g["ok"] else "OUT"))
    for k_, v in loop.items():
        L.append("- loop / exit form %s: %.2f%s" % (k_, v, "" if k_.startswith("F ") else " (sanity line, not in the gate)"))
    L.append("- **%s**" % ("comparable with step 0" if gate_ok else
                           "NOT comparable with step 0 (the within-run split below still stands)"))
    L.append("\n## (a) Cost ladder, ns per thread execution at R = 1 [projection %] (slope R1->Rbig)\n")
    for key, lad in ladder.items():
        L.append("### %s\n" % key)
        L.append("| level | ns [min-max] | % | slope ns | slope % |")
        L.append("|---|---:|---:|---:|---:|")
        for n, v in lad["levels"].items():
            L.append("| %s | %s [%s-%s] | %s | %s | %s |" % (n, f4(v["ns"]), f4(v["min"]), f4(v["max"]),
                                                          f1(v["pct"]), f4(v["slope_ns"]), f1(v["slope_pct"])))
        L.append("\n| term (paired per round) | ns [min-max] | % | resolved |")
        L.append("|---|---:|---:|---|")
        for n, v in lad["terms"].items():
            if v is None:
                continue
            L.append("| %s | %s [%s-%s] | %s | %s |" % (n, f4(v["median"]), f4(v["min"]), f4(v["max"]),
                                                    f1(v.get("pct")), v.get("resolved", "")))
        L.append("")
    da = derived["decision_a"]["primary"]
    L.append("**Decision (a), anchor, device memory, R = 1:** %s" % da["verdict"])
    for n in da.get("notes", []):
        L.append("- %s" % n)
    L.append("- single-digit levels: %s" % ", ".join(da.get("single_digit_levels", [])))
    for k_, v in derived["decision_a"]["secondary"].items():
        L.append("- secondary %s: %s%s" % (k_, v["verdict"], ("; " + "; ".join(v.get("notes", []))) if v.get("notes") else ""))
    L.append("\n## (b) Slot addressing, device memory, ns per thread execution at R = 1\n")
    for shape, sb in slots.items():
        L.append("### %s\n" % shape)
        L.append("| item | ns [min-max] | % | slope ns | regs |")
        L.append("|---|---:|---:|---:|---:|")
        for c, v in sb["levels"].items():
            L.append("| %s | %s [%s-%s] | %s | %s | %s |" % (c, f4(v["ns"]), f4(v["min"]), f4(v["max"]),
                                                          f1(v["pct"]), f4(v["slope_ns"]), v["regs"]))
        L.append("\n| term | ns [min-max] | % | resolved |")
        L.append("|---|---:|---:|---|")
        for n, v in sb["terms"].items():
            if v:
                L.append("| %s | %s [%s-%s] | %s | %s |" % (n, f4(v["median"]), f4(v["min"]), f4(v["max"]),
                                                        f1(v["pct"]), v["resolved"]))
        L.append("")
    L.append("**Decision (b):** " + json.dumps(dec_b))
    L.append("\n## (d) Fence kinds, ns per warp-op at R = 1\n")
    for shape, fr in fences.items():
        L.append("### %s\n" % shape)
        L.append("| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |")
        L.append("|---|---:|---:|---:|---:|")
        for n, v in fr.items():
            L.append("| %s | %s [%s-%s] | %s | %s | %s |" % (n, f3(v["ns_per_warp"]), f3(v["min"]), f3(v["max"]),
                                                          f3(v["fence_part_ns_per_warp"]),
                                                          f1(v["pct_per_thread_exec"]), f1(v["pct_per_warp_record"])))
        L.append("")
        for k_, v in derived["decision_d"][shape].items():
            L.append("- %s: %s%s" % (k_, v["verdict"], "" if "gl_ns_per_warp" not in v else
                                     " (gl %.2f, sys %.2f ns per warp)" % (v["gl_ns_per_warp"], v["sys_ns_per_warp"])))
        L.append("")
    L.append("## Counters\n")
    L.append("- validation failures: %s; hold-capped rows excluded: %d" % (derived["validation_failures"], len(capped)))
    L.append("- timed launches: slot misses %s, slot out-of-range %s, claim failures %s, inline-path misses %s" % (
        derived["timed_slot_misses"], derived["timed_slot_oor"], derived["timed_claim_failures"],
        derived["timed_inline_misses"]))
    open(os.path.join(out, "summary.md"), "w").write("\n".join(L) + "\n")
    print("\n".join(L[:13]))
    print("decision (a):", da["verdict"])


if __name__ == "__main__":
    main()
