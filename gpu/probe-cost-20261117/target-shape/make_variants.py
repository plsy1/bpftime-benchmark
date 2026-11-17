"""Target-shape variants of the patched PTX of `lat` on mul_mat_vec_q
(PT_SET_MMVQ), for the debug override BPFTIME_CUDA_PTX_OVERRIDE_DIR
(proto/map-model, 2026-11-17). Measurement only: never a product path.

usage: python3 -I make_variants.py DUMPED.ptx OUTROOT

Only the probed kernel's call sites and prologue change, and new probe
functions are inserted before the kernel; the generated probe functions stay
in the module, unused. Every variant keeps what the generated code checks:
the probe switch (read once per kernel and guarding both calls), the
no-start bucket (194), the sum map's NULL conditions (no copy, fewer than 32
shards, key out of range) and the add, so the sum map must still hold every
exit exactly.

Variants (directory names):
  t0      the dumped PTX unchanged (the hook alone)
  area    end shape, but the start in today's area: generic address passed
          to both probes, the 1530 key through a stack slot, two 32-bit
          generic stores and loads (everything of the design except carrying
          the start by value)
  reg     end shape: the entry returns the start, the kernel keeps it in a
          register and passes it to the exit (calls kept)
  loc     as reg, but the kernel keeps the start in a kernel-scope .local
          word (st.local / ld.local), the design's fallback form
  chain   as reg, with the 6-step log2 chain instead of clz (LLVM's structure,
          one setp fewer than the generated code)
  inline  as reg, with both probe bodies spliced into the kernel (no calls)
  none    the probe calls removed from the kernel (no switch load): the
          kernel as ptxas compiles it in this module, without probes
  nonecap none with .maxnreg 56 (how the register cap alone moves the kernel)
  inlinecap  inline with .maxnreg 56
  regcap  as reg, with .maxnreg 56 on the kernel (the generated kernel's
          register count: keeps 36 warps per SM by registers)
Map-residual variants (added 2026-11-17 afternoon; the tool unchanged, so
all of them keep the tool's 6-step log2 chain, never clz):
  empty   the switch and two predicated calls to empty functions
  nomap   calls kept; no carry, no add. The entry reads %globaltimer and
          keeps it alive with a never-true compare against the sum map's
          max_entries (constant memory) guarding a trap; the exit takes a
          stand-in start loaded from the same constant, then runs the exit's
          tool work (no-start test, timer, subtract, chain, key) and the bound
          compare, which guards a trap instead of skipping the add
  locc    loc with the chain (full map work)
  locc-noadd  locc with the 1531 part after the bound compare replaced by
          the trap (the carry kept, no add)
  regc-noadd  chain (reg form) without the add
  *cap    the same with .maxnreg 56 on the kernel: emptycap, nomapcap,
          loccap, loccap-noadd, regcapc, regcapc-noadd
End shape: %globaltimer read inline, bucket = 63 - clz(delta | 1), the sum
map's address computed inline from map_info[fd] (as sum_array_offset), an
isspacep.global check on that address before the generic atom.add.
"""
import os, sys, hashlib, re

KERNEL = '_Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj'
FD_TL = 4     # thread-local map (1530) in this tool build
FD_SUM = 14   # sum map (1531)
KIDX = 2      # pt_lat's kernel index for mul_mat_vec_q: keys 130 + bucket, 194
MI = 40       # sizeof(MapBasicInfo)
OFF_VALUE_SIZE, OFF_MAX_ENTRIES, OFF_EXTRA, OFF_THREADS = 8, 12, 24, 32


def mi(off):
    return 'map_info+%d' % (FD_SUM * MI + off)


def bucket_clz(d, out, p):
    """out (.u32) = 63 - clz(d | 1) = log2_u64(d) of pt_common.h, d .u64"""
    return f'''	or.b64 	{p}x2, {d}, 1;
	clz.b64 	{p}w1, {p}x2;
	mov.u32 	{p}w9, 63;
	sub.u32 	{out}, {p}w9, {p}w1;
'''


