#!/usr/bin/env python3
"""Matched BPFtime A/B across per-CPU hash loads and key sizes."""

import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import time


ROOT = Path("/home/jetson/src/bpftime")
OUT = Path(__file__).resolve().parent
CPU = 5
RUNS = 5
ITERATIONS = 3000
WARMUP = 200
CASES = ("empty", "lookup_control", "update_control", "hash_lookup",
         "hash_update", "percpu_hash_lookup", "percpu_hash_update")
CONFIGS = (
    ("lf64", 64, 1024, 4, 8),
    ("lf512", 512, 1024, 4, 8),
    ("lf1000", 1000, 1024, 4, 8),
    ("key16", 512, 1024, 16, 8),
    ("key64", 512, 1024, 64, 8),
)
BUILDS = {
    "baseline": ROOT / "build-uprobe-hash-equal-opt-20261013",
    "candidate": ROOT / "build-uprobe-hash-fast4-opt-20261013",
}
TOOL = BUILDS["baseline"] / "tools/bpftimetool/bpftimetool"


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def sample_frequency(samples):
    base = Path(f"/sys/devices/system/cpu/cpu{CPU}/cpufreq")
    current = {name: int((base / f"scaling_{name}_freq").read_text())
               for name in ("min", "max", "cur")}
    if current != {"min": 1728000, "max": 1728000, "cur": 1728000}:
        raise RuntimeError(f"CPU frequency is not locked: {current}")
    samples.append(current)


def build_config(tag, active, maximum, key_size, value_size):
    full_tag = f"fast4-validation-{tag}"
    command = [
        "/usr/bin/make", "-C", str(ROOT / "benchmark/uprobe"),
        "hash-workload-sweep-diagnostic", f"HWS_TAG={full_tag}",
        f"HWS_ACTIVE_KEYS={active}", f"HWS_MAX_ENTRIES={maximum}",
        f"HWS_KEY_SIZE={key_size}", f"HWS_VALUE_SIZE={value_size}",
        "CLANG=/usr/lib/llvm-15/bin/clang", "CC=/usr/bin/gcc-13",
    ]
    with (OUT / f"build-{tag}.log").open("w") as log:
        run(command, stdout=log, stderr=subprocess.STDOUT)
    return ROOT / "benchmark/uprobe/.output" / f"hash-workload-sweep-{full_tag}"


