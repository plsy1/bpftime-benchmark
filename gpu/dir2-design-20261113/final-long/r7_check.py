#!/usr/bin/env python3
"""Simulated R7 check for the final direction-2 design (UnitFold, merged).

Applies the design's decision function to the 59 bpftime host-call sites of
callsite-survey-20261113/records.json, using a hand-written per-site table
(unit shape, role, and the helper sources of the key / printk payload) that
stands in for the analyzer's abstract state. The real check replaces SITES
with the offline driver's output over the .bpf.o files.

Usage: python3 -I r7_check.py <records.json>
"""
import json
import sys
from collections import Counter

# shape: A counter, B non-atomic RMW + EXIST self-write, C leader lookup +
# update(ANY) with computed value, E entry/exit timestamp, F per-thread record,
# G leader publish, H printk, J benchmark hash op.
# role -> static decision (oracle-independent)
ROLE_DECISION = {
    "unit_lookup": "defer_unit",       # member of an UPSERT_ADD / ADD_IF_PRESENT unit
    "unit_insert": "defer_unit",
    "unit_exist_update": "defer_unit",
    "unit_update_any": "defer_unit",
    "set": "defer_set",
    "del": "defer_del",
    "read": "resident_read",           # value used now: GPU-local read, epoch-validated
    "printk": "defer_out",
}
# sources: c=constant, pid=helper 14 (launch constant), blk=503, sm=509,
# warp=510, tid=505, lane=511, t=502 globaltimer, mv(<scope>)=value loaded
# from a map value whose key has that scope.
S = {
 # benchmark/gpu/micro/cuda_probe.bpf.c
 ("micro/cuda_probe.bpf.c", 163): ("J", "set", {"c"}),
 ("micro/cuda_probe.bpf.c", 171): ("J", "unit_lookup", {"c"}),  # ADD_IF_PRESENT unit
 ("micro/cuda_probe.bpf.c", 182): ("J", "del", {"c"}),
 # cuda-counter
 ("cuda-counter/cuda_probe.bpf.c", 44): ("E", "set", {"blk"}),
 ("cuda-counter/cuda_probe.bpf.c", 48): ("A", "unit_lookup", {"pid"}),
 ("cuda-counter/cuda_probe.bpf.c", 52): ("A", "unit_insert", {"pid"}),
 ("cuda-counter/cuda_probe.bpf.c", 55): ("H", "printk", {"blk", "t"}),
 ("cuda-counter/cuda_probe.bpf.c", 66): ("E", "read", {"blk"}),
 ("cuda-counter/cuda_probe.bpf.c", 70): ("E", "del", {"pid"}),
 ("cuda-counter/cuda_probe.bpf.c", 73): ("B", "unit_lookup", {"pid"}),
 ("cuda-counter/cuda_probe.bpf.c", 76): ("B", "unit_exist_update", {"pid"}),
 ("cuda-counter/cuda_probe.bpf.c", 79): ("B", "unit_insert", {"pid"}),
 ("cuda-counter/cuda_probe.bpf.c", 82): ("H", "printk", {"blk", "t", "mv(blk)"}),
 ("cuda-counter/cuda_probe.bpf.c", 86): ("H", "printk", {"blk"}),
 # rocm-counter (no ROCm device attach; analysed as the nv path would run it)
 ("rocm-counter/rocm_probe.bpf.c", 44): ("E", "set", {"blk"}),
 ("rocm-counter/rocm_probe.bpf.c", 48): ("B", "unit_lookup", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 51): ("B", "unit_exist_update", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 53): ("B", "unit_insert", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 56): ("H", "printk", {"blk", "t"}),
 ("rocm-counter/rocm_probe.bpf.c", 67): ("E", "read", {"blk"}),
 ("rocm-counter/rocm_probe.bpf.c", 71): ("E", "del", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 74): ("B", "unit_lookup", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 77): ("B", "unit_exist_update", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 80): ("B", "unit_insert", {"pid"}),
 ("rocm-counter/rocm_probe.bpf.c", 83): ("H", "printk", {"blk", "t", "mv(blk)"}),
 ("rocm-counter/rocm_probe.bpf.c", 87): ("H", "printk", {"blk"}),
 # cudagraph (leader thread only)
 ("cudagraph/cuda_probe.bpf.c", 53): ("E", "set", {"pid"}),
 ("cudagraph/cuda_probe.bpf.c", 56): ("C", "unit_lookup", {"pid"}),
 ("cudagraph/cuda_probe.bpf.c", 58): ("C", "unit_update_any", {"pid"}),
 ("cudagraph/cuda_probe.bpf.c", 75): ("E", "read", {"pid"}),
 ("cudagraph/cuda_probe.bpf.c", 78): ("E", "del", {"pid"}),
 ("cudagraph/cuda_probe.bpf.c", 79): ("C", "unit_lookup", {"pid"}),
 ("cudagraph/cuda_probe.bpf.c", 81): ("C", "unit_update_any", {"pid"}),
 # host_map_test
 ("host_map_test/host_map_test.bpf.c", 108): ("H", "printk", {"tid", "t"}),
 ("host_map_test/host_map_test.bpf.c", 154): ("H", "printk", {"tid", "t", "mv(tid)"}),
 # gpu_shared_map
 ("gpu_shared_map/gpu_shared_map.bpf.c", 52): ("H", "printk", {"c"}),
 ("gpu_shared_map/gpu_shared_map.bpf.c", 55): ("G", "set", {"c"}),
 ("gpu_shared_map/gpu_shared_map.bpf.c", 73): ("H", "printk", {"c"}),
 # atomizer: constants, block id, values from constant-key 1503 lookups
 ("atomizer/atomizer.bpf.c", 48): ("H", "printk", {"mv(c)", "blk"}),
 ("atomizer/atomizer.bpf.c", 50): ("H", "printk", {"blk"}),
 ("atomizer/atomizer.bpf.c", 57): ("H", "printk", {"mv(c)", "blk"}),
 ("atomizer/atomizer.bpf.c", 59): ("H", "printk", {"blk"}),
 ("atomizer/atomizer.bpf.c", 68): ("H", "printk", {"blk", "mv(c)"}),
 ("atomizer/atomizer.bpf.c", 74): ("H", "printk", {"blk", "mv(c)"}),
 # launchlate (gpu_ts = globaltimer read first; uprobe_ts from a constant-key map)
 ("launchlate/launchlate.bpf.c", 114): ("H", "printk", {"t", "mv(c)"}),
 ("launchlate/launchlate.bpf.c", 117): ("H", "printk", {"t"}),
 ("launchlate-kernel-gpu-shared-map/launchlate.bpf.c", 119): ("H", "printk", {"t", "mv(c)"}),
 ("launchlate-kernel-gpu-shared-map/launchlate.bpf.c", 122): ("H", "printk", {"t"}),
 # threadscheduling (HASH) and the dynamic-hook copy (1501 -> HASH on Orin)
 ("threadscheduling/threadscheduling.bpf.c", 86): ("F", "set", {"blk", "tid"}),
 ("threadscheduling/threadscheduling.bpf.c", 90): ("A", "unit_lookup", {"sm"}),
 ("threadscheduling/threadscheduling.bpf.c", 95): ("A", "unit_insert", {"sm"}),
 ("threadscheduling/threadscheduling.bpf.c", 100): ("A", "unit_lookup", {"sm", "warp"}),
 ("threadscheduling/threadscheduling.bpf.c", 105): ("A", "unit_insert", {"sm", "warp"}),
 ("threadscheduling_dynamic_hook/threadscheduling.bpf.c", 89): ("F", "set", {"blk", "tid"}),
 ("threadscheduling_dynamic_hook/threadscheduling.bpf.c", 93): ("A", "unit_lookup", {"sm"}),
 ("threadscheduling_dynamic_hook/threadscheduling.bpf.c", 98): ("A", "unit_insert", {"sm"}),
 ("threadscheduling_dynamic_hook/threadscheduling.bpf.c", 103): ("A", "unit_lookup", {"sm", "warp"}),
 ("threadscheduling_dynamic_hook/threadscheduling.bpf.c", 108): ("A", "unit_insert", {"sm", "warp"}),
 # directly_run_on_gpu
 ("directly_run_on_gpu/directly_run.bpf.c", 79): ("H", "printk", {"c"}),
}

