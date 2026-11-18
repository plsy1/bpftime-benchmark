"""Checks one tool's maps after pt_victim against the victim's own counts."""
import json, struct, sys, os
tool, d = sys.argv[1], sys.argv[2]
exp = None
for line in open(os.path.join(d, 'client.log'), errors='replace'):
    if line.startswith('{"pt_kdiv"'):
        exp = json.loads(line)
if exp is None:
    print('  FAIL: no expected counts in client.log'); sys.exit(1)
K = [('pt_kdiv', 0), ('pt_kflat', 1)]
def words(name):
    b = open(os.path.join(d, name + '.bin'), 'rb').read()
    return struct.unpack('<%dQ' % (len(b) // 8), b)
ok = True
def check(label, got, want):
    global ok
    good = got == want
    ok = ok and good
    print('  %-28s got %-12d want %-12d %s' % (label, got, want, 'ok' if good else 'FAIL'))
if tool == 'carry':
    w = words('chk')
    for name, k in K:
        n = exp[name]['thread_execs']
        check(name + ' entries', w[k*8+0], n)
        check(name + ' exits', w[k*8+1], n)
        check(name + ' mismatches', w[k*8+2], 0)
        check(name + ' exits without entry', w[k*8+3], 0)
        check(name + ' NULL lookups', w[k*8+4], 0)
elif tool in ('lat', 'latold', 'latmix'):
    w = words('hist')
    for name, k in K:
        n = exp[name]['thread_execs']
        h = w[k*65:k*65+64]
        check(name + ' timed exits', sum(h), n)
        check(name + ' exits without start', w[k*65+64], 0)
        print('    buckets:', {b: v for b, v in enumerate(h) if v})
elif tool == 'tlatomic':
    w = words('chk')
    for name, k in K:
        n = exp[name]['thread_execs']
        check(name + ' exits', w[k*8+0], n)
        check(name + ' mismatches', w[k*8+1], 0)
        check(name + ' NULL lookups', w[k*8+2], 0)
        check(name + ' fetch-add results', w[k*8+3], 0)
elif tool in ('hist', 'histold'):
    w = words('calls')
    T = len(w) // 4
    for name, k in K:
        per = exp[name]['threads_per_launch']; L = exp[name]['launches']
        row = w[k*T:(k+1)*T]
        check(name + ' threads with L calls', sum(1 for x in row[:per] if x == L), per)
        check(name + ' calls beyond the grid', sum(row[per:]), 0)
# delivery status of the new maps: [deliveries, exit delivery, owner,
# syncs covered, syncs at exit]; documented coverage of the last sync
for mname in ('chk', 'hist', 'calls'):
    p = os.path.join(d, mname + '.status')
    if os.path.exists(p) and tool not in ('latold', 'histold'):
        st = words(mname + '.status'.replace('.status', '') + '.status') if False else struct.unpack('<5Q', open(p, 'rb').read()[:40])
        print('    status', mname, 'deliveries %d exit %d owner %d syncs covered %d at exit %d' % st)
        # judged for in-call delivery only (argv[3] == 'in-call'); in the
        # default mode a delivery may not have covered a synchronization
        # made just before exit: reported
        if len(sys.argv) > 3 and sys.argv[3] == 'in-call':
            check(mname + ' last sync covered', st[3], st[4])
            if st[4] == 0:
                check(mname + ' application synchronized', 0, 1)
        else:
            print('    reported: last sync covered: %s (%d of %d)' % ('yes' if st[3] == st[4] and st[4] > 0 else 'no', st[3], st[4]))
print('  RESULT', 'PASS' if ok else 'FAIL')
