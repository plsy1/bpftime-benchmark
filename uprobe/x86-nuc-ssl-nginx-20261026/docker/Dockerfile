# Runtime-only image for the ssl-nginx comparison: nginx + wrk + tools.
# bpftime libraries, sslsniff and the benchmark directory are mounted from
# the host at run time (same approach as the July official-no-btf runs).
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        nginx wrk curl python3 procps ca-certificates libelf1 zlib1g \
    && rm -rf /var/lib/apt/lists/*
CMD ["sleep", "infinity"]
