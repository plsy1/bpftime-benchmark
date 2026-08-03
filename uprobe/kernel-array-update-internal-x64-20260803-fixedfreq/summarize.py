#!/usr/bin/env python3
import csv
import re
import statistics
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RAW = ROOT / "raw-perf"
NET_CYCLES_PER_HELPER = 70.167633762
FRAME_RE = re.compile(r"^\s*[0-9a-f]+\s+([^\s+]+)(?:\+0x[0-9a-f]+)?\s+\(")


def samples(path: Path):
    command = [
        "sudo", "-n", "perf", "script", "-f", "-i", str(path),
        "-F", "comm,pid,tid,time,event,ip,sym,symoff,dso",
    ]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, text=True)
    frames = []
    assert process.stdout is not None
    for line in process.stdout:
        if line.strip() == "":
            if frames:
                yield frames
                frames = []
            continue
        match = FRAME_RE.match(line)
        if match:
            frames.append(match.group(1))
    if frames:
        yield frames
    if process.wait() != 0:
        raise SystemExit(f"perf script failed for {path}")


def category(frames):
    top = frames[0]
    if top.startswith("bpf_prog_") and top.endswith("_kaus_8_real"):
        return "jit_real"
    if top.startswith("bpf_prog_") and top.endswith("_kaus_8_ctrl"):
        return "jit_ctrl"
    if top == "array_map_update_elem":
        return "array_map_update_elem_own"
    if top == "bpf_obj_memcpy":
        return "bpf_obj_memcpy_own"
    if top in {"memcpy", "memcpy_orig", "__memcpy", "__pi_memcpy"}:
        return "memcpy_own"
    if top == "bpf_obj_free_fields":
        return "bpf_obj_free_fields_own"
    if "array_map_update_elem" in frames:
        return "other_callee"
    return None


rows = []
for data in sorted(RAW.glob("rep*.perf.data")):
    rep = int(re.search(r"rep(\d+)", data.name).group(1))
    counts = Counter()
    total = 0
    for frames in samples(data):
        total += 1
        name = category(frames)
        if name:
            counts[name] += 1
    helper_names = (
        "array_map_update_elem_own",
        "bpf_obj_memcpy_own",
        "memcpy_own",
        "bpf_obj_free_fields_own",
        "other_callee",
    )
    helper_total = sum(counts[name] for name in helper_names)
    jit_net = counts["jit_real"] - counts["jit_ctrl"]
    net_total = helper_total + jit_net
    output_counts = {name: counts[name] for name in helper_names}
    output_counts["jit_real_minus_control"] = jit_net
    for name, count in output_counts.items():
        share = count / net_total if net_total else 0
        rows.append({
            "rep": rep,
            "total_samples": total,
            "helper_path_samples": helper_total,
            "jit_real_samples": counts["jit_real"],
            "jit_ctrl_samples": counts["jit_ctrl"],
            "net_attributed_samples": net_total,
            "category": name,
            "samples": count,
            "share_of_net_attributed": f"{share:.9f}",
            "estimated_cycles_per_helper": f"{share * NET_CYCLES_PER_HELPER:.9f}",
        })

with (ROOT / "internal-samples.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=rows[0].keys(), lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)

summary = []
for name in sorted({row["category"] for row in rows}):
    selected = [row for row in rows if row["category"] == name]
    shares = [float(row["share_of_net_attributed"]) for row in selected]
    cycles = [float(row["estimated_cycles_per_helper"]) for row in selected]
    summary.append({
        "category": name,
        "repetitions": len(selected),
        "mean_share": f"{statistics.mean(shares):.9f}",
        "stdev_share": f"{statistics.stdev(shares):.9f}",
        "mean_estimated_cycles_per_helper": f"{statistics.mean(cycles):.9f}",
        "stdev_estimated_cycles_per_helper": f"{statistics.stdev(cycles):.9f}",
    })

with (ROOT / "internal-summary.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=summary[0].keys(), lineterminator="\n")
    writer.writeheader()
    writer.writerows(summary)

ip_rows = []
ip_re = re.compile(r"^\s*[0-9a-f]+\s+array_map_update_elem\+0x([0-9a-f]+)")
for data in sorted(RAW.glob("rep*.perf.data")):
    rep = int(re.search(r"rep(\d+)", data.name).group(1))
    command = [
        "sudo", "-n", "perf", "script", "-f", "-i", str(data),
        "--max-stack", "1", "-F", "ip,sym,symoff",
    ]
    result = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE)
    offsets = Counter()
    for line in result.stdout.splitlines():
        match = ip_re.match(line)
        if match:
            offsets[int(match.group(1), 16)] += 1
    total = sum(offsets.values())
    for offset, count in sorted(offsets.items()):
        ip_rows.append({
            "rep": rep,
            "offset": f"0x{offset:x}",
            "samples": count,
            "share_of_array_symbol": f"{count / total:.9f}",
        })

with (ROOT / "array-map-update-ip.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=ip_rows[0].keys(), lineterminator="\n")
    writer.writeheader()
    writer.writerows(ip_rows)
