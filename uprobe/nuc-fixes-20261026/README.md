# Fixes for the x86 NUC findings (2026-10-26)

The x86 NUC session (`summry/jetson` `uprobe/bpftime-x86-nuc-findings-20261026.md`) found four issues on the
dev branch; fixed on `dev/uprobe-official-177a1dc465ff` (then `codex/…`) on the Jetson:

| # | Commit | Issue | Test that fails without the fix | Official 177a1dc |
|---|---|---|---|---|
| 1 | `a0b0eb2f` | `bpf_get_stackid` returned -EEXIST as `0x00000000ffffffef` (both `?:` branches unsigned) | `get_stackid returns errors as negative 64-bit values`: got 4294967279 | casts every result to `uint32_t` |
| 2 | `fb506ff4` | configuration failed with only clang-19 (`DIAGNOSTIC_BPF_CLANG REQUIRED`) | `check-clang19-configure.sh`: old `configure-old.log` fails as on the NUC, new `configure-new.log` passes and builds the BPF object with clang-19 | no such check |
| 3 | `7b6fa4fd` | gum symbol lookups before `gum_init_embedded` crashed (`Test attach replace with ebpf` run alone) | the existing test run alone: SIGSEGV, exit 139 (`attach-replace-dev-before-fix.txt`) | same crash (`attach-replace-official-baseline.txt`, a build whose test and frida attach sources equal 177a1dc) |
| 4 | `627a2201` | signed/unsigned comparison warning in typed helper selection | none possible: warning only, behaviour unchanged | — |

After the fixes: daily filter 110 cases (`daily-filter-tests.log`), frida 18 cases (`frida-tests.log`),
daemon 10 cases with root (`daemon-tests.log`) all pass.
