#!/usr/bin/env python3
"""C3 (step 0b): the reference module of step 0's e2-cost and its variants.

Everything that must be real pass output is made the way step 0's
e2-cost/ref/build_ref.py made it (that file's functions are copied below
unchanged, from DEV = ... down to run_pass): the dev build's kretprobe pass,
libptxpass_kretprobe.so, called through process_input() as nv_attach_impl
does, wrapped with the dev tree's checked-in trampoline_ptx.h and the probe
switch, retargeted to sm_87.

Three modules (every one also has ref_empty, the per-module launch baseline):

  refF.ptx  today's op, byte for byte step 0's ref1512.ptx (checked):
            ref_{host,dev}_r1 (exit probe), ref_{host,dev}_rn (loop of probe
            calls).
  refN.ptx  the same pass output with ONE change in a COPY of the trampoline
            text: the membar.sys of the 1512 branch of _bpf_helper_ext_0001
            removed (found by walking the helper's control flow from the
            "== 1512" branch; exactly one fence is reachable from there, and
            it is the one removed). Same kernel names as refF.
  refX.ptx  the original trampoline plus:
            E_r1 / E_rn         the pass's exit probe for an EMPTY eBPF program
                                (r0 = 0; exit): probe switch + probe call only.
            I_{h,d}_r1 / _rn    the threadhist pass output with the helper's
                                CALL replaced by the 1512 fast path inlined in
                                the probe function (map-type guard, key load
                                from the eBPF stack, 3-D thread id, both bounds
                                checks, address); no fence. Guard or bounds
                                failure: counted in __c3_inline_miss, result 0.
            IF_{h,d}_r1 / _rn   I plus the membar.sys where the helper has it.
            IC_{h,d}_r1 / _rn   I whose failure path is counted and then calls
                                the real helper (same behaviour as the helper
                                for every map type, except that the 1512 fast
                                path has no fence, like I).
            K_{h,d}_r1 / _rn    I's probe body pasted into the kernel (no probe
                                CALL); the probe switch, the 592-B eBPF frame
                                and the key store stay.
            M_r1 / M_rn         the pass output's memory operation alone:
                                generic ld.u32 x2, add, st.u32 x2 on
                                buf + 8 x (1-D thread id), buf = kernel param.
            M64_r1 / M64_rn     the same with one ld.u64 / st.u64.
            (h = host fd 5 = registered memory, d = dev fd 6 = device memory)

Usage: python3 -I build_variants.py OUT_DIR
Writes OUT_DIR/refF.ptx, refN.ptx, refX.ptx, variants.meta.json,
trampoline_nofence.diff, and the eBPF words.
"""
import ctypes
import difflib
import hashlib
import json
import os
import re
import struct
import sys

STEP0_REF_PTX = "/home/jetson/src/bpftime-gpu-work/step0/e2-cost/build/ref/ref1512.ptx"
STEP0_REF_META = "/home/jetson/src/bpftime-gpu-work/step0/e2-cost/build/ref/ref1512.meta.json"

# ---- copied unchanged from step0/e2-cost/ref/build_ref.py (lines 43-214) ----
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

# ---- end of the copy --------------------------------------------------------

EMPTY_EBPF = [0x00000000000000b7, 0x0000000000000095]  # r0 = 0; exit
HELPER_SIG = ".visible .func  (.param .b64 func_retval0) _bpf_helper_ext_0001("
C3_GLOBALS = "\n.visible .global .align 8 .u64 __c3_inline_miss;\n"


def sha256_text(s):
    return hashlib.sha256(s.encode()).hexdigest()


