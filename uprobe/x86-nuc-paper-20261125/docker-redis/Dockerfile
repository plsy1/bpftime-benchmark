# I1 (eval plan L530): the frozen measurement image plus Ubuntu 24.04's redis-server / redis-tools (TLS built in,
# dynamically linked to the system libssl). libssl must stay as in the frozen image (checked below), like docker-haproxy.
FROM bpftime-ssl-nginx:x86-20261026
RUN dpkg-query -W -f='${Package} ${Version}\n' libssl3t64 openssl nginx > /pkg-before.txt 2>/dev/null || true
RUN apt-get update && apt-get install -y --no-install-recommends --no-upgrade redis-server redis-tools && rm -rf /var/lib/apt/lists/*
RUN dpkg-query -W -f='${Package} ${Version}\n' libssl3t64 openssl nginx redis-server redis-tools libjemalloc2 > /pkg-after.txt 2>/dev/null || true; \
    grep libssl3t64 /pkg-before.txt; grep libssl3t64 /pkg-after.txt; [ "$(grep libssl3t64 /pkg-before.txt)" = "$(grep libssl3t64 /pkg-after.txt)" ]; \
    redis-server --version; ldd /usr/bin/redis-server | grep -E 'libssl|libcrypto'; ldd /usr/bin/redis-benchmark | grep -E 'libssl'
