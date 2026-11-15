#!/usr/bin/env python3
"""Verdict of C5's capture-mode remedy check. Usage: python3 -I analyze.py OUTDIR

Decision rules (fixed before any real run; see ../README.md, claim 10; tightened by the
adversarial review before any real run, smoke only so far):
  errors      "all errors 0" below means: captest's begin, kernel-launch, end, instantiate,
              exec-update, graph-launch and sync error counts, status_errors and
              invalidated_seen are all 0, and its process exit status (rc) is 0.
  sanity      noshim-global: all errors 0; and every control and remedy case whose shim log
              exists ran with the copier mode its name says (-1 no call, 0 GLOBAL, 2 RELAXED);
              else BROKEN HARNESS. Reported-only cases are not part of sanity.
  control     async-global-default and asyncevent-global-default (step 0's copier, its thread
              left in the default mode) must have run (captest result present, rc 0, shim log
              present) and show end errors or invalidations > 0. If one crashed or is missing,
              its pair is UNDECIDED (control unusable); if it ran clean, its pair is UNDECIDED
              (the driver no longer treats these calls as unsafe, so the remedy cannot be told
              apart).
  control 2   async-global-global (the copier calls cuThreadExchangeStreamCaptureMode with
              GLOBAL, r_mode == 0) must also have run and show end errors or invalidations > 0.
              It differs from the remedy only in the mode value. Otherwise the async pair is
              UNDECIDED (calling the function at all, not RELAXED, could explain the result).
  remedy      async-global-relaxed and asyncevent-global-relaxed hold iff all errors 0 in
              captest, and in the copier: r_mode == 0, prev_mode == 0 (the documented default
              GLOBAL was in force before the call), set-up results all 0, copy errors == 0,
              wait errors == 0, poll cap hits == 0, and copies during an open capture >= 100
              (collisions were possible). A remedy case that crashed (rc != 0, no captest
              result) or wrote no shim log FAILS. (In the controls the copies succeed and the
              waits fail with error 900: in the sync variant that is cuStreamSynchronize; in
              the event variant the shim does not separate cuEventRecord, cuEventQuery and its
              fallback cuStreamSynchronize.)
  reported    async-global-tlocal (THREAD_LOCAL copier: the text says other threads'
              captures are ignored, so the documented prediction is the remedy's criteria),
              memalloc-global-relaxed (an unsafe call from a RELAXED thread: same prediction,
              plus 0 unsafe-call errors), ctxsync-global-relaxed (synchronizing the context
              during a capture is invalid in every mode: errors or a crash are the documented
              prediction).
"""
import json
import os
import sys


def load(o):
    res, case = {}, None
    for line in open(os.path.join(o, "captest.jsonl")):
        line = line.strip()
        if not line:
            continue
        j = json.loads(line)
        if "case" in j and len(j) == 1:
            case = j["case"]
        elif case:
            res[case] = j
    rc = {}
    for line in list(open(os.path.join(o, "runs.csv")))[1:]:
        parts = line.strip().split(",")
        rc[parts[0]] = int(parts[-1])
    return res, rc


def shim(o, case):
    p = os.path.join(o, case + ".shim.json")
    try:
        return json.load(open(p))
    except (OSError, ValueError):
        return None


def row(o, res, rc, case):
    r = res.get(case)
    s = shim(o, case)
    d = {"case": case, "rc": rc.get(case), "present": r is not None}
    if r:
        d.update(iters=r["iters"], begin_err=r["begin"]["errors"], end_err=r["end"]["errors"],
                 invalidated=r["invalidated_seen"], launch_err=r["kernel_launch"]["errors"],
                 other_err=sum(r[k]["errors"] for k in ("instantiate", "exec_update",
                                                        "graph_launch", "sync")),
                 status_err=r["status_errors"])
    if s:
        c = s.get("copies", {})
        cfg = s.get("config", {})
        d.update(copies=c.get("n"), copy_err=c.get("errors"), during=c.get("during_capture"),
                 wait_err=c.get("wait_errors"), wait_first=c.get("wait_first_err"),
                 poll_cap=c.get("poll_cap_hits"), r_mode=cfg.get("r_mode"),
                 prev_mode=cfg.get("prev_mode"), copier_mode=cfg.get("copier_mode"),
                 setup_bad=sum(1 for v in s.get("setup", {}).values() if v != 0),
                 unsafe_err=s.get("unsafe", {}).get("errors"))
    return d


