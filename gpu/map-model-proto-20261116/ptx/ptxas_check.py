"""Registers and local frames of the four probed llama.cpp kernels: the
original PTX against the patched PTX (kept with BPFTIME_CUDA_KEEP_PTX=1),
compiled with ptxas -v for sm_87. usage: ptxas_check.py SETDIR..."""
import glob, os, re, subprocess, sys
PTXAS = '/usr/local/cuda-13.2/bin/ptxas'
K = {'rms': '_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f',
     'mmq': '_Z9mul_mat_qIL9ggml_type12ELi128ELb0EL9ggml_prec30ELb1EEvPKcS3_PKiS5_PK25ggml_cuda_mm_id_tile_specPfS9_PKf11ggml_glu_opfSB_SB_5uint3iiiiiSD_SD_iiiSD_SD_iiiSD_',
     'mmvq': '_Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj',
     'ropek': '_Z9rope_normILb1ELb0Ef6__halfEvPKT1_PT2_iiiiiiiiiiiPKifff14rope_corr_dimsfPKfPKlib'}
cache = {}
def props(path):
    if path in cache:
        return cache[path]
    # the kept patched PTX has the trampoline's .target sm_61; the agent
    # compiles it for the device's architecture
    tmp = '/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/ptxas_check.ptx'
    txt = open(path, errors='replace').read()
    txt = re.sub(r'^\.target sm_\d+', '.target sm_87', txt, count=1, flags=re.M)
    open(tmp, 'w').write(txt)
    r = subprocess.run([PTXAS, '-arch=sm_87', '-v', tmp, '-o', '/dev/null'],
                       capture_output=True, text=True, timeout=1800)
    out, cur = {}, None
    for line in r.stderr.splitlines():
        m = re.search(r"Compiling entry function '([^']+)'", line)
        if m:
            cur = m.group(1); out[cur] = {}
        m = re.search(r'(\d+) bytes stack frame, (\d+) bytes spill stores, (\d+) bytes spill loads', line)
        if m and cur and 'frame' not in out[cur]:
            out[cur].update(frame=int(m.group(1)), spill_st=int(m.group(2)), spill_ld=int(m.group(3)))
        m = re.search(r'Used (\d+) registers', line)
        if m and cur:
            out[cur]['regs'] = int(m.group(1))
    if r.returncode:
        out['__error__'] = r.stderr[-400:]
    cache[path] = out
    return out
def warps_by_regs(r):
    per_warp = -(-r * 32 // 256) * 256
    return min(48, 65536 // per_warp)
for setdir in sys.argv[1:]:
    files = glob.glob(os.path.join(setdir, '*', '*.ptx'))
    print('##', setdir)
    for name, k in K.items():
        orig = [f for f in files if os.path.basename(f) == 'temp.1.sm_87.ptx' and ('.entry ' + k + '(') in open(f, errors='replace').read()]
        pat = [f for f in files if os.path.basename(f).startswith('patched.') and ('.entry ' + k + '(') in open(f, errors='replace').read() and ('__retprobe_func__' + k) in open(f, errors='replace').read()]
        if not orig or not pat:
            print(name, 'missing', len(orig), len(pat)); continue
        o, p = props(orig[0]).get(k, {}), props(pat[0]).get(k, {})
        print('%-6s original regs %3s frame %5s | patched regs %3s frame %5s spills %s/%s | warps/SM by regs %s -> %s' % (
            name, o.get('regs'), o.get('frame'), p.get('regs'), p.get('frame'), p.get('spill_st'), p.get('spill_ld'),
            warps_by_regs(o.get('regs', 0) or 1), warps_by_regs(p.get('regs', 0) or 1)))
