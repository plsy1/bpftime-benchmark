line-ab and the second stat_track batch ran 03:30-03:46 on 2026-10-30 while the bpftime-jetson-daily timer (started 03:34:49) built and measured on the same CPUs: discarded.
The per-build *.track.stat.txt of the first (clean) stat_track batch were overwritten by the second; only stat-track.jsonl keeps the clean numbers (c98951a4, stripe-tokens, stripe-view).
