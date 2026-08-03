#!/usr/bin/env python3
import csv
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
UPROBE = HERE.parent


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(name, header, data):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(header)
        writer.writerows(data)


top_rows = rows(UPROBE / "uprobe-top-arm64-20260803/net-helper.csv")
top = {(r["environment"], r["case"]): r for r in top_rows}
kernel_rows = rows(UPROBE / "kernel-map-runtime-arm64-20260801/summary.csv")
kernel = {r["operation"]: r for r in kernel_rows}
array_direct_rows = rows(UPROBE / "array-path-arm-diagnosis-20260801/direct-layers.csv")
array_direct = {(r["operation"], r["layer"]): r for r in array_direct_rows}
array_jit_rows = rows(UPROBE / "array-path-arm-diagnosis-20260801/jit-helper-ab.csv")
array_jit = {r["operation"]: r for r in array_jit_rows if r["implementation"] == "array-minus-noop"}
array_jit_impl = {(r["operation"], r["implementation"]): r for r in array_jit_rows}
hash_direct_rows = rows(UPROBE / "hash-path-arm-diagnosis-20260801/direct-layers.csv")
hash_direct = {(r["operation"], r["layer"]): r for r in hash_direct_rows}
hash_jit_rows = rows(UPROBE / "hash-path-arm-diagnosis-20260801/jit-helper-ab.csv")
hash_jit = {r["operation"]: r for r in hash_jit_rows if r["implementation"] == "hash-minus-noop"}
hash_jit_impl = {(r["operation"], r["implementation"]): r for r in hash_jit_rows}

operations = {
    "array_lookup": {
        "case": "__bench_array_map_lookup",
        "direct": array_direct[("lookup", "l3")],
        "jit": array_jit["lookup"],
        "jit_sd": math.hypot(float(array_jit_impl[("lookup", "noop")]["stddev_ns_per_helper"]),
                             float(array_jit_impl[("lookup", "array")]["stddev_ns_per_helper"])),
    },
    "array_update": {
        "case": "__bench_array_map_update",
        "direct": array_direct[("update", "l3")],
        "jit": array_jit["update"],
        "jit_sd": math.hypot(float(array_jit_impl[("update", "noop")]["stddev_ns_per_helper"]),
                             float(array_jit_impl[("update", "array")]["stddev_ns_per_helper"])),
    },
    "hash_lookup": {
        "case": "__bench_hash_map_lookup",
        "direct": hash_direct[("lookup", "l3")],
        "jit": hash_jit["lookup"],
        "jit_sd": math.hypot(float(hash_jit_impl[("lookup", "noop")]["stddev_ns_per_helper"]),
                             float(hash_jit_impl[("lookup", "hash")]["stddev_ns_per_helper"])),
    },
}

kernel_pmu = {}
for operation in ("array_lookup", "hash_lookup"):
    for metric in ("cycles", "instructions"):
        directory = HERE / "raw-kernel-pmu" / f"{operation}-{metric}"
        control = json.loads((directory / "control.json").read_text())[0]
        real = json.loads((directory / "real.json").read_text())[0]
        assert control["run_cnt"] == real["run_cnt"] == 80000
        assert control["enabled"] == control["running"]
        assert real["enabled"] == real["running"]
        kernel_pmu[(operation, metric)] = (
            real["value"] / real["run_cnt"] - control["value"] / control["run_cnt"]
        ) / 1000.0

update_profile = rows(UPROBE / "kernel-map-runtime-arm64-20260803-profile/profile-summary.csv")
for row in update_profile:
    if row["metric"] in ("cycles", "instructions"):
        kernel_pmu[("array_update", row["metric"])] = float(row["real_minus_control_per_helper"])

kernel_pmu_output = []
for operation in operations:
    runtime_ns = float(kernel[operation]["mean_ns_per_helper"])
    for metric in ("cycles", "instructions"):
        value = kernel_pmu[(operation, metric)]
        kernel_pmu_output.append((
            operation, metric, f"{value:.9f}",
            "80000", "yes",
            f"{runtime_ns * 1.728:.9f}" if metric == "cycles" else "",
            "new pairwise profile" if operation != "array_update" else
            "existing kernel-map-runtime profile",
        ))
