import struct, sys
data = open(sys.argv[1], 'rb').read()
e_shoff, = struct.unpack_from('<Q', data, 0x28)
e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 0x3a)
secs = []
for i in range(e_shnum):
    off = e_shoff + i * e_shentsize
    name, typ, flags, addr, offset, size = struct.unpack_from('<IIQQQQ', data, off)
    secs.append((name, offset, size))
shstr = secs[e_shstrndx]
def nm(o):
    s = data[shstr[1] + o:]
    return s[:s.index(b'\0')].decode()
S = {nm(n): (o, s) for n, o, s in secs}
bo, bs = S['.BTF']
# BTF header: magic(2) ver(1) flags(1) hdr_len(4) type_off type_len str_off str_len
magic, ver, fl, hl, toff, tlen, soff, slen = struct.unpack_from('<HBBIIIII', data, bo)
strbase = bo + hl + soff
def bstr(o):
    s = data[strbase + o:]
    return s[:s.index(b'\0')].decode(errors='replace')
eo, es = S['.BTF.ext']
magic, ver, fl, hl, fio, fil, lio, lil = struct.unpack_from('<HBBIIIII', data, eo)
p = eo + hl + lio
end = p + lil
rec_size, = struct.unpack_from('<I', data, p); p += 4
want = set(int(x) for x in sys.argv[2:])
while p < end:
    sec_off, n = struct.unpack_from('<II', data, p); p += 8
    sec = bstr(sec_off)
    for i in range(n):
        insn_off, fn_off, line_off, lc = struct.unpack_from('<IIII', data, p + i * rec_size)
        pc = insn_off // 8
        print(sec, 'pc', pc, 'line', lc >> 10)
    p += n * rec_size
