#!/usr/bin/env python3
"""Configs for the host-call layer-1 measurements: gen_l1.py RUN_ID [--level=info|debug]
Same victim (legacy launch), 1512/1513 micro probe copy and presets as compare-20261110; the
cases the change touches (CPU array maps, CUDA Counter) and controls. Writes
/home/jetson/src/bpftime-gpu-work/l1/configs/RUN_ID.json, results under
results/gpu/hostcall-l1-20261111/runs/RUN_ID/."""
import json, sys
from pathlib import Path
V = Path("/home/jetson/src/bpftime-gpu-work/compare")
L = Path("/home/jetson/src/bpftime-gpu-work/l1")
T = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e")
R = Path("/home/jetson/src/results/gpu/hostcall-l1-20261111")
SRC = T / "official/benchmark/gpu/micro"
args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--"))
run_id, = args
victim = str(V / "victims/vec_add_legacy")
probe = str(V / "fix/benchmark/gpu/micro/cuda_probe")
counter = str(T / "candidate/example/gpu/cuda-counter/cuda_probe")
retsnoop = str(T / "candidate/example/gpu/kernelretsnoop/kernelretsnoop")
base = json.loads((SRC / "micro_vec_add_config.json").read_text())
by = {c["name"]: c for c in base["test_cases"]}
by.setdefault("Baseline (minimal)", {"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"})
MICRO = ["Baseline (tiny)", "Baseline (small)", "Baseline (minimal)",
         "CPU Array map update (minimal)", "CPU Array map lookup (minimal)",
         "CPU Hash map update (minimal)", "CPU Hash map lookup (minimal)", "CPU Hash map delete (minimal)",
         "Empty probe (tiny)", "GPU Array map update (small)"]
cases = []
for n in MICRO:
    c = dict(by[n]); cmd = c.get("probe_binary_cmd", "")
    if cmd:
        c["probe_binary_cmd"] = probe + cmd[len("benchmark/gpu/micro/cuda_probe"):]
    cases.append(c)
cases += [{"name": "CUDA Counter (minimal)", "probe_binary_cmd": counter, "workload": "minimal",
           "baseline": "Baseline (minimal)"},
          {"name": "Kernel Retsnoop (minimal)", "probe_binary_cmd": retsnoop, "workload": "minimal",
           "baseline": "Baseline (minimal)"}]
presets = {k: v.replace("benchmark/gpu/workload/vec_add", victim, 1) for k, v in base["workload_presets"].items()}
presets.setdefault("minimal", f"{victim} 32 3 32 1")
genv = {"SPDLOG_LEVEL": opts.get("level", "info"), "BPFTIME_GLOBAL_SHM_NAME": f"bpftime_cmp_l{run_id}"}
cfg = {"output_prefix": str(R / "runs" / run_id / "micro_vec_add"), "global_env": genv,
       "probe_env": base["probe_env"], "agent_env": base["agent_env"], "workload_presets": presets,
       "test_cases": cases}
out = L / "configs" / f"{run_id}.json"
(R / "runs" / run_id).mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(cfg, indent=2) + "\n")
print(out, len(cases), "cases")