def sha256_file(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def func_span(text, header):
    """(start, end) of a PTX function whose header line starts with
    `header`; the body ends at the first column-0 '}' line."""
    assert text.count(header) == 1, "expected one %r" % header
    s = text.index(header)
    e = text.index("\n}\n", s) + 3
    return s, e


# ---- refN: remove the fence of the 1512 branch in a COPY of the trampoline --

LABEL = re.compile(r"^(\$L__BB\d+_\d+):\s*$")
CBRA = re.compile(r"^\s*@!?%p\d+\s+bra(?:\.uni)?\s+(\$L__BB\d+_\d+);")
UBRA = re.compile(r"^\s*bra(?:\.uni)?\s+(\$L__BB\d+_\d+);")


def helper_blocks(lines):
    """Basic blocks of a function body: list of (label or None, [line idx])."""
    blocks = [[None, []]]
    for i, ln in enumerate(lines):
        m = LABEL.match(ln)
        if m:
            blocks.append([m.group(1), []])
        else:
            blocks[-1][1].append(i)
    return blocks


def reachable_from(lines, start_label):
    blocks = helper_blocks(lines)
    index = {b[0]: n for n, b in enumerate(blocks) if b[0]}
    seen, todo = set(), [index[start_label]]
    while todo:
        n = todo.pop()
        if n in seen:
            continue
        seen.add(n)
        fall = True
        for i in blocks[n][1]:
            ln = lines[i]
            m = CBRA.match(ln)
            if m:
                todo.append(index[m.group(1)])
                continue
            m = UBRA.match(ln)
            if m:
                todo.append(index[m.group(1)])
                fall = False
                break
            if re.match(r"^\s*ret;", ln):
                fall = False
                break
        if fall and n + 1 < len(blocks):
            todo.append(n + 1)
    return [blocks[n] for n in sorted(seen)]


def remove_1512_fence(tramp):
    s, e = func_span(tramp, HELPER_SIG)
    body = tramp[s:e]
    lines = body.split("\n")
    m = re.search(r"setp\.eq\.s32\s+(%p\d+), %r\d+, 1512;\n\s*@\1 bra\s+(\$L__BB\d+_\d+);", body)
    assert m, "1512 dispatch not found in _bpf_helper_ext_0001"
    target = m.group(2)
    fences = []
    for label, idxs in reachable_from(lines, target):
        for i in idxs:
            if "membar" in lines[i] or "fence." in lines[i]:
                fences.append((label, i, lines[i].strip()))
    assert len(fences) == 1, "expected exactly one fence reachable from the 1512 branch: %r" % fences
    label, i, txt = fences[0]
    assert txt == "membar.sys;", txt
    assert lines[i - 1].strip() == "// begin inline asm" and lines[i + 1].strip() == "// end inline asm"
    new_lines = lines[:i - 1] + lines[i + 2:]
    info = {"branch_label": target, "fence_block": label, "removed": lines[i - 1:i + 2],
            "helper_line_in_trampoline_text": tramp[:s].count("\n") + i + 1}
    return tramp[:s] + "\n".join(new_lines) + tramp[e:], info


# ---- refX: inputs, inlined lookup, kernel-inlined probe, memory op alone -----

def r1_text(n):
    # same text as build_ref.kernel_input_ptx's r1()
    return ".visible .entry " + n + SIG.format(n=n) + "\n{\n\tret;\n}\n"


def rn_text(n, probe):
    # same text as build_ref.kernel_input_ptx's rn()
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


INLINE_VARIANTS = ("I", "IF", "IC")
MEMS = (("host", HOST_FD), ("dev", DEV_FD))


def x_input_ptx():
    parts = ["//\n// step0b c3-costparts: variant kernels (module X)\n//\n\n"
             ".version 8.1\n.target sm_61\n.address_size 64\n\n"]
    parts.append(r1_text("E_r1") + "\n" + rn_text("E_rn", "__retprobe_func__E_r1") + "\n")
    for v in INLINE_VARIANTS:
        for h, _ in MEMS:
            n = "%s_%s_r1" % (v, h)
            parts.append(r1_text(n) + "\n" + rn_text("%s_%s_rn" % (v, h), "__retprobe_func__" + n) + "\n")
    parts.append(r1_text("ref_empty"))
    return "".join(parts)


CALLSEQ = re.compile(r"\t\{ // callseq (\d+), 0\n.*?\n\t\} // callseq \1\n", re.S)
MEMOP_EXPECT = ["ld.u32", "ld.u32", "shl.b64", "or.b64", "add.s64", "st.u32", "shr.u64", "st.u32"]


def inline_block(variant, fd_reg, key_reg, res_reg, callseq, tag):
    """The 1512 branch of _bpf_helper_ext_0001 (trampoline PTX $L__BB2_31,
    $L__BB2_35, $L__BB2_36), written into the probe function: same loads from
    map_info, same 3-D thread id, same checks and address arithmetic."""
    fence = "\t// begin inline asm\n\tmembar.sys;\n\t// end inline asm\n" if variant == "IF" else ""
    if variant == "IC":
        slow_tail = callseq  # the real helper, as before (identical semantics)
    else:
        slow_tail = "\tmov.u64 \t%s, 0;\n" % res_reg
    return """	{ // c3 %(v)s: 1512 fast path of _bpf_helper_ext_0001 inlined
	.reg .pred 	%%c3p<5>;
	.reg .b32 	%%c3r<20>;
	.reg .b64 	%%c3d<24>;
	mul.lo.s64 	%%c3d1, %(fd)s, 40;
	mov.u64 	%%c3d2, map_info;
	add.s64 	%%c3d3, %%c3d2, %%c3d1;
	ld.const.u32 	%%c3r1, [%%c3d3+16];
	setp.ne.s32 	%%c3p1, %%c3r1, 1512;
	@%%c3p1 bra 	$C3_SLOW_%(t)s;
	ld.u32 	%%c3d4, [%(key)s];
	mov.u32 	%%c3r2, %%ctaid.x;
	mov.u32 	%%c3r3, %%ntid.x;
	mov.u32 	%%c3r4, %%tid.x;
	mad.lo.s32 	%%c3r5, %%c3r2, %%c3r3, %%c3r4;
	mov.u32 	%%c3r6, %%ctaid.y;
	mov.u32 	%%c3r7, %%ntid.y;
	mov.u32 	%%c3r8, %%tid.y;
	mad.lo.s32 	%%c3r9, %%c3r6, %%c3r7, %%c3r8;
	mov.u32 	%%c3r10, %%ctaid.z;
	mov.u32 	%%c3r11, %%ntid.z;
	mov.u32 	%%c3r12, %%tid.z;
	mad.lo.s32 	%%c3r13, %%c3r10, %%c3r11, %%c3r12;
	mov.u32 	%%c3r14, %%nctaid.x;
	mul.lo.s32 	%%c3r15, %%c3r14, %%c3r3;
	mov.u32 	%%c3r16, %%nctaid.y;
	mul.lo.s32 	%%c3r17, %%c3r16, %%c3r7;
	cvt.s64.s32 	%%c3d5, %%c3r13;
	mul.wide.s32 	%%c3d6, %%c3r17, %%c3r15;
	mul.lo.s64 	%%c3d7, %%c3d6, %%c3d5;
	mul.lo.s32 	%%c3r18, %%c3r9, %%c3r15;
	cvt.s64.s32 	%%c3d8, %%c3r18;
	cvt.s64.s32 	%%c3d9, %%c3r5;
	add.s64 	%%c3d10, %%c3d8, %%c3d9;
	add.s64 	%%c3d11, %%c3d10, %%c3d7;
	ld.const.u64 	%%c3d12, [%%c3d3+32];
	setp.lt.u64 	%%c3p2, %%c3d11, %%c3d12;
	ld.const.s32 	%%c3d13, [%%c3d3+12];
	setp.gt.u64 	%%c3p3, %%c3d13, %%c3d4;
	and.pred  	%%c3p4, %%c3p2, %%c3p3;
	@!%%c3p4 bra 	$C3_SLOW_%(t)s;
	ld.const.u64 	%%c3d14, [%%c3d3+24];
	ld.const.s32 	%%c3d15, [%%c3d3+8];
	mul.lo.s64 	%%c3d16, %%c3d12, %%c3d4;
	add.s64 	%%c3d17, %%c3d16, %%c3d11;
	mul.lo.s64 	%%c3d18, %%c3d17, %%c3d15;
	add.s64 	%(res)s, %%c3d18, %%c3d14;
%(fence)s	bra.uni 	$C3_DONE_%(t)s;
$C3_SLOW_%(t)s:
	mov.u64 	%%c3d19, __c3_inline_miss;
	atom.global.add.u64 	%%c3d20, [%%c3d19], 1;
%(slow)s$C3_DONE_%(t)s:
	}
""" % {"v": variant, "fd": fd_reg, "key": key_reg, "res": res_reg, "t": tag, "fence": fence,
       "slow": slow_tail}


def splice_inline(ptx, probe, variant, fd_value, tag):
    s, e = func_span(ptx, ".visible .func " + probe + "()")
    f = ptx[s:e]
    seqs = list(CALLSEQ.finditer(f))
    assert len(seqs) == 1, "%s: expected one callseq, found %d" % (probe, len(seqs))
    cs = seqs[0].group(0)
    assert "\t_bpf_helper_ext_0001, \n" in cs, "callee is not _bpf_helper_ext_0001"
    fd_reg = re.search(r"st\.param\.b64 \t\[param0\+0\], (%rd\d+);", cs).group(1)
    key_reg = re.search(r"st\.param\.b64 \t\[param1\+0\], (%rd\d+);", cs).group(1)
    res_reg = re.search(r"ld\.param\.b64 \t(%rd\d+), \[retval0\+0\];", cs).group(1)
    assert re.search(r"mov\.u64 \t%s, %d;\n" % (re.escape(fd_reg), fd_value), f), "fd %d not set" % fd_value
    # the memory operation after the lookup: exactly the JIT's u64 split
    after = f[seqs[0].end():]
    ops = re.findall(r"^\t(ld\.u32|ld\.u64|shl\.b64|or\.b64|add\.s64|st\.u32|st\.u64|shr\.u64)\s",
                     after, re.M)
    assert ops == MEMOP_EXPECT, ops
    nf = f[:seqs[0].start()] + inline_block(variant, fd_reg, key_reg, res_reg, cs, tag) + f[seqs[0].end():]
    return ptx[:s] + nf + ptx[e:], {"fd_reg": fd_reg, "key_reg": key_reg, "res_reg": res_reg,
                                     "callseq": seqs[0].group(1)}


def k_kernel(probe_func, name, loop):
    """Probe body (with the inlined lookup) pasted into a kernel: no CALL."""
    b0 = probe_func.index("{\n") + 2
    body = probe_func[b0:probe_func.rindex("\n}\n")]
    assert body.count("@%bpftime_off ret;") == 1
    body = body.replace("@%bpftime_off ret;", "@%bpftime_off bra.uni \t$K_NEXT;")
    assert re.search(r"\tret;\s*$", body), "probe body must end with ret"
    body = re.sub(r"\tret;\s*$", "", body)
    assert "ret;" not in body, "unexpected ret in probe body"
    head = ".visible .entry " + name + SIG.format(n=name) + "\n{\n"
    if not loop:
        return head + "\t{\n" + body + "\t}\n$K_NEXT:\n\tret;\n}\n"
    return head + """	.reg .pred 	%kp<3>;
	.reg .b32 	%kr<4>;

	ld.param.u32 	%kr1, [""" + name + """_param_2];
	mov.u32 	%kr2, 0;
	setp.eq.s32 	%kp1, %kr1, 0;
	@%kp1 bra 	$K_DONE;
$K_LOOP:
	{
""" + body + """	}
$K_NEXT:
	add.s32 	%kr2, %kr2, 1;
	setp.lt.u32 	%kp2, %kr2, %kr1;
	@%kp2 bra 	$K_LOOP;
$K_DONE:
	ret;
}
"""


def m_kernel(name, loop, wide):
    """The pass output's memory operation alone, at buf + 8 x thread id."""
    if wide:
        op = """	ld.u64 	%rd13, [%rd9];
	add.s64 	%rd14, %rd13, 1;
	st.u64 	[%rd9], %rd14;
"""
    else:  # the same instructions as the pass output (MEMOP_EXPECT)
        op = """	ld.u32 	%rd10, [%rd9];
	ld.u32 	%rd11, [%rd9+4];
	shl.b64 	%rd12, %rd11, 32;
	or.b64  	%rd13, %rd12, %rd10;
	add.s64 	%rd14, %rd13, 1;
	st.u32 	[%rd9], %rd14;
	shr.u64 	%rd15, %rd14, 32;
	st.u32 	[%rd9+4], %rd15;
"""
    head = ".visible .entry " + name + SIG.format(n=name) + """
{
	.reg .pred 	%p<3>;
	.reg .b32 	%r<8>;
	.reg .b64 	%rd<16>;

	ld.param.u64 	%rd1, [""" + name + """_param_0];
	mov.u32 	%r3, %ctaid.x;
	mov.u32 	%r4, %ntid.x;
	mov.u32 	%r5, %tid.x;
	mad.lo.s32 	%r6, %r3, %r4, %r5;
	mul.wide.u32 	%rd2, %r6, 8;
	add.s64 	%rd9, %rd1, %rd2;
"""
    if not loop:
        return head + op + "\tret;\n}\n"
    # Loop form: the address advances by 8 x k per iteration; k is a kernel
    # parameter, 0 at run time, so every iteration updates the same word, but
    # ptxas cannot prove it and keeps each iteration's loads and stores (with
    # a constant address it forwarded values across iterations).
    return head + """	ld.param.u32 	%r1, [""" + name + """_param_2];
	ld.param.u32 	%r7, [""" + name + """_param_3];
	mul.wide.u32 	%rd3, %r7, 8;
	mov.u32 	%r2, 0;
	setp.eq.s32 	%p1, %r1, 0;
	@%p1 bra 	$L__done;
$L__loop:
""" + op + """	add.s64 	%rd9, %rd9, %rd3;
	add.s32 	%r2, %r2, 1;
	setp.lt.u32 	%p2, %r2, %r1;
	@%p2 bra 	$L__loop;
$L__done:
	ret;
}
"""


def wrap(tramp, passes):
    return rewrite_ptx_target(filter_out_version_headers_ptx(tramp + PROBE_SWITCH_PTX + passes), SM_ARCH)


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    lib = ctypes.CDLL(PASS_SO)
    lib.process_input.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
    lib.process_input.restype = ctypes.c_int
    sec, words_host, lddw_idx = ebpf_words_from_object(BPF_O, HOST_FD)
    _, words_dev, _ = ebpf_words_from_object(BPF_O, DEV_FD)
    words = {HOST_FD: words_host, DEV_FD: words_dev}
    tramp = trampoline_ptx()
    meta = {"pass_library": PASS_SO, "pass_library_sha256": sha256_file(PASS_SO),
            "trampoline_h": TRAMPOLINE_H, "trampoline_h_sha256": sha256_file(TRAMPOLINE_H),
            "bpf_object": BPF_O, "bpf_object_sha256": sha256_file(BPF_O),
            "section": sec, "lddw_index": lddw_idx, "host_fd": HOST_FD, "dev_fd": DEV_FD,
            "sm_arch": SM_ARCH}

    # F: step 0's module, rebuilt the same way
    ptx_in = kernel_input_ptx()
    passF = run_pass(lib, ptx_in, "ref_host_r1", words_host)
    passF = run_pass(lib, passF, "ref_dev_r1", words_dev)
    F = wrap(tramp, passF)
    open(os.path.join(out_dir, "refF.ptx"), "w").write(F)
    step0 = open(STEP0_REF_PTX).read() if os.path.exists(STEP0_REF_PTX) else None
    meta["refF_identical_to_step0_ref1512_ptx"] = (step0 == F) if step0 is not None else "step0 file missing"
    if os.path.exists(STEP0_REF_META):
        m0 = json.load(open(STEP0_REF_META))
        meta["inputs_same_as_step0"] = all(m0.get(k) == meta[k] for k in
                                           ("pass_library_sha256", "trampoline_h_sha256", "bpf_object_sha256"))

    # N: the same pass output, trampoline copy without the 1512 fence
    trampN, info = remove_1512_fence(tramp)
    N = wrap(trampN, passF)
    open(os.path.join(out_dir, "refN.ptx"), "w").write(N)
    diff = list(difflib.unified_diff(F.split("\n"), N.split("\n"), "refF.ptx", "refN.ptx", lineterm="", n=3))
    open(os.path.join(out_dir, "trampoline_nofence.diff"), "w").write("\n".join(diff) + "\n")
    minus = [l for l in diff if l.startswith("-") and not l.startswith("---")]
    plus = [l for l in diff if l.startswith("+") and not l.startswith("+++")]
    assert len(minus) == 3 and not plus, (minus, plus)
    info["diff_removed_lines"] = minus
    meta["refN"] = info

    # X: empty-program probe, inlined lookups, kernel-inlined probe, memop
    p = run_pass(lib, x_input_ptx(), "E_r1", EMPTY_EBPF)
    splices = {}
    for v in INLINE_VARIANTS:
        for h, fd in MEMS:
            p = run_pass(lib, p, "%s_%s_r1" % (v, h), words[fd])
    for v in INLINE_VARIANTS:
        for h, fd in MEMS:
            probe = "__retprobe_func__%s_%s_r1" % (v, h)
            p, sinfo = splice_inline(p, probe, v, fd, "%s_%s" % (v, h))
            splices[probe] = sinfo
    extra = []
    for h, fd in MEMS:
        s, e = func_span(p, ".visible .func __retprobe_func__I_%s_r1()" % h)
        extra.append(k_kernel(p[s:e], "K_%s_r1" % h, False))
        extra.append(k_kernel(p[s:e], "K_%s_rn" % h, True))
    for w, nm in ((False, "M"), (True, "M64")):
        extra.append(m_kernel(nm + "_r1", False, w))
        extra.append(m_kernel(nm + "_rn", True, w))
    X = wrap(tramp + C3_GLOBALS, p) + "\n" + "\n".join(extra)
    open(os.path.join(out_dir, "refX.ptx"), "w").write(X)
    meta["refX_splices"] = splices
    with open(os.path.join(out_dir, "ebpf.txt"), "w") as f:
        f.write("# section %s, LDDW at insn %d patched to src=1 imm=fd\n" % (sec, lddw_idx))
        for tag, ws in (("host fd %d" % HOST_FD, words_host), ("dev fd %d" % DEV_FD, words_dev),
                        ("empty program (E)", EMPTY_EBPF)):
            f.write("# %s\n" % tag)
            for w in ws:
                f.write("%016x\n" % w)
    for n, t in (("refF.ptx", F), ("refN.ptx", N), ("refX.ptx", X)):
        meta[n + "_sha256"] = sha256_text(t)
    json.dump(meta, open(os.path.join(out_dir, "variants.meta.json"), "w"), indent=1)
    print("refF identical to step 0's ref1512.ptx:", meta["refF_identical_to_step0_ref1512_ptx"])
    print("refN: removed", info["removed"][1].strip(), "in block", info["fence_block"],
          "(reached from the 1512 branch", info["branch_label"] + ")")
    print("refX: spliced", len(splices), "probe functions;", len(extra), "generated kernels")


if __name__ == "__main__":
    main()
