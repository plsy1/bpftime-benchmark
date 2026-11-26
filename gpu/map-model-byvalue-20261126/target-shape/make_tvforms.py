"""By-value forms of the thread-local area on the generated `lat_mmvq` PTX of
the prototype on dev 70f7bc1f (build n70-33d5a3d8, sha256 923ea830...), for
the debug override (2026-11-26). Measurement only; exact replacements, each
asserted to occur once.

usage: python3 -I make_tvforms.py DUMPED.ptx OUTROOT [variant ...]

  tv0      the dump unchanged (control)
  tvtight  the 11-18 by-value form (make_gforms.byval) on this dump: the
           entry returns the timer value and takes nothing, the exit takes it
           and returns nothing, lookups and null tests removed
  tvpass   the text the routing pass is to emit (design 2026-11-26): the
           kernel keeps the area (8 bytes) in a .local slot of its own and
           passes it by value; a probe function that accesses the area takes
           it in (one .b64 parameter per 8 bytes), one that writes it also
           returns it; inside, the area is two .b32 halves per word, a
           constant-key lookup is a non-null constant, and every access
           through it a register move (32-bit accesses at 4-byte offsets:
           cvt.u32.u64 for a store of a 64-bit register, cvt.u64.u32 for a
           load into one); the halves are packed and returned before every
           ret
"""
import hashlib, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_gforms as G

K = G.K
ENTRY = G.ENTRY
EXIT = G.EXIT


def lookup_block(dst, key_reg):
    return ('\t{ // bpftime thread-local lookup, map 4\n'
            '\t.reg .pred \t%bptl_p0;\n'
            '\t.reg .b32 \t%bptl_k0;\n'
            '\t.reg .b64 \t%bptl_a0;\n'
            '\tmov.u32 \t%bptl_k0, ' + key_reg + ';\n'
            '\tsetp.lt.u32 \t%bptl_p0, %bptl_k0, 1;\n'
            '\tmul.wide.u32 \t%bptl_a0, %bptl_k0, 8;\n'
            '\tadd.u64 \t%bptl_a0, %bptl_a0, %bpftime_tl_in;\n'
            '\tadd.u64 \t%bptl_a0, %bptl_a0, 0;\n'
            '\tselp.b64 \t' + dst + ', %bptl_a0, 0, %bptl_p0;\n'
            '} // bpftime thread-local lookup\n')


def by_value_lookup(dst):
    return '\tmov.u64 \t' + dst + ', 1; // bpftime thread-local lookup by value, map 4\n'


def prologue(name):
    return ('\t.reg .b32 \t%bptv_h<2>;\n'
            '\t.reg .b64 \t%bptv_x;\n'
            '\tld.param.b64 \t%bptv_x, [' + name + '_bpftime_tv_a0];\n'
            '\tmov.b64 \t{%bptv_h0, %bptv_h1}, %bptv_x;\n')


def give_back(name):
    return ('\tmov.b64 \t%bptv_x, {%bptv_h0, %bptv_h1};\n'
            '\tst.param.b64 \t[' + name + '_bpftime_tv_r0], %bptv_x;\n')


def head(t, name, ret, arg):
    a, b = G.func_span(t, name)
    old = '.visible .func ' + name + '(\n\t.param .b64 ' + name + '_bpftime_tl\n)'
    assert t[a:].startswith(old), t[a:a + 200]
    new = ('.visible .func ' + ('(.param .b64 ' + name + '_bpftime_tv_r0) ' if ret else '') +
           name + '(\n' + ('\t.param .b64 ' + name + '_bpftime_tv_a0\n' if arg else '') + ')')
    return t[:a] + new + t[a + len(old):]


