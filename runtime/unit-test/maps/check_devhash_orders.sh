#!/bin/bash
# The orders the device hash map's seal rests on, read in the object code
# (plan results gpu/hash-build-plan-20261127, S2.12). The source is compiled
# for an RCpc core (-mcpu=cortex-a78ae, the Orin's), where an acquire load
# becomes LDAPR and may pass an earlier STLR or atomic: the default build
# emits LDAR for acquire and seq_cst alike and could not tell them apart.
#   writer: the PHASE load after the INFLIGHT increment is LDAR (seq_cst)
#   sealer: the first INFLIGHT load after the PHASE STLR is LDAR (seq_cst)
#   reader: the PHASE load is LDAR or LDAPR (acquire), not LDR
# Usage: check_devhash_orders.sh <build dir> [source file]
# (the source defaults to the one in compile_commands.json; tests of this
# script pass a modified copy). Exit 0: all three hold.
set -euo pipefail
BUILD=${1:?build dir}
SRC=${2:-}
OBJ=$(mktemp -d)/devhash.o
trap 'rm -rf "$(dirname "$OBJ")"' EXIT
CMD=$(python3 -I - "$BUILD/compile_commands.json" "$SRC" "$OBJ" <<'EOF'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(e for e in cc if e['file'].endswith('bpf_map/gpu/nv_gpu_device_hash_map.cpp'))
args = shlex.split(e['command'])
out = []
skip = False
for a in args:
    if skip:
        skip = False
        continue
    if a in ('-o', '-c'):
        skip = True
        continue
    out.append(a)
src = sys.argv[2] or e['file']
out += ['-mcpu=cortex-a78ae', '-c', src, '-o', sys.argv[3]]
print('cd ' + shlex.quote(e['directory']) + ' && ' + ' '.join(shlex.quote(a) for a in out))
EOF
)
bash -c "$CMD"
objdump -d --no-show-raw-insn "$OBJ" | python3 -I -c '
import re, sys
text = sys.stdin.read()
def body(name):
    m = re.search(r"<" + name + r">:\n(.*?)(?:\n\n|\Z)", text, re.S)
    if not m:
        sys.exit("no symbol " + name)
    return [l.split("\t", 1)[1].replace("\t", " ").strip() for l in m.group(1).splitlines() if "\t" in l]
def op(i):
    return i.split()[0]
LOADS = ("ldr", "ldar", "ldapr", "ldapur", "ldur")
bad = []
w = body("bpftime_devhash_writer_enter")
rmw = next((k for k, i in enumerate(w) if re.match(r"(ldadd|ldaxr|stlxr|bl\s)", i)), None)
if rmw is None:
    bad.append("writer: no INFLIGHT increment found: " + "; ".join(w))
else:
    # past an LL/SC loop, to the first load after the RMW
    k = rmw
    while k < len(w) and re.match(r"(ldaxr|ldxr|stlxr|stxr|add|cbnz)", w[k]):
        k += 1
    nxt = next((i for i in w[k:] if op(i) in LOADS), None)
    if nxt is None or op(nxt) != "ldar":
        bad.append("writer: PHASE load after the increment is %r, not ldar" % nxt)
s = body("bpftime_devhash_sealer_enter")
st = next((k for k, i in enumerate(s) if op(i) == "stlr"), None)
nxt = next((i for i in s[st + 1:] if op(i) in LOADS), None) if st is not None else None
if st is None or nxt is None or op(nxt) != "ldar":
    bad.append("sealer: first INFLIGHT load after the PHASE stlr is %r, not ldar" % nxt)
r = body("bpftime_devhash_reader_phase")
ld = next((i for i in r if op(i) in LOADS), None)
if ld is None or op(ld) not in ("ldar", "ldapr"):
    bad.append("reader: PHASE load is %r, not ldar or ldapr" % ld)
print("writer:", "; ".join(w))
print("sealer:", "; ".join(s))
print("reader:", "; ".join(r))
if bad:
    print("FAILED:\n  " + "\n  ".join(bad))
    sys.exit(1)
print("OK")
'
