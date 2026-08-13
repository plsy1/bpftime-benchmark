#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PRIOR = ROOT.parent / "relative-kernel-attribution-arm64-20260812" / "top-gap.csv"

gaps = {r["operation"]: float(r["gap_ns_per_helper"]) for r in csv.DictReader(PRIOR.open())}
pmu = {}
for r in csv.DictReader((ROOT / "pmu-effects.csv").open()):
    pmu[(r["case"], r["operation"], r["metric"])] = float(r["mean_saving_per_helper"])

case_by_operation = {
    "ordinary array lookup": "array_lookup",
    "ordinary array update": "array_update",
    "per-CPU array lookup": "percpu_array_lookup",
    "per-CPU array update": "percpu_array_update",
    "per-CPU hash lookup": "percpu_hash_lookup",
    "per-CPU hash update": "percpu_hash_update",
}
scope = {
    "SHM fd/variant lookup": "BPFtime-only shared-memory runtime machinery",
    "generic handler dispatch": "BPFtime-only generic userspace dispatch",
    "std::function wrapper": "BPFtime-only implementation machinery",
    "sched_getcpu": "logical CPU selection exists in both; measured userspace mechanism is BPFtime-specific",
    "8-byte value copy": "logical value copy exists in both; measured userspace copy is BPFtime's implementation",
    "Boost hash/find": "logical hash lookup exists in both; Boost.Interprocess implementation is BPFtime-specific",
    "existing value copy": "logical update copy exists in both; measured shared-memory vector copy is BPFtime-specific",
}

fields = ("operation", "prior_strict_gap_ns_per_helper", "concrete_operation",
          "production_ab_saving_ns_per_helper", "effect_as_percent_of_prior_gap",
          "pmu_cycles_saving_per_helper", "pmu_instructions_saving_per_helper",
          "mechanism_scope")
with (ROOT / "attribution.csv").open("w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
    w.writeheader()
    for r in csv.DictReader((ROOT / "effects.csv").open()):
        operation, label = r["operation"], r["concrete_operation"]
        effect, gap = float(r["mean_saving_ns_per_helper"]), gaps[operation]
        case = case_by_operation[operation]
        w.writerow({
            "operation": operation,
            "prior_strict_gap_ns_per_helper": f"{gap:.9f}",
            "concrete_operation": label,
            "production_ab_saving_ns_per_helper": f"{effect:.9f}",
            "effect_as_percent_of_prior_gap": f"{effect / gap * 100:.3f}",
            "pmu_cycles_saving_per_helper": f"{pmu[(case, label, 'cycles')]:.9f}",
            "pmu_instructions_saving_per_helper": f"{pmu[(case, label, 'instructions')]:.9f}",
            "mechanism_scope": scope[label],
        })
    # The corrected delete top-level comparison came from the earlier formal
    # run (981.38 BPFtime versus 150.21 kernel ns/helper).  Keep the provenance
    # explicit instead of pretending it was measured concurrently here.
    effect, gap = 786.5879488, 981.38 - 150.21
    w.writerow({
        "operation": "per-CPU hash delete-hit",
        "prior_strict_gap_ns_per_helper": f"{gap:.9f}",
        "concrete_operation": "synchronous vector/node destruction and SHM reclamation",
        "production_ab_saving_ns_per_helper": f"{effect:.9f}",
        "effect_as_percent_of_prior_gap": f"{effect / gap * 100:.3f}",
        "pmu_cycles_saving_per_helper": "see prior delete-layer PMU",
        "pmu_instructions_saving_per_helper": "see prior delete-layer PMU",
        "mechanism_scope": "BPFtime-only Boost.Interprocess destruction/reclamation; kernel uses preallocation/freelist",
    })

print((ROOT / "attribution.csv").read_text(), end="")
