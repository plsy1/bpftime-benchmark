import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
for c in ('c31', 'c32'):
    print('==', c)
    for s in SETS:
        x = [runs[(r, c, s)] for r in R]
        cpl = [y['cpu_s'] * 1e6 / cells[c]['launches'] for y in x]
        cpl2 = [y['cpu_s'] / y['wall_s'] * y['mean'] for y in x]
        line = f"  {s:5s} cpu/launch " + " ".join(f"{v:.1f}" for v in cpl) + " | ratio*mean " + " ".join(f"{v:.1f}" for v in cpl2) + " | mean " + " ".join(f"{y['mean']:.1f}" for y in x) + " | dcpu " + " ".join(f"{dcpu(r,c,s):+.3f}" for r in R)
        if s != 'P':
            g = [sum(v > 1_000_000 for v in lat(r, c, s)) for r in R]
            n = [len(lat(r, c, s)) for r in R]
            line += f" | >1ms {g} of {set(n)}"
        if s not in ('P', 'M100'):
            req = [stat(r, c, s, 'req') for r in R]
            open_ = [100 * (stat(r, c, s, 'req') - stat(r, c, s, 'win0')) / stat(r, c, s, 'req') for r in R]
            w80 = [100 * stat(r, c, s, 'win80') / stat(r, c, s, 'req') for r in R]
            hot = [stat(r, c, s, 'hot_iters') for r in R]
            line += " | open% " + " ".join(f"{v:.1f}" for v in open_) + " | win80% " + " ".join(f"{v:.1f}" for v in w80) + f" | hot {hot} | win0 {[stat(r,c,s,'win0') for r in R]}"
        print(line)
