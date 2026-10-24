#!/bin/bash
# Checks after booting the 6.8.12-1021-tegra-btf kernel. Read-only except a temporary
# kprobe event that is removed again. Run: sudo /home/jetson/src/l4t-r39.2.1/check-btf-kernel.sh
set -u
pass=0; fail=0
ok()  { echo "  [OK]   $1"; pass=$((pass + 1)); }
bad() { echo "  [FAIL] $1"; fail=$((fail + 1)); }
check() { if eval "$2" >/dev/null 2>&1; then ok "$1"; else bad "$1"; fi; }

echo "== Kernel"
echo "  uname -r: $(uname -r)"
check "running 6.8.12-1021-tegra-btf" '[ "$(uname -r)" = 6.8.12-1021-tegra-btf ]'

echo "== New tracing features"
check "BTF: /sys/kernel/btf/vmlinux exists" '[ -s /sys/kernel/btf/vmlinux ]'
check "module BTF (e.g. nvme)" '[ -e /sys/kernel/btf/nvme ]'
check "syscall tracepoints: syscalls/sys_enter_bpf" '[ -d /sys/kernel/tracing/events/syscalls/sys_enter_bpf ]'
echo "  syscall tracepoints: $(ls /sys/kernel/tracing/events/syscalls 2>/dev/null | wc -l)"
if (echo 'p:btfcheck_probe do_sys_openat2' > /sys/kernel/tracing/kprobe_events) 2>/dev/null; then
    ok "kprobe event can be created"
    echo '-:btfcheck_probe' >> /sys/kernel/tracing/kprobe_events
else
    bad "kprobe event can be created"
fi
check "function tracer available" 'grep -qw function /sys/kernel/tracing/available_tracers'

echo "== Hardware and drivers"
check "root on NVMe" 'findmnt -n -o SOURCE / | grep -q nvme0n1p1'
check "WiFi driver rtl8822ce loaded" 'lsmod | grep -q "^rtl8822ce "'
check "WiFi wlP1p1s0 is up" 'ip -br link show wlP1p1s0 | grep -q UP'
check "Tailscale connected" 'tailscale status --peers=false'
check "GPU driver nvgpu loaded" 'lsmod | grep -q "^nvgpu "'
check "display driver nvidia loaded" 'lsmod | grep -q "^nvidia "'
check "wired NIC driver r8168 available" 'modinfo -n r8168'
check "perf PMU available" '[ -d /sys/bus/event_source/devices/armv8_cortex_a78 ] || ls /sys/bus/event_source/devices | grep -qi arm'

echo "== Kernel log (errors since boot, first 15; stock-kernel baseline: dmesg-errors-stock.txt)"
dmesg -l err,crit,alert,emerg 2>/dev/null | head -15 | sed 's/^/  /'
echo "  module load failures: $(dmesg 2>/dev/null | grep -ciE 'disagrees about version|Unknown symbol|invalid module format')"

echo "== Result: $pass OK, $fail FAIL"
[ "$fail" = 0 ]
