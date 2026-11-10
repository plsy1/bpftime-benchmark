#!/usr/bin/env python3
"""Generate run configs for the official GPU benchmark (benchmark/gpu/run_cuda_bench.py) on the Orin.

Usage: gen_configs.py RUN_ID TREE PROBE VICTIM KIND [--level info|debug] [--cases NAME,...]
  TREE   official | candidate  (whose example loaders; the build comes from the runner view's build/)
  PROBE  stock | fix           (micro cuda_probe asking for 1502/1503, or a copy asking for 1512/1513)
  VICTIM default | legacy      (vec_add built by nvcc 13.2 as the framework builds it, or with
                                -D__NV_LEGACY_LAUNCH; both -cudart shared)
  KIND   micro | examples | gate
Writes configs/RUN_ID-KIND.json. Paths are absolute, except probe_env/agent_env's build/... which
the runner view resolves. Every run gets its own shared memory segment name.
"""
import json
import sys
from pathlib import Path

V = Path("/home/jetson/src/bpftime-gpu-work/compare")
T = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e")
R = Path("/home/jetson/src/results/gpu/compare-20261110")
SRC = T / "official/benchmark/gpu/micro"

args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--"))
run_id, tree, probe, victim, kind = args
level = opts.get("level", "info")
only = set(opts["cases"].split(",")) if "cases" in opts else None

victim_bin = str(V / "victims" / ("vec_add" if victim == "default" else "vec_add_legacy"))
micro_probe = str(V / probe / "benchmark/gpu/micro/cuda_probe")
examples = {
    "example/gpu/cuda-counter/cuda_probe": str(T / tree / "example/gpu/cuda-counter/cuda_probe"),
    "example/gpu/kernelretsnoop/kernelretsnoop": str(T / tree / "example/gpu/kernelretsnoop/kernelretsnoop"),
    "example/gpu/mem_trace/mem_trace": str(V / ("devex" if tree == "candidate" else "offex") / "example/gpu/mem_trace/mem_trace"),
    "example/gpu/threadhist/threadhist": str(V / ("devex" if tree == "candidate" else "offex") / "example/gpu/threadhist/threadhist"),
}
SKIP_ALWAYS = {"eBPF Direct Run VecAdd (minimal)", "Launch Latency (minimal)"}
# Official writes past a 1024-thread per-thread map (shipped trampoline PTX prints a warning per
# thread and returns the out-of-range address): not run on official.
SKIP_OFFICIAL = {"Per-GPU-thread array (medium)", "Per-GPU-thread array (large)", "Per-GPU-thread array (xlarge)"}

if kind == "evidence":
    # Every micro mode and the two example loaders on the minimal geometry (32 threads, 3 timed
    # launches + 1 warm-up): at debug level official logs each redirected launch.
    base = json.loads((SRC / "micro_vec_add_config.json").read_text())
    modes = ["empty", "entry", "exit", "both", "ringbuf", "globaltimer", "pergputd-array-lookup", "memtrace",
             "gpu-array-update", "gpu-array-lookup", "array-update", "array-lookup", "hash-update", "hash-lookup",
             "hash-delete"]
    cases = [{"name": f"E {m} (minimal)", "probe_binary_cmd": f"{micro_probe} {m}", "workload": "minimal",
              "baseline": "Baseline (minimal)"} for m in modes]
    cases.insert(0, {"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"})
    cases += [{"name": "E CUDA Counter (minimal)", "probe_binary_cmd": examples["example/gpu/cuda-counter/cuda_probe"],
               "workload": "minimal", "baseline": "Baseline (minimal)"},
              {"name": "E Kernel Retsnoop (minimal)", "probe_binary_cmd": examples["example/gpu/kernelretsnoop/kernelretsnoop"],
               "workload": "minimal", "baseline": "Baseline (minimal)"}]
elif kind == "gate":
    # Host-memory GPU maps (1512, 1513) through the examples, with copies asking for them.
    base = json.loads((SRC / "examples_vec_add_config.json").read_text())
    gate = V / "gate" / tree / "example/gpu"
    cases = [
        {"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"},
        {"name": "Thread Histogram 1512 (minimal)", "probe_binary_cmd": str(gate / "threadhist/threadhist"),
         "workload": "minimal", "baseline": "Baseline (minimal)"},
        {"name": "Memory Trace 1513 (minimal)", "probe_binary_cmd": str(gate / "mem_trace/mem_trace"),
         "workload": "minimal", "baseline": "Baseline (minimal)"},
    ]
else:
    base = json.loads((SRC / f"{kind}_vec_add_config.json").read_text())
    cases = []
    for c in base["test_cases"]:
        if c["name"] in SKIP_ALWAYS:
            continue
        if tree == "official" and c["name"] in SKIP_OFFICIAL:
            continue
        c = dict(c)
        cmd = c.get("probe_binary_cmd", "")
        if cmd.startswith("benchmark/gpu/micro/cuda_probe"):
            c["probe_binary_cmd"] = micro_probe + cmd[len("benchmark/gpu/micro/cuda_probe"):]
        elif cmd:
            c["probe_binary_cmd"] = examples[cmd.split()[0]]
        cases.append(c)
    if kind == "micro":
        # Same-geometry denominator for the CPU map rows (upstream divides them by Baseline (tiny)).
        cases.insert(5, {"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"})

if only is not None:
    cases = [c for c in cases if c["name"] in only]

presets = {k: v.replace("benchmark/gpu/workload/vec_add", victim_bin, 1) for k, v in base["workload_presets"].items()}
presets.setdefault("minimal", f"{victim_bin} 32 3 32 1")
cfg = {
    "output_prefix": str(R / "runs" / run_id / f"{kind}_vec_add"),
    "global_env": {"SPDLOG_LEVEL": level, "BPFTIME_GLOBAL_SHM_NAME": f"bpftime_cmp_{run_id}"},
    "probe_env": base["probe_env"],
    "agent_env": base["agent_env"],
    "workload_presets": presets,
    "test_cases": cases,
}
out = V / "configs" / f"{run_id}-{kind}.json"
out.parent.mkdir(parents=True, exist_ok=True)
(R / "runs" / run_id).mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(cfg, indent=2) + "\n")
print(out, len(cases), "cases")
