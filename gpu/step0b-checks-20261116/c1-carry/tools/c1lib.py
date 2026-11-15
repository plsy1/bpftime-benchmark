"""c1lib: shared code of check C1 (carry from entry to exit probe in .local).

Everything here runs on the CPU. The dev tree is read only: its pass
libraries are loaded with ctypes and called through process_input(), as
nv_attach_impl does (JSON RuntimeRequest in, JSON RuntimeResponse out), and
its checked-in trampoline PTX is read from trampoline_ptx.h.

Parts:
  ELF       read_elf_sections, program_words (map relocations -> fds, as
            libbpf does for BPF_PROG_LOAD: LDDW src_reg = 1, imm = fd)
  passes    run_pass (kprobe_entry / kretprobe)
  assembly  trampoline_ptx, filter_out_version_headers_ptx, rewrite_ptx_target
            (copies of step0/e2-cost/ref/build_ref.py, which copies
            ptxpass_core/src/core.cpp and nv_attach_utils.cpp)
  routing   route_kernel (kernel-scope .local + its generic address passed to
            every probe call), route_probe_function (the probe function takes
            the address; its helper-1 calls on new map types are redirected)
"""
import ctypes
import json
import re
import struct

DEV = "/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate"
PASS_DIR = DEV + "/build-cuda/attach/nv_attach_impl/pass"
PASS_SO = {
    "kprobe": PASS_DIR + "/ptxpass_kprobe_entry/libptxpass_kprobe_entry.so",
    "kretprobe": PASS_DIR + "/ptxpass_kretprobe/libptxpass_kretprobe.so",
}
TRAMPOLINE_H = DEV + "/attach/nv_attach_impl/trampoline_ptx.h"
# nv_attach_utils.cpp:23-24
PROBE_SWITCH_PTX = "\n.visible .global .align 4 .u32 __bpftime_probes_enabled = 1;\n"
SM_ARCH = "sm_87"

# New map types of this check (not in the dev tree). Their lookups are served
# by _bpf_helper_ext_0001_c1 (src/c1_helpers.cu) or, in the direct form, by
# the probe function itself.
MT_CARRY = 1540     # the thread's own carry: kernel-scope .local
MT_FRAME = 1541     # UNSOUND on purpose: the helper's own frame
MT_SLOTTAG = 1542   # (SM, warp slot, lane) slot in device memory + owner tag
WRAPPER = "_bpf_helper_ext_0001_c1"
HELPER1 = "_bpf_helper_ext_0001"


# ---------------------------------------------------------------- ELF
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


