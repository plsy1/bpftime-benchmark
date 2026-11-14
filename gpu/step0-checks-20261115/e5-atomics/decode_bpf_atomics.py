#!/usr/bin/env python3
"""decode_bpf_atomics.py OBJDUMP.txt

Decode the BPF atomic instructions in an llvm-objdump -d listing from their
bytes, not their mnemonic (llvm-objdump-15 prints the 32-bit fetch and
non-fetch adds alike). Prints one JSON object: a list of
{insn, opcode, width, imm, op, fetch, text}. Standard library only.

BPF_STX|BPF_ATOMIC: opcode 0xdb (64-bit, BPF_DW) or 0xc3 (32-bit, BPF_W).
imm: 0x00 ADD, 0x40 OR, 0x50 AND, 0xa0 XOR, |0x01 = BPF_FETCH,
     0xe1 XCHG, 0xf1 CMPXCHG.
"""
import json
import re
import sys

OPS = {0x00: 'add', 0x40: 'or', 0x50: 'and', 0xa0: 'xor'}
LINE = re.compile(r'^\s*(\d+):\s+((?:[0-9a-f]{2}\s){8})\s*(.*)$')


def decode(imm):
    if imm == 0xe1:
        return 'xchg', True
    if imm == 0xf1:
        return 'cmpxchg', True
    return OPS.get(imm & ~0x01, 'imm%#x' % imm), bool(imm & 0x01)


def main():
    out = []
    for l in open(sys.argv[1], errors='replace'):
        m = LINE.match(l)
        if not m:
            continue
        b = [int(x, 16) for x in m.group(2).split()]
        if b[0] not in (0xdb, 0xc3):
            continue
        imm = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24)
        op, fetch = decode(imm)
        out.append({'insn': int(m.group(1)), 'opcode': '%#04x' % b[0],
                    'width': 64 if b[0] == 0xdb else 32, 'imm': '%#x' % imm,
                    'op': op, 'fetch': fetch, 'text': m.group(3).strip()})
    print(json.dumps(out))


if __name__ == '__main__':
    main()
