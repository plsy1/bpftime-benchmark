#!/bin/bash
b=$1; B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/$b; i=$2
d=/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/example/bpf_features/get_stack_id_example-core
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/h5
sudo -n $B/bpftimetool remove >/dev/null 2>&1
cd $d && sudo -n timeout 40 env LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 ./get_stack_id_example > /dev/null 2>&1 &
/bin/sleep 3
sudo -n timeout 30 gdb -q -batch -ex 'set pagination off' -ex 'handle SIGTERM nostop noprint pass' -ex 'handle SIGCHLD nostop noprint pass' -ex 'handle SIGCONT nostop noprint pass' -ex "set exec-wrapper env LD_PRELOAD=$B/libbpftime-agent.so BPFTIME_LOG_OUTPUT=console" -ex run -ex 'info threads' -ex bt -ex 'thread apply all bt 12' --args /usr/bin/timeout 4 sh -c "echo VICTIM \$\$; exec $d/victim" > $S/gsi-gdb.$b.$i.txt 2>&1
grep -E "received signal|exited" $S/gsi-gdb.$b.$i.txt | head -2
/bin/sleep 1; sudo -n pkill -INT -x get_stack_id_ex; /bin/sleep 1; sudo -n pkill -9 -x get_stack_id_ex; sudo -n pkill -9 -x victim; sudo -n $B/bpftimetool remove >/dev/null 2>&1
