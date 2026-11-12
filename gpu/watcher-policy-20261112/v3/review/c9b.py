import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
for c in ('c01', 'c15', 'c21'):
    for s in SETS[1:]:
        print(c, s, '>1ms per run', [sum(v > 1e6 for v in lat(r, c, s)) for r in R], 'cost-AC per round', [round(mean(r,c,s)-mean(r,c,'AC'),1) for r in R])
# claim 10 checks: AC3 vs AC cost per call and CPU per round in burst cells
for c in ('c05','c06','c25','c26','c27','c28'):
    print(c, 'AC3<AC cost all rounds', all(cost(r,c,'AC3') < cost(r,c,'AC') for r in R), 'AC3 cpu>AC all rounds', all(dcpu(r,c,'AC3') > dcpu(r,c,'AC') for r in R))
