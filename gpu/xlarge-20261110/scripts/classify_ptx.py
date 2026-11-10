import glob, re, os, time, hashlib
for f in sorted(glob.glob('/tmp/bpftime-fatbin-work.*/patched.temp.1.sm_75.ptx'), key=os.path.getmtime):
    s = open(f).read()
    out = []
    for name in ('__retprobe_func___Z9vectorAddPKfS0_Pf', '__probe_func___Z9vectorAddPKfS0_Pf'):
        m = re.search(r'\.func\s+' + re.escape(name) + r'\(\)(.*?)(?=\n\.(visible|entry|func|weak))', s, re.S)
        calls = re.findall(r'call[^\n]*?(\w+),\s*\n?\s*\(', m.group(1)) if m else None
        calls = re.findall(r'(_bpf_helper_ext_\d+|bpf_main\w*|\w+_prog\w*)', m.group(1)) if m else None
        out.append(f"{name[:9]}={sorted(set(calls)) if calls is not None else '-'}")
    gate = s.count('__bpftime_probes_enabled')
    print(time.strftime('%H:%M:%S', time.localtime(os.path.getmtime(f))), f.split('/')[2], 'gate', gate, hashlib.md5(s.encode()).hexdigest()[:8], *out)
