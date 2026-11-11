#!/usr/bin/env python3
"""Configs with the agreed 24-case subset (memory gpu-bench-case-subset, 2026-11-11):
gen_sub.py RESULTS_DIR RUN_ID [--level=info|debug] [--tree=candidate|official]
Victim: legacy-launch vec_add; micro probe: the 1512/1513 copy (compare/fix); examples: the
candidate tree's cuda-counter and kernelretsnoop, compare/devex's threadhist and mem_trace
(shipped probes; dev creates 1502/1503 as 1512/1513 on the Orin). --tree=official: that tree's
cuda-counter and kernelretsnoop, and compare/gate/official's threadhist and mem_trace copies asking
for 1512/1513 directly (official cannot create 1502/1503 on the Orin). Writes
/home/jetson/src/bpftime-gpu-work/sub/configs/RUN_ID.json, results under RESULTS_DIR/runs/RUN_ID/."""
import json, sys
from pathlib import Path
V = Path("/home/jetson/src/bpftime-gpu-work/compare")
G = Path("/home/jetson/src/bpftime-gpu-work/sub")
T = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e")
SRC = T / "official/benchmark/gpu/micro"
args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--"))
results, run_id = Path(args[0]), args[1]
victim = str(V / "victims/vec_add_legacy")
probe = str(V / "fix/benchmark/gpu/micro/cuda_probe")
base = json.loads((SRC / "micro_vec_add_config.json").read_text())
by = {c["name"]: c for c in base["test_cases"]}
by.setdefault("Baseline (minimal)", {"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"})
MICRO = ["Baseline (tiny)", "Baseline (small)", "Baseline (large)", "Baseline (xlarge)", "Baseline (minimal)",
         "Empty probe (tiny)", "Empty probe (xlarge)", "Global timer (xlarge)",
         "GPU Ringbuf (small)", "GPU Ringbuf (xlarge)", "Per-GPU-thread array (small)",
         "Memtrace (small)", "Memtrace (large)",
         "GPU Array map update (small)", "GPU Array map update (large)",
         "GPU Array map lookup (small)", "GPU Array map lookup (large)",
         "CPU Array map update (minimal)", "CPU Array map lookup (minimal)"]
cases = []
for n in MICRO:
    c = dict(by[n]); cmd = c.get("probe_binary_cmd", "")
    if cmd:
        c["probe_binary_cmd"] = probe + cmd[len("benchmark/gpu/micro/cuda_probe"):]
    cases.append(c)
tree = opts.get("tree", "candidate")
hostmaps = V / ("devex" if tree == "candidate" else "gate/official") / "example/gpu"
EX = [("CUDA Counter (minimal)", T / tree / "example/gpu/cuda-counter/cuda_probe"),
      ("Kernel Retsnoop (minimal)", T / tree / "example/gpu/kernelretsnoop/kernelretsnoop"),
      ("Thread Histogram (minimal)", hostmaps / "threadhist/threadhist"),
      ("Memory Trace (minimal)", hostmaps / "mem_trace/mem_trace")]
cases += [{"name": n, "probe_binary_cmd": str(p), "workload": "minimal", "baseline": "Baseline (minimal)"} for n, p in EX]
presets = {k: v.replace("benchmark/gpu/workload/vec_add", victim, 1) for k, v in base["workload_presets"].items()}
presets.setdefault("minimal", f"{victim} 32 3 32 1")
genv = {"SPDLOG_LEVEL": opts.get("level", "info"), "BPFTIME_GLOBAL_SHM_NAME": f"bpftime_cmp_s{run_id}"}
cfg = {"output_prefix": str(results / "runs" / run_id / "micro_vec_add"), "global_env": genv,
       "probe_env": base["probe_env"], "agent_env": base["agent_env"], "workload_presets": presets,
       "test_cases": cases}
out = G / "configs" / f"{run_id}.json"
(results / "runs" / run_id).mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(cfg, indent=2) + "\n")
print(out, len(cases), "cases")
