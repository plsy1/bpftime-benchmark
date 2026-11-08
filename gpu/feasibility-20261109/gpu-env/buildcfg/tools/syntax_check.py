import json, shlex, subprocess, sys, time, os
bdir, outlog = sys.argv[1], sys.argv[2]
want = sys.argv[3:]
fallback = ['-I/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-probecheck/FridaGum-prefix/src/FridaGum',
            '-I/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-probecheck/libbpf',
            '-I/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-probecheck/runtime']
cc = json.load(open(os.path.join(bdir, 'compile_commands.json')))
with open(outlog, 'w') as log:
    for w in want:
        ents = [e for e in cc if e['file'].endswith(w)]
        if not ents:
            print(f'{w}: NOT IN compile_commands'); continue
        e = ents[0]
        args = shlex.split(e['command'])
        out = []
        skip = False
        for i, a in enumerate(args):
            if skip: skip = False; continue
            if a == '-o': skip = True; continue
            if a == '-c': continue
            out.append(a)
        out = ['nice', '-n', '19', 'timeout', '240'] + out[:-1] + fallback + ['-fsyntax-only', out[-1]]
        t0 = time.time()
        p = subprocess.run(out, cwd=e['directory'], capture_output=True, text=True)
        dt = time.time() - t0
        errs = [l for l in p.stderr.splitlines() if 'error' in l or 'fatal' in l]
        warns = [l for l in p.stderr.splitlines() if 'warning:' in l]
        print(f'{w}: rc={p.returncode} {dt:.1f}s errors={len(errs)} warnings={len(warns)}')
        for l in errs[:6]: print('   ', l[:300])
        log.write(f'===== {w} rc={p.returncode}\n{p.stderr}\n')
        sys.stdout.flush()
