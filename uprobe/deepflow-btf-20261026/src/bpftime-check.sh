#!/bin/bash
# DeepFlow socket_tracer with its Go uprobes run by bpftime: bpftime_daemon
# redirects the uprobes DeepFlow creates to userspace (kprobes/tracepoints
# stay in the kernel), then the agent is injected into the Go server with
# bpftime attach. Usage: bpftime-check.sh OUTDIR https|http N [daemon args]
set -u
D=/home/jetson/src/deepflow-20261026; I=$D/bpftime-install
T=$D/deepflow-v7.2.3/agent/src/ebpf/samples/rust/socket-tracer/target/release/socket_tracer
OUT=$D/$1; KIND=$2; N=$3; shift 3
PORT=446; SCHEME=https; [ "$KIND" = http ] && PORT=447 && SCHEME=http
mkdir -p "$OUT"
sudo -n pkill -x main; sudo -n pkill -x socket_tracer; sudo -n pkill -x bpftime_daemon; sleep 0.5
sudo -n $I/bpftimetool remove >/dev/null 2>&1
(cd $D/servers/$KIND && sudo -n env BPFTIME_LOG_OUTPUT=console ./main 1024 > "$OUT/server.txt" 2>&1 &)
sleep 1
P=$(pgrep -n -x main)
(sudo -n env BPFTIME_LOG_OUTPUT=console timeout -s INT 200 $I/bpftime_daemon "$@" > "$OUT/daemon.txt" 2>&1 &)
sleep 3
(cd "$OUT" && sudo -n env DF_GO_PIDS=$P timeout -s INT 180 $T > "$OUT/tracer.txt" 2>&1 &)
for i in $(seq 1 60); do grep -aq "adapt success" "$OUT/tracer.txt" 2>/dev/null && break; sleep 1; done
sleep ${DF_WAIT:-70}
[ -z "${NO_ATTACH:-}" ] && sudo -n env BPFTIME_LOG_OUTPUT=console $I/bpftime -i $I attach $P > "$OUT/attach.txt" 2>&1
sleep 2
for i in $(seq 1 $N); do curl --http1.1 -sk -o /dev/null $SCHEME://127.0.0.1:$PORT/req-$i; done
sleep 3
sudo -n pkill -INT -x socket_tracer; sleep 2; sudo -n pkill -x main; sudo -n pkill -INT -x bpftime_daemon; sleep 1
sudo -n $I/bpftimetool remove >/dev/null 2>&1
echo "server pid $P"
tail -2 "$OUT/attach.txt"
grep -aE "error|Failed|warn" "$OUT/daemon.txt" | cut -c27-200 | sort | uniq -c | sort -rn | head -8
grep -aE "error|warn" "$OUT/server.txt" | cut -c1-200 | sort | uniq -c | sort -rn | head -5
grep -aoE "<[A-Z0-9]+>" "$OUT/tracer.txt" | sort | uniq -c
grep -a "<HTTP1>" "$OUT/tracer.txt" | grep -aoE "PID $P .* SOURCE [0-9]+ ROLE [0-9]+" | sed -E 's/THREAD_ID.*CONTAINER_ID +/ /' | sort | uniq -c
grep -aoE "GET /req-[0-9]+" "$OUT/tracer.txt" | sort -u | wc -l
