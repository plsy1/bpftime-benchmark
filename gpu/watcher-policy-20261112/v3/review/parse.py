import os, re, sys, json, pickle
D = "/home/jetson/src/bpftime-gpu-work/policy/logs/v3"
# cell defs from rounds5.sh
cells = {}
for line in open("/home/jetson/src/bpftime-gpu-work/policy/rounds5.sh"):
    m = re.match(r'\s*"(c\d+) (\S+) (\S+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)"', line)
    if m:
        cells[m.group(1)] = dict(probe=m.group(2), load=m.group(3), spin=int(m.group(4)), launches=int(m.group(5)),
                                threads=int(m.group(6)), jitter=int(m.group(7)), gap=int(m.group(8)), graph=int(m.group(9)), procs=int(m.group(10)))
SETS = "P M100 AC AC2 AC3 AC4 AC5 ACn".split()
runs = {}
problems = []
for r in (1, 2, 3, 4):
    for c, cd in cells.items():
        for s in SETS:
            b = f"{D}/r{r}/{c}_{s}"
            rec = dict(r=r, c=c, s=s)
            # env
            try:
                et = open(b + ".env").read()
            except FileNotFoundError:
                problems.append((r, c, s, "no env")); continue
            first = et.splitlines()[0]
            rec['env_set'] = re.search(r"set=(\S+)", first).group(1)
            rec['env_cell_ok'] = (f"spin_us={cd['spin']} launches={cd['launches']} threads={cd['threads']} jitter_us={cd['jitter']}" in first) and (f"load={cd['load']}" in first)
            sl = [l for l in et.splitlines() if l.startswith(("before", "after"))]
            rec['n_state'] = len(sl)
            rec['clk'] = [tuple(re.search(r"cpu5=(\d+) cpu0=(\d+) gpu=(\d+)", l).groups()) for l in sl]
            temps = []
            for l in sl:
                m = re.search(r"temps=(\S+)", l)
                if m:
                    temps += [int(x) for x in m.group(1).split(",") if x.strip()]
            rec['tmax'] = max(temps) / 1000 if temps else None
            rec['vrc'] = [int(x) for x in re.findall(r"^victim \d+ rc=(-?\d+)", et, re.M)]
            rec['lrc'] = [int(x) for x in re.findall(r"^loader \d+ rc=(-?\d+)", et, re.M)]
            m = re.search(r"^load rc=(-?\d+)", et, re.M); rec['ldrc'] = int(m.group(1)) if m else None
            rec['notes'] = [l for l in et.splitlines() if not re.match(r"(set=|before |after |victim \d+ rc=|loader \d+ rc=|load rc=)", l)]
            # victim
            vt = open(b + ".1.victim").read()
            m = re.search(r"per_launch_us mean=([\d.]+) median=([\d.]+)", vt)
            rec['mean'] = float(m.group(1)); rec['median'] = float(m.group(2))
            m = re.search(r"cpu_s=([\d.]+) wall_s=([\d.]+)", vt)
            rec['cpu_s'] = float(m.group(1)); rec['wall_s'] = float(m.group(2))
            m = re.search(r"launches=(\d+) threads=(\d+) spin_us=(\d+) jitter_us=(\d+)", vt)
            rec['v_params_ok'] = tuple(int(x) for x in m.groups()) == (cd['launches'], cd['threads'], cd['spin'], cd['jitter'])
            m = re.search(r"window_ns=(\d+) (\d+)", vt); rec['win_ns'] = (int(m.group(1)), int(m.group(2)))
            # loader
            if s != "P":
                lines = open(b + ".1.loader").read().splitlines()
                rec['ld_lines'] = len(lines)
                m = re.match(r"hostspin count=(\d+)", lines[0]); rec['count'] = int(m.group(1))
                assert lines[1].startswith("hostspin ns=")
                vals = [int(x) for x in lines[1][len("hostspin ns="):].split(",") if x.strip()]
                rec['nvals'] = len(vals)
                rec['lat'] = vals[1:]
                # stats
                st = [dict(re.findall(r"(\w+)=(-?[\d.]+|\S+)", l)) for l in open(b + ".stats") if l.strip()] if os.path.exists(b + ".stats") else []
                rec['stats_lines'] = len(st)
                rec['st'] = st[0] if st else None
            # load
            if cd['load'] != 'off':
                pts = [tuple(int(x) for x in l.split()) for l in open(b + ".load") if len(l.split()) == 2]
                rec['loadpts'] = pts
            runs[(r, c, s)] = rec
pickle.dump(dict(runs=runs, cells=cells, problems=problems), open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "runs.pkl"), "wb"))
print(len(runs), problems)
