# E0 (Jetson): probe-site first instructions, kernel provenance, vulnerabilities

Date: 2026-11-23. Platform: Jetson Orin Nano, kernel 6.8.12-1021-tegra-btf.

## Probe-site first instructions (G12)

| binary | symbol | address | first instruction | simulable on 6.13+ arm64? |
|---|---|---|---|---|
| libssl.so.3 (OpenSSL 3.0.13-0ubuntu3.16, from image bpftime-ssl-nginx:20261023, sha256 cd89f6e6...d565) | SSL_read | 0x35e00 | `paciasp` (d503233f) | no (hint, not nop) |
| same | SSL_write | 0x36504 | `paciasp` (d503233f) | no |
| benchmark/test (D tree 9ccda7e8, gcc-13 -O3, sha256 7b721b4d...14a3) | __bench_uprobe | 0xca0 | `ldrb w0, [x0, w1, sxtw]` (3861c800) | no (register-offset load, single-stepped out of line) |

SSL_do_handshake is not probed (only with `--handshake`). Raw disassembly: `probe-sites-libssl.txt`, `probe-sites-bench.txt`.

Verdict for G12: no probe site starts with `nop`, so the 6.13 nop-simulation change would not apply; the kernel version stays a one-sentence note ("newer kernels only make the cache flush faster").

## Kernel provenance

- `/proc/version`: `6.8.12-1021-tegra-btf (jetson@orin-nano) (gcc 13.3.0-6ubuntu2~24.04.1, ld 2.42) #2 SMP PREEMPT Sat Oct 24 23:02:32 JST 2026`.
- `/boot/Image-btf` mtime 2026-10-24 23:23:05 JST, 69,227,008 bytes.
- Source: `~/src/l4t-r39.2.1/public_sources.tbz2`, sha256 check OK against `public_sources.sha256`. README-btf-kernel.md records the build (stock config + BTF/FTRACE_SYSCALLS/KPROBES/FUNCTION_TRACER).
- The kernel build output directory (`build-btf`) no longer exists, so the Image cannot be compared byte for byte; provenance rests on the build string, timestamps and the source tarball hash.
- `/proc/config.gz`: ARM64_BTI=y, ARM64_PTR_AUTH=y, ARM64_PTR_AUTH_KERNEL=y, DEBUG_INFO_BTF=y, UPROBES=y, IRQ_TIME_ACCOUNTING=y.

## Vulnerabilities (`/sys/devices/system/cpu/vulnerabilities`)

spec_store_bypass: Mitigation: Speculative Store Bypass disabled via prctl; spectre_v1: Mitigation: __user pointer sanitization; spectre_v2: Mitigation: CSV2, BHB; all others: Not affected. Full list: `../logs/vulnerabilities-jetson.txt`.
