#!/usr/bin/env python3
"""check_sass.py <b|c> BUILDDIR

Confirms in PTX and SASS (nvdisasm -c of the module cubin, which names every
function as $<kernel>$<function>) that the routing survives compilation:

  every routed kernel
    PTX: declares the kernel-scope .local carry and zeroes its first 8 bytes
         (the valid mark) through its generic address; every probe call passes it
         (cvta.local.u64 %c1_carry, __c1_carry right before the call);
    SASS: reserves a stack frame (IADD3 R1, R1, -X), calls the entry probe
         once and the exit probe at least once, and forms the carry's generic
         address as R1 + c[0x0][0x20] (the local window base), and stores
         zero (the mark) before its first probe CALL;
  helper form: both probe functions CALL $K$_bpf_helper_ext_0001_c1, which
         exists in the kernel's code;
  direct form: neither probe function calls the wrapper; the entry probe has a
         generic store (ST.E) and the exit probe a generic load (LD.E);
  every kernel: the number of exit-probe CALLs in SASS equals the number of
         exit-probe call sites in the PTX (kmr: 7; nvcc's kernels: 1);
  part c also: the unrouted kernels (none, empty, slottag, 1512, ref) have no
         __c1_carry; 'none' calls nothing; the slot+tag kernel's probes call
         the wrapper; c_value's entry probe calls the timer helper (0502);
         the empty probes may be inlined by ptxas (reported); the mark-1
         twin stores a register (1), not RZ, before its first probe call.

Writes BUILDDIR/sass_check.json and prints one line per kernel and a last
line "sass check: ok" or "sass check: FAILED (n)". Exit status 1 on failure.
"""
import json
import os
import re
import subprocess
import sys

NVDISASM = "/usr/local/cuda-13.2/bin/nvdisasm"


def split_functions(text):
    """{(kernel, function): [instruction lines]} from nvdisasm -c."""
    funcs = {}
    cur = None
    for line in text.split("\n"):
        m = re.match(r"^(\S+):\s*$", line)
        if m and not line.startswith(".") and not m.group(1).startswith(".L"):
            name = m.group(1)
            m2 = re.match(r"^\$(\w+)\$(\w+)$", name)
            if m2:
                cur = (m2.group(1), m2.group(2))
            elif re.match(r"^\w+$", name):
                cur = (name, name)
            else:
                cur = None
            if cur:
                funcs.setdefault(cur, [])
            continue
        if cur and re.search(r"/\*[0-9a-f]{4,}\*/", line):
            funcs[cur].append(line)
    return funcs


def calls(lines, target):
    return sum(1 for l in lines if "CALL" in l and "`(" + target + ")" in l)


