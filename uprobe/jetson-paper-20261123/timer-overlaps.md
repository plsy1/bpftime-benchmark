# Timer firings overlapping legs (flag only, PREREG 2.0.5)

From `journalctl` 14:50–17:05 (systemd Started/Starting lines, excluding sessions/docker). E2 (15:35–16:24) and pinbench had none.

| file | round | rep | leg | timer | time |
|---|---|---|---|---|---|
| e1/E1-rows.jsonl | 1 | 8 | O-stock | anacron | 15:30:37 |
| e1/E1-rows.jsonl | 1 | 9 | D-stock | fwupd-refresh | 15:32:39 |
| e1/E1x-rows.jsonl | 0 | 11 | O | anacron | 16:33:02 |
| e1/E1x-rows.jsonl | 0 | 14 | O-stock | fwupd-refresh | 16:38:18 |
| e1/E1x-rows.jsonl | 0 | 14 | K | fwupd-refresh | 16:38:18 |
| e1/E1x-rows.jsonl | 1 | 11 | D-stock | update-notifier-download | 16:49:33 |
| e1/E1x-rows.jsonl | 1 | 17 | O-stock | systemd-tmpfiles-clean | 16:58:17 |
| e1/E1x-rows.jsonl | 1 | 19 | D | fwupd-refresh | 17:01:24 |
