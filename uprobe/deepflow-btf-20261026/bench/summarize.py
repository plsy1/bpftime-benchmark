#!/usr/bin/env python3
"""Medians of run2/results.jsonl per protocol, size and leg, plus the audit."""
import json, statistics as st, collections, sys
rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run2/results.jsonl')]
legs = ['no-probe', 'no-uprobe', 'kernel-uprobe', 'user-uprobe']
d = collections.defaultdict(list); ev = collections.defaultdict(list); bad = []
for r in rows:
    k = (r['kind'], r['size'], r['leg'])
    d[k].append(r['req_per_sec'] or 0)
    ev[k].append((r.get('events_per_request_by_source') or {}, r.get('coroutine_events_per_request')))
    if not r.get('server_alive') or r.get('tracer_ready') is False or r.get('attached') is False:
        bad.append(r)
print('| Protocol | Size | ' + ' | '.join(legs) + ' | user vs kernel |')
print('|---|---|' + '---:|' * (len(legs) + 1))
for kind in ('https', 'http'):
    for size in (1024, 16384, 262144):
        m = {l: st.median(d[(kind, size, l)]) for l in legs}
        print(f"| {kind} | {size // 1024} KB | " + ' | '.join(f"{m[l]:,.0f}" for l in legs) +
              f" | {m['user-uprobe'] / m['kernel-uprobe'] * 100 - 100:+.1f}% |")
print(f'\nruns {len(rows)}, failed checks {len(bad)}')
print('\nAudit, events per request (source 0 syscall, 1 Go TLS uprobe) and events with a goroutine id, per rep:')
for kind in ('https', 'http'):
    for size in (1024, 16384, 262144):
        for leg in legs[1:]:
            print(f"{kind} {size // 1024}KB {leg}: " + '; '.join(
                f"{ {s: v for s, v in e.items()} } coro {c}" for e, c in ev[(kind, size, leg)]))
