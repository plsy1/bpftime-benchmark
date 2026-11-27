"""S0.2 (PLAN-stage1.md rev 4; S0-RULES.md): the device hash map's functions
called for real from llama.cpp's probed mul_mat_vec_q, hand-routed the way
round one's C2 forms were made (results gpu/hash-round1-20261126,
c2/make_hashforms.py), as edits of the current by-value build's generated
PTX (tvf-7d02d677, lat_mmvq dump).

usage: python3 -I make_s02.py LAT_DUMP.ptx DEVHASH_PTX_H OUTROOT

The lat tool's lookup of its sum map (map 14, 1531, key %r7: the log2
bucket) becomes what S6 will emit for a constant-fd lookup of a 1533 map:
  - key, home slot, one ld.relaxed.gpu.global.b64 of the header;
  - live K: the sharded value address (R = max_entries here, so no single
    rows exist);
  - empty: NULL, no call;
  - otherwise: a call of __bpftime_devhash_lookup;
  - one mov at the join writes the program's register (%rd55).
The program's NULL branch (it does nothing in lat) gets what a hash tool's
"lookup, else insert" does: the value 1 stored on the program's stack
(r10 - 16) and a call of __bpftime_devhash_insert(base, key, &one,
BPF_NOEXIST, max_entries, shards). The devhash functions (devhash_ptx.h,
the shipped text) go at the start of the module body (plan D1).

The block is a module-scope .global array, zero at load (an empty map):
the first calls insert, later ones hit inline. 260 entries (the 1531 map's
key range), 256 shards (8 SMs x 32 lanes, as 1531's).

Variants (directories under OUTROOT, each holding the dump's file name):
  dh_route  the routed form above (timed against hd_t0, today's 1531)
  dh_check  dh_route plus a 1531 add (the dump's own address) for every
            call that found its key or inserted/raced (insert 0 or -17):
            the 1531 sum equals the probe calls only if no call got E2BIG,
            -22, -1, or skipped both paths (correctness run, not timed)
  dh_after  dh_route with the devhash text appended after the probes
            (mutation: ptxas must refuse the calls, no prototype)
"""
import os, re, sys

MAX_ENTRIES = 260
SHARDS = 256
SLOTS = 1024          # next_pow2(2 * 260)
LOG_SLOTS = 10
R = 260
DIR = 14592           # layout_for(260, 256), gpu_devhash_layout.h
ROWS = 22784
TOTAL = 555264
SEED = 0x2545f4914f6cdd1d
MUL = 0x9e3779b97f4a7c15
ROW_MASK = (1 << 29) - 1
LIVE = 2
NOEXIST = 1

SUM_BLOCK = '''	{ // bpftime sum-map lookup, map 14
	.reg .pred 	%bpsm_p0;
	.reg .b32 	%bpsm_k0, %bpsm_s0, %bpsm_l0;
	.reg .b64 	%bpsm_a0, %bpsm_w0;
	mov.u32 	%bpsm_k0, %r7;
	setp.lt.u32 	%bpsm_p0, %bpsm_k0, 260;
	mov.u32 	%bpsm_s0, %smid;
	and.b32 	%bpsm_s0, %bpsm_s0, 7;
	shl.b32 	%bpsm_s0, %bpsm_s0, 5;
	mov.u32 	%bpsm_l0, %laneid;
	or.b32 	%bpsm_s0, %bpsm_s0, %bpsm_l0;
	mad.lo.u32 	%bpsm_k0, %bpsm_k0, 256, %bpsm_s0;
	mul.wide.u32 	%bpsm_a0, %bpsm_k0, 8;
	add.u64 	%bpsm_a0, %bpsm_a0, 6962749440;
	selp.b64 	%rd55, %bpsm_a0, 0, %bpsm_p0;
} // bpftime sum-map lookup
'''

TAIL = '''	setp.eq.s64 	%p10, %rd55, 0;
	@%p10 bra 	$L__BB0_5;
	{ // bpftime: atomic on a sum map's value
	.reg .b64 %bpag_a0;
	add.u64 %bpag_a0, %rd55, 0;
	cvta.to.global.u64 %bpag_a0, %bpag_a0;
	atom.global.add.u64 %rd56, [%bpag_a0], 1;
	}
$L__BB0_5:
	ret;
'''

BASE = '''	mov.u64 	%s, __bdh_block;
	cvta.global.u64 	%s, %s;
'''


def signed(v):
    return v - (1 << 64) if v >= 1 << 63 else v