def bucket_chain(d, out, p):
    """the same value by pt_common.h's shift chain, in the structure LLVM emits
    (one setp fewer than the generated code: c0 is reused for the r0 select)"""
    return f'''	setp.lt.u64 	{p}c0, {d}, 4294967296;
	shr.u64 	{p}y0, {d}, 32;
	selp.b64 	{p}v0, {d}, {p}y0, {p}c0;
	selp.b64 	{p}r0, 0, 32, {p}c0;
	setp.lt.u64 	{p}c1, {p}v0, 65536;
	or.b64 	{p}y1, {p}r0, 16;
	selp.b64 	{p}r1, {p}r0, {p}y1, {p}c1;
	shr.u64 	{p}y2, {p}v0, 16;
	selp.b64 	{p}v1, {p}v0, {p}y2, {p}c1;
	setp.lt.u64 	{p}c2, {p}v1, 256;
	or.b64 	{p}y3, {p}r1, 8;
	selp.b64 	{p}r2, {p}r1, {p}y3, {p}c2;
	shr.u64 	{p}y4, {p}v1, 8;
	selp.b64 	{p}v2, {p}v1, {p}y4, {p}c2;
	setp.lt.u64 	{p}c3, {p}v2, 16;
	or.b64 	{p}y5, {p}r2, 4;
	selp.b64 	{p}r3, {p}r2, {p}y5, {p}c3;
	shr.u64 	{p}y6, {p}v2, 4;
	selp.b64 	{p}v3, {p}v2, {p}y6, {p}c3;
	setp.lt.u64 	{p}c4, {p}v3, 4;
	add.s64 	{p}y7, {p}r3, 2;
	selp.b64 	{p}r4, {p}r3, {p}y7, {p}c4;
	shr.u64 	{p}y8, {p}v3, 2;
	selp.b64 	{p}v4, {p}v3, {p}y8, {p}c4;
	setp.gt.u64 	{p}c5, {p}v4, 1;
	selp.u64 	{p}y9, 1, 0, {p}c5;
	add.s64 	{p}r5, {p}r4, {p}y9;
	cvt.u32.u64 	{out}, {p}r5;
'''


def exit_body(start, tag, chain=False, mode='full'):
    """The exit's work for a start value in register `start` (.u64, 0 = no
    start). Labels and registers are prefixed by tag (unique per copy).
    mode 'noadd': the bound compare guards a trap (never taken: keys stay
    below max_entries) and the sum map's part after it is left out."""
    p = '%' + tag + '_'
    L = '$L' + tag
    regs = f'''	.reg .pred 	{p}q<6>;
	.reg .pred 	{p}c<6>;
	.reg .b32 	{p}w<10>;
	.reg .b32 	{p}k;
	.reg .b32 	{p}b;
	.reg .b64 	{p}x<16>;
	.reg .b64 	{p}y<10>;
	.reg .b64 	{p}v<5>;
	.reg .b64 	{p}r<6>;
'''
    b = bucket_chain(f'{p}x1', f'{p}b', p) if chain else bucket_clz(f'{p}x1', f'{p}b', p)
    return '\t{\n' + regs + f'''	mov.u32 	{p}k, {KIDX * 65 + 64};
	setp.eq.u64 	{p}q0, {start}, 0;
	@{p}q0 bra 	{L}_key;
	mov.u64 	{p}x0, %globaltimer;
	sub.u64 	{p}x1, {p}x0, {start};
''' + b + f'''	add.u32 	{p}k, {p}b, {KIDX * 65};
{L}_key:
	ld.const.u32 	{p}w3, [{mi(OFF_MAX_ENTRIES)}];
	setp.ge.u32 	{p}q1, {p}k, {p}w3;
''' + (f'''	@{p}q1 trap;
	}}
''' if mode == 'noadd' else f'''	@{p}q1 bra 	{L}_done;
	ld.const.u64 	{p}x3, [{mi(OFF_EXTRA)}];
	setp.eq.u64 	{p}q2, {p}x3, 0;
	@{p}q2 bra 	{L}_done;
	ld.const.u64 	{p}x4, [{mi(OFF_THREADS)}];
	setp.lt.u64 	{p}q3, {p}x4, 32;
	@{p}q3 bra 	{L}_done;
	shr.u64 	{p}x5, {p}x4, 5;
	add.s64 	{p}x6, {p}x5, -1;
	mov.u32 	{p}w4, %smid;
	cvt.u64.u32 	{p}x7, {p}w4;
	and.b64 	{p}x7, {p}x7, {p}x6;
	shl.b64 	{p}x7, {p}x7, 5;
	mov.u32 	{p}w5, %laneid;
	cvt.u64.u32 	{p}x8, {p}w5;
	or.b64 	{p}x7, {p}x7, {p}x8;
	cvt.u64.u32 	{p}x9, {p}k;
	mad.lo.u64 	{p}x10, {p}x9, {p}x4, {p}x7;
	ld.const.u32 	{p}w6, [{mi(OFF_VALUE_SIZE)}];
	cvt.u64.u32 	{p}x11, {p}w6;
	mad.lo.u64 	{p}x12, {p}x10, {p}x11, {p}x3;
	isspacep.global 	{p}q4, {p}x12;
	@{p}q4 atom.add.u64 	{p}x13, [{p}x12], 1;
{L}_done:
	}}
''')


