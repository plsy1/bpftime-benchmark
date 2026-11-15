#!/usr/bin/env python3
"""Build today's EMPTY bpftime probe pair offline, as bpftime builds probes (check C2's
"empty-probe baseline", real pass output).

Adapted from step0/e2-cost/ref/build_ref.py (same pass driving, same assembly):
  - the dev build's kprobe_entry and kretprobe pass libraries, called through their exported
    process_input() as nv_attach_impl.cpp does (JSON RuntimeRequest in, JSON RuntimeResponse out);
  - the eBPF program is the empty one: `r0 = 0; exit` (BPF_ALU64|BPF_MOV|BPF_K, BPF_JMP|BPF_EXIT);
  - assembly as nv_attach_impl::hack_fatbin: rewrite_ptx_target(filter_out_version_headers_ptx(
    TRAMPOLINE_PTX + PROBE_SWITCH_PTX + <PTX after every pass>), "sm_87").

Kernels (signature (u64 aux, u32 mode), like the c2bench cost kernels):
  rp_pair   entry probe (kprobe_entry pass) + exit probe (kretprobe pass)
  rp_entry  entry probe only
  rp_exit   exit probe only
  rp_empty  unpatched, empty (the delta reference of this module)

Usage: python3 -I build_refprobe.py OUT_DIR  -> OUT_DIR/refprobe.ptx, refprobe.input.ptx,
refprobe.meta.json. Reads the dev tree only.
"""
import ctypes
import hashlib
import json
import os
import sys

DEV = "/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate"
PASS_DIR = DEV + "/build-cuda/attach/nv_attach_impl/pass"
ENTRY_SO = PASS_DIR + "/ptxpass_kprobe_entry/libptxpass_kprobe_entry.so"
RET_SO = PASS_DIR + "/ptxpass_kretprobe/libptxpass_kretprobe.so"
TRAMPOLINE_H = DEV + "/attach/nv_attach_impl/trampoline_ptx.h"
SM_ARCH = "sm_87"
PROBE_SWITCH_PTX = "\n.visible .global .align 4 .u32 __bpftime_probes_enabled = 1;\n"
# r0 = 0 (opcode 0xb7, dst 0, imm 0); exit (opcode 0x95)
EMPTY_EBPF = [0x00000000000000B7, 0x0000000000000095]


def trampoline_ptx():
    text = open(TRAMPOLINE_H).read()
    start = text.index('R"(') + 3
    end = text.rindex(')";')
    return text[start:end]


def filter_out_version_headers_ptx(ptx):
    # ptxpass_core/src/core.cpp: keep the first line starting with each of
    # .version / .target / .address_size / "//", drop the later ones.
    prefixes = [".version", ".target", ".address_size", "//"]
    seen = set()
    out = []
    for line in ptx.split("\n"):
        skip = False
        for p in prefixes:
            if line.startswith(p):
                if p in seen:
                    skip = True
                else:
                    seen.add(p)
                break
        if not skip:
            out.append(line)
    return "\n".join(out) + "\n"


def rewrite_ptx_target(ptx, sm_arch):
    pos = ptx.find(".target")
    if pos < 0:
        return ptx
    pos += len(".target")
    while pos < len(ptx) and ptx[pos] in " \t":
        pos += 1
    start = pos
    while pos < len(ptx) and ptx[pos] not in " \t\n\r,":
        pos += 1
    return ptx[:start] + sm_arch + ptx[pos:]


def kernel_input_ptx():
    def k(n):
        return (".visible .entry " + n + "(\n\t.param .u64 " + n + "_param_0,\n\t.param .u32 " + n +
                "_param_1\n)\n{\n\tret;\n}\n")
    return ("//\n// step0b c2-claim: reference kernels for the empty probe pair\n//\n\n"
            ".version 8.1\n.target sm_61\n.address_size 64\n\n"
            + k("rp_pair") + "\n" + k("rp_entry") + "\n" + k("rp_exit") + "\n" + k("rp_empty"))


def run_pass(lib, ptx, kernel, words):
    req = {
        "input": {
            "full_ptx": ptx,
            "to_patch_kernel": kernel,
            "global_ebpf_map_info_symbol": "map_info",
            "ebpf_communication_data_symbol": "constData",
        },
        "ebpf_instructions": [
            {"upper_32bit": w >> 32, "lower_32bit": w & 0xffffffff} for w in words
        ],
    }
    inp = json.dumps(req).encode()
    size = 8 << 20
    buf = ctypes.create_string_buffer(size)
    rc = lib.process_input(inp, size, buf)
    if rc != 0:
        raise SystemExit("process_input(%s) returned %d" % (kernel, rc))
    resp = json.loads(buf.value.decode())
    if not resp.get("modified"):
        raise SystemExit("pass did not modify " + kernel)
    return resp["output_ptx"]


def load(path):
    lib = ctypes.CDLL(path)
    lib.process_input.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
    lib.process_input.restype = ctypes.c_int
    return lib


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    entry, ret = load(ENTRY_SO), load(RET_SO)
    ptx_in = kernel_input_ptx()
    open(os.path.join(out_dir, "refprobe.input.ptx"), "w").write(ptx_in)
    ptx = run_pass(entry, ptx_in, "rp_pair", EMPTY_EBPF)
    ptx = run_pass(ret, ptx, "rp_pair", EMPTY_EBPF)
    ptx = run_pass(entry, ptx, "rp_entry", EMPTY_EBPF)
    ptx = run_pass(ret, ptx, "rp_exit", EMPTY_EBPF)
    final = rewrite_ptx_target(
        filter_out_version_headers_ptx(trampoline_ptx() + PROBE_SWITCH_PTX + ptx), SM_ARCH)
    open(os.path.join(out_dir, "refprobe.ptx"), "w").write(final)

    def sha(p):
        return hashlib.sha256(open(p, "rb").read()).hexdigest()
    meta = {
        "entry_pass": ENTRY_SO, "entry_pass_sha256": sha(ENTRY_SO),
        "ret_pass": RET_SO, "ret_pass_sha256": sha(RET_SO),
        "trampoline_h": TRAMPOLINE_H, "trampoline_h_sha256": sha(TRAMPOLINE_H),
        "ebpf": ["%016x" % w for w in EMPTY_EBPF], "sm_arch": SM_ARCH,
    }
    json.dump(meta, open(os.path.join(out_dir, "refprobe.meta.json"), "w"), indent=1)
    print("wrote", os.path.join(out_dir, "refprobe.ptx"), len(final), "bytes")


if __name__ == "__main__":
    main()
