# E7 (eval plan L446): the frozen measurement image plus haproxy. libssl must stay 3.0.13-0ubuntu3.16 (checked below).
FROM bpftime-ssl-nginx:x86-20261026
RUN dpkg-query -W -f='${Package} ${Version}\n' libssl3t64 openssl nginx > /pkg-before.txt 2>/dev/null || true
RUN apt-get update && apt-get install -y --no-install-recommends --no-upgrade haproxy && rm -rf /var/lib/apt/lists/*
RUN dpkg-query -W -f='${Package} ${Version}\n' libssl3t64 openssl nginx haproxy > /pkg-after.txt 2>/dev/null || true; \
    grep libssl3t64 /pkg-before.txt; grep libssl3t64 /pkg-after.txt; [ "$(grep libssl3t64 /pkg-before.txt)" = "$(grep libssl3t64 /pkg-after.txt)" ]
