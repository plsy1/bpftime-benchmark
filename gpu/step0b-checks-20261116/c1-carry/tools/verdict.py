#!/usr/bin/env python3
"""verdict.py OUT: the combined C1 decision (README, "Combined decision").

  carry FAILS     (b) "carry FAILS", or (d) "FAIL", or a carry kernel of
                  part (c) (c_local, c_local_d) failed its validation (a
                  mismatch or NULL storage in an empty kernel);
  carry WORKS     otherwise, if all of: (a) every routed module compiled and
                  the SASS checks passed (OUT/build_ok); every GPU step of
                  run_all.sh exited 0 (OUT/steps_rc.txt); part (c)'s
                  validation had no failure, the valid mark held (an exit
                  without its entry found 0 in every thread) and its mark-1
                  twin read the prologue's 1 in every thread (so the mark
                  check can fail); (b) "carry WORKS" with the full
                  parameter minimums (b_summary.json mode "full"); (d) "PASS";
  INCONCLUSIVE    otherwise.
A QUICK run (OUT/quick) is stamped "QUICK smoke, not a result".
The cost table of part (c) is reported alongside (OUT/c/summary.md).
"""
import json
import os
import sys


def last(path, prefix):
    if not os.path.exists(path):
        return None
    for line in reversed(open(path).read().strip().split("\n")):
        if line.startswith(prefix):
            return line[len(prefix):].strip()
    return None


def steps(out):
    """[(log, rc)] of the GPU steps run_all.sh ran (build excluded)."""
    p = os.path.join(out, "steps_rc.txt")
    if not os.path.exists(p):
        return None
    res = []
    for line in open(p):
        parts = line.split()
        if len(parts) == 2 and parts[0] != "build.log":
            res.append((parts[0], int(parts[1])))
    return res


def main():
    out = sys.argv[1]
    a = os.path.exists(os.path.join(out, "build_ok"))
    quick = os.path.exists(os.path.join(out, "quick"))
    st = steps(out)
    steps_ok = st is not None and len(st) > 0 and all(rc == 0 for _, rc in st)
    b = last(os.path.join(out, "b_summary.txt"), "C1 part b verdict:")
    bs = os.path.join(out, "b_summary.json")
    bj = json.load(open(bs)) if os.path.exists(bs) else {}
    d = last(os.path.join(out, "d", "d_summary.txt"), "C1 part d verdict:")
    c = os.path.join(out, "c", "summary.json")
    cj = json.load(open(c)) if os.path.exists(c) else {}
    comparable = cj.get("comparable_with_e2")
    mark = cj.get("valid_mark_ok")
    mark_can_fail = cj.get("valid_mark_can_fail")
    c_carry_fail = cj.get("carry_validation_failed") or []
    c_fail = cj.get("validation_failed")
    if b == "carry FAILS" or d == "FAIL" or c_carry_fail:
        v = "carry FAILS: fall back to a %warpid-addressed slot with an owner tag (the slottag variant)"
    elif (a and steps_ok and mark and mark_can_fail and c_fail == [] and b == "carry WORKS" and d == "PASS"
          and (quick or bj.get("mode") == "full")):
        v = "carry WORKS"
    else:
        v = "INCONCLUSIVE"
    if quick:
        v = "QUICK smoke, not a result: " + v
    lines = ["(a) routed modules compiled, SASS checks passed: %s" % a,
             "GPU steps exited 0: %s %s" % (steps_ok, [x for x in (st or []) if x[1] != 0] or ""),
             "(c) validation: failures %s; carry kernels failed %s; valid mark held: %s; its mark-1 twin "
             "read back (the check can fail): %s; no-mark control read stale bytes as valid "
             "(reported): %s" % (c_fail, c_carry_fail or "none", mark, mark_can_fail,
                                 cj.get("noinit_read_stale_as_valid")),
             "(b) %s" % b,
             "(b) preemption link (measured on this board, time-slicing only, not documented): warps "
             "moved by preemption under the hog: %s; rule-compliant use keeps a thread-id tag beside "
             "the payload, checks it at exit and counts a mismatch as a fallback (PROG_CHK's id check)"
             % bj.get("id_changes_hog"),
             "(d) %s" % d,
             "(c) cost table: %s (comparable with e2: %s)" % (c if os.path.exists(c) else "missing", comparable),
             "C1 verdict: %s" % v]
    open(os.path.join(out, "c1_verdict.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
