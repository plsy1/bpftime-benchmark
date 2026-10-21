#!/usr/bin/env python3
"""Static array-JIT eligibility survey over compiled .bpf.o files.

Mirrors the program-level gate in runtime/src/bpftime_prog.cpp (5466f0d,
lines ~234-251) on the instructions libbpf would load:
  * any LDDW that libbpf turns into something other than BPF_PSEUDO_MAP_FD
    (global data .data/.rodata/.bss, string literals, .kconfig, ksyms,
    function pointers) rejects the whole program;
  * CALLX (0x8d) and calls with src_reg other than 0/1 (kfuncs) reject it;
  * at least one bpf_map_lookup_elem call and one map FD are required.
Per-site selection: a lookup whose r1 comes from a constant map LDDW of an
ordinary or per-CPU array (u32 key) can be inlined; other lookups keep the
helper. Runtime conditions (LLVM backend, standard helpers, no CUDA/MPK/AOT)
are assumed to hold. The r1 provenance is a backward scan inside the
function (stopping at the first write or call) - a heuristic, reported as such.
Subprograms: if a program makes BPF-to-BPF calls, all .text functions are
attributed to it (over-approximation).
"""
import collections
import csv
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
META = ROOT / 'maps-meta'
MAP_TYPES = {1: 'hash', 2: 'array', 3: 'prog_array', 4: 'perf_event_array', 5: 'percpu_hash',
             6: 'percpu_array', 7: 'stack_trace', 8: 'cgroup_array', 9: 'lru_hash',
             10: 'lru_percpu_hash', 11: 'lpm_trie', 12: 'array_of_maps', 13: 'hash_of_maps',
             16: 'cpumap', 22: 'queue', 23: 'stack', 25: 'sk_storage', 27: 'ringbuf',
             29: 'inode_storage', 30: 'task_storage', 31: 'bloom_filter', 33: 'user_ringbuf',
             34: 'cgrp_storage'}
ARRAYS = {'array', 'percpu_array'}
LOOKUP, CALL, CALLX, LDDW, EXIT = 1, 0x85, 0x8d, 0x18, 0x95


