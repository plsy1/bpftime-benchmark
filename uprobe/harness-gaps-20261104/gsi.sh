#!/bin/bash
# get_stack_id example by hand: BUILD VARIANT (plain | stdbuf); prints the workload rc and counts.
b=$1; v=$2; B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/$b
d=/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/example/bpf_features/get_stack_id_example-core
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/h5
sudo -n $B/bpftimetool remove >/dev/null 2>&1
cd $d && sudo -n timeout 30 env LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 ./get_stack_id_example > $S/gsi.$b.$v.tool.txt 2>&1 &
/bin/sleep 3
if [ $v = stdbuf ]; then w="exec stdbuf -oL $d/victim"; else w="exec $d/victim"; fi
sudo -n env LD_PRELOAD=$B/libbpftime-agent.so BPFTIME_LOG_OUTPUT=console timeout 4 sh -c "echo VICTIM \$\$; $w" > $S/gsi.$b.$v.wl.txt 2>&1
echo "$b $v rc=$? loops=$(grep -c 'continue malloc' $S/gsi.$b.$v.wl.txt) ringbuf_errors=$(grep -c 'ringbuf map fd' $S/gsi.$b.$v.wl.txt)"
/bin/sleep 1; sudo -n pkill -INT -x get_stack_id_ex; /bin/sleep 1; sudo -n pkill -9 -x get_stack_id_ex; sudo -n $B/bpftimetool remove >/dev/null 2>&1
