#!/bin/bash
# The orders the device hash map's seal rests on, read in the object code
# (plan results gpu/hash-build-plan-20261127, S2.12). The source is compiled
# for an RCpc core (-mcpu=cortex-a78ae, the Orin's), where an acquire load
# becomes LDAPR and may pass an earlier STLR or atomic: the default build
# emits LDAR for acquire and seq_cst alike and could not tell them apart.
#   writer: the PHASE load after its slot's count increment is LDAR
#           (seq_cst; the last memory load: a test hook may come between)
#   sealer: every count load after the PHASE STLR is LDAR (seq_cst), and
#           so is every load of its later polls
#   reader: the PHASE load is LDAR or LDAPR (acquire), not LDR
#   and elem_update/elem_delete call the writer's helper, elem_lookup and
#   map_get_next_key the reader's
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
objdump -dr -C --no-show-raw-insn "$OBJ" | python3 -I -c '
import re, sys
text = sys.stdin.read()
def body(name):
    m = re.search(r"<" + re.escape(name) + r"[^>]*>:\n(.*?)(?:\n\n|\Z)", text, re.S)
    if not m:
        sys.exit("no symbol " + name)
    return [l.split("\t", 1)[1].replace("\t", " ").strip() for l in m.group(1).splitlines() if "\t" in l]
def op(i):
    return i.split()[0]
LOADS = ("ldr", "ldar", "ldapr", "ldapur", "ldur")
def mem_loads(lines):
    # loads from memory other than the stack frame (spills, the epilogue)
    return [i for i in lines if op(i) in LOADS and not re.search(r"\[sp", i)]
bad = []
w = body("bpftime_devhash_writer_enter")
rmw = next((k for k, i in enumerate(w) if re.match(r"(ldadd|ldaxr|stlxr|bl\s+\S*ldadd)", i)), None)
if rmw is None:
    bad.append("writer: no count increment found")
else:
    # the PHASE load is the last memory load (a test hook may be loaded
    # and called in between)
    last = mem_loads(w[rmw + 1:])
    if not last or op(last[-1]) != "ldar":
        bad.append("writer: PHASE load after the increment is %r, not ldar" % (last[-1] if last else None))
s = body("bpftime_devhash_sealer_enter")
st = next((k for k, i in enumerate(s) if op(i) == "stlr"), None)
after = mem_loads(s[st + 1:]) if st is not None else []
if st is None or not after or any(op(i) != "ldar" for i in after):
    bad.append("sealer: count loads after the PHASE stlr are %r, not all ldar" % after)
pl = mem_loads(body("bpftime_devhash_sealer_poll"))
if not pl or any(op(i) != "ldar" for i in pl):
    bad.append("sealer poll: count loads are %r, not all ldar" % pl)
r = body("bpftime_devhash_reader_phase")
ld = next((i for i in r if op(i) in LOADS), None)
if ld is None or op(ld) not in ("ldar", "ldapr"):
    bad.append("reader: PHASE load is %r, not ldar or ldapr" % ld)
# the map operations take PHASE only through the helpers
for fn, helper in (("elem_update(", "bpftime_devhash_writer_enter"),
                   ("elem_delete(", "bpftime_devhash_writer_enter"),
                   ("elem_lookup(", "bpftime_devhash_reader_phase"),
                   ("map_get_next_key(", "bpftime_devhash_reader_phase")):
    m = re.search(r"<bpftime::nv_gpu_device_hash_map_impl::" + re.escape(fn) + r"[^>]*>:\n(.*?)(?:\n\n|\Z)", text, re.S)
    if not m or helper not in m.group(1):
        bad.append("%s does not call %s" % (fn, helper))
print("writer:", "; ".join(w))
print("sealer:", "; ".join(s))
print("poll:", "; ".join(pl))
print("reader:", "; ".join(r))
if bad:
    print("FAILED:\n  " + "\n  ".join(bad))
    sys.exit(1)
print("OK")
'
