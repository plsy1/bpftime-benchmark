#!/usr/bin/env python3
"""How often does killing a traced process (SIGKILL at a random moment) leave a map lock held?

Per trial: the bpftime loader (src of ../map-table-20261028, frozen build BUILD) attaches
count-hash.bpf.o (hash lookup + atomic add on every call, so every call takes the map's
spinlock) to count_event of countloop; countloop runs THREADS threads on NKEYS shared keys;
after a random 0.3-2.0 s the workload gets SIGKILL; then the loader gets SIGINT and reads the
whole hash map (get_next_key + lookup, both under the map lock). A loader still running 10 s
later is a hang. One JSON line per trial.
"""
import argparse, json, random, re, subprocess, tempfile, time
from pathlib import Path

T = Path(__file__).resolve().parent.parent / 'map-table-20261028' / 'src'
STACKS = None
BUILDS = Path('/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def cleanup(build):
    for name in ('attach', 'countloop'):
        sh(['sudo', '-n', 'pkill', '-9', '-x', name])
    sh(['sudo', '-n', str(BUILDS / build / 'bpftimetool'), 'remove'])
    time.sleep(0.3)


def trial(build, threads, nkeys):
    cleanup(build)
    b = BUILDS / build
    res = dict(build=build, threads=threads, nkeys=nkeys)
    out = tempfile.TemporaryFile(mode='w+', errors='replace')
    tool = subprocess.Popen(['sudo', '-n', 'env', 'SUM_MAP=counts', f'LD_PRELOAD={b / "libbpftime-syscall-server.so"}',
                             'BPFTIME_LOG_OUTPUT=console', 'BPFTIME_SHM_MEMORY_MB=256', str(T / 'attach'),
                             str(T / 'count-hash.bpf.o'), str(T / 'countloop'), 'on_count=count_event'],
                            stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.time() + 30
        while time.time() < deadline:
            out.seek(0)
            if 'attached' in out.read():
                break
            time.sleep(0.2)
        else:
            raise RuntimeError('loader did not attach')
        w = subprocess.Popen(['sudo', '-n', 'env', f'LD_PRELOAD={b / "libbpftime-agent.so"}', 'BPFTIME_LOG_OUTPUT=console',
                              str(T / 'countloop'), str(threads), '30', str(nkeys)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        delay = random.uniform(0.3, 2.0)
        time.sleep(delay)
        sh(['sudo', '-n', 'pkill', '-9', '-x', 'countloop'])
        w.wait(timeout=10)
        res['delay'] = round(delay, 3)
        time.sleep(0.2)
        sh(['sudo', '-n', 'kill', '-INT', '--', f'-{tool.pid}'])
        try:
            tool.wait(timeout=10)
            res['loader'] = 'exited'
            res['loader_rc'] = tool.returncode
        except subprocess.TimeoutExpired:
            res['loader'] = 'hung'
            if STACKS:
                pid = sh(['pgrep', '-n', '-x', 'attach']).stdout.strip()
                bt = sh(['sudo', '-n', 'timeout', '60', 'gdb', '-p', pid, '-batch', '-ex',
                         'thread apply all bt 12'], timeout=90)
                path = Path(STACKS) / f'hung-{int(time.time())}.txt'
                path.write_text(bt.stdout + bt.stderr)
                res['stack_file'] = path.name
        out.seek(0)
        text = out.read()
        m = re.search(r'map_sum (\d+)', text)
        res['map_sum'] = int(m.group(1)) if m else None
        res['tail'] = text[-300:] if res['loader'] != 'exited' or res['map_sum'] is None else ''
    except Exception as e:
        res['error'] = repr(e)
    finally:
        cleanup(build)
    return res


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default='c98951a4')
    ap.add_argument('--trials', type=int, default=40)
    ap.add_argument('--threads', type=int, default=1)
    ap.add_argument('--nkeys', type=int, default=64)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--out', required=True)
    ap.add_argument('--stacks', help='directory for gdb stacks of hung loaders')
    ap.add_argument('--stop-on-hang', action='store_true')
    a = ap.parse_args()
    random.seed(a.seed)
    STACKS = a.stacks
    for i in range(a.trials):
        r = trial(a.build, a.threads, a.nkeys)
        r['trial'] = i
        with open(a.out, 'a') as f:
            f.write(json.dumps(r) + '\n')
        print(json.dumps(r), flush=True)
        if a.stop_on_hang and r.get('loader') == 'hung':
            break
