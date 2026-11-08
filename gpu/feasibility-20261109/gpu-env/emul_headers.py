#!/usr/bin/env python3
# Emulates bpftime's PTX header handling (read from the repo, re-implemented here):
#  wrap_ptx_with_trampoline        nv_attach_utils.cpp:19-22   trampoline + input
#  wrap_ptx_with_trampoline_for_sm nv_attach_utils.cpp:62-66   rewrite(trampoline) + input
#  filter_out_version_headers_ptx  ptxpass_core/src/core.cpp:165-186  keep FIRST .version/.target/.address_size/'//'
#  filter_compiled_ptx_for_ebpf    ptxpass_core/src/core.cpp:248-285  drop ALL .version/.target/.address_size/'//'
#  rewrite_ptx_target              nv_attach_utils.cpp:24-60   first .target -> sm_arch; .version only if sm>=100
import sys

PFX = (".version", ".target", ".address_size", "//")


def keep_first_headers(s):
    seen, out = set(), []
    for line in s.split("\n"):
        hit = next((p for p in PFX if line.startswith(p)), None)
        if hit:
            if hit in seen:
                continue
            seen.add(hit)
        out.append(line)
    return "\n".join(out)


def drop_all_headers(s):
    return "\n".join(l for l in s.split("\n") if not l.startswith(PFX))


def rewrite_target(s, sm):
    n = int(sm[3:]) if len(sm) > 3 else 0
    ver = "8.7" if n >= 120 else "8.8" if n >= 103 else "8.5" if n >= 100 else None
    if ver:
        i = s.find(".version")
        if i >= 0:
            j = i + 8
            while s[j] == " ":
                j += 1
            k = j
            while s[k] not in "\n ":
                k += 1
            s = s[:j] + ver + s[k:]
    i = s.find(".target")
    if i < 0:
        return s
    j = i + len(".target")
    while s[j] in " \t":
        j += 1
    k = j
    while s[k] not in " \t\n\r,":
        k += 1
    return s[:j] + sm + s[k:]


mode, tramp_path, input_path, out_path, sm = sys.argv[1:6]
tramp = open(tramp_path).read()
inp = open(input_path).read()
if mode == "ptxpass":  # nv_attach_impl.cpp:1040-1044 then nv_attach_fatbin_record.cpp:127
    ptx = rewrite_target(keep_first_headers(tramp + inp), sm)
elif mode == "direct":  # nv_attach_impl.cpp:1136-1143 (ebpf ptx generated with detected sm)
    ptx = keep_first_headers(rewrite_target(tramp, sm) + drop_all_headers(inp))
else:
    raise SystemExit("mode?")
open(out_path, "w").write(ptx)
hdr = [l for l in ptx.split("\n") if l.startswith((".version", ".target", ".address_size"))]
print(f"{mode}: {input_path.split('/')[-1]} -> header {hdr}")
