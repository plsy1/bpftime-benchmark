# DX1-20261125-try1 — note written during the session (16:19)
Exploratory diagnostic, not a PREREG-2 main comparison.
Overlap: the C1 verification workflow (wf_c8e28d7f-f73, read-only analysis: python over rows.jsonl, `strings`/symbol
scans of the agent .so files, git grep) ran ~16:10–16:18, overlapping DX1's first rows (session start 16:14:56). Its
processes inherited the Claude process tree's affinity (CPUs 12–14 after session.sh's taskset), but shared L3/memory.
Rows with t_start < 16:18:30 are flagged for a sensitivity check.