FUNCS_BYVALUE = lambda chain, mode='full': f'''
.func  (.param .b64 __bpt_entry_ret) __bpt_entry(
	.param .b32 __bpt_entry_unused
)
{{
	.reg .b64 	%bpt_t;
	mov.u64 	%bpt_t, %globaltimer;
	st.param.b64 	[__bpt_entry_ret], %bpt_t;
	ret;
}}

.func __bpt_exit(
	.param .b64 __bpt_exit_start
)
{{
	.reg .b64 	%bpt_s;
	ld.param.b64 	%bpt_s, [__bpt_exit_start];
''' + exit_body('%bpt_s', 'bptx', chain, mode) + '''	ret;
}

'''

FUNCS_NOMAP = f'''
.func __bpt_entry_nm(
	.param .b32 __bpt_entry_nm_unused
)
{{
	.reg .b64 	%bpt_t;
	.reg .b64 	%bpt_m;
	.reg .b32 	%bpt_mw;
	.reg .pred 	%bpt_p;
	mov.u64 	%bpt_t, %globaltimer;
	ld.const.u32 	%bpt_mw, [{mi(OFF_MAX_ENTRIES)}];
	cvt.u64.u32 	%bpt_m, %bpt_mw;
	setp.eq.u64 	%bpt_p, %bpt_t, %bpt_m;
	@%bpt_p trap;
	ret;
}}

.func __bpt_exit_nm(
	.param .b64 __bpt_exit_nm_unused
)
{{
	.reg .b64 	%bpt_s;
	.reg .b32 	%bpt_sw;
	ld.const.u32 	%bpt_sw, [{mi(OFF_MAX_ENTRIES)}];
	cvt.u64.u32 	%bpt_s, %bpt_sw;
''' + exit_body('%bpt_s', 'bptx', True, 'noadd') + '''	ret;
}

'''

FUNCS_EMPTY = '''
.func __bpt_entry_e(
	.param .b32 __bpt_entry_e_unused
)
{
	ret;
}

.func __bpt_exit_e(
	.param .b64 __bpt_exit_e_unused
)
{
	ret;
}

'''

# map-residual variants: name -> (form, cap, mode); all with the chain
MAPRES = {
    'empty': ('empty', False, 'full'), 'emptycap': ('empty', True, 'full'),
    'nomap': ('nomap', False, 'noadd'), 'nomapcap': ('nomap', True, 'noadd'),
    'locc': ('loc', False, 'full'), 'locc-noadd': ('loc', False, 'noadd'),
    'loccap': ('loc', True, 'full'), 'loccap-noadd': ('loc', True, 'noadd'),
    'regc-noadd': ('reg', False, 'noadd'),
    'regcapc': ('reg', True, 'full'), 'regcapc-noadd': ('reg', True, 'noadd'),
}


def area_lookup(tag):
    """today's 1530 lookup shape: key 0 through a stack slot, generic reload,
    bounds check against max_entries 1, address in the area"""
    p = '%' + tag + '_'
    return f'''	.local .align 8 .b8 	{tag}_depot[8];
	.reg .b64 	{p}SPL;
	.reg .b64 	{p}SP;
	.reg .b32 	{p}key;
	.reg .pred 	{p}in;
	.reg .b64 	{p}a;
	.reg .b64 	{p}addr;
	.reg .b32 	{p}z;
	mov.u64 	{p}SPL, {tag}_depot;
	cvta.local.u64 	{p}SP, {p}SPL;
	mov.u32 	{p}z, 0;
	st.local.u32 	[{p}SPL+4], {p}z;
	ld.u32 	{p}key, [{p}SP+4];
	setp.lt.u32 	{p}in, {p}key, 1;
	mul.wide.u32 	{p}a, {p}key, 8;
	add.u64 	{p}a, {p}a, %bpt_tl;
	selp.b64 	{p}addr, {p}a, 0, {p}in;
'''


