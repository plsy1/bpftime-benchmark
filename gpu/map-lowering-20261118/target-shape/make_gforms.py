"""Attribution forms of the current generated `lat_mmvq` PTX (build
cur-7e5861b9, sha256 41b5bf76..., the same as newk-0890dbdd's), for the
debug override (2026-11-18). Measurement only; exact replacements, each
asserted to occur once in its function or kernel.

usage: python3 -I make_gforms.py DUMPED.ptx OUTROOT

  g0      the dump unchanged (control)
  gnomap  the map work removed, the tool's work kept: the entry calls the
          timer helper and keeps its result alive (a never-true compare with
          the sum map's max_entries, a constant ptxas cannot fold, guarding
          a trap) instead of the thread-local lookup and stores; the exit
          takes that constant as a stand-in start instead of the
          thread-local lookup and loads, runs the tool's work (no-start
          test, timer helper, subtract, log2, key) and the bound compare
          guarding a trap instead of the sum-map lookup and add
  gbyval  the start carried by value, the sum map as built: the entry
          returns the timer value, the kernel keeps it in a .local word of
          its own and passes it to the exit as a parameter; no thread-local
          area, lookup or access (section 6's .local-word form, on the
          generated code)
"""
import hashlib, os, sys

K = '_Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj'
ENTRY = '__probe_func__' + K
EXIT = '__retprobe_func__' + K
MAXENT = 'map_info+572'  # fd 14 x 40 + 12: the sum map's max_entries (260)


def func_span(text, name):
    # the definition (with or without a return parameter), not a call
    i = text.index(' ' + name + '(\n')
    a = text.rindex('.visible .func', 0, i)
    assert '\n' not in text[a:i], text[a:i]
    b = text.index('\n}\n', a) + 3
    return a, b


def edit(text, a, b, pairs):
    body = text[a:b]
    for old, new in pairs:
        n = body.count(old)
        assert n == 1, (old[:60], n)
        body = body.replace(old, new)
    return text[:a] + body + text[b:]


def edit_func(text, name, pairs):
    a, b = func_span(text, name)
    return edit(text, a, b, pairs)


TL_ENTRY = '''	{ // bpftime thread-local lookup, map 4
	.reg .pred 	%bptl_p0;
	.reg .b32 	%bptl_k0;
	.reg .b64 	%bptl_a0;
	mov.u32 	%bptl_k0, %r1;
	setp.lt.u32 	%bptl_p0, %bptl_k0, 1;
	mul.wide.u32 	%bptl_a0, %bptl_k0, 8;
	add.u64 	%bptl_a0, %bptl_a0, %bpftime_tl_in;
	add.u64 	%bptl_a0, %bptl_a0, 0;
	selp.b64 	%rd9, %bptl_a0, 0, %bptl_p0;
} // bpftime thread-local lookup
	setp.eq.s64 	%p1, %rd9, 0;
	@%p1 bra 	$L__BB0_2;
'''
ENTRY_STORE = '''	st.u32 	[%rd9], %rd14;
	shr.u64 	%rd16, %rd14, 32;
	st.u32 	[%rd9+4], %rd16;
'''
TL_EXIT = '''	{ // bpftime thread-local lookup, map 4
	.reg .pred 	%bptl_p0;
	.reg .b32 	%bptl_k0;
	.reg .b64 	%bptl_a0;
	mov.u32 	%bptl_k0, %r4;
	setp.lt.u32 	%bptl_p0, %bptl_k0, 1;
	mul.wide.u32 	%bptl_a0, %bptl_k0, 8;
	add.u64 	%bptl_a0, %bptl_a0, %bpftime_tl_in;
	add.u64 	%bptl_a0, %bptl_a0, 0;
	selp.b64 	%rd15, %bptl_a0, 0, %bptl_p0;
} // bpftime thread-local lookup
	setp.eq.s64 	%p1, %rd15, 0;
	mov.u32 	%r7, 194;
	@%p1 bra 	$L__BB0_3;
	ld.u32 	%rd17, [%rd15];
	ld.u32 	%rd18, [%rd15+4];
	shl.b64 	%rd19, %rd18, 32;
	or.b64  	%rd4, %rd19, %rd17;
'''
EXIT_RELOAD = '''	ld.u32 	%rd26, [%rd15];
	ld.u32 	%rd27, [%rd15+4];
	shl.b64 	%rd28, %rd27, 32;
	or.b64  	%rd29, %rd28, %rd26;
'''


def sum_part(text):
    """the exit's sum-map key store, lookup, null check and add"""
    a, b = func_span(text, EXIT)
    body = text[a:b]
    s = body.index('$L__BB0_3:\n')
    e = body.index('$L__BB0_5:\n')
    return body[s:e]


