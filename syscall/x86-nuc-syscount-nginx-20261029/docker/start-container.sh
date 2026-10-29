#!/usr/bin/env bash
# Start the measurement container: as on the Jetson (--privileged --pid=host
# --shm-size=1g, dev tree read-only at its own path, this result directory
# writable), plus the host's tracefs and debugfs (Docker does not mount them:
# the first attempt's syscount failed with "failed to determine tracepoint
# 'raw_syscalls/sys_exit' perf event ID: -ENOENT") and --cpuset-cpus 0-7
# (the four P cores, pinned by ../session). Name without the words the
# official script's cleanup matches. Remove with: docker rm -f ngx-x86
set -euo pipefail
N=/home/y/src/bpftime-wt-results-x86/syscall/x86-nuc-syscount-nginx-20261029
T=/home/y/src/bpftime-wt-uprobe-177a1dc
sudo -n docker run -d --name ngx-x86 --privileged --pid=host --shm-size=1g --cpuset-cpus 0-7 \
    -v /sys/kernel/tracing:/sys/kernel/tracing -v /sys/kernel/debug:/sys/kernel/debug \
    -v "$T:$T:ro" -v "$N:$N" -w "$N/view" x86-nginx-wrk:20261029
