# Array JIT eligibility survey (2026-10-21)

Question: before generalizing the array lookup JIT, which real programs can
already use it, which are blocked, by what, and how often do real programs
look up arrays versus hashes? Analysis only; no runtime code changed and
nothing was loaded into bpftime or the kernel.

## Corpus and method

- `bpftime`: all 48 `example/**/*.bpf.c` of `5466f0d` (46 compiled; x86-only
  `goroutine` and `hotpatch/vim` failed, neither has map lookups).
- `libbpf-tools`: all 57 tools of bcc `v0.37.0` (`306a2819`), cloned to
  `/home/jetson/src/survey-sources/bcc-v0.37.0`, all compiled.
- `compile.sh`: BPF clang 15, `-g -O2 -target bpf -D__TARGET_ARCH_arm64`,
  libbpf headers from the baseline build.
- `maps-meta.c`: libbpf `bpf_object__open_file` (no kernel access) for map
  types/key sizes and program sections.
- `analyze.py`: own ELF/relocation parser applying the program-level gate of
  `runtime/src/bpftime_prog.cpp` (5466f0d, ~L234-251) and per-site selection.
  Limits: lookup map identity is a backward scan for the last write to r1
  (heuristic); BPF-to-BPF callees in `.text` are attributed to every calling
  program; counts are static call sites, not dynamic frequency; runtime
  conditions (LLVM backend, standard helpers, no CUDA/MPK/AOT) are assumed.
  Checked against known objects: official `uprobe.bpf.o` and both consuming
  workloads classify exactly as the runtime logs show.
- Output: `programs.csv` / `programs.json` (449 program rows).

## Results

| | bpftime examples | libbpf-tools |
|---|---:|---:|
| Objects / programs | 46 / 84 | 57 / 365 |
| Programs with any map lookup | 55 | 183 |
| ... passing the JIT program gate | 44 | 8 |
| ... **blocked by global data** (`.rodata`/`.bss`) | 11 | **175 (96%)** |
| Lookup sites: hash | 36 | 376 (93%) |
| Lookup sites: ordinary array | 19 | 0 |
| Lookup sites: per-CPU array | 3 | 19 (5%) |
| Lookup sites: per-CPU hash | 0 | 8 |
| Programs with array lookups / currently inlinable | 16 / 14 | 19 / **0** |

(bpftime also has 27 sites on bpftime-specific GPU map types 1501-1513, plus
1 task_storage and 1 lpm_trie.)

Restricted to attach types bpftime can run in userspace (uprobe/uretprobe/usdt,
syscall tracepoints): bpftime 50 programs, 13 with array lookups, 12 inlinable;
libbpf-tools 11 objects / 77 programs, 38 with lookups of which 37 are blocked
by global data; lookup sites hash 65, per-CPU array 9 (mountsnoop, opensnoop).

Global-data references: libbpf-tools `.rodata` 788 references in 46 of 57
objects (`const volatile` configuration such as target pid/filters), `.bss`
74 references in 23 objects. No program was blocked by CALLX; extern/kconfig
references occur in 4 libbpf-tools programs and 1 bpftime example, always
together with global data.

The libbpf-tools per-CPU array lookups are in 7 tools (filelife, hardirqs,
mountsnoop, oomkill, opensnoop, softirqs, tcppktlat); all 19 programs are
blocked only by global data. In bpftime examples, array lookups are mostly in
the `bpf_features` demos; `sslsniff`'s per-CPU array buffer is already eligible.

## Conclusions

1. Real libbpf-style programs look up hash maps far more than arrays: 93% of
   libbpf-tools lookup sites are hash, 5% per-CPU array, no ordinary array.
2. The current JIT reaches **none** of the libbpf-tools corpus: 96% of their
   lookup programs are rejected for referencing global data, the standard
   libbpf configuration mechanism. Accepting direct map-value LDDWs (which the
   base LLVM JIT already supports via `map_val`) is a prerequisite for any
   JIT specialization reaching such programs, including a future hash JIT.
3. Accepting global data alone would make 19 libbpf-tools per-CPU array sites
   (9 of them in userspace-runnable tools) inlinable: a small reach.
4. The large lever for real programs is hash lookup, which is already 0.94x
   kernel through the optimized C++ path; inlining it is much harder (shared
   memory layout, concurrency, lifetime).