FUNCS_AREA = '''
.func __bpt_entry_area(
	.param .b64 __bpt_entry_area_tl
)
{
	.reg .b64 	%bpt_tl;
	.reg .b64 	%bpt_t;
	.reg .b64 	%bpt_hi;
	.reg .pred 	%bpt_null;
	ld.param.b64 	%bpt_tl, [__bpt_entry_area_tl];
''' + area_lookup('bpte') + '''	setp.eq.s64 	%bpt_null, %bpte_addr, 0;
	@%bpt_null bra 	$Lbpte_end;
	mov.u64 	%bpt_t, %globaltimer;
	st.u32 	[%bpte_addr], %bpt_t;
	shr.u64 	%bpt_hi, %bpt_t, 32;
	st.u32 	[%bpte_addr+4], %bpt_hi;
$Lbpte_end:
	ret;
}

.func __bpt_exit_area(
	.param .b64 __bpt_exit_area_tl
)
{
	.reg .b64 	%bpt_tl;
	.reg .b64 	%bpt_lo;
	.reg .b64 	%bpt_hi;
	.reg .b64 	%bpt_s;
	.reg .pred 	%bpt_null;
	ld.param.b64 	%bpt_tl, [__bpt_exit_area_tl];
''' + area_lookup('bptl') + '''	mov.u64 	%bpt_s, 0;
	setp.eq.s64 	%bpt_null, %bptl_addr, 0;
	@%bpt_null bra 	$Lbptl_have;
	ld.u32 	%bpt_lo, [%bptl_addr];
	ld.u32 	%bpt_hi, [%bptl_addr+4];
	shl.b64 	%bpt_hi, %bpt_hi, 32;
	or.b64 	%bpt_s, %bpt_hi, %bpt_lo;
$Lbptl_have:
''' + exit_body('%bpt_s', 'bptx') + '''	ret;
}

'''

SWITCH = '''	.reg .pred 	%bpt_on;
	{
	.reg .b32 	%bpt_w;
	ld.global.u32 	%bpt_w, [__bpftime_probes_enabled];
	setp.ne.s32 	%bpt_on, %bpt_w, 0;
	}
'''


