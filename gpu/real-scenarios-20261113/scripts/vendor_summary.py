"""Summarise cuobjdump PTX/ELF arch counts and NEEDED entries of the vendor libraries.

Usage: python3 -I vendor_summary.py <listing-dir-with-*.ptx.txt-and-*.elf.txt> > summary.txt
NEEDED entries come from readelf -d on the installed library.
"""
import collections, glob, os, re, subprocess, sys

d = sys.argv[1]
LIBS = {
    'libcublas.so.13.4.1.3': '/usr/local/cuda/lib64/libcublas.so.13',
    'libcublasLt.so.13.4.1.3': '/usr/local/cuda/lib64/libcublasLt.so.13',
    'libcudnn_ops.so.9.20.0': '/usr/lib/aarch64-linux-gnu/libcudnn_ops.so.9',
    'libcudnn_cnn.so.9.20.0': '/usr/lib/aarch64-linux-gnu/libcudnn_cnn.so.9',
    'libcudnn_adv.so.9.20.0': '/usr/lib/aarch64-linux-gnu/libcudnn_adv.so.9',
    'libcudnn_engines_precompiled.so.9.20.0': '/usr/lib/aarch64-linux-gnu/libcudnn_engines_precompiled.so.9',
    'libcudnn_engines_runtime_compiled.so.9.20.0': '/usr/lib/aarch64-linux-gnu/libcudnn_engines_runtime_compiled.so.9',
    'libcurand.so.10.4.2.66': '/usr/local/cuda/lib64/libcurand.so.10',
    'libcusolver.so.12.2.0.11': '/usr/local/cuda/lib64/libcusolver.so.12',
    'libcusparse.so.12.7.10.12': '/usr/local/cuda/lib64/libcusparse.so.12',
    'libcufft.so.12.2.0.57': '/usr/local/cuda/lib64/libcufft.so.12',
    'nvinfer': None,
}


def counts(path):
    if not os.path.exists(path):
        return None
    c = collections.Counter(re.findall(r'\.(sm_\d+[a-z]?)\.', open(path).read()))
    return ' '.join(f'{k}:{v}' for k, v in sorted(c.items(), key=lambda kv: (int(re.sub(r'\D', '', kv[0])), kv[0])))


def ok_for_sm87(path):
    if not os.path.exists(path):
        return None
    archs = re.findall(r'\.sm_(\d+)[a-z]?\.', open(path).read())
    return sum(1 for a in archs if int(a) <= 87)


print('library | PTX entries by arch | PTX entries at or below sm_87 | ELF (SASS) entries by arch | NEEDED')
for name, inst in LIBS.items():
    p = os.path.join(d, name + '.ptx.txt')
    e = os.path.join(d, name + '.elf.txt')
    needed = ''
    if inst and os.path.exists(inst):
        out = subprocess.run(['readelf', '-d', os.path.realpath(inst)], capture_output=True, text=True).stdout
        needed = ' '.join(re.findall(r'NEEDED.*\[(.+?)\]', out))
    print(f'{name} | {counts(p)} | {ok_for_sm87(p)} | {counts(e)} | {needed}')
