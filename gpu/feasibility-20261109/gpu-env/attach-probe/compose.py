"""Re-implements, line for line, the PTX text transforms bpftime applies
between cuobjdump and nvPTXCompiler, so their output can be fed to the
real CUDA 13.2 compiler on this board:

  ptxpass_core/src/core.cpp  filter_compiled_ptx_for_ebpf_program()
  ptxpass_kprobe_entry/main.cpp  patch_entry()  (no stub call -> entry insert)
  ptxpass_core/src/core.cpp  find_kernel_body()
  nv_attach_impl.cpp  hack_fatbin(): wrap_ptx_with_trampoline +
                      filter_out_version_headers_ptx
  nv_attach_utils.cpp  rewrite_ptx_target()  (called by compile_ptxs)

usage: python3 -I compose.py TRAMPOLINE_HEADER APP_PTX PROBE_RAW_PTX KERNEL SM OUT
"""
import re
import sys


def read_trampoline(header_path):
    lines = open(header_path).read().split("\n")
    # static const char TRAMPOLINE_PTX[] = R"(   ...   )";
    assert lines[0].startswith("static const char TRAMPOLINE_PTX[] = R\"(")
    body = "\n".join(lines[1:])
    end = body.rindex(')";')
    return body[:end]


def for_each_line(s):
    # same split as the C++ for_each_line (no trailing empty piece)
    out = []
    start = 0
    while start < len(s):
        end = s.find("\n", start)
        if end == -1:
            out.append(s[start:])
            break
        out.append(s[start:end])
        start = end + 1
    return out


EXTERN_RE = re.compile(
    r"\.extern\s+\.func\s+\(\s*\.param\s+\.b64\s+func_retval0\s*\)\s+_bpf_helper_ext_\d{4}\s*\(\s*(?:\.param\s+\.b64\s+_bpf_helper_ext_\d{4}_param_\d+\s*,\s*)*\.param\s+\.b64\s+_bpf_helper_ext_\d{4}_param_\d+\s*\)\s*;")


def filter_compiled_ptx_for_ebpf_program(inp):
    prefixes = (".version", ".target", ".address_size", "//")
    filtered = ""
    for line in for_each_line(inp):
        if not line.startswith(prefixes):
            filtered += line + "\n"
    for sec in (".visible .func bpf_main(\n\t.param .b64 bpf_main_param_0,\n\t.param .b64 bpf_main_param_1\n)",
                ".visible .func bpf_main()"):
        pos = filtered.find(sec)
        if pos != -1:
            filtered = filtered[:pos] + filtered[pos + len(sec):]
    return EXTERN_RE.sub("", filtered)


KERNEL_ENTRY = re.compile(r"(\.visible\s+)?\.entry\s+(\w+)\s*\(([^)]*)\)")


def find_kernel_body(ptx, kernel):
    for m in KERNEL_ENTRY.finditer(ptx):
        if m.group(2) == kernel:
            begin = m.start()
            end = begin
            st = []
            while True:
                while end < len(ptx) and ptx[end] not in "{}":
                    end += 1
                if end >= len(ptx):
                    break
                if ptx[end] == "{":
                    st.append("{")
                else:
                    st.pop()
                end += 1
                if not st:
                    break
            return begin, end
    return None, None


def patch_entry(ptx, kernel, func_ptx):
    fname = "__probe_func__" + kernel
    out = ptx
    begin, _ = find_kernel_body(ptx, kernel)
    if begin is None:
        return ptx, False
    brace = out.find("{", begin)
    if brace == -1:
        return ptx, False
    ins = brace + 1
    if ins < len(out) and out[ins] == "\n":
        ins += 1
    out = out[:ins] + "\n    call " + fname + ";\n" + out[ins:]
    out = func_ptx + "\n" + out
    return out, True


def filter_out_version_headers_ptx(inp):
    prefixes = (".version", ".target", ".address_size", "//")
    seen = set()
    filtered = ""
    for line in for_each_line(inp):
        skip = False
        for p in prefixes:
            if line.startswith(p):
                if p in seen:
                    skip = True
                else:
                    seen.add(p)
                break
        if not skip:
            filtered += line + "\n"
    return filtered


def rewrite_ptx_target(ptx, sm_arch):
    if not sm_arch:
        return ptx
    sm_num = int(re.match(r"\d*", sm_arch[3:]).group(0) or 0) if len(sm_arch) > 3 else 0
    ver = "8.7" if sm_num >= 120 else "8.8" if sm_num >= 103 else "8.5" if sm_num >= 100 else None
    if ver:
        v = ptx.find(".version")
        if v != -1:
            v += 8
            while v < len(ptx) and ptx[v] == " ":
                v += 1
            vs = v
            while v < len(ptx) and ptx[v] not in "\n ":
                v += 1
            if v > vs:
                ptx = ptx[:vs] + ver + ptx[v:]
    pos = ptx.find(".target")
    if pos == -1:
        return ptx
    pos += len(".target")
    while pos < len(ptx) and ptx[pos] in " \t":
        pos += 1
    start = pos
    while pos < len(ptx) and ptx[pos] not in " \t\n\r,":
        pos += 1
    if pos <= start:
        return ptx
    return ptx[:start] + sm_arch + ptx[pos:]


def main():
    hdr, app, probe_raw, kernel, sm, out = sys.argv[1:7]
    tramp = read_trampoline(hdr)
    func_ptx = filter_compiled_ptx_for_ebpf_program(open(probe_raw).read())
    open(out + ".probe_filtered.ptx", "w").write(func_ptx)
    patched, modified = patch_entry(open(app).read(), kernel, func_ptx)
    assert modified, "kernel body not found"
    open(out + ".after_pass.ptx", "w").write(patched)
    wrapped = filter_out_version_headers_ptx(tramp + patched)
    open(out + ".after_hack_fatbin.ptx", "w").write(wrapped)
    final = rewrite_ptx_target(wrapped, sm)
    open(out, "w").write(final)
    hdrs = [l for l in for_each_line(final) if l.startswith((".version", ".target", ".address_size"))]
    print("headers kept:", hdrs)


if __name__ == "__main__":
    main()
