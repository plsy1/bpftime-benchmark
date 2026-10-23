# db3d475f: report dropped perf samples and wake the consumer before the ring fills (2026-10-23)

Fix for Finding 3 of `../ssl-nginx-2x2-20261023/README.md` (bpftime silently lost about half the
256 KB sslsniff events). Diagnosis, builds and audits: `../ssl-nginx-2x2-20261023/README-perf-lost.md`.

| Check | Result |
|---|---|
| Daily filter | 78 cases / 239653 assertions |
| `[perf_event],[epoll]` | 9 cases / 15062 assertions (new `[perf-lost]`, `[perf-wakeup]` tests) |
| ssl-nginx audit 256 KB | 17.85-18.04 events/request (kernel 17.90-17.98), drops reported like kernel |
| Full ten rounds (`full10-db3d475f`) | all 15 items within +/-2% of a90fa447; uprobe +1.4% (known layout range; the change does not touch uprobe or map paths) |
| ssl-nginx 2 x 10 | 16 B +7.3% vs kernel (a90fa447 +7.8%); 256 KB +6.7% with all events (a90fa447 +10.7% with half) |

The daily filter now also includes `[perf_event],[epoll]` (round 7 of the stack replay).