def main():
    part, bdir = sys.argv[1], sys.argv[2]
    pre = part + "_"
    man = json.load(open(os.path.join(bdir, pre + "manifest.json")))
    ptx = open(os.path.join(bdir, pre + "module.ptx")).read()
    cubin = os.path.join(bdir, pre + "module.cubin")
    dis = subprocess.run([NVDISASM, "-c", cubin], capture_output=True, text=True, check=True).stdout
    open(os.path.join(bdir, pre + "module.nvdisasm.txt"), "w").write(dis)
    funcs = split_functions(dis)
    fails = []
    out = []
    for m in man:
        k = m["name"]
        routed = m.get("routed", True)
        res = {"kernel": k, "routed": routed, "direct": m.get("direct", False)}
        body = funcs.get((k, k), [])
        if not body:
            fails.append("%s: no SASS" % k)
            continue
        pe, px = "$%s$__probe_func__%s" % (k, k), "$%s$__retprobe_func__%s" % (k, k)
        res["entry_calls"] = calls(body, pe)
        res["exit_calls"] = calls(body, px)
        res["frame_reserve"] = sum(1 for l in body if re.search(r"IADD3 R1, R1, -0x[0-9a-f]+", l))
        res["carry_addr"] = sum(1 for l in body if re.search(r"IADD3 R\d+, P\d, R1, c\[0x0\]\[0x20\]", l))
        kb = re.search(r"\.entry\s+" + re.escape(k) + r"\s*\(", ptx)
        kend = ptx.find("\n}", kb.end())
        kptx = ptx[kb.end():kend]
        res["ptx_carry_decl"] = bool(re.search(r"\.local\s+\.align\s+16\s+\.b8\s+__c1_carry\[\d+\]", kptx))
        res["ptx_routed_calls"] = len(re.findall(r"call(\.uni)?\s+__(ret)?probe_func__\w+,\s*\(%c1_carry\)", kptx))
        res["ptx_plain_probe_calls"] = len(re.findall(r"call(\.uni)?\s+__(ret)?probe_func__\w+\s*;", kptx))
        kind = m.get("kind", "chk")
        if kind == "none":
            ok = res["entry_calls"] == 0 and res["exit_calls"] == 0 and not res["ptx_carry_decl"]
        elif m.get("program") == "value":
            # the timer helper call must survive in the entry probe (c_value
            # isolates its cost)
            res["entry_timer_calls"] = calls(funcs.get((k, "__probe_func__" + k), []),
                                             "$%s$_bpf_helper_ext_0502" % k)
            ok = res["entry_calls"] == 1 and res["exit_calls"] <= 1 and res["entry_timer_calls"] >= 1
            ok = ok and not res["ptx_carry_decl"] and res["ptx_routed_calls"] == 0
        elif m.get("program") == "empty":
            # ptxas may inline an empty probe (gate load and return): either
            # one call each or none, never a carry
            ok = res["entry_calls"] <= 1 and not res["ptx_carry_decl"] and res["ptx_routed_calls"] == 0
            res["probes_inlined"] = res["entry_calls"] == 0 and res["exit_calls"] == 0
        else:
            ok = res["entry_calls"] == (1 if m.get("has_entry", True) else 0) and res["exit_calls"] >= 1
            # every exit-probe call site of the PTX survives as its own CALL
            # (kmr: 7, one per exit form; nvcc's kernels: 1, nvcc merges returns)
            xs = m.get("call_sites", {}).get("__retprobe_func__" + k)
            if xs is not None:
                res["ptx_exit_sites"] = xs
                ok = ok and res["exit_calls"] == xs
            if kind == "wrapper":
                wr = "$%s$_bpf_helper_ext_0001_c1" % k
                res["entry_wrapper_calls"] = calls(funcs.get((k, "__probe_func__" + k), []), wr)
                res["exit_wrapper_calls"] = calls(funcs.get((k, "__retprobe_func__" + k), []), wr)
                ok = ok and res["entry_wrapper_calls"] >= 1 and res["exit_wrapper_calls"] >= 1
            if routed:
                ok = ok and res["ptx_carry_decl"] and res["ptx_plain_probe_calls"] == 0
                ok = ok and res["ptx_routed_calls"] >= (2 if m.get("has_entry", True) else 1)
                ok = ok and res["frame_reserve"] >= 1 and res["carry_addr"] >= 1
                # the valid mark: a store of zero before the first probe call
                first = next((i for i, l in enumerate(body) if "CALL" in l and
                              ("`(" + pe + ")" in l or "`(" + px + ")" in l)), None)
                mv = m.get("mark_value", 0)
                src = "RZ" if mv == 0 else r"R\d+"
                res["mark_before_first_call"] = first is not None and any(
                    re.search(r"\bST\S* \[R\d+(\.64)?\], " + src + r"\b", l) for l in body[:first])
                res["ptx_mark"] = bool(re.search(r"st\.u32\s+\[%c1_carry\], " + str(mv) +
                                                 r";\s*st\.u32\s+\[%c1_carry\+4\], 0;", kptx))
                if m.get("init_mark", True):
                    ok = ok and res["mark_before_first_call"] and res["ptx_mark"]
                else:  # the no-mark control must have no mark
                    ok = ok and not res["ptx_mark"]
                ent = funcs.get((k, "__probe_func__" + k), [])
                ext = funcs.get((k, "__retprobe_func__" + k), [])
                wr = "$%s$_bpf_helper_ext_0001_c1" % k
                res["entry_wrapper_calls"] = calls(ent, wr)
                res["exit_wrapper_calls"] = calls(ext, wr)
                res["entry_generic_st"] = sum(1 for l in ent if re.search(r"\bST\.E", l))
                res["exit_generic_ld"] = sum(1 for l in ext if re.search(r"\bLD\.E", l))
                res["wrapper_present"] = (k, "_bpf_helper_ext_0001_c1") in funcs
                he = m.get("has_entry", True)  # exit-only kernels: no entry function
                if m.get("direct"):
                    ok = ok and res["entry_wrapper_calls"] == 0 and res["exit_wrapper_calls"] == 0
                    ok = ok and (res["entry_generic_st"] >= 1 or not he) and res["exit_generic_ld"] >= 1
                else:
                    ok = ok and (res["entry_wrapper_calls"] >= 1 or not he) and res["exit_wrapper_calls"] >= 1
                    ok = ok and res["wrapper_present"]
            else:
                ok = ok and not res["ptx_carry_decl"] and res["ptx_routed_calls"] == 0
        res["ok"] = bool(ok)
        if not ok:
            fails.append(k)
        out.append(res)
        print("%-16s %s %s" % (k, "ok  " if ok else "FAIL", json.dumps(
            {x: y for x, y in res.items() if x not in ("kernel", "ok")})))
    json.dump({"kernels": out, "fails": fails}, open(os.path.join(bdir, "sass_check.json"), "w"), indent=1)
    print("sass check: ok" if not fails else "sass check: FAILED (%d) %s" % (len(fails), fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
