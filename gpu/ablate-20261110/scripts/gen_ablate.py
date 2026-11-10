#!/usr/bin/env python3
"""Configs for the ablation rounds: gen_ablate.py RUN_ID TREE KIND [--level=] [--env=K=V,...]
TREE official|candidate (whose kernelretsnoop loader); KIND micro|examples|smoke.
All builds use the legacy-launch victim and the 1512/1513 micro probe copy, as sets A/B of
compare-20261110. Writes configs/RUN_ID-KIND.json under ablate/, results under
results/gpu/ablate-20261110/runs/RUN_ID/."""
import json, sys
from pathlib import Path
V = Path("/home/jetson/src/bpftime-gpu-work/compare")
A = Path("/home/jetson/src/bpftime-gpu-work/ablate")
T = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e")
R = Path("/home/jetson/src/results/gpu/ablate-20261110")
SRC = T / "official/benchmark/gpu/micro"
args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--"))
run_id, tree, kind = args
env = dict(kv.split("=", 1) for kv in opts["env"].split(",")) if opts.get("env") else {}
victim = str(V / "victims/vec_add_legacy")
probe = str(V / "fix/benchmark/gpu/micro/cuda_probe")
retsnoop = str(T / tree / "example/gpu/kernelretsnoop/kernelretsnoop")
MICRO = ["Baseline (tiny)", "Baseline (small)", "Baseline (medium)", "Baseline (large)", "Baseline (xlarge)",
         "Entry+Exit (small_64x16)", "Entry+Exit (small)", "Entry+Exit (large)",
         "Empty probe (large)", "Empty probe (xlarge)", "Entry probe (xlarge)",
         "Memtrace (tiny)", "Memtrace (large)", "GPU Array map lookup (xlarge)",
         "Per-GPU-thread array (tiny)", "Per-GPU-thread array (small_64x16)",
         "GPU Array map update (small_64x16)", "GPU Array map update (small)",
         "GPU Array map update (large)", "GPU Array map update (xlarge)",
         "GPU Ringbuf (small_64x16)", "GPU Ringbuf (small)", "GPU Ringbuf (medium)"]
if kind == "smoke":
    base = json.loads((SRC / "micro_vec_add_config.json").read_text())
    modes = ["both", "memtrace", "pergputd-array-lookup", "gpu-array-update", "ringbuf", "empty"]
    cases = [{"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"}]
    cases += [{"name": f"S {m} (minimal)", "probe_binary_cmd": f"{probe} {m}", "workload": "minimal",
               "baseline": "Baseline (minimal)"} for m in modes]
    cases.append({"name": "S Kernel Retsnoop (minimal)", "probe_binary_cmd": retsnoop, "workload": "minimal",
                  "baseline": "Baseline (minimal)"})
elif kind == "micro":
    base = json.loads((SRC / "micro_vec_add_config.json").read_text())
    by = {c["name"]: c for c in base["test_cases"]}
    cases = []
    for n in MICRO:
        c = dict(by[n]); cmd = c.get("probe_binary_cmd", "")
        if cmd:
            c["probe_binary_cmd"] = probe + cmd[len("benchmark/gpu/micro/cuda_probe"):]
        cases.append(c)
else:
    base = json.loads((SRC / "examples_vec_add_config.json").read_text())
    cases = [{"name": "Baseline (minimal)", "probe_binary_cmd": "", "workload": "minimal"},
             {"name": "Kernel Retsnoop (minimal)", "probe_binary_cmd": retsnoop, "workload": "minimal",
              "baseline": "Baseline (minimal)"}]
presets = {k: v.replace("benchmark/gpu/workload/vec_add", victim, 1) for k, v in base["workload_presets"].items()}
presets.setdefault("minimal", f"{victim} 32 3 32 1")
genv = {"SPDLOG_LEVEL": opts.get("level", "info"), "BPFTIME_GLOBAL_SHM_NAME": f"bpftime_cmp_{run_id}"}
genv.update(env)
cfg = {"output_prefix": str(R / "runs" / run_id / f"{kind}_vec_add"), "global_env": genv,
       "probe_env": base["probe_env"], "agent_env": base["agent_env"], "workload_presets": presets,
       "test_cases": cases}
out = A / "configs" / f"{run_id}-{kind}.json"
out.parent.mkdir(parents=True, exist_ok=True); (R / "runs" / run_id).mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(cfg, indent=2) + "\n")
print(out, len(cases), "cases")
