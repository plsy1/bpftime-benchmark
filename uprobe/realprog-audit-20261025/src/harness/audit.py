#!/usr/bin/env python3
"""Functional audit of real uprobe tools on kernel eBPF and bpftime builds.

For each tool and backend: start the tool, run a workload, and compare what the tool reports
with what the workload did. Backends: kernel, and every bpftime build named on the command line
(directories under BUILDS containing libbpftime-agent.so / libbpftime-syscall-server.so /
bpftimetool). bpftime tools run with LD_PRELOAD=syscall-server and BPFTIME_SHM_MEMORY_MB=1024,
workloads with LD_PRELOAD=agent. Writes one JSON line per run to OUT and keeps raw outputs.
"""
import argparse, json, os, re, signal, subprocess, sys, tempfile, time
from pathlib import Path

ROOT = Path('/home/jetson/src/realprog-20261025')
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')
LT = ROOT / 'libbpf-tools'
EX = Path('/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/example')
WL = ROOT / 'workloads'
SHORT = Path('/home/jetson/src/rp')   # memleak -c takes a command of at most 31 characters

def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)

def env_prefix(backend, role):
    if backend == 'kernel':
        return ['sudo', '-n', 'env']
    b = BUILDS / backend
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    extra = ['BPFTIME_SHM_MEMORY_MB=1024'] if role == 'tool' else []
    return ['sudo', '-n', 'env', f'LD_PRELOAD={b / so}', 'BPFTIME_LOG_OUTPUT=console', *extra]

def cleanup(backend):
    for name in ('memleak', 'funclatency', 'gethostlatency', 'bashreadline', 'malloc', 'usdt_minimal',
                 'mallocloop', 'callloop', 'resolveloop', 'victim'):
        run(['sudo', '-n', 'pkill', '-9', '-x', name])
    if backend != 'kernel':
        run(['sudo', '-n', str(BUILDS / backend / 'bpftimetool'), 'remove'])
    time.sleep(0.3)

def ops(text):
    m = re.search(r'ops_per_sec_median ([\d.]+)', text)
    return float(m.group(1)) if m else None

def start_tool(cmd, backend, cwd=None):
    # Tool output goes to a file, not a pipe: a pipe nobody reads fills at 64 KB and then blocks
    # the tool's printf, which stalls event polling (gethostlatency stopped after ~1200 lines).
    f = tempfile.TemporaryFile(mode='w+', errors='replace')
    p = subprocess.Popen(env_prefix(backend, 'tool') + cmd, cwd=cwd, stdout=f, stderr=subprocess.STDOUT,
                         start_new_session=True)
    p.outfile = f
    return p

def stop_tool(p, wait=12):
    try:
        p.wait(timeout=wait)
    except subprocess.TimeoutExpired:
        run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            run(['sudo', '-n', 'kill', '-KILL', '--', f'-{p.pid}'])
            p.wait()
    p.outfile.seek(0)
    return p.outfile.read()

PRINTK_FLOOD = b'malloc called from pid'   # malloc-example's bpf_printk, once per malloc

