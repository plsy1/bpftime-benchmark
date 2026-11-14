#!/usr/bin/env python3
"""summarize_e6.py a|c RESULTS_DIR : summaries of e6 part (a) (a.jsonl) and part
(c) (c.jsonl, mem-samples-*.txt). Part (b): analyze_b.py. Decision rules:
README.md. Writes summary_<part>.json. Standard library only."""
import json
import os
import re
import sys
import time

RUNS = '/home/jetson/src/bpftime-gpu-work/runs'

DECODE_WARP_RECORDS_PER_S = 77.3e6  # design note section 1 (one record per warp, decode)
PREFILL_WARP_RECORDS_PER_S = 29.4e6


def rows(p):
    out = []
    if os.path.exists(p):
        for l in open(p):
            l = l.strip()
            if l.startswith('{'):
                try:
                    out.append(json.loads(l))
                except ValueError:
                    pass
    return out


def part_a(R):
    r = rows(os.path.join(R, 'a.jsonl'))
    s = {'results_dir': R, 'lookup': [], 'batch': [], 'queue': None}
    for x in r:
        if x.get('part') == 'a' and 'records_per_s' in x:
            s['lookup'].append({k: x[k] for k in ('map', 'value_size', 'entries', 'pool_bytes', 'records_per_s',
                                                  'bytes_per_s', 'ns_per_lookup', 'verify_bad', 'lookup_bad',
                                                  'fill_fail', 'interrupted')})
        elif x.get('part') == 'a' and 'create_errno' in x:
            s['lookup'].append(x)
        elif x.get('part') == 'a-batch':
            s['batch'].append(x)
        elif x.get('part') == 'a-queue':
            s['queue'] = x
    g40 = next((x for x in s['lookup'] if x.get('map') == 'gpu1503' and x.get('value_size') == 40 and 'records_per_s' in x), None)
    if g40:
        # README (a): R40 counts only if nothing failed and the window was not cut short
        s['r40_valid'] = (g40['verify_bad'] == 0 and g40['lookup_bad'] == 0 and g40['fill_fail'] == 0
                          and not g40['interrupted'])
        s['gpu1503_40B_records_per_s'] = g40['records_per_s']
        s['fraction_of_decode_offer'] = g40['records_per_s'] / DECODE_WARP_RECORDS_PER_S
        s['fraction_of_prefill_offer'] = g40['records_per_s'] / PREFILL_WARP_RECORDS_PER_S
    s['batch_supported'] = any(b.get('supported') for b in s['batch'])
    json.dump(s, open(os.path.join(R, 'summary_a.json'), 'w'), indent=1)
    print('e6 part a (%s)' % R)
    for x in s['lookup']:
        if 'records_per_s' in x:
            print('  %-8s %8d B x %8d: %10.0f records/s %8.2f GB/s %8.1f ns/lookup  bad=%d/%d' % (
                x['map'], x['value_size'], x['entries'], x['records_per_s'], x['bytes_per_s'] / 1e9,
                x['ns_per_lookup'], x['verify_bad'], x['lookup_bad']))
        else:
            print('  create failed:', x)
    for b in s['batch']:
        print('  BPF_MAP_LOOKUP_BATCH on %s: rc=%s errno=%s (%s) supported=%s' % (b['map'], b['rc'], b['errno'], b['errstr'], b['supported']))
    if s['queue']:
        q = s['queue']
        print('  QUEUE pop (lookup_and_delete): %.0f pops/s, bad=%s, empty pop errno=%s' % (
            q.get('pops_per_s', 0), q.get('pop_bad'), q.get('empty_pop_errno')))
    if g40:
        print('  R40 valid (no failed fill/verify/lookup, not interrupted): %s' % s['r40_valid'])
        print('  1513 40-B lookups = %.1f %% of decode\'s 77.3M warp records/s, %.1f %% of prefill\'s 29.4M' % (
            100 * s['fraction_of_decode_offer'], 100 * s['fraction_of_prefill_offer']))
    else:
        print('  R40: no 1513 40-B result')