def call(fn, args, ret):
    ps = ''.join('\t.param .b64 __bdh_a%d;\n\tst.param.b64 \t[__bdh_a%d], %s;\n' % (i, i, a)
                 for i, a in enumerate(args))
    return ('\t{\n' + ps + '\t.param .b64 __bdh_r;\n'
            '\tcall (__bdh_r), %s, (%s);\n' % (fn, ', '.join('__bdh_a%d' % i for i in range(len(args)))) +
            '\tld.param.b64 \t%s, [__bdh_r];\n\t}\n' % ret)


LOOKUP = ('''	{ // S0.2 hand-routed: device hash map lookup, map 14 (inline hit path)
	.reg .pred 	%bdh_p<3>;
	.reg .b32 	%bdh_k, %bdh_s, %bdh_l, %bdh_hk, %bdh_row;
	.reg .b64 	%bdh_b, %bdh_x, %bdh_a, %bdh_h, %bdh_v, %bdh_kk, %bdh_m, %bdh_n;
	mov.u32 	%bdh_k, %r7;
''' + BASE % ('%bdh_b', '%bdh_b', '%bdh_b') + '''	cvt.u64.u32 	%bdh_kk, %bdh_k;
	xor.b64 	%bdh_x, %bdh_kk, ''' + str(SEED) + ''';
	mul.lo.u64 	%bdh_x, %bdh_x, ''' + str(signed(MUL)) + ''';
	shr.u64 	%bdh_x, %bdh_x, ''' + str(64 - LOG_SLOTS) + ''';
	shl.b64 	%bdh_x, %bdh_x, 3;
	add.u64 	%bdh_a, %bdh_b, %bdh_x;
	add.u64 	%bdh_a, %bdh_a, ''' + str(DIR) + ''';
	cvta.to.global.u64 	%bdh_a, %bdh_a;
	ld.relaxed.gpu.global.b64 	%bdh_h, [%bdh_a];
	and.b64 	%bdh_x, %bdh_h, 7;
	shr.u64 	%bdh_v, %bdh_h, 32;
	cvt.u32.u64 	%bdh_hk, %bdh_v;
	setp.eq.u64 	%bdh_p0, %bdh_x, ''' + str(LIVE) + ''';
	setp.eq.and.u32 	%bdh_p0, %bdh_hk, %bdh_k, %bdh_p0;
	@%bdh_p0 bra 	$Lbdh_hit_0;
	mov.u64 	%bdh_v, 0;
	setp.eq.u64 	%bdh_p1, %bdh_x, 0;
	@%bdh_p1 bra 	$Lbdh_join_0;
	mov.u64 	%bdh_m, ''' + str(MAX_ENTRIES) + ''';
	mov.u64 	%bdh_n, ''' + str(SHARDS) + ''';
''' + call('__bpftime_devhash_lookup', ['%bdh_b', '%bdh_kk', '%bdh_m', '%bdh_n'], '%bdh_v') +
'''	bra.uni 	$Lbdh_join_0;
$Lbdh_hit_0:
	shr.u64 	%bdh_x, %bdh_h, 3;
	cvt.u32.u64 	%bdh_row, %bdh_x;
	and.b32 	%bdh_row, %bdh_row, ''' + str(ROW_MASK) + ''';
	mov.u32 	%bdh_s, %smid;
	and.b32 	%bdh_s, %bdh_s, ''' + str(SHARDS // 32 - 1) + ''';
	shl.b32 	%bdh_s, %bdh_s, 5;
	mov.u32 	%bdh_l, %laneid;
	or.b32 	%bdh_s, %bdh_s, %bdh_l;
	mad.lo.u32 	%bdh_s, %bdh_row, ''' + str(SHARDS) + ''', %bdh_s;
	mul.wide.u32 	%bdh_x, %bdh_s, 8;
	add.u64 	%bdh_v, %bdh_b, %bdh_x;
	add.u64 	%bdh_v, %bdh_v, ''' + str(ROWS) + ''';
$Lbdh_join_0:
	mov.u64 	%rd55, %bdh_v;
	} // S0.2 hand-routed lookup
''')

CHECK_ADD = '''	{ // dh_check: the 1531 map's add at the dump's own address
	.reg .pred 	%bpsm_p0;
	.reg .b32 	%bpsm_k0, %bpsm_s0, %bpsm_l0;
	.reg .b64 	%bpsm_a0, %bpsm_w0;
	mov.u32 	%bpsm_k0, %r7;
	setp.lt.u32 	%bpsm_p0, %bpsm_k0, 260;
	mov.u32 	%bpsm_s0, %smid;
	and.b32 	%bpsm_s0, %bpsm_s0, 7;
	shl.b32 	%bpsm_s0, %bpsm_s0, 5;
	mov.u32 	%bpsm_l0, %laneid;
	or.b32 	%bpsm_s0, %bpsm_s0, %bpsm_l0;
	mad.lo.u32 	%bpsm_k0, %bpsm_k0, 256, %bpsm_s0;
	mul.wide.u32 	%bpsm_a0, %bpsm_k0, 8;
	add.u64 	%bpsm_a0, %bpsm_a0, 6962749440;
	cvta.to.global.u64 	%bpsm_a0, %bpsm_a0;
	@%bpsm_p0 red.global.add.u64 	[%bpsm_a0], 1;
	}
'''


def insert_block(check):
    out = ('''	{ // S0.2 hand-routed: the program's update(map 14, &key, &one, BPF_NOEXIST)
	.reg .pred 	%bdu_p<2>;
	.reg .b64 	%bdu_one, %bdu_vp, %bdu_b, %bdu_k, %bdu_f, %bdu_m, %bdu_n, %bdu_r;
	mov.u64 	%bdu_one, 1;
	st.local.u64 	[%rd1+496], %bdu_one;
	add.s64 	%bdu_vp, %rd2, -16;
''' + BASE % ('%bdu_b', '%bdu_b', '%bdu_b') + '''	cvt.u64.u32 	%bdu_k, %r7;
	mov.u64 	%bdu_f, ''' + str(NOEXIST) + ''';
	mov.u64 	%bdu_m, ''' + str(MAX_ENTRIES) + ''';
	mov.u64 	%bdu_n, ''' + str(SHARDS) + ''';
''' + call('__bpftime_devhash_insert', ['%bdu_b', '%bdu_k', '%bdu_vp', '%bdu_f', '%bdu_m', '%bdu_n'], '%bdu_r'))
    if check:
        out += ('''	setp.eq.s64 	%bdu_p0, %bdu_r, 0;
	setp.eq.s64 	%bdu_p1, %bdu_r, -17;
	or.pred 	%bdu_p0, %bdu_p0, %bdu_p1;
	@!%bdu_p0 bra 	$Lbdh_ins_done_0;
''' + CHECK_ADD + '$Lbdh_ins_done_0:\n')
    return out + '\t} // S0.2 hand-routed update\n'


def tail(check):
    return ('''	setp.eq.s64 	%p10, %rd55, 0;
	@%p10 bra 	$Lbdh_miss_0;
	{ // bpftime: atomic on a sum map's value
	.reg .b64 %bpag_a0;
	add.u64 %bpag_a0, %rd55, 0;
	cvta.to.global.u64 %bpag_a0, %bpag_a0;
	atom.global.add.u64 %rd56, [%bpag_a0], 1;
	}
''' + (CHECK_ADD if check else '') + '''	bra.uni 	$L__BB0_5;
$Lbdh_miss_0:
''' + insert_block(check) + '''$L__BB0_5:
	ret;
''')


def devhash_text(header_path):
    t = open(header_path).read()
    m = re.search(r'R"PTX\((.*)\)PTX"', t, re.S)
    assert m, 'no PTX in ' + header_path
    return m.group(1)


def edit_once(t, old, new):
    assert t.count(old) == 1, (old[:80], t.count(old))
    return t.replace(old, new)


def make(t, dh, check, after):
    t = edit_once(t, SUM_BLOCK, LOOKUP)
    t = edit_once(t, TAIL, tail(check))
    block = '.global .align 256 .b8 __bdh_block[%d];\n' % TOTAL
    head = '.address_size 64\n'
    if after:
        t = edit_once(t, head, head + '\n' + block)
        return t + '\n// devhash text (mutation: after the probes)\n' + dh
    return edit_once(t, head, head + '\n// devhash text (plan D1: start of the module body)\n' + dh + '\n' + block)


def main():
    dump, header, root = sys.argv[1:4]
    t = open(dump).read()
    dh = devhash_text(header)
    name = os.path.basename(dump)
    for var, check, after in (('dh_route', False, False), ('dh_check', True, False),
                              ('dh_after', False, True)):
        d = os.path.join(root, var)
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, name), 'w') as f:
            f.write(make(t, dh, check, after))
        print(var, os.path.join(d, name))


if __name__ == '__main__':
    main()
