Runs of the S5 GPU suite before the claim-check fix (commit 41d68ea8c fixes it).

- `run4-claim-loss-failure.log`: the one failure (pass-B death test): the owner took its claim for
  lost ("description of the segment was closed") and stopped delivering.
- `stress-summary.txt` (the whole suite, 6 runs) and `deaths-summary.txt` (the death test, 8 runs):
  all passed. Their `closed=` column counts the warning in the parent's log, which carries a
  child's output only when that child fails: 0 there says nothing about passing children.
