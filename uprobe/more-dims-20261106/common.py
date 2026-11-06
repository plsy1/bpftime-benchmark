"""Shared pieces of the more-dims-20261106 runners: start the loader (outattach of
output-dims-20261106) for kernel bpf or a bpftime build, run a workload under the matching
environment, stop the loader with SIGINT and parse its sums. bpftime builds are the frozen ones
of ssl-nginx-2x2-20261023/builds (fix-final = dev d46ceb8f)."""
import re, subprocess, tempfile, time
from pathlib import Path

OUTATTACH = Path('/home/jetson/src/results/uprobe/output-dims-20261106/src/outattach')
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(backend, names=('outattach',)):
    for name in names:
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    if backend not in ('none', 'kernel'):
        sh(['sudo', '-n', str(BUILDS / backend / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def env(backend, role, extra=(), shm_mb=None):
    """sudo env ... for the tool (syscall server) or a traced workload (agent)."""
    if backend in ('none', 'kernel'):
        return ['sudo', '-n', 'env', *extra]
    so = 'libbpftime-syscall-server.so' if role == 'tool' else 'libbpftime-agent.so'
    shm = [f'BPFTIME_SHM_MEMORY_MB={shm_mb}'] if (role == 'tool' and shm_mb) else []
    return ['sudo', '-n', 'env', f'LD_PRELOAD={BUILDS / backend / so}',
            'BPFTIME_LOG_OUTPUT=console', *shm, *extra]


def agent_env(backend):
    """The agent's variables alone (for a workload started some other way)."""
    if backend in ('none', 'kernel'):
        return []
    return [f'LD_PRELOAD={BUILDS / backend / "libbpftime-agent.so"}', 'BPFTIME_LOG_OUTPUT=console']


def num(pattern, text):
    m = re.search(pattern, text, re.M)
    return float(m.group(1)) if m else None


class Tool:
    """outattach OBJ BINARY SPEC... in its own session; text after stop()."""

    def __init__(self, backend, obj, binary, specs, extra=(), shm_mb=None, timeout=60):
        self.out = tempfile.TemporaryFile(mode='w+', errors='replace')
        self.t0 = time.time()
        self.p = subprocess.Popen(env(backend, 'tool', extra, shm_mb) +
                                  [str(OUTATTACH), str(obj), str(binary), *specs],
                                  stdout=self.out, stderr=subprocess.STDOUT,
                                  start_new_session=True)
        deadline = time.time() + timeout
        while time.time() < deadline and self.p.poll() is None:
            if 'attached' in self.text():
                break
            time.sleep(0.05)
        self.attach_s = time.time() - self.t0
        if 'attached' not in self.text():
            self.kill()
            raise RuntimeError('loader did not attach: ' + self.text()[-600:])

    def text(self):
        self.out.seek(0)
        return self.out.read()

    def stop(self, timeout=60):
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{self.p.pid}'])
        self.p.wait(timeout=timeout)
        return self.text()

    def kill(self):
        if self.p.poll() is None:
            sh(['sudo', '-n', 'kill', '-9', '--', f'-{self.p.pid}'])

    def sums(self, text):
        return {f'sum{i}': num(rf'^sum{i} (\d+)$', text) for i in range(8)}