write_csv("kernel-pmu.csv",
          ("operation", "metric", "net_per_helper", "run_count_each",
           "enabled_equals_running", "nominal_cycles_from_wall", "source"),
          kernel_pmu_output)

closure_output = []
component_output = []
for operation, info in operations.items():
    case = info["case"]
    kernel_top = float(top[("kernel", case)]["median_net_ns_per_helper"])
    bpftime_top = float(top[("bpftime", case)]["median_net_ns_per_helper"])
    kernel_top_sd = float(top[("kernel", case)]["stdev_net_ns_per_helper"])
    bpftime_top_sd = float(top[("bpftime", case)]["stdev_net_ns_per_helper"])
    kernel_runtime = float(kernel[operation]["mean_ns_per_helper"])
    kernel_runtime_sd = float(kernel[operation]["stddev_ns_per_helper"])
    direct = float(info["direct"]["net_ns_vs_control"])
    direct_sd = float(info["direct"]["stddev_ns_per_op"])
    jit = float(info["jit"]["mean_ns_per_helper"])
    jit_sd = info["jit_sd"]

    top_gap = bpftime_top - kernel_top
    direct_gap = direct - kernel_runtime
    jit_context = jit - direct
    bpftime_top_integration = bpftime_top - jit
    kernel_top_integration = kernel_top - kernel_runtime
    integration_residual = bpftime_top_integration - kernel_top_integration
    residual_sd = math.sqrt(bpftime_top_sd ** 2 + kernel_top_sd ** 2 +
                            jit_sd ** 2 + kernel_runtime_sd ** 2)
    reconstructed = direct_gap + jit_context + integration_residual

    quantities = (
        ("kernel_top", kernel_top, kernel_top_sd, "official five-process median", "measured"),
        ("bpftime_top", bpftime_top, bpftime_top_sd, "official five-process median", "measured"),
        ("top_gap", top_gap, math.hypot(kernel_top_sd, bpftime_top_sd),
         "bpftime_top - kernel_top", "inferred compatible subtraction"),
        ("kernel_runtime", kernel_runtime, kernel_runtime_sd,
         "matched kernel real-control mean", "measured"),
        ("bpftime_direct_l3", direct, direct_sd,
         "direct L3-control mean", "measured"),
        ("direct_helper_gap", direct_gap, math.hypot(direct_sd, kernel_runtime_sd),
         "bpftime_direct_l3 - kernel_runtime", "inferred separate-run subtraction"),
        ("bpftime_jit_helper", jit, jit_sd,
         "real-helper minus byte-identical no-op-helper", "measured"),
        ("jit_context_premium", jit_context, math.hypot(jit_sd, direct_sd),
         "bpftime_jit_helper - bpftime_direct_l3", "inferred separate-run subtraction"),
        ("bpftime_top_above_jit", bpftime_top_integration,
         math.hypot(bpftime_top_sd, jit_sd), "bpftime_top - bpftime_jit_helper",
         "inferred separate-run subtraction"),
        ("kernel_top_above_runtime", kernel_top_integration,
         math.hypot(kernel_top_sd, kernel_runtime_sd), "kernel_top - kernel_runtime",
         "inferred separate-run subtraction"),
        ("top_integration_residual", integration_residual, residual_sd,
         "(bpftime_top-jit) - (kernel_top-runtime)",
         "inferred; not internally localized"),
        ("reconstructed_top_gap", reconstructed, residual_sd,
         "direct_helper_gap + jit_context_premium + top_integration_residual",
         "algebraic closure"),
        ("closure_error", reconstructed - top_gap, 0.0,
         "reconstructed_top_gap - top_gap", "algebraic check"),
    )
    for quantity, value, uncertainty, formula, classification in quantities:
        closure_output.append((operation, quantity, f"{value:.9f}",
                               f"{uncertainty:.9f}", formula, classification))

    component_output.extend((
        (operation, "kernel_runtime_denominator", "", f"{-kernel_runtime:.9f}",
         f"{-kernel_pmu[(operation, 'cycles')]:.9f}",
         f"{-kernel_pmu[(operation, 'instructions')]:.9f}",
         "kernel work subtracted from BPFtime helper path", "measured then sign-inverted"),
        (operation, "jit_context_premium", "", f"{jit_context:.9f}",
         f"{float(info['jit']['delta_cycles_per_helper']) - float(info['direct']['net_cycles_vs_control']):.9f}",
         f"{float(info['jit']['delta_instructions_per_helper']) - float(info['direct']['net_instructions_vs_control']):.9f}",
         "difference between JIT helper A/B and direct L3", "inferred separate-run subtraction"),
        (operation, "top_integration_residual", "", f"{integration_residual:.9f}", "", "",
         "remaining top-level difference after helper and JIT boundaries",
         "inferred; not internally localized"),
    ))

