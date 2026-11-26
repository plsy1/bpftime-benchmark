#!/usr/bin/env python3
"""Print the C6 results table from logs/<name>.log (the numbers C6-report.md quotes)."""
import os
import re
import sys

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
names = sys.argv[1:] or ["rule-A", "rule-B", "rule-C", "rule-D", "check-S-1e7", "check-SD-1e7"]


def parse(name):
    t = open(os.path.join(D, name + ".log")).read()
    c = {m.group(1): int(m.group(2)) for m in re.finditer(r"^counter (\S+)\s+(\d+)$", t, re.M)}
    r = {}
    m = re.search(r"^launches=(\d+) instances=(\d+) wall_s=([\d.]+)$", t, re.M)
    r["launches"], r["instances"], r["wall"] = int(m.group(1)), int(m.group(2)), float(m.group(3))
    r["stop"] = re.search(r"^stop: (.*)$", t, re.M).group(1)
    r["exit"] = re.search(r"^### exit=(\d+)", t, re.M).group(1)
    r["scan"] = re.search(r"nonzero data/flag words after the last reset=(\d+)", t).group(1)
    r["mean_polls"] = re.search(r"mean polls per flag-seen instance=([\d.]+)", t).group(1)
    return c, r


print("| run | instances | flag seen | weak same-SM | weak diff-SM | timeouts | same-SM inst | diff-SM inst "
      "| saw 0 then 1 | first poll | stress ops | wall s | exit |")
print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
for n in names:
    if not os.path.exists(os.path.join(D, n + ".log")):
        continue
    c, r = parse(n)
    seen = c["strong_same_sm"] + c["strong_diff_sm"] + c["weak_same_sm"] + c["weak_diff_sm"]
    print(f"| {n} | {r['instances']:,} | {seen:,} | {c['weak_same_sm']:,} | {c['weak_diff_sm']:,} | "
          f"{c['timeout_same_sm'] + c['timeout_diff_sm']:,} | {c['inst_same_sm']:,} | {c['inst_diff_sm']:,} | "
          f"{c['saw0_then_1']:,} | {c['seen_on_first_poll']:,} | {c.get('stress_ops', 'n/a')} | {r['wall']:.2f} | {r['exit']} |")
print()
print("| run | stop | launches | mean polls | bad flag | bad data | missing | final not 1 | smid bad | "
      "same-block pairs | writer wait timeouts | barrier timeouts | stress op cap | nonzero after scan |")
print("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
for n in names:
    if not os.path.exists(os.path.join(D, n + ".log")):
        continue
    c, r = parse(n)
    print(f"| {n} | {r['stop']} | {r['launches']:,} | {r['mean_polls']} | {c['bad_flag']} | {c['bad_data']} | "
          f"{c['missing_record']} | {c['final_not_1']} | {c['smid_bad']} | {c.get('pair_same_block', 'n/a')} | "
          f"{c['writer_wait_timeouts']} | {c['barrier_timeouts']} | {c['stress_hit_op_cap']} | {r['scan']} |")