# Uniformity rule sets (which sources count as warp-uniform). mv(x) follows x.
RULES = {
    # PR #624 helper table: 502 UNIFORM, 509 VARYING, 510 UNIFORM, 505/511 VARYING
    "pr624": {"c", "pid", "blk", "t", "warp"},
    # hardware-position ids are warp-invariant; globaltimer only if read converged
    "hw_strict": {"c", "pid", "blk", "sm", "warp"},
    "hw_timing_uniform": {"c", "pid", "blk", "sm", "warp", "t"},
}


def base(src):
    return src[3:-1] if src.startswith("mv(") else src


def aggregatable(srcs, uniform):
    return all(base(s) in uniform for s in srcs)


def aggregatable3(srcs):
    """Final design's reporting rule: lane-varying sources -> no; else any
    %globaltimer source -> timing; else yes. Hardware positions (509/510)
    are reported warp-invariant (grouping never relies on it)."""
    b = {base(s) for s in srcs}
    if b & {"tid", "lane"}:
        return "no"
    if "t" in b:
        return "timing"
    return "yes"


SLICE1 = {"A", "B", "J"}  # J only for its lookup (ADD_IF_PRESENT) unit


SCOPE_RANK = ["constant", "per_block", "per_warp", "per_thread"]
SCOPE_OF = {"c": "constant", "pid": "constant", "blk": "per_block", "sm": "per_block",
            "warp": "per_warp", "tid": "per_thread", "lane": "per_thread", "t": "per_thread"}