write_csv("closure.csv",
          ("operation", "quantity", "value_ns_per_helper", "uncertainty_sample_sd_ns",
           "formula", "evidence_class"), closure_output)


def add_direct_layers(operation, direct_rows, stages):
    previous = 0.0
    previous_cycles = 0.0
    previous_instructions = 0.0
    output = []
    for component, row, role in stages:
        cumulative = float(row["net_ns_vs_control"])
        cycles = float(row["net_cycles_vs_control"])
        instructions = float(row["net_instructions_vs_control"])
        output.append((operation, component, f"{cumulative:.9f}",
                       f"{cumulative - previous:.9f}",
                       f"{cycles - previous_cycles:.9f}",
                       f"{instructions - previous_instructions:.9f}",
                       role, "measured cumulative; adjacent delta inferred"))
        previous, previous_cycles, previous_instructions = cumulative, cycles, instructions
    return output


layer_output = []
for operation, op in (("array_lookup", "lookup"), ("array_update", "update")):
    layer_output += add_direct_layers(operation, array_direct, (
        ("map_implementation", array_direct[(op, "l0")],
         "array bounds/address and lookup or 8-byte update work"),
        ("generic_handler", array_direct[(op, "l1")],
         "map-type dispatch, offset_ptr and lock-policy checks"),
        ("shm_fd_variant_dispatch", array_direct[(op, "l2")],
         "fd bounds and handler variant extraction"),
        ("lto_shaped_final_helper", array_direct[(op, "l3")],
         "production helper after whole-path LTO reshaping"),
    ))

hash_lock = hash_direct[("lookup", "lock")]
hash_l0 = hash_direct[("lookup", "l0")]
hash_l1 = hash_direct[("lookup", "l1")]
hash_l2 = hash_direct[("lookup", "l2")]
hash_l3 = hash_direct[("lookup", "l3")]
lock_ns = float(hash_lock["net_ns_vs_control"])
lock_cycles = float(hash_lock["net_cycles_vs_control"])
lock_ins = float(hash_lock["net_instructions_vs_control"])
hash_stages = (
    ("spin_lock", lock_ns, lock_cycles, lock_ins,
     "uncontended lock-only reference; directional component"),
    ("hash_map_body_after_lock", float(hash_l0["net_ns_vs_control"]) - lock_ns,
     float(hash_l0["net_cycles_vs_control"]) - lock_cycles,
     float(hash_l0["net_instructions_vs_control"]) - lock_ins,
     "hash, modulo, probing, key compare and value access after lock estimate"),
    ("generic_handler", float(hash_l1["net_ns_vs_control"]) - float(hash_l0["net_ns_vs_control"]),
     float(hash_l1["net_cycles_vs_control"]) - float(hash_l0["net_cycles_vs_control"]),
     float(hash_l1["net_instructions_vs_control"]) - float(hash_l0["net_instructions_vs_control"]),
     "map-type/offset_ptr and handler path"),
    ("shm_fd_variant_dispatch", float(hash_l2["net_ns_vs_control"]) - float(hash_l1["net_ns_vs_control"]),
     float(hash_l2["net_cycles_vs_control"]) - float(hash_l1["net_cycles_vs_control"]),
     float(hash_l2["net_instructions_vs_control"]) - float(hash_l1["net_instructions_vs_control"]),
     "fd bounds and handler variant extraction"),
    ("lto_shaped_final_helper", float(hash_l3["net_ns_vs_control"]) - float(hash_l2["net_ns_vs_control"]),
     float(hash_l3["net_cycles_vs_control"]) - float(hash_l2["net_cycles_vs_control"]),
     float(hash_l3["net_instructions_vs_control"]) - float(hash_l2["net_instructions_vs_control"]),
     "production helper after whole-path LTO reshaping"),
)
cumulative_ns = cumulative_cycles = cumulative_ins = 0.0
for component, ns, cycles, instructions, role in hash_stages:
    cumulative_ns += ns
    cumulative_cycles += cycles
    cumulative_ins += instructions
    layer_output.append(("hash_lookup", component, f"{cumulative_ns:.9f}",
                         f"{ns:.9f}", f"{cycles:.9f}", f"{instructions:.9f}",
                         role, "measured references; lock/body split is directional"))