def tvpass(t):
    # entry: reads nothing, writes the 8 bytes: in and out
    t = head(t, ENTRY, True, True)
    t = G.edit_func(t, ENTRY, [
        ('\t.reg .b64 \t%bpftime_tl_in;\n\tld.param.b64 \t%bpftime_tl_in, [' + ENTRY + '_bpftime_tl];\n',
         prologue(ENTRY)),
        ('\t@%bpftime_off ret;\n', give_back(ENTRY) + '\t@%bpftime_off ret;\n'),
        (lookup_block('%rd9', '%r1'), by_value_lookup('%rd9')),
        ('\tst.u32 \t[%rd9], %rd14;\n', '\tcvt.u32.u64 \t%bptv_h0, %rd14;\n'),
        ('\tst.u32 \t[%rd9+4], %rd16;\n', '\tcvt.u32.u64 \t%bptv_h1, %rd16;\n'),
        ('$L__BB0_2:\n\tret;\n', '$L__BB0_2:\n' + give_back(ENTRY) + '\tret;\n'),
    ])
    # exit: reads only: in
    t = head(t, EXIT, False, True)
    t = G.edit_func(t, EXIT, [
        ('\t.reg .b64 \t%bpftime_tl_in;\n\tld.param.b64 \t%bpftime_tl_in, [' + EXIT + '_bpftime_tl];\n',
         prologue(EXIT)),
        (lookup_block('%rd15', '%r4'), by_value_lookup('%rd15')),
        ('\tld.u32 \t%rd17, [%rd15];\n', '\tcvt.u64.u32 \t%rd17, %bptv_h0;\n'),
        ('\tld.u32 \t%rd18, [%rd15+4];\n', '\tcvt.u64.u32 \t%rd18, %bptv_h1;\n'),
        ('\tld.u32 \t%rd26, [%rd15];\n', '\tcvt.u64.u32 \t%rd26, %bptv_h0;\n'),
        ('\tld.u32 \t%rd27, [%rd15+4];\n', '\tcvt.u64.u32 \t%rd27, %bptv_h1;\n'),
    ])
    # the kernel: a .local slot instead of the area, the value around calls
    ke = t.index('\tcall ' + ENTRY + ', (%bpftime_tl);')
    k0 = t.rindex('.entry ' + K + '(', 0, ke)
    k1 = t.index('\n}\n', ke) + 3
    kern = t[k0:k1]
    area = ('\t.local .align 16 .b8 \t__bpftime_tl[8];\n'
            '\t.reg .b64 \t%bpftime_tl;\n'
            '\tcvta.local.u64 \t%bpftime_tl, __bpftime_tl;\n'
            '\tst.u64 \t[%bpftime_tl+0], 0;\n')
    assert kern.count(area) == 1
    kern = kern.replace(area, ('\t.local .align 8 .b8 \t__bpftime_tv[8];\n'
                               '\t.reg .b64 \t%bpftime_tv_a<1>, %bpftime_tv_r<1>;\n'
                               '\tst.local.u64 \t[__bpftime_tv+0], 0;\n'))
    for old, new in [
        ('    cvta.local.u64 \t%bpftime_tl, __bpftime_tl;\n\tcall ' + ENTRY + ', (%bpftime_tl);\n',
         '    ld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n'
         '\tcall (%bpftime_tv_r0), ' + ENTRY + ', (%bpftime_tv_a0);\n'
         '\tst.local.u64 \t[__bpftime_tv+0], %bpftime_tv_r0;\n'),
        ('cvta.local.u64 \t%bpftime_tl, __bpftime_tl;\n\tcall ' + EXIT + ', (%bpftime_tl);\n',
         'ld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n'
         '\tcall ' + EXIT + ', (%bpftime_tv_a0);\n'),
    ]:
        assert kern.count(old) == 1, old[:60]
        kern = kern.replace(old, new)
    assert '%bpftime_tl' not in kern and '__bpftime_tl' not in kern
    return t[:k0] + kern + t[k1:]


def variant(t, name):
    return {'tv0': lambda x: x, 'tvtight': G.byval, 'tvpass': tvpass}[name](t)


if __name__ == '__main__':
    src, root = sys.argv[1], sys.argv[2]
    raw = open(src).read()
    sha = os.path.basename(src)[:-4]
    assert hashlib.sha256(raw.encode()).hexdigest() == sha
    for name in sys.argv[3:] or ('tv0', 'tvtight', 'tvpass'):
        d = os.path.join(root, name)
        os.makedirs(d, exist_ok=True)
        out = variant(raw, name)
        open(os.path.join(d, sha + '.ptx'), 'w').write(out)
        print(name, d, len(out) - len(raw))
