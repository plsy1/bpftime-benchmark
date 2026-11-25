# I3-20261125-try1 — deviations (written during the session)

1. **External ssh during the session.** The Jetson session ran one `ssh nuc 'ls -la …/paper-20261124/ | grep -i j6;
   pgrep -fa "run_paper|wrk|nginx"'` at about 00:27:05–00:27:12 JST (2026-11-26), under 1 s of work (its report).
   Cause: my message said E3/E4/E5 were done without saying I3/I9 had started. Overlapping row (by t_start/t_end):
   round 2 (256 KB) rep 9 leg K (rps 4618.51, t_start 3.1 s before, t_end 12.0 s after the window start).
   Flag only; check this row against K's other round-2 reps and report results with and without it.
   Check: K round-2 rps across 10 reps 4618.5–4757.4 (median 4663.5); the flagged row is the lowest, 0.04 % below the next (4620.3), inside the normal spread.
