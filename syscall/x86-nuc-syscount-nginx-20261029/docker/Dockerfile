# x86 NUC runtime image for syscount-nginx (10/29): what the Jetson image
# (benchmark-results/jetson syscall/syscount-nginx-20261029/docker/Dockerfile:
# the ssl-nginx Ubuntu 24.04 image plus sudo, matplotlib, numpy) provides, on
# debian:trixie so that bpftime libraries built on the Debian 13 host load
# (same glibc). procps for pgrep/pkill, gdb for the crash backtrace.
FROM debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
        nginx wrk curl python3 sudo python3-matplotlib python3-numpy procps gdb \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*
CMD ["sleep", "infinity"]
