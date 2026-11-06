# opensnoop delivery against the burst size of the opens (same average rate).
# Uses the realtools audit harness (traced(): tool started, workload run under
# the transformer for bpftime, tool stopped with SIGINT).
import json, re, sys
from pathlib import Path
sys.path.insert(0, '/home/jetson/src/realprog-20261026-btf/harness')
import audit

HERE = Path(__file__).resolve().parent
WL = HERE / 'openburst'
CONFIGS = [(100, 10000), (50, 5000), (25, 2500), (10, 1000), (1, 100)]

def one(backend, burst, pause, secs, rep):
    audit.cleanup(backend)
    audit.run(['sudo', '-n', 'pkill', '-9', '-x', 'openburst'])
    rc, wout, tout = audit.traced([str(audit.LT / 'opensnoop')],
                                  [str(WL), str(secs), str(burst), str(pause)],
                                  backend, syscall=True)
    audit.cleanup(backend)
    total = audit.expected_calls(wout, None, secs)
    got = len(re.findall(r'\bopenburst\b.*/etc/hostname', tout))
    m = re.search(r'burst_us_median ([\d.]+)', wout)
    r = re.search(r'ops_per_sec ([\d.]+)', wout)
    res = dict(backend=backend, burst=burst, pause_us=pause, rep=rep, workload_rc=rc,
               expected=total, reported=got,
               delivered=round(got / total, 4) if total else None,
               burst_us_median=float(m.group(1)) if m else None,
               ops_per_sec=float(r.group(1)) if r else None)
    raw = HERE / 'raw'
    raw.mkdir(exist_ok=True)
    stem = f'{backend}.b{burst}.r{rep}'
    (raw / f'{stem}.workload.txt').write_text(wout)
    (raw / f'{stem}.tool.txt').write_text(tout)
    return res

def main():
    backends = sys.argv[1].split(',')
    reps = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    out = HERE / (sys.argv[3] if len(sys.argv) > 3 else 'results.jsonl')
    for rep in range(reps):
        for burst, pause in CONFIGS:
            for backend in backends:
                res = one(backend, burst, pause, 4, rep)
                with out.open('a') as f:
                    f.write(json.dumps(res) + '\n')
                print(json.dumps(res), flush=True)

if __name__ == '__main__':
    main()