def program_words(path, section_prefix, fds):
    """eBPF words of the one section starting with section_prefix, every map
    relocation resolved to fds[map symbol name]. Returns (section, words,
    {symbol: [insn index, ...]})."""
    secs = read_elf_sections(path)
    progs = [i for i, s in enumerate(secs) if s["name"].startswith(section_prefix)]
    assert len(progs) == 1, (path, section_prefix, [s["name"] for s in secs])
    pi = progs[0]
    insns = bytearray(secs[pi]["data"])
    symtab = [s for s in secs if s["type"] == 2][0]
    strtab = secs[symtab["link"]]

    def sym_name(idx):
        st_name, = struct.unpack_from("<I", symtab["data"], idx * 24)
        end = strtab["data"].index(b"\0", st_name)
        return strtab["data"][st_name:end].decode()

    used = {}
    for s in secs:
        if s["type"] == 9 and s["info"] == pi:  # SHT_REL of this program
            for j in range(s["size"] // 16):
                r_offset, r_info = struct.unpack_from("<QQ", s["data"], j * 16)
                name = sym_name(r_info >> 32)
                if name not in fds:
                    raise SystemExit("%s: no fd for map %s" % (path, name))
                assert insns[r_offset] == 0x18, "relocation must be on an LDDW"
                insns[r_offset + 1] = (insns[r_offset + 1] & 0x0F) | (1 << 4)
                struct.pack_into("<i", insns, r_offset + 4, fds[name])
                used.setdefault(name, []).append(r_offset // 8)
    words = [int.from_bytes(insns[k:k + 8], "little") for k in range(0, len(insns), 8)]
    return secs[pi]["name"], words, used


# ---------------------------------------------------------------- passes
_libs = {}


def run_pass(kind, ptx, kernel, words):
    """One dev pass on one kernel, as nv_attach_impl calls it."""
    if kind not in _libs:
        lib = ctypes.CDLL(PASS_SO[kind])
        lib.process_input.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
        lib.process_input.restype = ctypes.c_int
        _libs[kind] = lib
    lib = _libs[kind]
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
    size = 64 << 20
    buf = ctypes.create_string_buffer(size)
    rc = lib.process_input(inp, size, buf)
    if rc != 0:
        raise SystemExit("%s process_input(%s) returned %d" % (kind, kernel, rc))
    resp = json.loads(buf.value.decode())
    if not resp.get("modified"):
        raise SystemExit("%s pass did not modify %s" % (kind, kernel))
    return resp["output_ptx"]


# ---------------------------------------------------------------- assembly
def trampoline_ptx():
    text = open(TRAMPOLINE_H).read()
    start = text.index('R"(') + 3
    end = text.rindex(')";')
    return text[start:end]


def filter_out_version_headers_ptx(ptx):
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


def rewrite_ptx_target(ptx, sm_arch=SM_ARCH):
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


def strip_headers(ptx):
    """Drop .version/.target/.address_size lines (for PTX appended after the
    trampoline, which supplies the module's header)."""
    return "\n".join(l for l in ptx.split("\n")
                     if not re.match(r"\s*\.(version|target|address_size)\b", l)) + "\n"


# ---------------------------------------------------------------- routing
def find_entry_body(ptx, kernel):
    """(start of '{', end after matching '}') of kernel's body."""
    m = re.search(r"(\.visible\s+)?\.entry\s+" + re.escape(kernel) + r"\s*\(", ptx)
    if not m:
        raise SystemExit("kernel %s not found" % kernel)
    close = ptx.index(")", m.end())
    brace = ptx.index("{", close)
    depth = 0
    i = brace
    while True:
        c = ptx[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return brace, i + 1
        i += 1


CARRY_SYM = "__c1_carry"
CARRY_REG = "%c1_carry"


def route_kernel(ptx, kernel, carry_bytes, probe_names, init=True, mark=0):
    """Kernel side of the routing. Declares a kernel-scope .local of
    carry_bytes (PTX ISA 5.1.5: with the ABI, .local variables are declared
    in function scope and allocated on the stack, so the kernel's frame holds
    it for the thread's whole run), and rewrites every
        [@p] call <probe>;
    for probe in probe_names into
        cvta.local.u64 %c1_carry, __c1_carry;
        [@p] call <probe>, (%c1_carry);
    (PTX ISA 7.1: call arguments may be .reg variables). The address is
    recomputed at every call site, so no register stays live across the
    kernel body. With init, the kernel's first instructions zero the carry's
    first 8 bytes (generic st.u32 x 2 through the carry's generic address),
    before the entry probe runs: the "valid mark" a design can check, so that an exit whose entry did not run (probe
    switch flipped between them, no entry program) never reads stale bytes
    as valid. mark (validation only, part c's twin c_local_d_noentry_mark1):
    the value stored in the first 32-bit half instead of 0, so that an
    exit-only kernel must read it back as valid in every thread; together
    with the mark-0 kernel this shows deterministically that the exit reads
    what the prologue wrote. Returns (ptx, {probe: number of call sites})."""
    b0, b1 = find_entry_body(ptx, kernel)
    body = ptx[b0:b1]
    counts = {}
    for name in probe_names:
        pat = re.compile(r"(@!?%\w+\s+)?call(\.uni)?\s+" + re.escape(name) + r"\s*;")
        n = len(pat.findall(body))
        counts[name] = n
        body = pat.sub(lambda m: "cvta.local.u64 \t%s, %s;\n\t%scall%s %s, (%s);" % (
            CARRY_REG, CARRY_SYM, m.group(1) or "", m.group(2) or "", name, CARRY_REG), body)
    decl = "\n\t.local .align 16 .b8 \t%s[%d];\n\t.reg .b64 \t%s;\n" % (
        CARRY_SYM, carry_bytes, CARRY_REG)
    if init:
        # through the same generic address the probes use, in the 32-bit
        # halves the JIT emits for eBPF's 64-bit accesses: every write and
        # read of the mark then overlaps completely, via the generic proxy
        # (PTX ISA 8.6, 8.7)
        decl += ("\tcvta.local.u64 \t%s, %s;\n\tst.u32 \t[%s], %d;\n\tst.u32 \t[%s+4], 0;\n"
                 % (CARRY_REG, CARRY_SYM, CARRY_REG, int(mark), CARRY_REG))
    body = body[:1] + decl + body[1:]
    return ptx[:b0] + body + ptx[b1:], counts


def find_func(ptx, fname):
    """(header start, body '{' index, body end) of .func fname."""
    m = re.search(r"\.visible\s+\.func\s+" + re.escape(fname) + r"\s*\(", ptx)
    if not m:
        raise SystemExit("function %s not found" % fname)
    close = ptx.index(")", m.end())
    brace = ptx.index("{", close)
    depth = 0
    i = brace
    while True:
        c = ptx[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return m.start(), m.end(), close, brace, i + 1
        i += 1


CALLSEQ = re.compile(r"\{ // callseq \d+, \d+\n.*?\} // callseq \d+\n", re.S)


def _fd_of_call(fbody, block):
    """The map fd a helper-1 call passes (param0), as a constant, or None."""
    m = re.search(r"st\.param\.b64\s+\[param0\+0\],\s*([^;]+);", block)
    if not m:
        return None
    arg = m.group(1).strip()
    if re.fullmatch(r"-?\d+", arg):
        return int(arg)
    defs = re.findall(r"mov\.[ub]64\s+" + re.escape(arg) + r",\s*(-?\d+)\s*;", fbody)
    if len(defs) == 1:
        return int(defs[0])
    return None


def route_probe_function(ptx, fname, routed, map_types, direct):
    """Probe-function side. routed: the function gets a .param .b64 (the
    carry's generic address). Every helper-1 call whose map argument is a
    constant fd with map_types[fd] in (MT_CARRY, MT_FRAME, MT_SLOTTAG) is
    redirected:
      - direct (MT_CARRY only, single-entry carry map): the call is removed
        and its result is the carry address itself;
      - otherwise: the call goes to WRAPPER with param4 (eBPF r5, unused by
        every lookup) set to the carry address (0 when not routed).
    Calls on other maps are left alone. Every helper-1 call's fd must be a
    constant (else SystemExit). Returns (ptx, stats)."""
    h0, h1, close, brace, end = find_func(ptx, fname)
    params = ptx[h1:close]
    if params.strip():
        raise SystemExit("%s already has parameters: %r" % (fname, params))
    pname = fname + "_c1carry"
    head = ptx[h0:h1] + ("\n\t.param .b64 %s\n" % pname if routed else "") + ")"
    fbody = ptx[brace:end]
    stats = {"helper1_calls": 0, "redirected": 0, "direct": 0, "fds": []}
    carry_in = "%c1_carry_in"

    def fix(m):
        block = m.group(0)
        if not re.search(r"\b" + HELPER1 + r",", block):
            return block
        stats["helper1_calls"] += 1
        fd = _fd_of_call(fbody, block)
        if fd is None:
            raise SystemExit("%s: helper-1 call with a non-constant map fd" % fname)
        stats["fds"].append(fd)
        mt = map_types.get(fd)
        if mt not in (MT_CARRY, MT_FRAME, MT_SLOTTAG):
            return block
        if direct and mt == MT_CARRY:
            if not routed:
                raise SystemExit("direct form needs a routed function")
            b2, n1 = re.subn(r"call\.uni\s+\(retval0\),\s*" + HELPER1 + r",\s*\([^)]*\);\n", "", block)
            b2, n2 = re.subn(r"ld\.param\.b64\s+(%rd\d+),\s*\[retval0\+0\];",
                             r"mov.b64 \t\1, " + carry_in + ";", b2)
            if n1 != 1 or n2 != 1:
                raise SystemExit("%s: unexpected call block shape" % fname)
            stats["direct"] += 1
            return b2
        b2, n1 = re.subn(r"\b" + HELPER1 + r",", WRAPPER + ",", block)
        b2, n2 = re.subn(r"st\.param\.b64\s+\[param4\+0\],\s*[^;]+;",
                         "st.param.b64 \t[param4+0], %s;" % (carry_in if routed else "0"), b2)
        if n1 != 1 or n2 != 1:
            raise SystemExit("%s: unexpected call block shape (%d, %d)" % (fname, n1, n2))
        stats["redirected"] += 1
        return b2

    fbody2 = CALLSEQ.sub(fix, fbody)
    if routed:
        fbody2 = (fbody2[:1] + "\n\t.reg .b64 \t%s;\n\tld.param.b64 \t%s, [%s];\n" % (
            carry_in, carry_in, pname) + fbody2[1:])
    return ptx[:h0] + head + ptx[close + 1:brace] + fbody2 + ptx[end:], stats


def helpers_ptx(path):
    """src/c1_helpers.cu's PTX (nvcc -rdc=true -ptx), ready to go after the
    trampoline: headers dropped, and the .extern declarations of symbols the
    trampoline defines (map_info, _bpf_helper_ext_0001) removed."""
    ptx = strip_headers(open(path).read())
    ptx, n1 = re.subn(r"\.extern\s+\.func\s+\(\s*\.param\s+\.b64\s+func_retval0\s*\)\s*"
                      + HELPER1 + r"\s*\([^)]*\)\s*;", "", ptx)
    ptx, n2 = re.subn(r"\.extern\s+\.const\s+\.align\s+8\s+\.b8\s+map_info\[\d+\];", "", ptx)
    if n1 != 1 or n2 != 1:
        raise SystemExit("unexpected helper PTX (%d, %d)" % (n1, n2))
    return ptx


def assemble(pass_output, helpers):
    """The module bpftime would build (hack_fatbin): trampoline + probe
    switch + PTX after the passes, here with the C1 helpers between the
    switch and the pass output (they must precede their callers)."""
    return rewrite_ptx_target(filter_out_version_headers_ptx(
        trampoline_ptx() + PROBE_SWITCH_PTX + helpers + pass_output))
