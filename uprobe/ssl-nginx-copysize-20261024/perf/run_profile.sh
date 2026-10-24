#!/bin/sh
# Inside the container (run with docker exec -d): start nginx traced by sslsniff stage $1
# with bpftime build $2, then keep wrk running for 40 s.
STAGE=$1; BUILD=$2
pkill -9 -x nginx; pkill -9 -x stage$STAGE; pkill -9 -x wrk; sleep 0.3
/builds/$BUILD/bpftimetool remove >/dev/null 2>&1
cd /perf
if [ "$STAGE" != none ]; then
    LD_PRELOAD=/builds/$BUILD/libbpftime-syscall-server.so /sslsniff/stage$STAGE </dev/null >/dev/null 2>&1 &
    sleep 2
    PRELOAD=/builds/$BUILD/libbpftime-agent.so
fi
rm -f /perf/nginx.pid
LD_PRELOAD=$PRELOAD nginx -c /perf/nginx.conf -p /perf </dev/null >/dev/null 2>&1 &
for i in $(seq 75); do
    [ "$(curl -k -s -o /dev/null -w '%{http_code}' https://127.0.0.1:4043/index.html)" = 200 ] && break
    sleep 0.2
done
wrk https://127.0.0.1:4043/index.html -c 100 -d 40 > /perf/wrk-stage$STAGE.txt 2>&1
pkill -9 -x nginx; pkill -9 -x stage$STAGE
/builds/$BUILD/bpftimetool remove >/dev/null 2>&1