def variant(lines, name):
    text = '\n'.join(lines)
    if name == 't0':
        return text
    # the kernel
    k0 = next(i for i, l in enumerate(lines) if l.startswith('.entry ' + KERNEL + '('))
    # its prologue (area, cvta, zeroing) and the entry call site
    pro = [i for i in range(k0, k0 + 60) if '__bpftime_tl' in lines[i] or ('call __probe_func__' + KERNEL) in lines[i]]
    ent = next(i for i in pro if ('call __probe_func__' + KERNEL) in lines[i])
    assert lines[ent - 1].strip().startswith('cvta.local.u64'), lines[ent - 1]
    first = pro[0]
    assert lines[first].strip().startswith('.local .align 16 .b8') and '__bpftime_tl[8]' in lines[first], lines[first]
    ex = [i for i in range(k0, len(lines)) if ('call __retprobe_func__' + KERNEL) in lines[i]]
    end = next(i for i in range(k0, len(lines)) if lines[i] == '}')
    ex = [i for i in ex if i < end]
    assert len(ex) == 1, ex
    ex = ex[0]
    assert lines[ex - 1].strip().startswith('cvta.local.u64'), lines[ex - 1]
    # what replaces them
    cap = name in ('regcap', 'nonecap', 'inlinecap')
    if name in MAPRES:
        form, cap, mode = MAPRES[name]
        if form == 'empty':
            new_pro = SWITCH + '\t@%bpt_on call __bpt_entry_e, (0);\n'
            new_exit = '\t@%bpt_on call __bpt_exit_e, (0);\n'
            funcs = FUNCS_EMPTY
        elif form == 'nomap':
            new_pro = SWITCH + '\t@%bpt_on call __bpt_entry_nm, (0);\n'
            new_exit = '\t@%bpt_on call __bpt_exit_nm, (0);\n'
            funcs = FUNCS_NOMAP
        elif form == 'reg':
            new_pro = SWITCH + '''	.reg .b64 	%bpt_start;
	mov.u64 	%bpt_start, 0;
	@%bpt_on call (%bpt_start), __bpt_entry, (0);
'''
            new_exit = '\t@%bpt_on call __bpt_exit, (%bpt_start);\n'
            funcs = FUNCS_BYVALUE(True, mode)
        elif form == 'loc':
            new_pro = SWITCH + '''	.local .align 8 .b8 	__bpt_slot[8];
	.reg .b64 	%bpt_start;
	.reg .b64 	%bpt_start2;
	mov.u64 	%bpt_start, 0;
	st.local.u64 	[__bpt_slot], %bpt_start;
	@%bpt_on call (%bpt_start), __bpt_entry, (0);
	st.local.u64 	[__bpt_slot], %bpt_start;
'''
            new_exit = '''	ld.local.u64 	%bpt_start2, [__bpt_slot];
	@%bpt_on call __bpt_exit, (%bpt_start2);
'''
            funcs = FUNCS_BYVALUE(True, mode)
    elif name == 'area':
        new_pro = '''	.local .align 16 .b8 	__bpftime_tl[8];
	.reg .b64 	%bpftime_tl;
	cvta.local.u64 	%bpftime_tl, __bpftime_tl;
	st.u64 	[%bpftime_tl+0], 0;
''' + SWITCH + '''	@%bpt_on call __bpt_entry_area, (%bpftime_tl);
'''
        new_exit = '\t@%bpt_on call __bpt_exit_area, (%bpftime_tl);\n'
        funcs = FUNCS_AREA
    elif name in ('reg', 'chain', 'regcap'):
        new_pro = SWITCH + '''	.reg .b64 	%bpt_start;
	mov.u64 	%bpt_start, 0;
	@%bpt_on call (%bpt_start), __bpt_entry, (0);
'''
        new_exit = '\t@%bpt_on call __bpt_exit, (%bpt_start);\n'
        funcs = FUNCS_BYVALUE(name == 'chain')
    elif name == 'loc':
        new_pro = SWITCH + '''	.local .align 8 .b8 	__bpt_slot[8];
	.reg .b64 	%bpt_start;
	.reg .b64 	%bpt_start2;
	mov.u64 	%bpt_start, 0;
	st.local.u64 	[__bpt_slot], %bpt_start;
	@%bpt_on call (%bpt_start), __bpt_entry, (0);
	st.local.u64 	[__bpt_slot], %bpt_start;
'''
        new_exit = '''	ld.local.u64 	%bpt_start2, [__bpt_slot];
	@%bpt_on call __bpt_exit, (%bpt_start2);
'''
        funcs = FUNCS_BYVALUE(False)
    elif name in ('none', 'nonecap'):
        new_pro = ''
        new_exit = ''
        funcs = ''
    elif name in ('inline', 'inlinecap'):
        new_pro = SWITCH + '''	.reg .b64 	%bpt_start;
	mov.u64 	%bpt_start, 0;
	@%bpt_on mov.u64 	%bpt_start, %globaltimer;
'''
        new_exit = '\t@!%bpt_on bra 	$Lbpt_skip;\n' + exit_body('%bpt_start', 'bpti') + '$Lbpt_skip:\n'
        funcs = ''
    else:
        raise SystemExit('unknown variant ' + name)
    head = lines[k0:first]
    if cap:
        i = next(j for j, l in enumerate(head) if l.startswith('.minnctapersm'))
        head = head[:i + 1] + ['.maxnreg 56'] + head[i + 1:]
    out = lines[:k0] + [funcs] + head + [new_pro] + lines[ent + 1:ex - 1] + [new_exit] + lines[ex + 1:]
    return '\n'.join(out)


if __name__ == '__main__':
    src, root = sys.argv[1], sys.argv[2]
    raw = open(src).read()
    sha = os.path.basename(src)[:-4]
    assert hashlib.sha256(raw.encode()).hexdigest() == sha, 'file name is not the sha256 of its content'
    lines = raw.split('\n')
    for name in sys.argv[3:] or ('t0', 'area', 'reg', 'loc', 'chain', 'inline', 'regcap'):
        d = os.path.join(root, name)
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, sha + '.ptx'), 'w').write(variant(lines, name))
        print(name, d)
