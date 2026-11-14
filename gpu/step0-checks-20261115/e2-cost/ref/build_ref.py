#!/usr/bin/env python3
"""Build today's 1512 lookup-and-increment as bpftime builds it, offline.

Inputs (read only):
  - threadhist.bpf.o of example/gpu/threadhist-llama (the probe the llama.cpp
    census timed: lookup of key 0 in a per-thread array, then *cnt += 1);
  - the dev build's kretprobe pass library (libptxpass_kretprobe.so), called
    through its exported process_input() exactly as nv_attach_impl.cpp does
    (JSON RuntimeRequest in, JSON RuntimeResponse out);
  - the dev tree's checked-in trampoline PTX (attach/nv_attach_impl/
    trampoline_ptx.h, the clang build of trampoline/default_trampoline.cu).

Assembly, as nv_attach_impl::hack_fatbin + nv_attach_fatbin_record do it:
  out = rewrite_ptx_target(
          filter_out_version_headers_ptx(
            TRAMPOLINE_PTX + PROBE_SWITCH_PTX + <PTX after every pass>),
          "sm_87")

The eBPF program is the .o's kretprobe section with its one map relocation
resolved the way libbpf does for BPF_PROG_LOAD (LDDW src_reg =
BPF_PSEUDO_MAP_FD, imm = map fd). bpftime's JIT then emits the fd itself as
the helper's map argument, and the helper indexes map_info[fd].

Kernels in the module (all with the bench signature
(u64 buf, u64 mask, u32 R, u32 k, u64 aux, u32 mode)):
  ref_host_r1  exit probe inserted by the pass (fd HOST_FD -> registered map)
  ref_dev_r1   exit probe inserted by the pass (fd DEV_FD -> device memory)
  ref_host_rn  calls __retprobe_func__ref_host_r1 R times (no exit probe)
  ref_dev_rn   calls __retprobe_func__ref_dev_r1 R times
  ref_empty    unpatched, empty

Usage: python3 -I build_ref.py OUT_DIR
Writes OUT_DIR/ref1512.ptx, OUT_DIR/ref1512.input.ptx,
OUT_DIR/ref1512.ebpf.txt, OUT_DIR/ref1512.meta.json.
"""
import ctypes
import hashlib
import json
import os
import struct
import sys

DEV = "/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate"
PASS_SO = DEV + "/build-cuda/attach/nv_attach_impl/pass/ptxpass_kretprobe/libptxpass_kretprobe.so"
TRAMPOLINE_H = DEV + "/attach/nv_attach_impl/trampoline_ptx.h"
BPF_O = "/home/jetson/src/bpftime-gpu-work/example/gpu/threadhist-llama/.output/threadhist.bpf.o"
HOST_FD = 5
DEV_FD = 6
SM_ARCH = "sm_87"
# nv_attach_utils.cpp:23-24
PROBE_SWITCH_PTX = "\n.visible .global .align 4 .u32 __bpftime_probes_enabled = 1;\n"


def read_elf_sections(path):
    data = open(path, "rb").read()
    assert data[:4] == b"\x7fELF" and data[4] == 2 and data[5] == 1, "ELF64 LE expected"
    e_shoff, = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    secs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        (name, typ, flags, addr, offset, size, link, info, align,
         entsize) = struct.unpack_from("<IIQQQQIIQQ", data, off)
        secs.append(dict(name_off=name, type=typ, offset=offset, size=size,
                         link=link, info=info, entsize=entsize))
    shstr = secs[e_shstrndx]
    for s in secs:
        start = shstr["offset"] + s["name_off"]
        end = data.index(b"\0", start)
        s["name"] = data[start:end].decode()
        s["data"] = data[s["offset"]:s["offset"] + s["size"]]
    return secs