def elf(path):
    data = path.read_bytes()
    assert data[:4] == b'\x7fELF' and data[4] == 2 and data[5] == 1, path
    shoff, = struct.unpack_from('<Q', data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x3a)
    secs = []
    for i in range(shnum):
        name, typ, flags, _, off, size, link, info, _, entsize = struct.unpack_from(
            '<IIQQQQIIQQ', data, shoff + i * shentsize)
        secs.append(dict(name=name, type=typ, flags=flags, off=off, size=size, link=link, info=info))
    strtab = secs[shstrndx]

    def cstr(sec, idx):
        start = sec['off'] + idx
        return data[start:data.index(b'\0', start)].decode()
    for s in secs:
        s['name'] = cstr(strtab, s['name'])
    symtab = next(s for s in secs if s['type'] == 2)
    syms = []
    for i in range(symtab['size'] // 24):
        name, info, other, shndx, value, size = struct.unpack_from('<IBBHQQ', data, symtab['off'] + 24 * i)
        syms.append(dict(name=cstr(secs[symtab['link']], name), type=info & 0xf, shndx=shndx,
                         value=value, size=size))
    relocs = collections.defaultdict(dict)  # code section index -> {insn byte offset: symbol}
    for s in secs:
        if s['type'] == 9:  # SHT_REL
            for i in range(s['size'] // 16):
                off, info = struct.unpack_from('<QQ', data, s['off'] + 16 * i)
                relocs[s['info']][off] = syms[info >> 32]
    return data, secs, syms, relocs


def insns(data, sec, start, size):
    out = []
    for off in range(start, start + size, 8):
        code, regs, offv, imm = struct.unpack_from('<BBhi', data, sec['off'] + off)
        out.append(dict(off=off, code=code, dst=regs & 0xf, src=regs >> 4, imm=imm))
    return out


def classify_reloc(sym, secs):
    if sym['shndx'] == 0:
        return 'extern'  # .kconfig / ksym / kfunc target
    name = secs[sym['shndx']]['name'] if sym['shndx'] < len(secs) else '?'
    if name == '.maps':
        return 'map'
    if name.startswith('.rodata.str') or name.startswith('.rodata.cst'):
        return 'string-literal'
    if name.startswith(('.data', '.rodata', '.bss')):
        return 'global-data'
    if secs[sym['shndx']]['flags'] & 0x4:
        return 'function-pointer'
    return 'other:' + name


def writes(inst, reg):
    cls = inst['code'] & 7
    return inst['dst'] == reg and cls in (0, 1, 4, 7)  # LD/LDX/ALU/ALU64


def analyze(path, corpus):
    meta = json.loads(subprocess.run([str(META), str(path)], capture_output=True, text=True).stdout or '{}')
    if 'error' in meta or not meta:
        return [dict(corpus=corpus, object=path.stem.removesuffix('.bpf'), program='', section='',
                     status='libbpf-open-failed')]
    maps = {m['name']: m for m in meta['maps']}
    sections = {p['name']: p['section'] for p in meta['programs']}
    data, secs, syms, relocs = elf(path)
    funcs = [(s, i) for i, sec in enumerate(secs) for s in syms
             if s['type'] == 2 and s['shndx'] == i and sec['flags'] & 0x4]
    text_funcs = [(s, i) for s, i in funcs if secs[i]['name'] == '.text']
    rows = []
    for sym, si in funcs:
        if sym['name'] not in sections:
            continue  # subprogram
        body = [(si, insns(data, secs[si], sym['value'], sym['size']))]
        if any(x['code'] == CALL and x['src'] == 1 for x in body[0][1]):
            body += [(i, insns(data, secs[i], s['value'], s['size'])) for s, i in text_funcs]
        blockers = collections.Counter()
        lookups = collections.Counter()
        helpers = collections.Counter()
        map_fds = set()
        for idx, code in body:
            rel = relocs.get(idx, {})
            skip = False
            for k, x in enumerate(code):
                if skip:
                    skip = False
                    continue
                if x['code'] == LDDW:
                    skip = True
                    if x['off'] in rel:
                        kind = classify_reloc(rel[x['off']], secs)
                        if kind == 'map':
                            map_fds.add(rel[x['off']]['name'])
                        else:
                            blockers['lddw-' + kind] += 1
                    elif x['src'] != 0:
                        blockers[f'lddw-src{x["src"]}'] += 1
                elif x['code'] == CALLX:
                    blockers['callx'] += 1
                elif x['code'] == CALL:
                    if x['src'] == 1:
                        continue
                    if x['src'] != 0 or x['off'] in rel:
                        blockers['kfunc-or-pseudo-call'] += 1
                        continue
                    helpers[x['imm']] += 1
                    if x['imm'] != LOOKUP:
                        continue
                    target = 'dynamic'
                    for y in reversed(code[:k]):
                        if y['code'] == CALL or y['code'] == EXIT:
                            break
                        if writes(y, 1):
                            if y['code'] == LDDW and y['off'] in rel and classify_reloc(rel[y['off']], secs) == 'map':
                                m = maps.get(rel[y['off']]['name'])
                                target = MAP_TYPES.get(m['type'], f'type{m["type"]}') if m else 'unknown-map'
                                if m and target in ARRAYS and m['key_size'] != 4:
                                    target += '-keysize'
                            break
                    lookups[target] += 1
        n_lookup = sum(lookups.values())
        if n_lookup == 0:
            status = 'no-lookup'
        elif blockers:
            status = 'blocked'
        else:
            status = 'eligible'
        array_sites = sum(v for k, v in lookups.items() if k in ARRAYS)
        rows.append(dict(
            corpus=corpus, object=path.stem.removesuffix('.bpf'), program=sym['name'],
            section=sections[sym['name']], status=status, lookups=n_lookup,
            array_lookups=array_sites,
            inlinable_sites=array_sites if status == 'eligible' else 0,
            lookup_targets=';'.join(f'{k}={v}' for k, v in sorted(lookups.items())),
            blockers=';'.join(f'{k}={v}' for k, v in sorted(blockers.items())),
            attach=attach_class(sections[sym['name']])))
    return rows


def attach_class(section):
    s = section.split('/')[0]
    if s in ('uprobe', 'uretprobe', 'uprobe.s', 'uretprobe.s', 'usdt'):
        return 'userspace-probe'
    if section.startswith(('tracepoint/syscalls/', 'tp/syscalls/', 'raw_tracepoint/sys_', 'raw_tp/sys_')):
        return 'syscall-tracepoint'
    return 'kernel-only'


def main():
    rows = []
    for corpus in ('bpftime', 'libbpf-tools'):
        for obj in sorted((ROOT / 'objs' / corpus).glob('*.bpf.o')):
            rows.extend(analyze(obj, corpus))
    fields = ['corpus', 'object', 'program', 'section', 'attach', 'status', 'lookups',
              'array_lookups', 'inlinable_sites', 'lookup_targets', 'blockers']
    with (ROOT / 'programs.csv').open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        w.writeheader()
        w.writerows(rows)
    json.dump(rows, (ROOT / 'programs.json').open('w'), indent=1)
    print(f'{len(rows)} program rows written', file=sys.stderr)


if __name__ == '__main__':
    main()
