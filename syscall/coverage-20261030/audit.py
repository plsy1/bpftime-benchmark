#!/usr/bin/env python3
"""Which system calls does bpftime's aarch64 rewriting miss? For each workload: the reference
count per system call from `strace -f -c` on the plain program (no bpftime: every call the
kernel sees, the dynamic loader's included), and bpftime's count from libbpf-tools syscount
running on the syscall server while the program runs with the transformer (AGENT_SO +
LD_PRELOAD). Prints one JSON line per workload with both tables and the differences.
Usage: audit.py <build dir> [workload ...]"""
import json, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
SYSCOUNT = '/home/jetson/src/results/syscall/syscount-nginx-20261029/view/example/libbpf-tools/syscount/syscount'
WORKLOADS = {
    'python': ['python3', os.path.join(HERE, 'workload.py')],
    'ls': ['ls', '-l', '/usr/bin'],
}


# syscount's names for the same aarch64 numbers that strace calls otherwise.
ALIASES = {'getdents': 'getdents64', 'newfstat': 'fstat', 'newfstatat': 'newfstatat'}


def sh(args, **kw):
    return subprocess.run(args, capture_output=True, text=True, **kw)


def strace_counts(cmd):
    out = os.path.join(HERE, 'strace.tmp')
    sh(['sudo', '-n', 'strace', '-f', '-c', '-o', out] + cmd)
    sh(['sudo', '-n', 'chown', 'jetson:jetson', out])
    rows = {}
    for line in open(out):
        f = line.split()
        if len(f) >= 5 and f[-1] not in ('total', 'syscall') and f[3].isdigit():
            rows[f[-1]] = int(f[3])
    os.remove(out)
    return rows


def bpftime_counts(cmd, build):
    tool = os.path.join(build, 'tools/bpftimetool/bpftimetool')
    sh(['sudo', '-n', tool, 'remove'])
    server = os.path.join(build, 'runtime/syscall-server/libbpftime-syscall-server.so')
    sc = subprocess.Popen(['sudo', '-n', 'env', 'LD_PRELOAD=' + server, SYSCOUNT, '-d', '8', '-T', '400'],
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    time.sleep(2)
    agent_log = os.path.join(HERE, 'agent.log.tmp')
    penv = os.environ.get('AUDIT_EXTRA_ENV', '').split() + [
            'AGENT_SO=' + os.path.join(build, 'runtime/agent/libbpftime-agent.so'),
            'LD_PRELOAD=' + os.path.join(build, 'attach/text_segment_transformer/libbpftime-agent-transformer.so')]
    p = sh(['sudo', '-n', 'env'] + penv + cmd, timeout=60)
    out, _ = sc.communicate(timeout=30)
    sh(['sudo', '-n', tool, 'remove'])
    open(agent_log, 'w').write(p.stderr)
    rows = {}
    for line in out.splitlines():
        m = re.match(r'^\s*([a-z_0-9]+)\s+(\d+)\s*$', line)
        if m:
            name = ALIASES.get(m.group(1), m.group(1))
            rows[name] = rows.get(name, 0) + int(m.group(2))
    log = open(os.path.join(HERE, 'agent.log.tmp')).read() if os.path.exists(os.path.join(HERE, 'agent.log.tmp')) else ''
    if os.path.exists(os.path.join(HERE, 'agent.log.tmp')):
        os.remove(os.path.join(HERE, 'agent.log.tmp'))
    rewrite = re.findall(r'(Rewrote \d+ of \d+ svc[^\n]*|No trampoline space[^\n]*|Lazy rewriting[^\n]*)', log)
    return rows, p.returncode, rewrite


if __name__ == '__main__':
    build = sys.argv[1]
    for name in sys.argv[2:] or list(WORKLOADS):
        ref = strace_counts(WORKLOADS[name])
        got, rc, rewrite = bpftime_counts(WORKLOADS[name], build)
        diff = {k: ref.get(k, 0) - got.get(k, 0) for k in sorted(set(ref) | set(got))
                if ref.get(k, 0) != got.get(k, 0)}
        print(json.dumps({'workload': name, 'rc': rc, 'strace': ref, 'bpftime': got,
                          'missing': diff, 'rewrite_log': rewrite}), flush=True)