def main():
    run(["/usr/bin/sudo", "-n", "true"])
    if Path("/dev/shm/bpftime_maps_shm").exists():
        raise RuntimeError("BPFtime shared memory already exists")

    dirs = {config[0]: build_config(*config) for config in CONFIGS}
    artifacts = [TOOL]
    for build in BUILDS.values():
        artifacts += [build / "runtime/agent/libbpftime-agent.so",
                      build / "runtime/syscall-server/libbpftime-syscall-server.so"]
    for directory in dirs.values():
        artifacts += [directory / "hash-workload-sweep",
                      directory / "hash-workload-sweep-victim",
                      directory / "hash_workload_sweep.bpf.o"]
    hashes = {str(path): digest(path) for path in artifacts}
    environment = os.environ.copy()
    for key in list(environment):
        if key.startswith("BPFTIME_") or key == "LD_PRELOAD":
            del environment[key]
    environment.update(BPFTIME_VM_NAME="llvm", SPDLOG_LEVEL="info",
                       BPFTIME_LOG_OUTPUT="console")
    metadata = {
        "started": datetime.datetime.now().astimezone().isoformat(),
        "source_commit": run(["git", "rev-parse", "HEAD"], capture_output=True,
                             text=True).stdout.strip(),
        "configs": CONFIGS, "runs": RUNS, "iterations": ITERATIONS,
        "warmup": WARMUP, "cpu": CPU, "binary_sha256": hashes,
        "status": "running",
    }
    (OUT / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (OUT / "source.diff").write_text(run(
        ["git", "diff", "--", "runtime"], capture_output=True,
        text=True).stdout)
    rows = []
    frequencies = []

    def measure(tag, engine, round_number):
        sample_frequency(frequencies)
        directory = dirs[tag]
        loader = directory / "hash-workload-sweep"
        victim = directory / "hash-workload-sweep-victim"
        raw = OUT / "raw" / tag / f"{engine}-{round_number:02d}"
        raw.mkdir(parents=True)
        build = BUILDS[engine]
        loader_env = environment.copy()
        loader_env["LD_PRELOAD"] = str(
            build / "runtime/syscall-server/libbpftime-syscall-server.so")
        victim_env = environment.copy()
        victim_env["LD_PRELOAD"] = str(
            build / "runtime/agent/libbpftime-agent.so")

        def target_env(selected):
            return ["/usr/bin/env", *[
                f"{key}={value}" for key, value in selected.items()
                if key in ("LD_PRELOAD", "BPFTIME_VM_NAME", "SPDLOG_LEVEL",
                           "BPFTIME_LOG_OUTPUT")]]

        command = ["/usr/bin/sudo", "-n", "/usr/bin/taskset", "-c", str(CPU),
                   *target_env(loader_env), str(loader)]
        with (raw / "loader.log").open("w") as loader_log:
            proc = subprocess.Popen(command, cwd=ROOT, stdout=loader_log,
                                    stderr=subprocess.STDOUT, env=environment,
                                    start_new_session=True)
            try:
                deadline = time.monotonic() + 30
                while "Successfully started!" not in (raw / "loader.log").read_text():
                    if proc.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError(f"loader failed: {raw / 'loader.log'}")
                    time.sleep(0.1)
                time.sleep(1)
                victim_command = [
                    "/usr/bin/sudo", "-n", "/usr/bin/taskset", "-c", str(CPU),
                    *target_env(victim_env), str(victim), str(ITERATIONS),
                    str(round_number % len(CASES)), str(WARMUP),
                ]
                with (raw / "victim.log").open("w") as victim_log:
                    run(victim_command, stdout=victim_log,
                        stderr=subprocess.STDOUT, env=environment, timeout=300)
                output = (raw / "victim.log").read_text()
                if "shm_open_type 1" not in output:
                    raise RuntimeError("userspace agent was not confirmed")
                values = {}
                for line in output.splitlines():
                    fields = line.split(",")
                    if len(fields) == 5 and fields[0] in CASES:
                        values[fields[0]] = float(fields[4])
                if set(values) != set(CASES) or any(
                        not math.isfinite(value) or value <= 0
                        for value in values.values()):
                    raise RuntimeError(f"incomplete timings: {values}")
                for case, raw_value in values.items():
                    control = ("lookup_control" if case.endswith("lookup")
                               else "update_control" if case.endswith("update")
                               else None)
                    rows.append({
                        "config": tag, "engine": engine,
                        "round": round_number, "case": case,
                        "raw_ns_per_invocation": raw_value,
                        "net_ns_per_helper": ((raw_value - values[control]) / 1000
                                              if control else None),
                    })
                sample_frequency(frequencies)
            finally:
                if proc.poll() is None:
                    run(["/usr/bin/sudo", "-n", "/usr/bin/kill", "-TERM",
                         str(proc.pid)])
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        run(["/usr/bin/sudo", "-n", "/usr/bin/kill", "-KILL",
                             str(proc.pid)])
                        proc.wait()
                run(["/usr/bin/sudo", "-n", str(TOOL), "remove"],
                    stdout=subprocess.DEVNULL)
        print(f"completed {tag} {engine} round {round_number}/{RUNS}", flush=True)

    try:
        time.sleep(5)
        for tag, *_ in CONFIGS:
            for index in range(RUNS):
                order = (("baseline", "candidate") if index % 2 == 0
                         else ("candidate", "baseline"))
                for engine in order:
                    measure(tag, engine, index + 1)
        if hashes != {str(path): digest(path) for path in artifacts}:
            raise RuntimeError("an input binary changed during the experiment")
        with (OUT / "wall-raw.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)
        summary = []
        for tag, *_ in CONFIGS:
            for case in ("hash_lookup", "hash_update", "percpu_hash_lookup",
                         "percpu_hash_update"):
                item = {"config": tag, "case": case}
                for engine in BUILDS:
                    values = [row["net_ns_per_helper"] for row in rows
                              if row["config"] == tag and row["case"] == case
                              and row["engine"] == engine]
                    item[engine] = {
                        "median": statistics.median(values),
                        "mean": statistics.mean(values),
                        "stdev": statistics.stdev(values),
                        "raw_values": values,
                    }
                base = item["baseline"]["median"]
                item["reduction_percent"] = (
                    (base - item["candidate"]["median"]) / base * 100)
                summary.append(item)
        (OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        metadata.update(status="success",
                        finished=datetime.datetime.now().astimezone().isoformat(),
                        frequency_samples=frequencies)
        print(json.dumps(summary, indent=2))
    except BaseException as error:
        metadata.update(status="failed", error=repr(error),
                        finished=datetime.datetime.now().astimezone().isoformat(),
                        frequency_samples=frequencies)
        raise
    finally:
        (OUT / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n")
        if Path("/dev/shm/bpftime_maps_shm").exists():
            run(["/usr/bin/sudo", "-n", str(TOOL), "remove"],
                stdout=subprocess.DEVNULL)


if __name__ == "__main__":
    main()
