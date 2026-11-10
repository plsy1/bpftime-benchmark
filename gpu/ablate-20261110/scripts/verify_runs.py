#!/usr/bin/env python3
"""Check each case of run_cuda_bench.py logs: verify_runs.py RUN_DIR... -> CSV on stdout.

Per case (the log section after 'Running test: NAME'):
  time_us      the framework's result (result json), empty if it failed
  valid        the victim printed 'Validation check: C[0] = 0.00, C[1] = 3.00'
  ended        the victim printed its closing '====' line after the validation line
  fail         failure or silent-fallback strings found in the section (';'-joined)
  dev_patched  dev: count of 'loaded the patched image ... probes on _Z9vectorAddPKfS0_Pf'
  off_record   official: count of 'Recorded pass ... for func _Z9vectorAddPKfS0_Pf'
  resolved     debug-level official launches redirected to the patched kernel
  unpatched    debug-level official launches with no patched kernel
  loader       numbers the example loaders print (calls, events, counter[0], threads with counts)
"""
import csv
import json
import re
import sys
from pathlib import Path

FAIL = [
    "Failed to load and verify", "Unable to allocate map", "failed to create", "cannot be preloaded",
    "Unable to initialize shared memory", "Failed to find a defined symbol", "Unable to launch patched kernel",
    "Unable to launch kernel", "bootstrap failed", "unable to resolve host function", "No pass definition matched",
    "falling back to sm_61", "cudaHostRegister() failed", "cudaHostRegister error", "Probe binary not found",
    "BPF benchmark failed", "Error running", "This example can only be used under bpftime", "Unable to poll",
    "Failed to attach", "Failed to open and load", "illegal memory access", "CUDA error", "Segmentation fault",
    "runs unpatched", "never ran", "found no patched kernel", "could not be named",
]
VALID = "Validation check: C[0] = 0.00, C[1] = 3.00"


def sections(log: str):
    parts = re.split(r"^.*Running test: (.+)$", log, flags=re.M)
    for i in range(1, len(parts), 2):
        yield parts[i].strip(), parts[i + 1]


def loader_numbers(text: str) -> str:
    out = []
    calls = [int(x) for x in re.findall(r"calls: (\d+)", text)]
    if calls:
        out.append(f"calls={max(calls)}")
    ev = [int(x) for x in re.findall(r"Total events collected: (\d+)", text)]
    if ev:
        out.append(f"events={max(ev)}")
    ctr = [int(x) for x in re.findall(r"counter\[0\]=(\d+)", text)]
    if ctr:
        out.append(f"counter0={max(ctr)}")
    th = re.findall(r"Thread (\d+): (\d+)", text)
    if th:
        nz = {int(a) for a, b in th if int(b) > 0}
        out.append(f"threads_with_counts={len(nz)}")
    return " ".join(out)


def main():
    w = csv.writer(sys.stdout)
    w.writerow(["run", "config", "case", "time_us", "valid", "ended", "fail", "dev_patched", "off_record",
                "resolved", "unpatched", "loader"])
    for run_dir in sys.argv[1:]:
        for log_path in sorted(Path(run_dir).glob("*_bench.log")):
            cfg = log_path.name.replace("_bench.log", "")
            res_path = log_path.with_name(f"{cfg}_result.json")
            times = {}
            if res_path.exists():
                for t in json.loads(res_path.read_text()).get("tests", []):
                    times[t["name"]] = t.get("avg_time_us") or t.get("time_us") or t.get("avg_time")
            log = log_path.read_text(errors="replace")
            for name, sec in sections(log):
                vpos = sec.find(VALID)
                ended = vpos >= 0 and "====" in sec[vpos:]
                fails = sorted({f for f in FAIL if f in sec})
                w.writerow([
                    Path(run_dir).name, cfg, name, times.get(name, ""), int(vpos >= 0), int(ended), ";".join(fails),
                    len(re.findall(r"loaded the patched image of the fatbin.*probes on [^)]*_Z9vectorAddPKfS0_Pf", sec)),
                    len(re.findall(r"Recorded pass .* for func _Z9vectorAddPKfS0_Pf", sec)),
                    len(re.findall(r"resolved _Z9vectorAddPKfS0_Pf -> patched CUfunction", sec)),
                    len(re.findall(r"no patched CUfunction is registered", sec)),
                    loader_numbers(sec),
                ])


main()
