#!/usr/bin/env python3
"""Per-write cost by layer (copied from ../../breakdown-20261029, with BPFTIME_TRANSFORMER selecting
the transformer .so: transformer-old.so = dev 1aa74c4c, transformer-new.so = d9c4f1b6).

Legs (writeloop issues N writes of 7 bytes to fd 42 = /dev/null):
  none           no tracing
  rewrite        transformer, dummy agent: svc rewritten, the hook issues the call
                 (trampoline + register save + the transformer's C entry)
  passthrough    + one indirect call into a hook in the agent's dlmopen namespace
  agent-other    real agent; one program on sys_enter_getppid (never called):
                 the agent's dispatch for every system call, no program for write
  agent-exit     real agent; one empty program on sys_exit_write (as the official
                 benchmark/syscall)
  agent-count    real agent; counting programs on sys_enter_write and sys_exit_write
  kernel-other / kernel-exit / kernel-count   the same objects on kernel tracepoints
Counting legs check that the counters equal N. One JSON line per run.
BPFTIME_BUILD selects the bpftime build directory.
"""
import argparse, json, os, re, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = Path('/home/jetson/src/results/syscall/breakdown-20261029/src')
B = Path(os.environ.get('BPFTIME_BUILD',
                        '/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily'))
SERVER = B / 'runtime/syscall-server/libbpftime-syscall-server.so'
AGENT = B / 'runtime/agent/libbpftime-agent.so'
TRANSFORMER = Path(os.environ.get('BPFTIME_TRANSFORMER', str(HERE / 'transformer-new.so')))
OBJ = {'other': 'tp_other.bpf.o', 'exit': 'tp_exit_empty.bpf.o', 'count': 'tp_count.bpf.o'}
LEGS = ['none', 'rewrite', 'passthrough', 'agent-other', 'agent-exit', 'agent-count',
        'kernel-other', 'kernel-exit', 'kernel-count']


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup():
    for name in ('tp_attach', 'writeloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    sh(['sudo', '-n', str(B / 'tools/bpftimetool/bpftimetool'), 'remove'])
    time.sleep(0.3)


def run(leg, n):
    cleanup()
    res = dict(leg=leg, n=n)
    loader = out = None
    try:
        kind = leg.split('-')[0]
        if kind in ('agent', 'kernel'):
            obj = SRC / OBJ[leg.split('-')[1]]
            env = ['sudo', '-n', 'env']
            if kind == 'agent':
                env += [f'LD_PRELOAD={SERVER}', 'BPFTIME_LOG_OUTPUT=console']
            out = tempfile.TemporaryFile(mode='w+', errors='replace')
            loader = subprocess.Popen(env + [str(SRC / 'tp_attach'), str(obj)], stdout=out,
                                      stderr=subprocess.STDOUT, start_new_session=True)
            deadline = time.time() + 30
            while time.time() < deadline:
                out.seek(0)
                if 'attached' in out.read():
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError('loader did not attach')
        cmd = ['sudo', '-n', 'env']
        if leg in ('rewrite', 'passthrough'):
            cmd += [f'AGENT_SO={SRC / "libdummy_agent.so"}', f'LD_PRELOAD={TRANSFORMER}']
            if leg == 'passthrough':
                cmd += ['DUMMY_PASSTHROUGH=1']
        elif kind == 'agent':
            cmd += [f'AGENT_SO={AGENT}', f'LD_PRELOAD={TRANSFORMER}', 'BPFTIME_LOG_OUTPUT=console']
        perf = ['/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf', 'stat', '-x,', '-e', 'instructions:u,cycles:u', '--'] if os.environ.get('PERF_STAT') else []
        w = sh(cmd + perf + [str(SRC / 'writeloop'), str(n)], timeout=300)
        res['perf'] = re.findall(r'^([0-9]+),,(instructions:u|cycles:u)', w.stderr, re.M)
        m = re.search(r'ns_per_write ([\d.]+)', w.stdout)
        res['ns_per_write'] = float(m.group(1)) if m else None
        res['rc'] = w.returncode
        res['rewrote'] = (re.search(r'Rewrote (\d+) of (\d+)', w.stdout + w.stderr) or [None])[0]
        if loader:
            sh(['sudo', '-n', 'kill', '-INT', '--', f'-{loader.pid}'])
            loader.wait(timeout=20)
            out.seek(0)
            text = out.read()
            counts = [int(x) for x in re.findall(r'count\d (\d+)', text)]
            res['counts'] = counts[:3]
        ok = res['ns_per_write'] is not None and res['rc'] == 0
        if leg.endswith('count'):
            ok = ok and res.get('counts', [0, 0])[:2] == [n, n]
        res['ok'] = ok
    except Exception as e:
        res['error'] = repr(e)
        res['ok'] = False
    finally:
        cleanup()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--legs', default=','.join(LEGS))
    ap.add_argument('--rounds', type=int, default=5)
    ap.add_argument('--n', type=int, default=1000000)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    legs = a.legs.split(',')
    for r in range(a.rounds):
        order = legs[r % len(legs):] + legs[:r % len(legs)]
        for leg in order:
            res = run(leg, a.n)
            res['round'] = r
            with open(a.out, 'a') as f:
                f.write(json.dumps(res) + '\n')
            print(json.dumps(res), flush=True)


if __name__ == '__main__':
    main()
