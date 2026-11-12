import pickle, os, statistics as st
d = pickle.load(open("/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk/runs.pkl", "rb"))
runs, cells = d['runs'], d['cells']
SETS = "P M100 AC AC2 AC3 AC4 AC5 ACn".split()
R = (1, 2, 3, 4)
def cpu(r, c, s):
    x = runs[(r, c, s)]; return x['cpu_s'] / x['wall_s']
def dcpu(r, c, s): return cpu(r, c, s) - cpu(r, c, 'P')
def cost(r, c, s): return (runs[(r, c, s)]['mean'] - runs[(r, c, 'P')]['mean']) / cells[c]['threads']
def mean(r, c, s): return runs[(r, c, s)]['mean']
def stat(r, c, s, k): return int(runs[(r, c, s)]['st'][k])
def lat(r, c, s): return runs[(r, c, s)]['lat']
def med(v): return st.median(v)
