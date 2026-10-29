#!/usr/bin/env python3
"""Does each syscount-nginx configuration trace anything? Runs in the container, cwd = view/.

One short traced load per leg (wrk -c 100 -d 5, as benchmark.py but 5 s), syscount output
captured (benchmark.py sends it to /dev/null):
  kernel             nginx; then `syscount -p <nginx> -d 10` (kernel tracepoints)
  official           benchmark.py's userbpf leg: nginx with LD_PRELOAD=agent only, then
                     syscount -p <nginx> with LD_PRELOAD=syscall-server
  transformer-late   nginx with AGENT_SO=agent LD_PRELOAD=transformer (system call
                     rewriting), then syscount -p <nginx> (benchmark.py's order)
  transformer        syscount first (no -p: only nginx runs the programs), then nginx with
                     the transformer, as in benchmark/syscall
Prints one JSON object per leg: requests, syscount table, total counted calls.
"""
import json, os, re, subprocess, sys, time

SYSCOUNT = 'example/libbpf-tools/syscount/syscount'
AGENT = os.path.abspath('build/runtime/agent/libbpftime-agent.so')
SERVER = os.path.abspath('build/runtime/syscall-server/libbpftime-syscall-server.so')
TRANSFORMER = os.path.abspath('build/attach/text_segment_transformer/libbpftime-agent-transformer.so')
TOOL = os.path.abspath('build/tools/bpftimetool/bpftimetool')
CONF = os.path.abspath('benchmark/syscount-nginx/nginx.conf')
WRK = ['wrk', 'http://127.0.0.1:801/index.html', '-c', '100', '-d', '5']


def sh(*a):
    return subprocess.run(list(a), capture_output=True, text=True)


def clean():
    for n in ('nginx', 'syscount', 'wrk'):
        sh('pkill', '-9', '-x', n)
    time.sleep(0.5)
    sh(TOOL, 'remove')


def start_nginx(env):
    p = subprocess.Popen(['nginx', '-c', CONF, '-p', os.path.dirname(CONF)], env=env,
                         stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for _ in range(100):
        if sh('curl', '-s', '-o', '/dev/null', '-w', '%{http_code}',
              'http://127.0.0.1:801/index.html').stdout == '200':
            return p
        if p.poll() is not None:
            raise RuntimeError('nginx exited: ' + p.stderr.read().decode()[-2000:])
        time.sleep(0.1)
    raise RuntimeError('nginx not ready')


def start_syscount(args, env):
    return subprocess.Popen([SYSCOUNT, '-d', '10', '-T', '40'] + args, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def parse_table(out):
    rows = {}
    for line in out.splitlines():
        m = re.match(r'^\s*([a-z_0-9]+)\s+(\d+)\s*$', line)
        if m:
            rows[m.group(1)] = int(m.group(2))
    return rows


def run(leg):
    clean()
    base = dict(os.environ)
    server_env = dict(base, LD_PRELOAD=SERVER)
    if leg == 'kernel':
        nginx = start_nginx(base)
        sc = start_syscount(['-p', str(nginx.pid)], base)
    elif leg == 'official':
        nginx = start_nginx(dict(base, LD_PRELOAD=AGENT))
        sc = start_syscount(['-p', str(nginx.pid)], server_env)
    elif leg == 'transformer-late':
        nginx = start_nginx(dict(base, AGENT_SO=AGENT, LD_PRELOAD=TRANSFORMER))
        sc = start_syscount(['-p', str(nginx.pid)], server_env)
    elif leg == 'transformer':
        sc = start_syscount([], server_env)
        time.sleep(2)
        nginx = start_nginx(dict(base, AGENT_SO=AGENT, LD_PRELOAD=TRANSFORMER))
    time.sleep(2)
    w = sh(*WRK)
    m = re.search(r'(\d+) requests in', w.stdout)
    try:
        out, _ = sc.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        sc.kill()
        out, _ = sc.communicate()
    nginx.kill()
    nginx.wait()
    table = parse_table(out)
    res = {'leg': leg, 'requests': int(m.group(1)) if m else None,
           'counted': sum(table.values()), 'table': table, 'syscount_rc': sc.returncode,
           'syscount_tail': out[-1500:]}
    clean()
    return res


if __name__ == '__main__':
    for leg in sys.argv[1:] or ['kernel', 'official', 'transformer-late', 'transformer']:
        print(json.dumps(run(leg)), flush=True)
