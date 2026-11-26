"""Hand-written forms of the device hash map's hit path (round one, C2;
RULES-round1.md), as edits of the current build's generated PTX.

usage: python3 -I make_hashforms.py LAT_DUMP.ptx COUNT_DUMP.ptx OUTROOT

The directory is a module-scope .global array of 1024 64-bit headers,
initialized in the PTX: key in bits 63..32, row in bits 31..3, state in
bits 2..0 (2 = live); row = key, so every add lands in the 1531 row the
generated code uses and the counts stay checkable. Home slot:
((key ^ SEED) * 0x9E3779B97F4A7C15) >> 54. SEED is chosen so that the keys
lat uses (130..194) have distinct home slots, none adjacent to another's.

lat_mmvq (runtime key %r7, the log2 bucket; result %rd55):
  hd_t0     the dump unchanged (today's 1531 inline lookup, the reference)
  hd_hh     directory hit at the home slot; a mismatch walks one slot more
  hd_hh2    the same code; the directory puts a live dummy key at each used
            key's home slot and the key in the next slot (the walk's second
            load always runs)
  hd_hcall  hd_hh's walk in a .func called from the probe
count_mmvq (constant key 130 in %r1; result %rd9):
  hd_c0     the dump unchanged
  hd_chh    the directory hit, row read from the header
  hd_chpre  the row fixed in the PTX (130); the header load only decides
            whether the key is present
"""
import hashlib, os, sys

M64 = (1 << 64) - 1
GOLD = 0x9E3779B97F4A7C15
SLOTS = 1024
USED = range(130, 195)
BASE = '6962749440'
LIVE = 2


def home(key, seed):
    return (((key ^ seed) * GOLD) & M64) >> 54


def pick_seed():
    for seed in range(1, 1 << 20):
        hs = [home(k, seed) for k in USED]
        s = set(hs)
        if len(s) != len(hs):
            continue
        if any(((h + 1) % SLOTS) in s for h in hs):
            continue
        return seed
    raise SystemExit('no seed')


SEED = pick_seed()


def header(key, row):
    return (key << 32) | (row << 3) | LIVE


def directory(displaced):
    """keys 0..259 with row = key; with displaced, a dummy live key at each
    used key's home slot and the key one slot on"""
    slots = [0] * SLOTS

    def put(key, row, start):
        i = start
        while slots[i]:
            i = (i + 1) % SLOTS
        slots[i] = header(key, row)
        return i

    if displaced:
        for n, k in enumerate(USED):
            h = home(k, SEED)
            slots[h] = header(100000 + n, 0)       # a dummy, never looked up
            assert not slots[(h + 1) % SLOTS]
            slots[(h + 1) % SLOTS] = header(k, k)
    else:
        for k in USED:
            assert put(k, k, home(k, SEED)) == home(k, SEED)
    for k in list(range(0, 130)) + list(range(195, 260)):
        put(k, k, home(k, SEED))
    return slots


def dir_decl(slots):
    vals = ', '.join(str(v) for v in slots)
    return '.visible .global .align 8 .b64 __bph_dir[%d] = {%s};\n' % (SLOTS, vals)


SUM_BLOCK = '''	{ // bpftime sum-map lookup, map %(fd)s
	.reg .pred 	%%bpsm_p0;
	.reg .b32 	%%bpsm_k0, %%bpsm_s0, %%bpsm_l0;
	.reg .b64 	%%bpsm_a0, %%bpsm_w0;
	mov.u32 	%%bpsm_k0, %(key)s;
	setp.lt.u32 	%%bpsm_p0, %%bpsm_k0, 260;
	mov.u32 	%%bpsm_s0, %%smid;
	and.b32 	%%bpsm_s0, %%bpsm_s0, 7;
	shl.b32 	%%bpsm_s0, %%bpsm_s0, 5;
	mov.u32 	%%bpsm_l0, %%laneid;
	or.b32 	%%bpsm_s0, %%bpsm_s0, %%bpsm_l0;
	mad.lo.u32 	%%bpsm_k0, %%bpsm_k0, 256, %%bpsm_s0;
	mul.wide.u32 	%%bpsm_a0, %%bpsm_k0, 8;
	add.u64 	%%bpsm_a0, %%bpsm_a0, ''' + BASE + ''';
	selp.b64 	%(dst)s, %%bpsm_a0, 0, %%bpsm_p0;
} // bpftime sum-map lookup
'''

# one directory probe at slot index %bph_i: load, compare key and state
PROBE = '''	shl.b64 	%bph_t, %bph_i, 3;
	add.u64 	%bph_t, %bph_d, %bph_t;
	ld.relaxed.gpu.global.b64 	%bph_h, [%bph_t];
	shr.u64 	%bph_x, %bph_h, 32;
	cvt.u32.u64 	%bph_r, %bph_x;
	setp.eq.u32 	%bph_p0, %bph_r, %bph_k;
	and.b64 	%bph_x, %bph_h, 7;
	setp.eq.and.u64 	%bph_p0, %bph_x, 2, %bph_p0;
'''

WALK = '''	cvt.u64.u32 	%bph_x, %bph_k;
	xor.b64 	%bph_x, %bph_x, ''' + str(SEED) + ''';
	mul.lo.u64 	%bph_x, %bph_x, ''' + str(GOLD - (1 << 64)) + ''';
	shr.u64 	%bph_i, %bph_x, 54;
	mov.u64 	%bph_d, __bph_dir;
''' + PROBE + '''	@%bph_p0 bra 	$Lbph_hit;
	add.u64 	%bph_i, %bph_i, 1;
	and.b64 	%bph_i, %bph_i, 1023;
''' + PROBE + '''$Lbph_hit:
'''