layer_output.extend(component_output)
write_csv("layer-attribution.csv",
          ("operation", "component", "cumulative_bpftime_ns", "incremental_ns",
           "incremental_cycles", "incremental_instructions", "role", "evidence_class"),
          layer_output)

compatibility = (
    ("official_top", "uprobe-top-arm64-20260803", "8ed291e", "production uprobe programs; 1000 helpers; hit/existing",
     "same-process empty uprobe subtraction", "RelWithDebInfo; LLVM15; GCC13; Boost1.83; JIT+LTO",
     "definitive top boundary"),
    ("array_direct_layers", "array-path-arm-diagnosis-20260801", "ead56c9 + diagnostic dirty tree later committed as 8ed291e",
     "1024-entry array; 4B key; 8B value; keys 0..999", "matched direct-call control",
     "same production runtime sources and build flags", "compatible separate-run helper attribution"),
    ("array_jit_ab", "array-path-arm-diagnosis-20260801", "ead56c9 + diagnostic dirty tree later committed as 8ed291e",
     "byte-identical JIT program; helper address only differs", "no-op helper subtraction",
     "same production runtime sources and build flags", "compatible JIT helper boundary"),
    ("hash_direct_layers", "hash-path-arm-diagnosis-20260801", "ead56c9 + diagnostic dirty tree later committed as 8ed291e",
     "1024-slot hash; 1000 keys; insertion order 0..999; lookup-hit", "matched direct-call control",
     "same production runtime sources and build flags", "compatible separate-run helper attribution"),
    ("hash_jit_ab", "hash-path-arm-diagnosis-20260801", "ead56c9 + diagnostic dirty tree later committed as 8ed291e",
     "same map load, insertion order and keys as top-level lookup", "no-op helper subtraction",
     "same production runtime sources and build flags", "compatible helper boundary; top residual remains unlocalized"),
    ("kernel_runtime_wall", "kernel-map-runtime-arm64-20260801", "ead56c9 + diagnostic dirty tree later committed as 8ed291e",
     "same 1024 maps, 4B key, 8B value, hit/existing", "matched BPF control/real",
     "Clang15; GCC13; CPU5 locked", "compatible kernel runtime denominator"),
    ("kernel_lookup_pmu", "arm64-bpftime-vs-kernel-paths-20260803/raw-kernel-pmu", "176eb291",
     "unchanged kernel_map_runtime source from 8ed291e", "same matched BPF control/real",
     "Clang15; GCC13; CPU5 locked; no multiplex", "directly compatible with kernel runtime wall harness"),
    ("kernel_value_size", "kernel-array-update-sizes-arm64-20260803", "176eb291",
     "fixed-struct value-size sweep with different BPF source/control", "matched within sweep only",
     "Clang15; GCC13; CPU5 locked", "supporting trend only; not a closure denominator"),
)
write_csv("data-compatibility.csv",
          ("dataset", "source_path", "recorded_commit", "program_semantics",
           "control", "build", "compatibility_decision"), compatibility)
