import json, shlex, subprocess, sys, time
B='/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/gpu-env/buildcfg'
cc=json.load(open(B+'/build-cuda/compile_commands.json'))
fb=['-I/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-probecheck/FridaGum-prefix/src/FridaGum','-I/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-probecheck/libbpf','-I/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-probecheck/runtime']
cudainc='cuda-root-sbsa/targets/aarch64-linux/include'
sel=[e for e in cc if cudainc not in e['command'] and '/third_party/' not in e['file'] and 'BPFTIME_ENABLE_CUDA_ATTACH=1' in e['command']]
print('TUs with CUDA define but no CUDA include path:', len(sel))
bad=0; t0=time.time()
for e in sel:
    a=shlex.split(e['command']); out=[]; skip=False
    for x in a:
        if skip: skip=False; continue
        if x=='-o': skip=True; continue
        if x=='-c': continue
        out.append(x)
    cmd=['nice','-n','19','timeout','60']+out[:-1]+fb+['-M',out[-1]]
    p=subprocess.run(cmd,cwd=e['directory'],capture_output=True,text=True)
    deps=p.stdout
    hit=[h for h in ('cuda.h','nv_attach_impl.hpp','bpf_attach_ctx.hpp') if ('/'+h) in deps]
    if p.returncode!=0 or 'cuda.h' in deps:
        bad+=1
        err=[l for l in p.stderr.splitlines() if 'error' in l][:2]
        print('  ', e['file'].split('candidate/')[-1], 'rc', p.returncode, hit, err)
print('problem TUs', bad, 'secs', round(time.time()-t0,1))
