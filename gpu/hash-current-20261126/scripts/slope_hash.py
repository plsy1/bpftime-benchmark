"""Per-call cost by slope (RULES-hash-current.md, addendum): the larger sizes
(D4, G8; passes s*) against the smaller ones (D, G; passes h*, median ms).
usage: python3 -I slope_hash.py 'out/h*' 'out/s*'"""
import glob, os, statistics, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location('ah', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'analyze_hash.py'))
ah = importlib.util.module_from_spec(spec)
sys.argv_saved = sys.argv
sys.argv = [sys.argv[0]]  # analyze_hash runs its report on import over argv[1:]
spec.loader.exec_module(ah)
sys.argv = sys.argv_saved


def load(pattern):
    out = {}
    for d in sorted(glob.glob(pattern)):
        for f in os.listdir(d):
            if f.startswith('run-'):
                s, t = f[4:].split('-', 1)
                it = ah.item(os.path.realpath(os.path.join(d, f)))
                out.setdefault((s, t), []).append((d, it))
    return out


small = load(sys.argv[1])
large = load(sys.argv[2])
print('item     size   small ms (median)  calls small  |  large ms per pass  calls large  |  us per call per pass')
for big, sm in (('D4', 'D'), ('G8', 'G')):
    for t in ('empty', 'pre', 'miss', 'upd', 'del', 'tidnd', 'tid'):
        if (sm, t) not in small or (big, t) not in large:
            continue
        k = ah.KIDX[sm]
        sms = statistics.median(it['ms'] for _, it in small[(sm, t)])
        if t == 'empty':
            print('%-8s %-4s  %8.0f  |  %s' % (t, big, sms, ' '.join('%d' % it['ms'] for _, it in large[(big, t)])))
            continue
        cs = statistics.median(ah.calls(t, it['chk'], k) for _, it in small[(sm, t)])
        per = []
        for d, it in large[(big, t)]:
            cl = ah.calls(t, it['chk'], k)
            per.append((it['ms'] - sms) * 1000.0 / (cl - cs))
        print('%-8s %-4s  %8.0f  %9d  |  %s  %9d  |  %s  spread %.0f%%' % (
            t, big, sms, cs, ' '.join('%d' % it['ms'] for _, it in large[(big, t)]),
            ah.calls(t, large[(big, t)][0][1]['chk'], k), ' '.join('%.2f' % p for p in per),
            100 * (max(per) - min(per)) / statistics.median(per)))
