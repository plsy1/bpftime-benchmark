#!/bin/bash
# DeepFlow socket_tracer on the kernel against the Go server: start the
# server, start the tracer with Go uprobes on its pid, wait until tracing runs,
# send N requests, stop. Usage: kernel-check.sh OUTDIR https|http N [extra env]
set -u
D=/home/jetson/src/deepflow-20261026
T=$D/deepflow-v7.2.3/agent/src/ebpf/samples/rust/socket-tracer/target/release/socket_tracer
OUT=$D/$1; KIND=$2; N=$3; shift 3
PORT=446; SCHEME=https; [ "$KIND" = http ] && PORT=447 && SCHEME=http
mkdir -p "$OUT"
sudo -n pkill -x main; sudo -n pkill -x socket_tracer; sleep 0.5
(cd $D/servers/$KIND && sudo -n ./main 1024 > "$OUT/server.txt" 2>&1 &)
sleep 1
P=$(pgrep -n -x main)
(cd "$OUT" && sudo -n env DF_GO_PIDS=$P "$@" timeout -s INT 120 $T > "$OUT/tracer.txt" 2>&1 &)
for i in $(seq 1 60); do grep -aq "adapt success" "$OUT/tracer.txt" 2>/dev/null && break; sleep 1; done
# Go processes already running are attached 60 s after they are matched.
sleep ${DF_WAIT:-70}
for i in $(seq 1 $N); do curl --http1.1 -sk -o /dev/null $SCHEME://127.0.0.1:$PORT/req-$i; done
sleep 3
sudo -n pkill -INT -x socket_tracer; sleep 2; sudo -n pkill -x main
echo "server pid $P"
grep -aE "golang|Go process|uprobe" "$OUT/tracer.txt" | grep -av "Result" | cut -c1-200 | head -8
grep -aoE "<[A-Z0-9]+>" "$OUT/tracer.txt" | sort | uniq -c
grep -aE "<HTTP1>" "$OUT/tracer.txt" | grep -aoE "SOURCE [0-9]+ ROLE [0-9]+" | sort | uniq -c
grep -aoE "GET /req-[0-9]+" "$OUT/tracer.txt" | sort -u | wc -l