def key_scope(srcs):
    return max((SCOPE_OF[base(s)] for s in srcs), key=SCOPE_RANK.index)


# Cross-check of the oracle-free decision against the survey's own fields.
DECISION_EXPECT = {
    "defer_set": ({"overwrite"}, {"unused"}),
    "defer_del": ({"delete"}, {"unused"}),
    "resident_read": ({"read"}, {"value_used"}),
    "defer_out": ({"output"}, {"unused"}),
    "defer_unit": ({"accumulate", "insert_if_absent", "update_if_exists"},
                   {"pointer_rmw", "unused", "value_used"}),
}


def klass(decision, cpu_reads_map):
    """Survey class from the static decision and the external input."""
    if decision == "unchanged":
        return "gpu_local_already"
    if cpu_reads_map == "no":
        return "eliminate"
    if decision == "resident_read":
        return "sync"            # CPU-shared and the value is needed now
    return "defer"


def short(path):
    for marker in ("example/gpu/", "benchmark/gpu/"):
        if marker in path:
            return path.split(marker, 1)[1]
    return path


def main():
    d = json.load(open(sys.argv[1]))
    seen, rows = set(), []
    for r in d["results"]:
        if not r["key"].startswith("bpftime"):
            continue
        for x in r["verified"]["records"]:
            k = (x["file"], x["line"], x["section"])
            if k in seen:
                continue
            seen.add(k)
            rows.append(x)
    host = [x for x in rows if x["class"] in ("eliminate", "defer", "sync")]
    local = [x for x in rows if x["class"] == "gpu_local_already"]
    print("records", len(rows), "host-call", len(host), "gpu_local", len(local))
    out, mism = [], []
    agg = {name: Counter() for name in RULES}
    flips = {name: [] for name in RULES}
    for x in host:
        key = (short(x["file"]), x["line"])
        shape, role, srcs = S[key]
        decision = ROLE_DECISION[role]
        c = klass(decision, x["cpu_reads_map"])
        if c != x["class"]:
            mism.append((key, c, x["class"]))
        rec = {f: x.get(f) for f in ("file", "line", "section", "helper", "map", "map_type",
                                      "host_call_today", "result_use", "key_scope", "operation",
                                      "cpu_reads_map", "frequency")}
        rec.update({"idiom": shape, "decision": decision, "class": c})
        ops, uses = DECISION_EXPECT[decision]
        if x["operation"] not in ops or x["result_use"] not in uses:
            mism.append((key, "decision_vs_fields", decision, x["operation"], x["result_use"]))
        if role != "printk":
            ks = key_scope(srcs)
            if ks != x["key_scope"]:
                mism.append((key, "key_scope", ks, x["key_scope"]))
        rec["aggregatable3"] = aggregatable3(srcs)
        rec["slice1_rewritten"] = (decision == "defer_unit" and shape in SLICE1)
        for name, uni in RULES.items():
            a = aggregatable(srcs, uni)
            rec["aggregatable_" + name] = a
            agg[name][a] += 1
            if a != x["aggregatable"]:
                flips[name].append((key, x["aggregatable"], a))
        out.append(rec)
    for x in local:
        if x["class"] != klass("unchanged", x["cpu_reads_map"]):
            mism.append(((short(x["file"]), x["line"]), "?", x["class"]))
    print("class tally", Counter(r["class"] for r in out), "+ gpu_local_already", len(local))
    print("decision tally", Counter(r["decision"] for r in out))
    print("decision x (operation, result_use)", sorted(Counter((r["decision"], r["operation"], r["result_use"]) for r in out).items()))
    print("class mismatches vs survey:", mism)
    a3 = Counter(r["aggregatable3"] for r in out)
    print("aggregatable3 tally", dict(a3))
    sv = {(short(x["file"]), x["line"]): x["aggregatable"] for x in host}
    bad = [(short(r["file"]), r["line"], r["aggregatable3"]) for r in out
           if (r["aggregatable3"] == "yes") != sv[(short(r["file"]), r["line"])] and r["aggregatable3"] != "timing"]
    print("aggregatable3 yes/no records disagreeing with survey:", bad)
    print("timing records (survey value):", [(short(r["file"]), r["line"], sv[(short(r["file"]), r["line"])]) for r in out if r["aggregatable3"] == "timing"])
    print("slice-1 rewritten sites:", sum(r["slice1_rewritten"] for r in out),
          sorted((short(r["file"]), r["line"]) for r in out if r["slice1_rewritten"]))
    print("survey aggregatable:", Counter(x["aggregatable"] for x in host))
    for name in RULES:
        print(f"rule {name}: aggregatable {dict(agg[name])}; records differing from survey: {len(flips[name])}")
        for k, s, a in flips[name]:
            print(f"    {k[0]}:{k[1]} survey={s} rule={a}")
    json.dump(out, open(sys.argv[2], "w"), indent=1) if len(sys.argv) > 2 else None


if __name__ == "__main__":
    main()
