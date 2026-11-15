#!/usr/bin/env python3
"""Self-test of analyze.py (no GPU): can its verdicts fail? Usage: python3 -I selftest_analyze.py

Builds variants of the saved smoke smoke/run1 in a temporary directory next to this file
(removed afterwards) and checks the verdict lines analyze.py prints for each:
  base      run1 plus a synthetic explicit-GLOBAL control (a copy of async-global-default
            with copier_mode 0, r_mode 0): both pairs REMEDY HOLDS.
  swapped   the default-mode control's data put in place of each relaxed case: both pairs
            REMEDY FAILS (the remedy's failure is detectable).
  waiterr   the relaxed copiers report wait errors (error 900), captest clean: both pairs
            REMEDY FAILS (the refused call in the controls is the wait, not the copy).
  glclean   the explicit-GLOBAL control ran (mode call made) but its captures were clean:
            the async pair UNDECIDED, the event pair REMEDY HOLDS.
  missing   the explicit-GLOBAL control crashed (no captest result, rc 139, no log): the
            async pair UNDECIDED.
  remcrash  async-global-relaxed crashed (no captest result, rc 139, no log): the async pair
            REMEDY FAILS.
  memcrash  memalloc-global-relaxed (reported only) crashed: both verdicts unchanged
            (REMEDY HOLDS).
  ctlcrash  asyncevent-global-default crashed: the event pair UNDECIDED (control unusable).
  ctlclean  the default-mode controls made clean: both pairs UNDECIDED.
  wrongmode async-global-relaxed's log says copier_mode 1: BROKEN HARNESS.
Exit 0 iff every expectation holds.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "smoke", "run1")
ASYNC = "verdict async-global-relaxed vs async-global-default: "
EVENT = "verdict asyncevent-global-relaxed vs asyncevent-global-default: "


def read_jsonl(d):
    out, case = {}, None
    for line in open(os.path.join(d, "captest.jsonl")):
        j = json.loads(line)
        if "case" in j and len(j) == 1:
            case = j["case"]
        else:
            out[case] = j
    return out


def write(d, cap, rcs):
    with open(os.path.join(d, "captest.jsonl"), "w") as f:
        for c, j in cap.items():
            f.write(json.dumps({"case": c}) + "\n")
            if j is not None:
                f.write(json.dumps(j) + "\n")
    with open(os.path.join(d, "runs.csv"), "w") as f:
        f.write("case,copier_mode,shim_env,rc\n")
        for c in cap:
            f.write('%s,x,"y",%d\n' % (c, rcs.get(c, 0)))


def shim(d, c):
    return json.load(open(os.path.join(d, c + ".shim.json")))


def put_shim(d, c, j):
    json.dump(j, open(os.path.join(d, c + ".shim.json"), "w"))


def variant(tmp, name, edit):
    d = os.path.join(tmp, name)
    shutil.copytree(SRC, d)
    cap = read_jsonl(d)
    rcs = {"ctxsync-global-relaxed": 139}
    # synthetic explicit-GLOBAL control: the default control with the mode call recorded
    g = shim(d, "async-global-default")
    g["config"]["copier_mode"], g["config"]["r_mode"], g["config"]["prev_mode"] = 0, 0, 0
    put_shim(d, "async-global-global", g)
    order = ["noshim-global", "async-global-default", "async-global-global",
             "async-global-relaxed", "async-global-tlocal", "asyncevent-global-default",
             "asyncevent-global-relaxed", "memalloc-global-relaxed", "ctxsync-global-relaxed"]
    cap["async-global-global"] = cap["async-global-default"]
    cap["ctxsync-global-relaxed"] = None
    cap = {c: cap.get(c) for c in order}
    edit(d, cap, rcs)
    write(d, cap, rcs)
    out = subprocess.run([sys.executable, "-I", os.path.join(HERE, "analyze.py"), d],
                         capture_output=True, text=True, check=False).stdout
    return out


def swapped(d, cap, rcs):
    for ctl, rem in (("async-global-default", "async-global-relaxed"),
                     ("asyncevent-global-default", "asyncevent-global-relaxed")):
        cap[rem] = cap[ctl]
        j = shim(d, ctl)
        j["config"]["copier_mode"], j["config"]["r_mode"], j["config"]["prev_mode"] = 2, 0, 0
        put_shim(d, rem, j)


def waiterr(d, cap, rcs):
    for rem in ("async-global-relaxed", "asyncevent-global-relaxed"):
        j = shim(d, rem)
        j["copies"]["wait_errors"], j["copies"]["wait_first_err"] = 5, 900
        put_shim(d, rem, j)


def glclean(d, cap, rcs):
    cap["async-global-global"] = cap["noshim-global"]


def missing(d, cap, rcs):
    os.remove(os.path.join(d, "async-global-global.shim.json"))
    cap["async-global-global"] = None
    rcs["async-global-global"] = 139


def remcrash(d, cap, rcs):
    os.remove(os.path.join(d, "async-global-relaxed.shim.json"))
    cap["async-global-relaxed"] = None
    rcs["async-global-relaxed"] = 139


def ctlcrash(d, cap, rcs):
    os.remove(os.path.join(d, "asyncevent-global-default.shim.json"))
    cap["asyncevent-global-default"] = None
    rcs["asyncevent-global-default"] = 139


def memcrash(d, cap, rcs):
    os.remove(os.path.join(d, "memalloc-global-relaxed.shim.json"))
    cap["memalloc-global-relaxed"] = None
    rcs["memalloc-global-relaxed"] = 139


def ctlclean(d, cap, rcs):
    for ctl in ("async-global-default", "asyncevent-global-default", "async-global-global"):
        cap[ctl] = cap["noshim-global"]


def wrongmode(d, cap, rcs):
    j = shim(d, "async-global-relaxed")
    j["config"]["copier_mode"] = 1
    put_shim(d, "async-global-relaxed", j)


CASES = [
    ("base", lambda d, c, r: None, "REMEDY HOLDS", "REMEDY HOLDS"),
    ("swapped", swapped, "REMEDY FAILS", "REMEDY FAILS"),
    ("waiterr", waiterr, "REMEDY FAILS", "REMEDY FAILS"),
    ("glclean", glclean, "UNDECIDED", "REMEDY HOLDS"),
    ("missing", missing, "UNDECIDED", "REMEDY HOLDS"),
    ("remcrash", remcrash, "REMEDY FAILS", "REMEDY HOLDS"),
    ("memcrash", memcrash, "REMEDY HOLDS", "REMEDY HOLDS"),
    ("ctlcrash", ctlcrash, "REMEDY HOLDS", "UNDECIDED (control crashed"),
    ("ctlclean", ctlclean, "UNDECIDED", "UNDECIDED"),
    ("wrongmode", wrongmode, "BROKEN HARNESS", "BROKEN HARNESS"),
]


def main():
    tmp = tempfile.mkdtemp(prefix=".selftest_", dir=HERE)
    bad = 0
    try:
        for name, edit, want_a, want_e in CASES:
            out = variant(tmp, name, edit)
            got_a = [l[len(ASYNC):] for l in out.splitlines() if l.startswith(ASYNC)]
            got_e = [l[len(EVENT):] for l in out.splitlines() if l.startswith(EVENT)]
            ok = (len(got_a) == 1 and got_a[0].startswith(want_a)
                  and len(got_e) == 1 and got_e[0].startswith(want_e))
            bad += not ok
            print("%-9s async: %-40s event: %-40s %s" % (
                name, got_a[0] if got_a else "-", got_e[0] if got_e else "-",
                "ok" if ok else "UNEXPECTED (want %s / %s)" % (want_a, want_e)))
    finally:
        shutil.rmtree(tmp)
    print("selftest: %s" % ("PASS" if bad == 0 else "FAIL (%d)" % bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
