#!/usr/bin/env python3
"""Set up one leg of run_correct.py and hold the load for perf (runs in the container, cwd = view/).

Prints "NGINX_PID <pid>" once nginx serves (syscount attached first for traced legs), then runs
wrk -c 100 -d 20 and prints its summary; the host profiles the pid meanwhile (profile_nginx.sh).
"""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_correct as rc

leg = sys.argv[1]
rc.clean()
bpftime = leg.startswith('bpftime') or leg == 'transformer_only'
env_args = [f'AGENT_SO={rc.AGENT}', f'LD_PRELOAD={rc.TRANSFORMER}'] if bpftime else []
nginx = rc.spawn_nginx(env_args)
sc = None
if leg not in ('native', 'transformer_only'):
    target = nginx.pid if leg.endswith('_targeted') else 1
    sc = rc.Syscount(target, leg.startswith('bpftime'))
rc.release_nginx(nginx)
print('NGINX_PID', nginx.pid, flush=True)
w = rc.sh('wrk', rc.URL, '-c', '100', '-d', '20', timeout=40)
m = re.search(r'Requests/sec:\s*([\d.]+)', w.stdout)
print('RPS', m.group(1) if m else None, flush=True)
if sc is not None:
    print(rc.parse_table(sc.finish()), flush=True)
nginx.kill()
nginx.wait()
rc.clean()