def workload(cmd, backend, cwd=None, timeout=60):
    # Read output in large chunks and drop printk floods (bpftime prints bpf_printk to the traced
    # process's stdout: ~2M lines/s for malloc-example), keeping their count, the head and the tail.
    # Slow line-by-line reading would back-pressure the workload.
    p = subprocess.Popen(env_prefix(backend, 'workload') + cmd, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    head, tail, flood, deadline = b'', b'', 0, time.time() + timeout
    while True:
        chunk = p.stdout.read1(1 << 20)
        if not chunk:
            break
        flood += chunk.count(PRINTK_FLOOD)
        if len(head) < 65536:
            head += chunk[:65536 - len(head)]
        tail = (tail + chunk)[-65536:]
        if time.time() > deadline:
            p.kill()
    rc = p.wait()
    text = head.decode(errors='replace')
    if len(head) == 65536:
        text += f'\n[... output trimmed; {flood} "{PRINTK_FLOOD.decode()}" lines in total; tail follows ...]\n'
        text += tail.decode(errors='replace')
    return rc, text

def hist_total(text):
    return sum(int(m.group(1)) for m in re.finditer(r'->\s*\d+\s*:\s*(\d+)', text))

def t_funclatency(binary, backend, secs):
    p = start_tool([str(binary), '-u', '-d', str(secs + 6), f'{WL}/callloop:target_func'], backend)
    time.sleep(3)
    rc, wout = workload([str(WL / 'callloop'), str(secs)], backend)
    tout = stop_tool(p, wait=secs + 10)
    rate = ops(wout)
    return dict(workload_rc=rc, ops_per_sec=rate, expected=rate * secs if rate else None,
                reported=hist_total(tout)), wout, tout

def t_gethostlatency(backend, secs):
    p = start_tool([str(LT / 'gethostlatency')], backend)
    time.sleep(3)
    rc, wout = workload([str(WL / 'resolveloop'), str(secs)], backend)
    time.sleep(2)
    run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
    tout = stop_tool(p, wait=5)
    rate = ops(wout)
    return dict(workload_rc=rc, ops_per_sec=rate, expected=rate * secs if rate else None,
                reported=len(re.findall(r'\bresolveloop\b.*localhost', tout))), wout, tout

def t_memleak(backend, secs):
    # memleak -c forks the command from the tool process, so it inherits the tool's LD_PRELOAD.
    # A shell-script wrapper then runs sh/env with the syscall-server loaded, and their fd closes
    # broke the tool's maps (agent: "Expected fd 4 to be a map fd"). SHORT/sw is a static binary
    # (workloads/staticwrap.c), which ignores LD_PRELOAD and execs SW_CMD with SW_PRELOAD.
    wrapper = SHORT / 'sw'
    assert len(str(wrapper)) <= 31
    pre = '' if backend == 'kernel' else str(BUILDS / backend / 'libbpftime-agent.so')
    r = run(env_prefix(backend, 'tool') + [f'SW_PRELOAD={pre}', f'SW_CMD={WL}/mallocloop',
            f'SW_ARGS={secs} 64 1024', str(LT / 'memleak'), '-c', str(wrapper), '1'], timeout=secs + 30)
    out = r.stdout + r.stderr
    reports = out.split('Top ')
    last = reports[-1] if len(reports) > 1 else ''
    allocs = sum(int(m.group(1)) for m in re.finditer(r'bytes in (\d+) allocations', last))
    rate = ops(out)
    return dict(tool_rc=r.returncode, ops_per_sec=rate, expected='~1024 outstanding 64-byte blocks',
                reported=allocs, stack_errors=out.count('Unable to get stack trace'),
                stack_lookup_failures=sum(int(m.group(1)) for m in re.finditer(r'WARNING: (\d+) stack traces could not', last)),
                map_errors=out.count('Expected fd')), out, ''

def t_malloc_example(backend, secs):
    p = start_tool([str(EX / 'malloc-nobtf' / 'malloc')], backend)
    time.sleep(3)
    rc, wout = workload([str(WL / 'mallocloop'), str(secs), '64', '1024'], backend)
    time.sleep(2)
    run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
    tout = stop_tool(p, wait=5)
    rate = ops(wout)
    # The tool prints and deletes each pid's count every second; sum per pid over the run and
    # take the busiest pid (the workload). One op of mallocloop is one malloc.
    per_pid = {}
    for m in re.finditer(r'pid=(\d+)\s+malloc calls: (\d+)', tout):
        per_pid[m.group(1)] = per_pid.get(m.group(1), 0) + int(m.group(2))
    counts = [int(m.group(1)) for m in re.finditer(r'malloc calls: (\d+)', tout)]
    return dict(workload_rc=rc, ops_per_sec=rate, expected=rate * secs if rate else None,
                reported=max(per_pid.values()) if per_pid else 0,
                max_interval=max(counts) if counts else 0), wout, tout

def t_bashreadline(backend, n):
    p = start_tool([str(LT / 'bashreadline')], backend)
    time.sleep(3)
    driver = ROOT / 'harness' / 'bash_driver.py'
    rc, wout = workload(['python3', str(driver), str(n)], backend, timeout=120)
    time.sleep(2)
    run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
    tout = stop_tool(p, wait=5)
    return dict(workload_rc=rc, expected=n, reported=len(re.findall(r'echo audit-line-\d+', tout)),
                driver=wout.strip().splitlines()[-1:] if wout.strip() else []), wout, tout

def t_usdt_minimal(backend, secs):
    d = EX / 'minimal' / 'usdt_minimal-nobtf'
    tp = None
    if backend == 'kernel':
        run(['sudo', '-n', 'sh', '-c', 'echo > /sys/kernel/tracing/trace'])
    p = start_tool(['./usdt_minimal'], backend, cwd=d)
    time.sleep(3)
    try:
        r = run(env_prefix(backend, 'workload') + ['timeout', str(secs), './victim'], cwd=d, timeout=secs + 10)
        wout = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        wout = ''
    time.sleep(1)
    run(['sudo', '-n', 'kill', '-INT', '--', f'-{p.pid}'])
    tout = stop_tool(p, wait=5)
    probes = 2 * len(re.findall(r'^\d+ \+ \d+ = \d+', wout, re.M))
    if backend == 'kernel':
        trace = run(['sudo', '-n', 'cat', '/sys/kernel/tracing/trace']).stdout
        hits = trace.count('bpf: ')
    else:
        # bpf_printk goes to the traced process's stdout; count only lines it printed (the
        # tool's libbpf debug lines also contain "bpf: ").
        hits = len(re.findall(r'^bpf: ', wout, re.M))
    return dict(expected=probes, reported=hits), wout, tout

TOOLS = {
    'funclatency-libbpf': lambda b, s: t_funclatency(LT / 'funclatency', b, s),
    'funclatency-bpftime-example': lambda b, s: t_funclatency(EX / 'tracing' / 'funclatency-nobtf' / 'funclatency', b, s),
    'gethostlatency': t_gethostlatency,
    'memleak': t_memleak,
    'malloc-example': t_malloc_example,
    'bashreadline': lambda b, s: t_bashreadline(b, 200),
    'usdt_minimal': lambda b, s: t_usdt_minimal(b, 4),
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--backends', default='kernel,official,bca2ebce')
    ap.add_argument('--tools', default=','.join(TOOLS))
    ap.add_argument('--secs', type=int, default=4)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    out = Path(a.out)
    raw = out.with_suffix('')
    raw.mkdir(parents=True, exist_ok=True)
    for tool in a.tools.split(','):
        for backend in a.backends.split(','):
            cleanup(backend)
            t0 = time.time()
            try:
                res, wout, tout = TOOLS[tool](backend, a.secs)
            except Exception as e:  # keep auditing the rest
                res, wout, tout = dict(error=repr(e)), '', ''
            cleanup(backend)
            res.update(tool=tool, backend=backend, seconds=round(time.time() - t0, 1))
            (raw / f'{tool}.{backend}.workload.txt').write_text(wout)
            (raw / f'{tool}.{backend}.tool.txt').write_text(tout)
            with out.open('a') as f:
                f.write(json.dumps(res) + '\n')
            print(json.dumps(res), flush=True)

if __name__ == '__main__':
    main()
