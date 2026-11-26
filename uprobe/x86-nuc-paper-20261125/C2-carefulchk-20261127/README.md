# C2 careful-mode activation check (2026-11-27 ~02:00 JST, between C2 night 1 and night 2; no measurement running)

run_paper.py sends nginx's stderr to /dev/null, so C2 rows carry no evidence that BPFTIME_CAREFUL=1 took effect on the
leg `Dnextpc+BPFTIME_CAREFUL=1` (the rows do record env {"BPFTIME_CAREFUL": "1"}). This check runs the same image
(bpftime-ssl-nginx:x86-20261026), the same build (/builds/dnextpc, agent sha 53f7f88f…, commit 6b6b247e), the same loader
(/sslsniff/sslsniff-s4 under libbpftime-syscall-server.so) and the harness nginx.conf (port changed to 14043), with
BPFTIME_LOG_OUTPUT=console added so the agent logs to stderr. Script: chk.sh.

- careful=1: ngx1.err has "Careful mode: interposers bound, caches on" (agent log in nginx's master at load; the flag is a
  process static read once at load, so the forked worker inherits it). 5/5 requests 200, 10/10 events delivered.
- careful=0: no such line (ngx0.err). 5/5 requests 200, 10/10 events delivered.
