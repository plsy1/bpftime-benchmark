#!/usr/bin/env python3
"""Per-tool, per-map runtime usage from the counting build (stats/<tool>.jsonl, one line per
map per process) joined with the audit's traced-call counts (audit.jsonl, `expected`).

Prints one table row per (tool, map) with program-side operations per traced call, the
operation mix, lookup miss rate, inserts/deletes, threads and the loader's bpf() operations.
"""
import collections, json, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TYPES = {1: 'hash', 2: 'array', 3: 'prog_array', 4: 'perf_event_array', 5: 'percpu_hash',
         6: 'percpu_array', 7: 'stack_trace', 9: 'lru_hash', 10: 'lru_percpu_hash', 27: 'ringbuf'}
OPS = ('lookup_hit', 'lookup_miss', 'update_insert', 'update_existing', 'update_failed',
       'delete_hit', 'delete_miss')


def load():
    audit = {}
    for line in (HERE / 'audit.jsonl').read_text().splitlines():
        r = json.loads(line)
        audit[r['tool']] = r
    maps = collections.OrderedDict()
    for path in sorted((HERE / 'stats').glob('*.jsonl')):
        tool = path.stem
        for line in path.read_text().splitlines():
            r = json.loads(line)
            key = (tool, r['map'])
            m = maps.setdefault(key, dict(type=r['type'], key_size=r['key_size'],
                                          value_size=r['value_size'], max_entries=r['max_entries'],
                                          program=collections.Counter(), syscall=collections.Counter(),
                                          threads=0, procs=set()))
            for side in ('program', 'syscall'):
                for op in OPS:
                    m[side][op] += r[side][op]
            if sum(r['program'][op] for op in OPS):
                m['procs'].add((r['comm'], r['pid']))
                m['threads'] = max(m['threads'], r['program']['threads'])
    return audit, maps


def main():
    audit, maps = load()
    print('| Tool | Map | Type | Key | Value | Max | Prog ops/call | Lookups (miss) | Inserts | Updates | Deletes (miss) | Threads | bpf() ops |')
    print('|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
    for (tool, name), m in maps.items():
        p, s = m['program'], m['syscall']
        total = sum(p.values())
        calls = audit.get(tool, {}).get('expected')
        if not total:
            per_call = '-'
        elif isinstance(calls, int) and calls:
            per_call = f'{total / calls:.2f}'
        else:
            per_call = f'? ({total} ops)'
        lookups = p['lookup_hit'] + p['lookup_miss']
        miss = f" ({p['lookup_miss'] / lookups:.0%})" if lookups else ''
        deletes = p['delete_hit'] + p['delete_miss']
        dmiss = f" ({p['delete_miss'] / deletes:.0%})" if deletes else ''
        print(f"| {tool} | {name} | {TYPES.get(m['type'], m['type'])} | {m['key_size']} | "
              f"{m['value_size']} | {m['max_entries']} | {per_call} | {lookups}{miss} | "
              f"{p['update_insert']} | {p['update_existing'] + p['update_failed']} | {deletes}{dmiss} | "
              f"{m['threads'] or '-'} | {sum(s.values())} |")


if __name__ == '__main__':
    sys.exit(main())