def nomap(t):
    t = edit_func(t, ENTRY, [
        (TL_ENTRY, ''),
        (ENTRY_STORE, '''	{
	.reg .b32 	%bpg_m;
	.reg .b64 	%bpg_m64;
	.reg .pred 	%bpg_p;
	ld.const.u32 	%bpg_m, [@M@];
	cvt.u64.u32 	%bpg_m64, %bpg_m;
	setp.eq.u64 	%bpg_p, %rd14, %bpg_m64;
	@%bpg_p trap;
	}
'''.replace('@M@', MAXENT)),
    ])
    sp = sum_part(t)
    t = edit_func(t, EXIT, [
        (TL_EXIT, '''	mov.u32 	%r7, 194;
	{
	.reg .b32 	%bpg_s;
	ld.const.u32 	%bpg_s, [@M@];
	cvt.u64.u32 	%rd4, %bpg_s;
	}
'''.replace('@M@', MAXENT)),
        (EXIT_RELOAD, '\tmov.b64 \t%rd29, %rd4;\n'),
        (sp, '''$L__BB0_3:
	{
	.reg .b32 	%bpg_x;
	.reg .pred 	%bpg_q;
	ld.const.u32 	%bpg_x, [@M@];
	setp.ge.u32 	%bpg_q, %r7, %bpg_x;
	@%bpg_q trap;
	}
'''.replace('@M@', MAXENT)),
    ])
    return t


def byval(t):
    # the probe functions: no area parameter; the entry returns the start,
    # the exit takes it
    a, b = func_span(t, ENTRY)
    head_old = '.visible .func ' + ENTRY + '(\n\t.param .b64 ' + ENTRY + '_bpftime_tl\n)'
    assert t[a:].startswith(head_old), t[a:a + 200]
    t = t[:a] + '.visible .func (.param .b64 __bpg_ret) ' + ENTRY + '(\n)' + t[a + len(head_old):]
    t = edit_func(t, ENTRY, [
        ('\t.reg .b64 \t%bpftime_tl_in;\n\tld.param.b64 \t%bpftime_tl_in, [' + ENTRY + '_bpftime_tl];\n',
         '\tst.param.b64 \t[__bpg_ret], 0;\n'),
        (TL_ENTRY, ''),
        (ENTRY_STORE, '\tst.param.b64 \t[__bpg_ret], %rd14;\n'),
    ])
    a, b = func_span(t, EXIT)
    head_old = '.visible .func ' + EXIT + '(\n\t.param .b64 ' + EXIT + '_bpftime_tl\n)'
    assert t[a:].startswith(head_old), t[a:a + 200]
    t = t[:a] + '.visible .func ' + EXIT + '(\n\t.param .b64 __bpg_start\n)' + t[a + len(head_old):]
    t = edit_func(t, EXIT, [
        ('\t.reg .b64 \t%bpftime_tl_in;\n\tld.param.b64 \t%bpftime_tl_in, [' + EXIT + '_bpftime_tl];\n', ''),
        (TL_EXIT, '''	mov.u32 	%r7, 194;
	ld.param.b64 	%rd4, [__bpg_start];
'''),
        (EXIT_RELOAD, '\tmov.b64 \t%rd29, %rd4;\n'),
    ])
    # the kernel: no area; a .local word for the start, zeroed at start
    ke = t.index('\tcall ' + ENTRY + ', (%bpftime_tl);')
    k0 = t.rindex('.entry ' + K + '(', 0, ke)
    k1 = t.index('\n}\n', ke) + 3
    kern = t[k0:k1]
    area = '''	.local .align 16 .b8 	__bpftime_tl[8];
	.reg .b64 	%bpftime_tl;
	cvta.local.u64 	%bpftime_tl, __bpftime_tl;
	st.u64 	[%bpftime_tl+0], 0;
'''
    assert kern.count(area) == 1
    kern = kern.replace(area, '''	.local .align 8 .b8 	__bpg_slot[8];
	.reg .b64 	%bpg_start, %bpg_start2;
	st.local.u64 	[__bpg_slot], 0;
''')
    for old, new in [
        ('\tcall ' + ENTRY + ', (%bpftime_tl);',
         '\tcall (%bpg_start), ' + ENTRY + ', ();\n\tst.local.u64 \t[__bpg_slot], %bpg_start;'),
        ('\tcall ' + EXIT + ', (%bpftime_tl);',
         '\tld.local.u64 \t%bpg_start2, [__bpg_slot];\n\tcall ' + EXIT + ', (%bpg_start2);'),
    ]:
        assert kern.count(old) == 1, old[:40]
        kern = kern.replace(old, new)
    # the cvta lines before each call go with the area
    n = kern.count('cvta.local.u64 \t%bpftime_tl, __bpftime_tl;')
    assert n == 2, n
    kern = '\n'.join(l for l in kern.split('\n') if l.strip() != 'cvta.local.u64 \t%bpftime_tl, __bpftime_tl;')
    assert '%bpftime_tl' not in kern
    return t[:k0] + kern + t[k1:]


def variant(t, name):
    return {'g0': lambda x: x, 'gnomap': nomap, 'gbyval': byval}[name](t)


if __name__ == '__main__':
    src, root = sys.argv[1], sys.argv[2]
    raw = open(src).read()
    sha = os.path.basename(src)[:-4]
    assert hashlib.sha256(raw.encode()).hexdigest() == sha
    for name in sys.argv[3:] or ('g0', 'gnomap', 'gbyval'):
        d = os.path.join(root, name)
        os.makedirs(d, exist_ok=True)
        out = variant(raw, name)
        open(os.path.join(d, sha + '.ptx'), 'w').write(out)
        print(name, d, len(out) - len(raw))
