# For each binary: list every symbol (symtab+dynsym, FUNC/OBJECT/NOTYPE, value!=0, named)
# that shares the start address of the given host symbols -- i.e. what
# bpftime's resolve_host_function_symbol() would have to choose between.
import subprocess, sys, collections
for spec in sys.argv[1:]:
    path, names = spec.split(':', 1)
    out = subprocess.run(['readelf', '-sW', path], capture_output=True, text=True, timeout=20).stdout
    by_addr = collections.defaultdict(list)
    for line in out.splitlines():
        p = line.split()
        if len(p) < 8 or not p[0].endswith(':'):
            continue
        val, size, typ, bind, vis, ndx, name = p[1], p[2], p[3], p[4], p[5], p[6], p[7]
        if typ not in ('FUNC', 'OBJECT', 'NOTYPE') or int(val, 16) == 0:
            continue
        by_addr[int(val, 16)].append((name, typ, bind, vis, size))
    print('==', path)
    for want in names.split(','):
        hits = [a for a, l in by_addr.items() if any(n[0] == want for n in l)]
        for a in hits:
            print('  %-28s @0x%x -> entries at same address: %s' % (want, a, by_addr[a]))
        if not hits:
            print('  %-28s not found' % want)
