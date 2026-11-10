#!/usr/bin/env python3
"""Configs for the array-update-xlarge experiments: gen_x.py RUN_ID [--env=K=V,...] [--cases=a|b]
Same victim (legacy launch), 1512/1513 probe copy and presets as ablate-20261110; only the cases
below. Writes /home/jetson/src/bpftime-gpu-work/xlarge/configs/RUN_ID.json, results under
results/gpu/xlarge-20261110/runs/RUN_ID/."""
import json, sys
from pathlib import Path
V = Path("/home/jetson/src/bpftime-gpu-work/compare")
X = Path("/home/jetson/src/bpftime-gpu-work/xlarge")
T = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e")
R = Path("/home/jetson/src/results/gpu/xlarge-20261110")
SRC = T / "official/benchmark/gpu/micro"
args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--"))
run_id, = args
env = dict(kv.split("=", 1) for kv in opts["env"].split(",")) if opts.get("env") else {}
victim = str(V / "victims/vec_add_legacy")
probe = str(V / "fix/benchmark/gpu/micro/cuda_probe")
CASES = opts.get("cases", "Baseline (large)|Baseline (xlarge)|GPU Array map update (large)|"
                 "GPU Array map update (xlarge)|GPU Array map lookup (xlarge)").split("|")
base = json.loads((SRC / "micro_vec_add_config.json").read_text())
by = {c["name"]: c for c in base["test_cases"]}
# extra geometry: the xlarge grid (512 x 1954) with N=32, so almost every thread skips the add
# but still runs the retprobe (it is called after the bounds branch)
base["workload_presets"]["xlarge_n32"] = "benchmark/gpu/workload/vec_add 32 1000 512 1954"
by["Baseline (xlarge_n32)"] = {"name": "Baseline (xlarge_n32)", "probe_binary_cmd": "", "workload": "xlarge_n32"}
by["GPU Array map update (xlarge_n32)"] = dict(by["GPU Array map update (xlarge)"], name="GPU Array map update (xlarge_n32)",
                                               workload="xlarge_n32", baseline="Baseline (xlarge_n32)")
cases = []
for n in CASES:
    c = dict(by[n]); cmd = c.get("probe_binary_cmd", "")
    if cmd:
        c["probe_binary_cmd"] = probe + cmd[len("benchmark/gpu/micro/cuda_probe"):]
    cases.append(c)
presets = {k: v.replace("benchmark/gpu/workload/vec_add", victim, 1) for k, v in base["workload_presets"].items()}
genv = {"SPDLOG_LEVEL": opts.get("level", "info"), "BPFTIME_GLOBAL_SHM_NAME": f"bpftime_cmp_x{run_id}"}
genv.update(env)
cfg = {"output_prefix": str(R / "runs" / run_id / "micro_vec_add"), "global_env": genv,
       "probe_env": base["probe_env"], "agent_env": base["agent_env"], "workload_presets": presets,
       "test_cases": cases}
out = X / "configs" / f"{run_id}.json"
(R / "runs" / run_id).mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(cfg, indent=2) + "\n")
print(out, len(cases), "cases")
