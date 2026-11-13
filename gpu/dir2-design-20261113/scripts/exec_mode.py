#!/usr/bin/env python3
"""Add exec_mode (what the runtime would actually run) to the R7 records.

Input: final-long/r7_records.json (59 host-call sites, one record per logical
site, decision from the hand provenance table). Output: records-exec.json and
the tallies on stdout. exec_mode is a stated function of the decision and the
configuration, so it checks what UnitFold runs, not the survey's labels.
"""
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Full design, default configuration (step 0 passed, no C-SB).
FULL_DEFAULT = {
    "defer_unit": "gpu_after_first_touch",   # one host op per (key, agent), then GPU records
    "defer_set": "gpu_after_first_touch",
    "defer_del": "gpu_after_first_touch",    # first UNIT_DEL marks ABSENT, later deletes free
    "defer_out": "ring_record",              # printk ring; full ring -> synchronous printk
    "resident_read": "round_trip_per_group", # LOOKUP_COPY; with C-SB: READ_LOCAL
}
FULL_CSB = dict(FULL_DEFAULT, resident_read="gpu_read_after_first_touch")


def slice1(rec):
    return "gpu_after_first_touch" if rec.get("slice1_rewritten") else "sync_per_lane (today)"


def main():
    recs = json.load(open(os.path.join(HERE, "final-long", "r7_records.json")))
    out = []
    for r in recs:
        r = dict(r)
        r["exec_mode_default"] = FULL_DEFAULT[r["decision"]]
        r["exec_mode_csb"] = FULL_CSB[r["decision"]]
        r["exec_mode_slice1"] = slice1(r)
        r["runs_on_orin"] = "rocm-counter" not in r["file"]
        out.append(r)
    json.dump(out, open(os.path.join(HERE, "records-exec.json"), "w"), indent=1)

    def tally(key, rows=out):
        return dict(sorted(collections.Counter(r[key] for r in rows).items()))

    print("sites:", len(out))
    print("survey class:", tally("class"))
    print("exec_mode, full design, default:", tally("exec_mode_default"))
    print("exec_mode, full design, with C-SB:", tally("exec_mode_csb"))
    print("exec_mode, slice 1:", tally("exec_mode_slice1"))
    orin = [r for r in out if r["runs_on_orin"]]
    print("exec_mode, slice 1, sites that run on the Orin (%d):" % len(orin), tally("exec_mode_slice1", orin))
    print("survey class x default exec_mode:")
    for k, v in sorted(collections.Counter((r["class"], r["exec_mode_default"]) for r in out).items()):
        print("   ", k, v)
    print("aggregatable3 x default exec_mode:")
    for k, v in sorted(collections.Counter((r["aggregatable3"], r["exec_mode_default"]) for r in out).items()):
        print("   ", k, v)
    # The check: every record's exec_mode follows from (operation, result_use).
    expect = {("read", "value_used"): "round_trip_per_group", ("output", "unused"): "ring_record"}
    bad = [(r["file"].split("/")[-2], r["line"]) for r in out
           if expect.get((r["operation"], r["result_use"]), "gpu_after_first_touch") != r["exec_mode_default"]]
    print("exec_mode mismatches against (operation, result_use):", bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