def part_c(R):
    r = rows(os.path.join(R, 'c.jsonl'))
    s = {'results_dir': R, 'sizes': []}
    for x in r:
        mb = x['size_mb']
        if x.get('skipped'):
            s['sizes'].append({'size_mb': mb, 'skipped': True, 'mem_available_mib': x.get('mem_available_mib')})
            continue
        m = re.search(r'size=(\d+)', x.get('registered_line') or '')
        reg_size = int(m.group(1)) if m else None
        vm, ve, e5, vic = x.get('victim_main') or {}, x.get('victim_end') or {}, x.get('e5') or {}, x.get('victim') or {}
        # MemAvailable samples during the run (epoch s, MiB)
        samp = []
        sp = os.path.join(R, 'mem-samples-%d.txt' % mb)
        if os.path.exists(sp):
            for l in open(sp):
                f = l.split()
                if len(f) >= 3 and f[1] == 'MemAvailable:':
                    samp.append((float(f[0]), int(f[2]) // 1024))
        # The server commits the whole segment when it creates it (Boost posix_fallocate),
        # before the victim starts. MemAvailable at run.sh's "server ready" (the last sample
        # before it) separates the server's share from the agent's (prefault + register).
        t_ready = None
        rcs = {}
        mp = os.path.join(RUNS, x.get('run', ''), 'meta.txt')
        if x.get('run') and os.path.exists(mp):
            mt = open(mp).read()
            for k in ('client_rc', 'server_rc'):
                y = re.search(k + r'=(\d+)', mt)
                rcs[k] = int(y.group(1)) if y else None
            d0 = re.search(r'^start (\d{4}-\d\d-\d\d) ', mt, re.M)
            rd = re.search(r'^server ready (\d\d:\d\d:\d\d)(\.\d+)?', mt, re.M)
            if d0 and rd:
                t_ready = time.mktime(time.strptime(d0.group(1) + ' ' + rd.group(1), '%Y-%m-%d %H:%M:%S')) + \
                    float(rd.group(2) or 0)
        before_ready = [m for t, m in samp if t_ready is not None and t <= t_ready]
        avail_ready = before_ready[-1] if before_ready else None
        avail_main = (vm.get('MemAvailable_kB') or 0) // 1024 if vm else None
        d = {'size_mb': mb,
             'registered_bytes': reg_size,
             'registered_whole_segment': reg_size == mb << 20 if reg_size else False,
             'register_error': x.get('register_error') or None,
             'ms_process_start_to_main': vm.get('ms_since_start'),
             'segment_mapping_kB': vm.get('seg_Size_kB'),
             'segment_rss_kB_at_main': vm.get('seg_Rss_kB'),
             'segment_locked_kB_at_main': vm.get('seg_Locked_kB'),
             'prefaulted_fraction': (vm.get('seg_Rss_kB') / vm.get('seg_Size_kB')) if vm.get('seg_Size_kB') else None,
             'victim_VmRSS_kB_at_main': vm.get('VmRSS_kB'),
             'victim_VmPin_kB_at_main': vm.get('VmPin_kB'),
             'MemAvailable_before_mib': x.get('mem_available_before_mib'),
             'MemAvailable_at_server_ready_mib': avail_ready,
             'MemAvailable_at_main_mib': avail_main,
             'MemAvailable_drop_server_mib': (x.get('mem_available_before_mib') - avail_ready)
             if avail_ready is not None and x.get('mem_available_before_mib') is not None else None,
             'MemAvailable_drop_agent_mib': (avail_ready - avail_main)
             if avail_ready is not None and avail_main is not None else None,
             'MemAvailable_min_during_mib': min(m for _, m in samp) if samp else None,
             'Shmem_at_main_mib': (vm.get('Shmem_kB') or 0) // 1024 if vm else None,
             'e5_pass': e5.get('pass'), 'alloc_256mib_ok': vic.get('alloc_ok'),
             'client_ms': x.get('client_ms')}
        # README (c), three states. no: the agent logged a registration error, or registered another size,
        # or a complete run failed the e5 check or the 256 MiB allocation. inconclusive: no registration line
        # at all, or the run was cut short (run.sh rc, victim line, calls != expected, loader timed out).
        d['client_rc'], d['server_rc'] = rcs.get('client_rc'), rcs.get('server_rc')
        complete = (d['client_rc'] == 0 and d['server_rc'] == 0 and bool(vic) and
                    e5.get('calls') == e5.get('expected_calls') and not e5.get('timed_out'))
        if d['register_error'] or (reg_size is not None and not d['registered_whole_segment']):
            d['verdict'] = 'no'
        elif not d['registered_whole_segment']:
            d['verdict'] = 'inconclusive (no registration line)'
        elif not complete:
            d['verdict'] = 'inconclusive (run cut short: client_rc %s, server_rc %s, calls %s of %s)' % (
                d['client_rc'], d['server_rc'], e5.get('calls'), e5.get('expected_calls'))
        elif d['e5_pass'] == 1 and d['alloc_256mib_ok'] == 1:
            d['verdict'] = 'yes'
        else:
            d['verdict'] = 'no'
        d['works'] = d['verdict'] == 'yes'
        s['sizes'].append(d)
    ok = [d['size_mb'] for d in s['sizes'] if d.get('works')]
    s['largest_working_size_mb'] = max(ok) if ok else None
    json.dump(s, open(os.path.join(R, 'summary_c.json'), 'w'), indent=1)
    print('e6 part c (%s)' % R)
    for d in s['sizes']:
        if d.get('skipped'):
            print('  %5d MiB: skipped (MemAvailable %s MiB)' % (d['size_mb'], d['mem_available_mib']))
            continue
        print('  %5d MiB: registered %s whole=%s start->main %s ms seg Rss %s kB (%.0f %%) Locked %s kB '
              'MemAvailable %s -> %s at server ready (server -%s) -> %s at main (agent -%s), min %s MiB; '
              'Shmem %s MiB e5_pass=%s alloc_ok=%s works=%s' % (
                  d['size_mb'], d['registered_bytes'], d['registered_whole_segment'], d['ms_process_start_to_main'],
                  d['segment_rss_kB_at_main'], 100 * (d['prefaulted_fraction'] or 0), d['segment_locked_kB_at_main'],
                  d['MemAvailable_before_mib'], d['MemAvailable_at_server_ready_mib'], d['MemAvailable_drop_server_mib'],
                  d['MemAvailable_at_main_mib'], d['MemAvailable_drop_agent_mib'], d['MemAvailable_min_during_mib'],
                  d['Shmem_at_main_mib'], d['e5_pass'], d['alloc_256mib_ok'], d['verdict']))
        if d['register_error']:
            print('         agent: %s' % d['register_error'])
    print('  largest working size: %s MiB' % s['largest_working_size_mb'])


if __name__ == '__main__':
    part, R = sys.argv[1], os.path.abspath(sys.argv[2])
    part_a(R) if part == 'a' else part_c(R)
