#!/usr/bin/env python3
"""syscount-nginx with bpftime run the way bpftime traces system calls. Runs in the container,
cwd = view/. Same nginx.conf, syscount, wrk command (-c 100 -d 10) and syscount duration (-d 20,
wrk 2 s after it starts) as benchmark/syscount-nginx/benchmark.py; what differs:

- bpftime legs load the programs first and start nginx with the transformer
  (AGENT_SO=agent, LD_PRELOAD=libbpftime-agent-transformer.so), as benchmark/syscall does.
  benchmark.py preloads only the agent and starts nginx first: nothing is traced (audit.py).
- nginx is started as `sh -c 'read; exec env ... nginx'`, so its pid is known before it runs and
  the bpftime targeted leg can use `syscount -p <nginx>` like the kernel leg.
- Every traced leg keeps syscount's output: system calls counted per request, compared with kernel.
- Legs rotate each round; nginx and syscount restart for every leg; the access log is deleted
  before each start (it grew to 600 MB in one benchmark.py run).

Legs:
  native              nginx
  kernel_targeted     kernel syscount -p <nginx>
  kernel_untargeted   kernel syscount -p 1
  bpftime_targeted    bpftime syscount -p <nginx>, nginx with the transformer
  bpftime_untargeted  bpftime syscount -p 1, nginx with the transformer (its programs run in
                      nginx and return at the pid check)
  transformer_only    nginx with the transformer, no programs (rewriting + dispatch alone)
One JSON line per run.
"""
import argparse, json, os, pty, re, subprocess, threading, time

SYSCOUNT = os.path.abspath('example/libbpf-tools/syscount/syscount')
B = os.path.abspath('build')
AGENT = f'{B}/runtime/agent/libbpftime-agent.so'
SERVER = f'{B}/runtime/syscall-server/libbpftime-syscall-server.so'
TRANSFORMER = f'{B}/attach/text_segment_transformer/libbpftime-agent-transformer.so'
TOOL = f'{B}/tools/bpftimetool/bpftimetool'
DIR = os.path.abspath('benchmark/syscount-nginx')
CONF = f'{DIR}/nginx.conf'
URL = 'http://127.0.0.1:801/index.html'
WRK = ['wrk', URL, '-c', '100', '-d', '10']
LEGS = ['native', 'kernel_targeted', 'kernel_untargeted', 'bpftime_targeted',
        'bpftime_untargeted', 'transformer_only']


def sh(*a, **kw):
    return subprocess.run(list(a), capture_output=True, text=True, **kw)


def clean():
    for n in ('nginx', 'syscount', 'wrk'):
        sh('pkill', '-9', '-x', n)
    time.sleep(0.5)
    sh(TOOL, 'remove')
    for f in ('access.log', 'error.log'):
        try:
            os.remove(f'{DIR}/{f}')
        except FileNotFoundError:
            pass


def spawn_nginx(env_args):
    """Start a shell that execs nginx (through env) once released: the pid is nginx's."""
    cmd = 'read x; exec env ' + ' '.join(env_args) + f' nginx -c {CONF} -p {DIR}'
    return subprocess.Popen(['sh', '-c', cmd], stdin=subprocess.PIPE, text=True,
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def release_nginx(p):
    p.stdin.write('\n')
    p.stdin.flush()
    for _ in range(150):
        if sh('curl', '-s', '-o', '/dev/null', '-w', '%{http_code}', URL).stdout == '200':
            return
        if p.poll() is not None:
            raise RuntimeError('nginx exited: ' + p.stderr.read()[-2000:])
        time.sleep(0.1)
    raise RuntimeError('nginx not ready')


class Syscount:
    """syscount on a pseudo-terminal, so that its stdout is line-buffered and the
    "Tracing ..." line (printed once its programs are attached) arrives at once."""

    def __init__(self, pid, bpftime):
        env = dict(os.environ, LD_PRELOAD=SERVER) if bpftime else None
        master, slave = pty.openpty()
        self.p = subprocess.Popen([SYSCOUNT, '-d', '20', '-T', '40', '-p', str(pid)], env=env,
                                  stdin=subprocess.DEVNULL, stdout=slave, stderr=slave)
        os.close(slave)
        self.chunks = []
        self.reader = threading.Thread(target=self._read, args=(master,), daemon=True)
        self.reader.start()
        deadline = time.time() + 30
        while 'Tracing' not in self.text():
            if self.p.poll() is not None or time.time() > deadline:
                raise RuntimeError('syscount did not start: ' + self.text()[-2000:])
            time.sleep(0.05)

    def _read(self, fd):
        while True:
            try:
                data = os.read(fd, 65536)
            except OSError:
                break
            if not data:
                break
            self.chunks.append(data.decode(errors='replace'))
        os.close(fd)

    def text(self):
        return ''.join(self.chunks)

    def finish(self):
        try:
            self.p.wait(timeout=30)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.reader.join(timeout=5)
        return self.text()


def parse_table(out):
    rows = {}
    for line in out.splitlines():
        m = re.match(r'^\s*([a-z_0-9]+)\s+(\d+)\s*$', line)
        if m:
            rows[m.group(1)] = int(m.group(2))
    return rows


def run_leg(leg):
    clean()
    bpftime = leg.startswith('bpftime') or leg == 'transformer_only'
    env_args = [f'AGENT_SO={AGENT}', f'LD_PRELOAD={TRANSFORMER}'] if bpftime else []
    nginx = spawn_nginx(env_args)
    sc = None
    if leg != 'native' and leg != 'transformer_only':
        target = nginx.pid if leg.endswith('_targeted') else 1
        sc = Syscount(target, leg.startswith('bpftime'))
    release_nginx(nginx)
    time.sleep(2)  # benchmark.py starts wrk 2 s after syscount
    w = sh(*WRK, timeout=30)
    rps = re.search(r'Requests/sec:\s*([\d.]+)', w.stdout)
    reqs = re.search(r'(\d+) requests in', w.stdout)
    table, out = {}, ''
    if sc is not None:
        out = sc.finish()
        table = parse_table(out)
    alive = nginx.poll() is None
    nginx.kill()
    nginx.wait()
    clean()
    counted = sum(table.values())
    requests = int(reqs.group(1)) if reqs else None
    return {'leg': leg, 'rps': float(rps.group(1)) if rps else None, 'requests': requests,
            'counted': counted, 'per_request': counted / requests if requests and sc else None,
            'table': table, 'nginx_alive': alive, 'syscount_tail': out[-800:]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rounds', type=int, default=10)
    ap.add_argument('--legs', default=','.join(LEGS))
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    legs = a.legs.split(',')
    with open(a.out, 'a') as f:
        for r in range(a.rounds):
            order = legs[r % len(legs):] + legs[:r % len(legs)]
            for leg in order:
                try:
                    res = run_leg(leg)
                except Exception as e:
                    clean()
                    res = {'leg': leg, 'error': str(e)[-1500:]}
                res['round'] = r
                f.write(json.dumps(res) + '\n')
                f.flush()
                print(r, leg, res.get('rps'), res.get('per_request'), res.get('error', ''), flush=True)


if __name__ == '__main__':
    main()