REGS = '''	.reg .pred 	%bph_p0;
	.reg .b32 	%bph_k, %bph_s, %bph_l, %bph_r;
	.reg .b64 	%bph_x, %bph_d, %bph_h, %bph_t, %bph_i, %bph_a;
'''

SHARD_ADDR = '''	mov.u32 	%bph_s, %smid;
	and.b32 	%bph_s, %bph_s, 7;
	shl.b32 	%bph_s, %bph_s, 5;
	mov.u32 	%bph_l, %laneid;
	or.b32 	%bph_s, %bph_s, %bph_l;
	mad.lo.u32 	%bph_r, %bph_r, 256, %bph_s;
	mul.wide.u32 	%bph_a, %bph_r, 8;
	add.u64 	%bph_a, %bph_a, ''' + BASE + ''';
'''

ROW_FROM_HEADER = '''	shr.u64 	%bph_x, %bph_h, 3;
	cvt.u32.u64 	%bph_r, %bph_x;
	and.b32 	%bph_r, %bph_r, 536870911;
'''


def hit_block(key, dst):
    return ('\t{ // hand form: device hash map, directory hit (round one, C2)\n' + REGS +
            '\tmov.u32 \t%bph_k, ' + key + ';\n' + WALK + ROW_FROM_HEADER + SHARD_ADDR +
            '\tselp.b64 \t' + dst + ', %bph_a, 0, %bph_p0;\n\t}\n')


def pre_block(key, dst, row):
    # the row fixed in the PTX; the header load only decides presence
    return ('\t{ // hand form: device hash map, row fixed at patch time (round one, C2)\n' + REGS +
            '\tmov.u32 \t%bph_k, ' + key + ';\n' + WALK +
            '\tmov.u32 \t%bph_r, ' + str(row) + ';\n' + SHARD_ADDR +
            '\tselp.b64 \t' + dst + ', %bph_a, 0, %bph_p0;\n\t}\n')


WALK_FUNC = ('.func (.param .b64 __bph_ret) __bph_walk(\n\t.param .b32 __bph_key\n)\n{\n' + REGS +
             '\tld.param.b32 \t%bph_k, [__bph_key];\n' + WALK +
             '\tselp.b64 \t%bph_h, %bph_h, 0, %bph_p0;\n\tst.param.b64 \t[__bph_ret], %bph_h;\n\tret;\n}\n')


def call_block(key, dst):
    return ('\t{ // hand form: device hash map, walk in a called function (round one, C2)\n' + REGS +
            '\tmov.u32 \t%bph_k, ' + key + ';\n'
            '\t{\n\t.param .b32 __bph_pk;\n\tst.param.b32 \t[__bph_pk], %bph_k;\n'
            '\t.param .b64 __bph_pr;\n\tcall.uni (__bph_pr), __bph_walk, (__bph_pk);\n'
            '\tld.param.b64 \t%bph_h, [__bph_pr];\n\t}\n'
            '\tsetp.ne.u64 \t%bph_p0, %bph_h, 0;\n' + ROW_FROM_HEADER + SHARD_ADDR +
            '\tselp.b64 \t' + dst + ', %bph_a, 0, %bph_p0;\n\t}\n')


def insert_decls(t, slots, with_func):
    anchor = '.visible .global .align 4 .u32 __bpftime_probes_enabled = 1;\n'
    assert t.count(anchor) == 1
    extra = dir_decl(slots) + (WALK_FUNC if with_func else '')
    return t.replace(anchor, anchor + extra)


def edit_once(t, old, new):
    assert t.count(old) == 1, (old[:80], t.count(old))
    return t.replace(old, new)


def lat_forms(t):
    old = SUM_BLOCK % dict(fd='14', key='%r7', dst='%rd55')
    out = {'hd_t0': t}
    out['hd_hh'] = insert_decls(edit_once(t, old, hit_block('%r7', '%rd55')), directory(False), False)
    out['hd_hh2'] = insert_decls(edit_once(t, old, hit_block('%r7', '%rd55')), directory(True), False)
    out['hd_hcall'] = insert_decls(edit_once(t, old, call_block('%r7', '%rd55')), directory(False), True)
    return out


def count_forms(t):
    old = SUM_BLOCK % dict(fd='4', key='%r1', dst='%rd9')
    out = {'hd_c0': t}
    out['hd_chh'] = insert_decls(edit_once(t, old, hit_block('%r1', '%rd9')), directory(False), False)
    out['hd_chpre'] = insert_decls(edit_once(t, old, pre_block('%r1', '%rd9', 130)), directory(False), False)
    return out


if __name__ == '__main__':
    lat, cnt, root = sys.argv[1], sys.argv[2], sys.argv[3]
    print('seed', SEED, 'homes of 130..132:', [home(k, SEED) for k in (130, 131, 132)])
    for path, maker in ((lat, lat_forms), (cnt, count_forms)):
        raw = open(path).read()
        sha = os.path.basename(path)[:-4]
        assert hashlib.sha256(raw.encode()).hexdigest() == sha
        for name, text in maker(raw).items():
            d = os.path.join(root, name)
            os.makedirs(d, exist_ok=True)
            open(os.path.join(d, sha + '.ptx'), 'w').write(text)
            print(name, sha[:8], len(text) - len(raw))
