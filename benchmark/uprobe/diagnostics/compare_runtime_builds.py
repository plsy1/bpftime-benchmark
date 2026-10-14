#!/usr/bin/env python3
"""Paired, production-mode map helper measurements for two runtime builds.

Build the selected helper closure diagnostic before running this script. The
existing operation-specific controls and unmodified BPF object are shared by
all engines. Run as the normal user with non-interactive sudo available,
without other BPFtime experiments running. Output must be a new directory.
"""

import argparse
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time


ROOT = Path(__file__).resolve().parents[3]
SUITES = {
    "array": {
        "directory": "helper-map-closure",
        "object": "helper_map_closure.bpf.o",
        "cases": ("empty", "lookup_control", "update_control", "array_lookup",
                  "array_update", "percpu_array_lookup", "percpu_array_update"),
        "summary": ("array_lookup", "array_update", "percpu_array_lookup",
                    "percpu_array_update"),
    },
    "hash": {
        "directory": "helper-hash-closure",
        "object": "helper_hash_closure.bpf.o",
        "cases": ("empty", "lookup_control", "update_control", "hash_lookup",
                  "hash_update", "percpu_hash_lookup", "percpu_hash_update"),
        "summary": ("hash_lookup", "hash_update", "percpu_hash_lookup",
                    "percpu_hash_update"),
    },
}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--suite", choices=SUITES, default="array")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--iterations", type=int, default=20000)
    parser.add_argument("--warmup", type=int, default=1000)
    parser.add_argument("--cpu", type=int, default=5)
    parser.add_argument("--full", action="store_true",
                        help="Then run the candidate's full 10-round benchmark")
    args = parser.parse_args()
    if min(args.runs, args.iterations) < 1 or args.warmup < 0:
        parser.error("runs/iterations must be positive; warmup must be nonnegative")
    builds = {"baseline": args.baseline.resolve(),
              "candidate": args.candidate.resolve()}
    suite = SUITES[args.suite]
    cases = suite["cases"]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    diag = ROOT / "benchmark/uprobe/.output" / suite["directory"]
    loader = diag / suite["directory"]
    victim = diag / f"{suite['directory']}-victim"
    # Cleanup must understand the candidate's shared-memory object layout.
    # This also lets a previously archived baseline remain immutable even if
    # it was built without the optional command-line tools target.
    tool = builds["candidate"] / "tools/bpftimetool/bpftimetool"
    artifacts = [loader, victim, diag / suite["object"]]
    for build in builds.values():
        artifacts.extend([build / "runtime/agent/libbpftime-agent.so",
                          build / "runtime/syscall-server/libbpftime-syscall-server.so"])
    if args.full:
        artifacts.extend([ROOT / "benchmark/test", ROOT / "benchmark/uprobe/uprobe",
                          ROOT / "benchmark/uprobe/.output/uprobe.bpf.o",
                          builds["candidate"] / "benchmark/simple-benchmark-with-embed-ebpf-calling"])
    hashes = {str(path): digest(path) for path in artifacts}
    if Path("/dev/shm/bpftime_maps_shm").exists():
        raise RuntimeError("Existing BPFtime shared memory: check its owner before cleanup")
    run(["sudo", "-n", "true"])
    environment = os.environ.copy()
    for key in list(environment):
        if key.startswith("BPFTIME_") or key == "LD_PRELOAD":
            del environment[key]
    environment.update(BPFTIME_VM_NAME="llvm", SPDLOG_LEVEL="info",
                       BPFTIME_LOG_OUTPUT="console")
    metadata = {
        "started": datetime.datetime.now().astimezone().isoformat(),
        "arguments": {key: str(value) if isinstance(value, Path) else value
                      for key, value in vars(args).items()},
        "source_commit": run(["git", "rev-parse", "HEAD"],
                             capture_output=True, text=True).stdout.strip(),
        "binary_sha256": hashes,
        "mode": "base (diagnostic optimizations disabled)",
        "status": "running",
    }
    (output / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (output / "source.diff").write_text(run(
        ["git", "diff", "--", "runtime"], capture_output=True, text=True).stdout)
    shutil.copy2(__file__, output / "compare_runtime_builds.py")
    shutil.copy2(ROOT / "benchmark/uprobe/benchmark.py", output / "benchmark.py")
    for label, build in builds.items():
        shutil.copy2(build / "CMakeCache.txt", output / f"{label}-CMakeCache.txt")
    rows = []
    frequencies = []
    freq = Path(f"/sys/devices/system/cpu/cpu{args.cpu}/cpufreq")
    pinned = ["sudo", "-n", "taskset", "-c", str(args.cpu)]

    def run_target(command, stream, timeout):
        proc = subprocess.Popen(command, cwd=ROOT, stdout=stream,
                                stderr=subprocess.STDOUT, env=environment,
                                start_new_session=True)
        try:
            return_code = proc.wait(timeout=timeout)
            if return_code:
                raise subprocess.CalledProcessError(return_code, command)
        finally:
            if proc.poll() is None:
                # Include sudo's target and any benchmark subprocesses.
                run(["sudo", "-n", "kill", "-TERM", "--", f"-{proc.pid}"])
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    run(["sudo", "-n", "kill", "-KILL", "--", f"-{proc.pid}"])
                    proc.wait()

    def sample_frequency():
        current = {key: int((freq / f"scaling_{key}_freq").read_text())
                   for key in ("min", "max", "cur")}
        if current != {"min": 1728000, "max": 1728000, "cur": 1728000}:
            raise RuntimeError(f"CPU frequency not locked to the established baseline: {current}")
        frequencies.append(current)

    def measure(engine, round_number):
        sample_frequency()
        dest = output / f"{engine}-{round_number:02d}"
        dest.mkdir()
        loader_env = environment.copy()
        victim_env = environment.copy()
        if engine != "kernel":
            build = builds[engine]
            loader_env["LD_PRELOAD"] = str(build / "runtime/syscall-server/libbpftime-syscall-server.so")
            victim_env["LD_PRELOAD"] = str(build / "runtime/agent/libbpftime-agent.so")
        # env is placed after sudo so the dynamic loader sees LD_PRELOAD only
        # in the target process, not in sudo or taskset.
        target_env = lambda env: ["env", *[f"{k}={v}" for k, v in env.items()
                                           if k in ("LD_PRELOAD", "BPFTIME_VM_NAME",
                                                    "SPDLOG_LEVEL", "BPFTIME_LOG_OUTPUT")]]
        with (dest / "loader.log").open("w") as log:
            proc = subprocess.Popen(pinned + target_env(loader_env) + [str(loader)],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                    env=environment, start_new_session=True)
            try:
                deadline = time.monotonic() + 30
                while "Successfully started!" not in (dest / "loader.log").read_text():
                    if proc.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError(f"Loader failed: {dest / 'loader.log'}")
                    time.sleep(0.1)
                time.sleep(1)
                with (dest / "victim.log").open("w") as victim_log:
                    run_target(pinned + target_env(victim_env) + [str(victim),
                        str(args.iterations), str(round_number % len(cases)),
                        str(args.warmup)], victim_log, timeout=300)
                text = (dest / "victim.log").read_text()
                if engine != "kernel" and "shm_open_type 1" not in text:
                    raise RuntimeError("Userspace agent was not confirmed in output")
                values = {}
                for line in text.splitlines():
                    fields = line.split(",")
                    if fields[0] in cases and len(fields) == 5:
                        if fields[0] in values or int(fields[2]) != args.iterations:
                            raise RuntimeError("Duplicate or incomplete timing row")
                        values[fields[0]] = float(fields[4])
                if set(values) != set(cases) or any(
                        not math.isfinite(v) or v <= 0 for v in values.values()):
                    raise RuntimeError(f"Incomplete timings: {values}")
                for case, raw in values.items():
                    control = ("lookup_control" if case.endswith("lookup")
                               else "update_control" if case.endswith("update") else None)
                    rows.append({"engine": engine, "round": round_number,
                                 "case": case, "raw_ns_per_invocation": raw,
                                 "net_ns_per_helper": (raw - values[control]) / 1000
                                 if control else None})
                if proc.poll() is not None:
                    raise RuntimeError("Loader exited during measurement")
                sample_frequency()
            finally:
                if proc.poll() is None:
                    run(["sudo", "-n", "kill", "-TERM", str(proc.pid)])
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        run(["sudo", "-n", "kill", "-KILL", "--", f"-{proc.pid}"])
                        proc.wait()
                if engine != "kernel":
                    run(["sudo", "-n", str(tool), "remove"],
                        stdout=subprocess.DEVNULL)
        print(f"Completed {engine} round {round_number}/{args.runs}", flush=True)

    try:
        for index in range(args.runs):
            # Alternate both build order and kernel placement between rounds.
            engines = ("kernel", "baseline", "candidate") if index % 2 == 0 else (
                "candidate", "baseline", "kernel")
            for engine in engines:
                measure(engine, index + 1)
        if hashes != {str(path): digest(path) for path in artifacts}:
            raise RuntimeError("A binary changed during the experiment")
        with (output / "wall-raw.csv").open("w") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)
        summary = []
        for case in suite["summary"]:
            record = {"case": case}
            for engine in ("kernel", "baseline", "candidate"):
                values = [row["net_ns_per_helper"] for row in rows
                          if row["case"] == case and row["engine"] == engine]
                record[engine] = {"median": statistics.median(values),
                                  "mean": statistics.mean(values),
                                  "stdev": statistics.stdev(values) if len(values) > 1 else 0,
                                  "raw_values": values}
            record["reduction_percent"] = 100 * (1 - record["candidate"]["median"] /
                                                  record["baseline"]["median"])
            summary.append(record)
        (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(json.dumps(summary, indent=2), flush=True)
        if args.full:
            metadata["stage"] = "full benchmark (matched A/B completed)"
            (output / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n")
            full = output / "full"
            full.mkdir()
            started = time.time()
            with (full / "benchmark.log").open("w") as stream:
                run_target(pinned + ["env", f"BPFTIME_BUILD_DIR={builds['candidate']}",
                    "BPFTIME_VM_NAME=llvm", "/usr/bin/python3", "-u",
                    "benchmark/uprobe/benchmark.py", "--iter", "10"],
                    stream, timeout=10800)
            source = ROOT / "benchmark/uprobe"
            result = json.loads((source / "benchmark-output.json").read_text())
            for group, count in (("kernel_uprobe", 17), ("userspace_uprobe", 17), ("embed", 1)):
                assert len(result[group]) == count
                assert all(len(item["raw_values"]) == 10 for item in result[group].values())
                assert all(math.isfinite(value) and value > 0
                           for item in result[group].values() for value in item["raw_values"])
            for name in ("benchmark-output.json", "results.md", "benchmark-logs.txt"):
                assert (source / name).stat().st_mtime >= started
                shutil.copy2(source / name, full / name)
            run(["sudo", "-n", str(tool), "remove"], stdout=subprocess.DEVNULL)
            sample_frequency()
            if hashes != {str(path): digest(path) for path in artifacts}:
                raise RuntimeError("A binary changed during the full experiment")
        metadata["status"] = "success"
    except BaseException as error:
        metadata["status"] = "failed"
        metadata["error"] = repr(error)
        raise
    finally:
        metadata["finished"] = datetime.datetime.now().astimezone().isoformat()
        metadata["frequency_samples"] = frequencies
        (output / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
