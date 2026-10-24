# Jetson kernel with BTF, syscall tracepoints and kprobes (prepared 2026-10-24)

Why: the stock L4T R39.2.1 kernel (`6.8.12-1021-tegra`) has no BTF (`DEBUG_INFO_REDUCED`), no
`CONFIG_FTRACE_SYSCALLS` (no `syscalls:sys_enter_*` tracepoints), no `CONFIG_KPROBES` and no
`CONFIG_FUNCTION_TRACER`. DeepFlow, `bpftime_daemon` and most libbpf-tools need them.

## Phase A (done, nothing in the boot path changed)

- Sources: NVIDIA `public_sources.tbz2` for L4T R39.2.1 (sha256 in `public_sources.sha256`),
  `Linux_for_Tegra/source/{kernel,nvidia-oot,nvgpu,hwpm,nvdisplay,unifiedgpudisp}`.
- Config: `/proc/config.gz` of the running kernel plus DEBUG_INFO_BTF (+_MODULES, REDUCED off),
  FTRACE_SYSCALLS, KPROBES, KPROBE_EVENTS, FUNCTION_TRACER, DYNAMIC_FTRACE, IKHEADERS
  (`build-btf/config.original` vs `build-btf/.config`; other differences are toolchain versions,
  GCC_PLUGINS off with no plugin enabled before, and ftrace dependencies).
- Build: native gcc 13.3, `LOCALVERSION=-1021-tegra-btf` -> `6.8.12-1021-tegra-btf`.
  BTF encoding needed `pahole-j2` (pahole with 2 threads) and a temporary 16 GB swapfile;
  the first attempt was OOM-killed. Swapfile removed afterwards.
- Out-of-tree modules (hwpm, nvidia-oot incl. rtl8822ce WiFi and r8168/nvethernet, nvgpu, both
  display drivers): 184 modules, vermagic `6.8.12-1021-tegra-btf`. Every module of the stock
  kernel has a counterpart.
- Installed side by side (stock files untouched):
  - `/boot/Image-btf` (69 MB; stock `/boot/Image` 53 MB)
  - `/lib/modules/6.8.12-1021-tegra-btf` (stripped with INSTALL_MOD_STRIP=1, .BTF kept; 179 MB)
  - `/boot/initrd-btf` (11.6 MB) built by `make-initrd-btf.sh` like `nv-update-initrd`
    (same module list `/etc/nv-update-initrd/list.d/modules_k6.8`, modprobe.d configs)
  - `/boot/extlinux/extlinux.conf`: new `LABEL btf` with the same APPEND line;
    `DEFAULT primary` unchanged; backup `extlinux.conf.before-btf`.
- Secure Boot is disabled, so an unsigned kernel can boot.

## Phase B (needs someone at the Jetson with a monitor and keyboard)

1. Reboot; in the extlinux menu (3 s) pick `6.8.12-1021-tegra-btf ...`.
2. Check: `uname -r`; `ls /sys/kernel/btf/vmlinux`; `ls /sys/kernel/tracing/events/syscalls | head`;
   `grep -c kprobe /proc/kallsyms`; WiFi up and Tailscale reachable; `lsmod | grep -E "nvgpu|nvidia"`;
   `nvidia-smi` or tegrastats; NVMe root mounted; `dmesg -l err,warn`.
3. If all good, make it the default: `DEFAULT btf` in `/boot/extlinux/extlinux.conf`.
4. Re-run kernel-side baselines (uprobe suite, ssl-nginx kernel leg): kprobes/ftrace support
   may shift kernel numbers.

## Rollback

- Not booting: reboot and pick `primary` in the menu (or it boots by default since DEFAULT is
  unchanged). Worst case: put the NVMe in another Linux machine and restore
  `extlinux.conf.before-btf`.
- To remove completely: restore `extlinux.conf.before-btf`, then delete `/boot/Image-btf`,
  `/boot/initrd-btf` and `/lib/modules/6.8.12-1021-tegra-btf`.
- L4T package updates may rewrite `extlinux.conf` (`nv-update-extlinux`) and drop the `btf` entry.

## Leftovers you may delete (by hand)

`stage/` (2.2 GB unstripped staging), `stage-strip.*`, `initrd-root.*` (root-owned initrd work
dirs). Keep `build-btf/` (9.3 GB) while using the kernel: `/lib/modules/.../build` points to it
for building external modules (e.g. bpftime examples needing kernel headers).