def ran(d):
    """captest wrote its result with rc 0, and the shim wrote its log."""
    return bool(d.get("present") and d.get("rc") == 0 and d.get("copier_mode") is not None)


def failed(d):
    """The capture broke: end errors, invalidations or begin errors (control criterion)."""
    return bool(d.get("present") and (d.get("end_err", 0) > 0 or d.get("invalidated", 0) > 0
                                      or d.get("begin_err", 0) > 0))


def app_clean(d):
    """All errors 0 in captest (see 'errors' in the rules)."""
    return bool(d.get("present") and d.get("rc") == 0
                and all(d.get(k) == 0 for k in ("begin_err", "end_err", "invalidated",
                                                 "launch_err", "other_err", "status_err")))


def holds(d):
    return bool(app_clean(d) and d.get("r_mode") == 0 and d.get("prev_mode") == 0
                and d.get("setup_bad") == 0 and d.get("copy_err") == 0
                and d.get("wait_err") == 0 and d.get("poll_cap") == 0
                and (d.get("during") or 0) >= 100)


def main():
    o = sys.argv[1]
    res, rc = load(o)
    cases = ["noshim-global", "async-global-default", "async-global-global",
             "async-global-relaxed", "async-global-tlocal", "asyncevent-global-default",
             "asyncevent-global-relaxed", "memalloc-global-relaxed", "ctxsync-global-relaxed"]
    rows = {c: row(o, res, rc, c) for c in cases}
    for c in cases:
        print(json.dumps(rows[c], sort_keys=True))
    # each control and remedy case whose shim log exists must have run with the copier mode
    # its name says (-1 = no call); a missing log is judged by the control and remedy rules
    want = {"async-global-default": -1, "async-global-global": 0, "async-global-relaxed": 2,
            "asyncevent-global-default": -1, "asyncevent-global-relaxed": 2}
    wrong = [c for c, m in want.items()
             if rows[c].get("copier_mode") is not None and rows[c].get("copier_mode") != m]
    sane = app_clean(rows["noshim-global"]) and not wrong
    print("sanity (noshim-global clean, copier modes as named%s): %s" %
          ("; wrong: " + ",".join(wrong) if wrong else "", "ok" if sane else "BROKEN HARNESS"))
    for ctl, rem, ctl2 in (("async-global-default", "async-global-relaxed", "async-global-global"),
                           ("asyncevent-global-default", "asyncevent-global-relaxed", None)):
        if not sane:
            v = "BROKEN HARNESS"
        elif not ran(rows[ctl]):
            v = "UNDECIDED (control crashed or missing)"
        elif not failed(rows[ctl]):
            v = "UNDECIDED (control did not fail)"
        elif ctl2 and not (ran(rows[ctl2]) and failed(rows[ctl2])
                           and rows[ctl2].get("r_mode") == 0):
            v = "UNDECIDED (explicit-GLOBAL control crashed, did not fail, or its mode call failed)"
        elif holds(rows[rem]):
            v = "REMEDY HOLDS"
        else:
            v = "REMEDY FAILS"
        print("verdict %s vs %s: %s" % (rem, ctl, v))
    for c in ("async-global-tlocal", "memalloc-global-relaxed"):
        ok = holds(rows[c]) and (c != "memalloc-global-relaxed" or rows[c].get("unsafe_err") == 0)
        print("reported %s: %s (documented prediction: clean)" % (c, "clean" if ok else "NOT clean"))
    d = rows["ctxsync-global-relaxed"]
    what = ("crashed or killed, no result" if not d.get("present") else
            "errors" if failed(d) else "no errors seen")
    print("reported ctxsync-global-relaxed: rc=%s, %s (documented prediction: invalid; "
          "a crash is one form of it)" % (d.get("rc"), what))


if __name__ == "__main__":
    main()