def ebpf_words_from_object(path, fd):
    secs = read_elf_sections(path)
    progs = [i for i, s in enumerate(secs) if s["name"].startswith("kretprobe/")]
    assert len(progs) == 1, "one kretprobe section expected"
    pi = progs[0]
    insns = bytearray(secs[pi]["data"])
    symtab = [s for s in secs if s["type"] == 2][0]
    strtab = secs[symtab["link"]]

    def sym_name(idx):
        off = idx * 24
        st_name, = struct.unpack_from("<I", symtab["data"], off)
        end = strtab["data"].index(b"\0", st_name)
        return strtab["data"][st_name:end].decode()

    relocs = []
    for s in secs:
        if s["type"] == 9 and s["info"] == pi:  # SHT_REL for the program
            for j in range(s["size"] // 16):
                r_offset, r_info = struct.unpack_from("<QQ", s["data"], j * 16)
                relocs.append((r_offset, r_info >> 32, r_info & 0xffffffff))
    assert len(relocs) == 1, relocs
    r_offset, sym, rtype = relocs[0]
    assert sym_name(sym) == "call_count", sym_name(sym)
    i = r_offset // 8
    assert insns[r_offset] == 0x18, "relocation must be on an LDDW"
    # libbpf: insn[0].src_reg = BPF_PSEUDO_MAP_FD (1), insn[0].imm = map fd
    insns[r_offset + 1] = (insns[r_offset + 1] & 0x0F) | (1 << 4)
    struct.pack_into("<i", insns, r_offset + 4, fd)
    words = [int.from_bytes(insns[k:k + 8], "little") for k in range(0, len(insns), 8)]
    return secs[pi]["name"], words, i


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
    # nv_attach_utils.cpp rewrite_ptx_target: .version only changes for
    # sm_100 and newer; the first .target gets sm_arch.
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


SIG = """(
	.param .u64 {n}_param_0,
	.param .u64 {n}_param_1,
	.param .u32 {n}_param_2,
	.param .u32 {n}_param_3,
	.param .u64 {n}_param_4,
	.param .u32 {n}_param_5
)"""


def kernel_input_ptx():
    def r1(n):
        return ".visible .entry " + n + SIG.format(n=n) + "\n{\n\tret;\n}\n"

    def rn(n, probe):
        return (".visible .entry " + n + SIG.format(n=n) + """
{
	.reg .pred 	%p<3>;
	.reg .b32 	%r<4>;

	ld.param.u32 	%r1, [""" + n + """_param_2];
	mov.u32 	%r2, 0;
	setp.eq.s32 	%p1, %r1, 0;
	@%p1 bra 	$L__done;
$L__loop:
	call """ + probe + """;
	add.s32 	%r2, %r2, 1;
	setp.lt.u32 	%p2, %r2, %r1;
	@%p2 bra 	$L__loop;
$L__done:
	ret;
}
""")

    return ("//\n// step0 e2-cost: reference kernels for the 1512 path\n//\n\n"
            ".version 8.1\n.target sm_61\n.address_size 64\n\n"
            + r1("ref_host_r1") + "\n" + r1("ref_dev_r1") + "\n"
            + rn("ref_host_rn", "__retprobe_func__ref_host_r1") + "\n"
            + rn("ref_dev_rn", "__retprobe_func__ref_dev_r1") + "\n"
            + r1("ref_empty"))


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


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    lib = ctypes.CDLL(PASS_SO)
    lib.process_input.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
    lib.process_input.restype = ctypes.c_int
    sec, words_host, lddw_idx = ebpf_words_from_object(BPF_O, HOST_FD)
    _, words_dev, _ = ebpf_words_from_object(BPF_O, DEV_FD)
    ptx_in = kernel_input_ptx()
    open(os.path.join(out_dir, "ref1512.input.ptx"), "w").write(ptx_in)
    ptx = run_pass(lib, ptx_in, "ref_host_r1", words_host)
    ptx = run_pass(lib, ptx, "ref_dev_r1", words_dev)
    final = rewrite_ptx_target(
        filter_out_version_headers_ptx(trampoline_ptx() + PROBE_SWITCH_PTX + ptx),
        SM_ARCH)
    open(os.path.join(out_dir, "ref1512.ptx"), "w").write(final)
    with open(os.path.join(out_dir, "ref1512.ebpf.txt"), "w") as f:
        f.write("# section %s, LDDW at insn %d patched to src=1 imm=fd\n" % (sec, lddw_idx))
        for tag, words in (("host fd %d" % HOST_FD, words_host), ("dev fd %d" % DEV_FD, words_dev)):
            f.write("# %s\n" % tag)
            for w in words:
                f.write("%016x\n" % w)
    meta = {
        "bpf_object": BPF_O,
        "bpf_object_sha256": hashlib.sha256(open(BPF_O, "rb").read()).hexdigest(),
        "pass_library": PASS_SO,
        "pass_library_sha256": hashlib.sha256(open(PASS_SO, "rb").read()).hexdigest(),
        "trampoline_h": TRAMPOLINE_H,
        "trampoline_h_sha256": hashlib.sha256(open(TRAMPOLINE_H, "rb").read()).hexdigest(),
        "host_fd": HOST_FD,
        "dev_fd": DEV_FD,
        "sm_arch": SM_ARCH,
        "section": sec,
    }
    json.dump(meta, open(os.path.join(out_dir, "ref1512.meta.json"), "w"), indent=1)
    print("wrote", os.path.join(out_dir, "ref1512.ptx"), len(final), "bytes")


if __name__ == "__main__":
    main()
