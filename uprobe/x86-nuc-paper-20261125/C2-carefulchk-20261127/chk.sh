cd /bench
B=/builds/dnextpc
for c in 1 0; do
  rm -f /dev/shm/*bpftime* 2>/dev/null
  BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-syscall-server.so /sslsniff/sslsniff-s4 >sniff$c.out 2>sniff$c.err &
  S=$!; sleep 2
  BPFTIME_CAREFUL=$c BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-agent.so nginx -c /bench/nginx.conf -p /bench >ngx$c.out 2>ngx$c.err &
  N=$!; sleep 2
  for i in 1 2 3 4 5; do curl -k -s -o /dev/null -w '%{http_code} ' https://127.0.0.1:14043/; done; echo
  kill $N; sleep 1; kill -INT $S; sleep 1; kill -9 $S $N 2>/dev/null; wait 2>/dev/null
  echo "=== careful=$c nginx stderr:"; grep -i -E 'careful|interposer' ngx$c.err ngx$c.out | head; echo "(lines: $(cat ngx$c.err | wc -l))"; tail -2 sniff$c.err
done
