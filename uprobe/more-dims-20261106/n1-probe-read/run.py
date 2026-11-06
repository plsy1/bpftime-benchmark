#!/usr/bin/env python3
"""bpf_probe_read cost and errors (more-dims-20261106/n1-probe-read), kernel bpf vs bpftime.

cost: readstr LEN SECS calls probe_me(s) on one thread (CPU 5); a uprobe on it reads the string
  (pr-str-SIZE: bpf_probe_read_user_str into a SIZE buffer) or SIZE bytes (pr-read-SIZE), or
  nothing (pr-control). ns per call = 1e9 / ops_per_sec; the read's cost = that minus control's.
container: the same with pr-str-256 and a 16-byte string, the workload in a Docker container
  (image bpftime-ssl-nginx:20261023, Ubuntu 24.04 like the host; src/ and the bpftime build
  bind-mounted read-only at their host paths, /dev/shm shared, its own pid namespace):
  Docker's default seccomp profile and capabilities, unconfined, or default plus
  CAP_SYS_PTRACE. The loader runs on the host.
bad: the probe reads a bad pointer (NULL, an unmapped page, a kernel address) passed to
  probe_bad, which never touches it: the kernel returns -EFAULT; bpftime as the daily and
  fix-final builds are configured (ENABLE_PROBE_READ_CHECK=OFF, as the official benchmark
  build) copies with memcpy; fix-final-probecheck has the checks on.
Checks: hits == the workload's calls. One JSON line per run.
"""
import argparse, json, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

SRC = HERE / 'src'
IMAGE = 'bpftime-ssl-nginx:20261023'
DOCKER = {
    'host': None,
    'docker-default': [],
    'docker-unconfined': ['--security-opt', 'seccomp=unconfined'],
    'docker-ptrace': ['--cap-add', 'SYS_PTRACE'],
}


def workload_cmd(backend, mode, length, secs):
    wl = [str(SRC / 'readstr'), str(length), str(secs)]
    if mode.startswith('bad'):
        return common.env(backend, 'workload', [f'BAD={mode[3:]}']) + wl
    if DOCKER[mode] is None:
        return common.env(backend, 'workload', ['PVR_CHECK=1']) + wl
    mounts = ['-v', f'{SRC}:{SRC}:ro', '-v', '/dev/shm:/dev/shm']
    if backend not in ('none', 'kernel'):
        b = common.BUILDS / backend
        mounts += ['-v', f'{b}:{b}:ro']
    return (['sudo', '-n', 'docker', 'run', '--rm', '--name', 'n1-probe-read', *mounts,
             *DOCKER[mode], '--entrypoint', 'env', IMAGE, *common.agent_env(backend),
             'PVR_CHECK=1'] + wl)


def run_one(backend, obj, length, secs, mode):
    common.cleanup(backend, ('outattach', 'readstr'))
    common.sh(['sudo', '-n', 'docker', 'rm', '-f', 'n1-probe-read'])
    res = dict(backend=backend, obj=obj, length=length, mode=mode, secs=secs)
    tool = None
    try:
        if backend != 'none':
            func = 'probe_bad' if mode.startswith('bad') else 'probe_me'
            tool = common.Tool(backend, SRC / obj, SRC / 'readstr', [f'on_probe={func}'])
        w = common.sh(['timeout', str(secs + 90)] + workload_cmd(backend, mode, length, secs))
        res['workload_rc'] = w.returncode
        if w.returncode:
            res['workload_tail'] = (w.stdout + w.stderr)[-400:]
        res['total_ops'] = common.num(r'^total_ops (\d+)$', w.stdout)
        res['ops_per_sec'] = common.num(r'^ops_per_sec ([\d.]+)$', w.stdout)
        res['pvr_self'] = common.num(r'^pvr_self (-?\d+)', w.stdout)
        res['pvr_errno'] = common.num(r'errno (\d+)$', w.stdout)
        if res['ops_per_sec']:
            res['ns_per_call'] = round(1e9 / res['ops_per_sec'], 1)
        if tool:
            text = tool.stop()
            s = tool.sums(text)
            res.update(hits=s['sum0'], failed=s['sum1'], last_err=s['sum2'], bytes=s['sum3'])
            res['ok'] = res['hits'] == res['total_ops'] and res['total_ops'] is not None
            if not res['ok']:
                res['tool_tail'] = text[-400:]
                res['workload_tail'] = (w.stdout + w.stderr)[-400:]
        else:
            res['ok'] = res['total_ops'] is not None
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        if tool:
            tool.kill()
        common.sh(['sudo', '-n', 'docker', 'rm', '-f', 'n1-probe-read'])
        common.cleanup(backend, ('outattach', 'readstr'))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--matrix', choices=('cost', 'container', 'bad'), required=True)
    ap.add_argument('--backends', default='kernel,fix-final')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--secs', type=int, default=3)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    backends = a.backends.split(',')
    if a.matrix == 'cost':
        cells = [('pr-control.bpf.o', 16, 'host'), ('pr-str-256.bpf.o', 16, 'host'),
                 ('pr-str-256.bpf.o', 255, 'host'), ('pr-str-4096.bpf.o', 1024, 'host'),
                 ('pr-str-4096.bpf.o', 4095, 'host'), ('pr-read-64.bpf.o', 4095, 'host'),
                 ('pr-read-256.bpf.o', 4095, 'host'), ('pr-read-4096.bpf.o', 4095, 'host')]
    elif a.matrix == 'container':
        cells = [('pr-str-256.bpf.o', 16, m) for m in DOCKER]
    else:
        # bad1 NULL, bad2 an unmapped page, bad3 a kernel address.
        cells = [(o, 16, f'bad{k}') for o in ('pr-str-256.bpf.o', 'pr-read-64.bpf.o')
                 for k in (1, 2, 3)]
    for rep in range(a.reps):
        order = backends[rep % len(backends):] + backends[:rep % len(backends)]
        for obj, length, mode in cells:
            for backend in order:
                t0 = time.time()
                res = run_one(backend, obj, length, a.secs, mode)
                res.update(rep=rep, seconds=round(time.time() - t0, 1))
                with open(a.out, 'a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
