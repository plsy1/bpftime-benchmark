"""Hand-written forms of the thread-local map's (1530) accesses in the
generated `lat_mmvq` PTX of build new-00f12354 (dump-new-1), for the debug
override (2026-11-18). Measurement only. Every edit is an exact replacement
asserted to occur once inside its function (or the kernel's prologue).

usage: python3 -I make_tlvariants.py DUMPED.ptx OUTROOT

  tl0    the dump unchanged (the override's control)
  tlK    keys forwarded: each map lookup takes its key from the register the
         program stored into the stack slot (st.local) instead of loading it
         back through the slot's generic address; this covers the two
         thread-local lookups (constant key 0) and the sum-map lookup
  tlKL   tlK, and the thread-local value's loads and stores through the
         local state space (cvta.to.local of the lookup's address,
         ld.local/st.local), and the kernel's zeroing of the area with
         st.local
  tlKLW  tlKL with each pair of 32-bit accesses of the 8-byte value as one
         64-bit access (the value is 8-byte aligned: area .align 16, offset 0)
"""
import hashlib, os, sys

ENTRY = '__probe_func___Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj'
EXIT = '__retprobe_func___Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj'


def func_span(text, name):
    a = text.index('.visible .func ' + name + '(')
    b = text.index('\n}\n', a) + 3
    return a, b


def edit(text, name, pairs):
    a, b = func_span(text, name)
    body = text[a:b]
    for old, new in pairs:
        n = body.count(old)
        assert n == 1, (name[:20], old, n)
        body = body.replace(old, new)
    return text[:a] + body + text[b:]


def keys(t):
    t = edit(t, ENTRY, [('\tld.u32 \t%bptl_k0, [%rd1];', '\tmov.u32 \t%bptl_k0, %r1;')])
    t = edit(t, EXIT, [('\tld.u32 \t%bptl_k0, [%rd10];', '\tmov.u32 \t%bptl_k0, %r4;'),
                       ('\tld.u32 \t%bpsm_k0, [%rd51];', '\tmov.u32 \t%bpsm_k0, %r7;')])
    return t


def local(t, wide):
    t = edit(t, ENTRY, [
        ('\tselp.b64 \t%rd9, %bptl_a0, 0, %bptl_p0;\n} // bpftime thread-local lookup\n',
         '\tselp.b64 \t%rd9, %bptl_a0, 0, %bptl_p0;\n} // bpftime thread-local lookup\n\t.reg .b64 \t%tlL;\n\tcvta.to.local.u64 \t%tlL, %rd9;\n'),
        ('\tst.u32 \t[%rd9], %rd14;\n\tshr.u64 \t%rd16, %rd14, 32;\n\tst.u32 \t[%rd9+4], %rd16;\n',
         '\tst.local.u64 \t[%tlL], %rd14;\n' if wide else
         '\tst.local.u32 \t[%tlL], %rd14;\n\tshr.u64 \t%rd16, %rd14, 32;\n\tst.local.u32 \t[%tlL+4], %rd16;\n'),
    ])
    t = edit(t, EXIT, [
        ('\tselp.b64 \t%rd15, %bptl_a0, 0, %bptl_p0;\n} // bpftime thread-local lookup\n',
         '\tselp.b64 \t%rd15, %bptl_a0, 0, %bptl_p0;\n} // bpftime thread-local lookup\n\t.reg .b64 \t%tlL;\n\tcvta.to.local.u64 \t%tlL, %rd15;\n'),
        ('\tld.u32 \t%rd17, [%rd15];\n\tld.u32 \t%rd18, [%rd15+4];\n\tshl.b64 \t%rd19, %rd18, 32;\n\tor.b64  \t%rd4, %rd19, %rd17;\n',
         '\tld.local.u64 \t%rd4, [%tlL];\n' if wide else
         '\tld.local.u32 \t%rd17, [%tlL];\n\tld.local.u32 \t%rd18, [%tlL+4];\n\tshl.b64 \t%rd19, %rd18, 32;\n\tor.b64  \t%rd4, %rd19, %rd17;\n'),
        ('\tld.u32 \t%rd26, [%rd15];\n\tld.u32 \t%rd27, [%rd15+4];\n\tshl.b64 \t%rd28, %rd27, 32;\n\tor.b64  \t%rd29, %rd28, %rd26;\n',
         '\tld.local.u64 \t%rd29, [%tlL];\n' if wide else
         '\tld.local.u32 \t%rd26, [%tlL];\n\tld.local.u32 \t%rd27, [%tlL+4];\n\tshl.b64 \t%rd28, %rd27, 32;\n\tor.b64  \t%rd29, %rd28, %rd26;\n'),
    ])
    # the probed kernel's zeroing of its area (the kernel holding the call)
    k = t.index('\tcall ' + ENTRY + ', (%bpftime_tl);')
    z = t.rindex('\tst.u64 \t[%bpftime_tl+0], 0;\n', 0, k)
    assert k - z < 400, k - z
    t = t[:z] + '\tst.local.u64 \t[__bpftime_tl+0], 0;\n' + t[z + len('\tst.u64 \t[%bpftime_tl+0], 0;\n'):]
    return t


def variant(t, name):
    if name == 'tl0':
        return t
    t = keys(t)
    if name == 'tlK':
        return t
    return local(t, name == 'tlKLW')


if __name__ == '__main__':
    src, root = sys.argv[1], sys.argv[2]
    raw = open(src).read()
    sha = os.path.basename(src)[:-4]
    assert hashlib.sha256(raw.encode()).hexdigest() == sha
    for name in sys.argv[3:] or ('tl0', 'tlK', 'tlKL', 'tlKLW'):
        d = os.path.join(root, name)
        os.makedirs(d, exist_ok=True)
        out = variant(raw, name)
        open(os.path.join(d, sha + '.ptx'), 'w').write(out)
        print(name, d, len(out) - len(raw))
