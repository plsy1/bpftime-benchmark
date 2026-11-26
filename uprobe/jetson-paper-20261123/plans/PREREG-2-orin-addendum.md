# PREREG-2, Orin addendum (written 2026-11-25 14:57 JST, before any J1/J4/J5/J8 measurement)

The user approved starting the Orin ATC week-1 runs at about 14:55 JST on 11-25.

## §3 margin in us/event (M5 and other TOST)

Inputs are the E18 medians over both rounds, stock legs:
- B = 19,634 rps;
- Dn-stock = 15,776 rps;
- x_Dn = (1/rps_Dn - 1/rps_B) * 1e6 / 2 = 6.227 us/event.

The ±2 % margin, converted:
- (a) ±2 % of Dn's time per request, halved (2 events per request): **±0.634 us/event**.
- (b) ±2 % of x_Dn: ±0.125 us/event.

Which base x86 used (±1.5 % -> ±0.084 us/event) is being asked from the NUC.
- If x86 used the time per request, Orin uses (a).
- If x86 used x, Orin uses (b).

The choice is fixed when NUC answers, before J8 starts. J1, J4 and J5 are exploratory and do not use it.

## §6 noisy core

Orin has no unused core under policy M:
- cores 0-1: wrk;
- core 2: harness, plus this Claude session moved there by session.sh;
- core 3: sslsniff;
- cores 4-5: nginx.

So the x86 noisy_core rule (a non-measured core > 5 % busy) has no Orin counterpart. Core 2 is not recorded.

Noise control instead:
- start guard: load < 0.3, timers outside the window;
- waitload: 3 readings < 0.15, 20 s apart;
- no tool calls during measurement legs.

A row is flagged in the analysis if the sslsniff loader is unhooked (sniff CPU < 0.3 s on a non-B stock leg), as in E18.

## §4 samples

- J8: n = 40 as 2 rounds x 20 reps, 1 warm-up rep per session (not analysed). The sensitivity table drops the first measured rep of each round.
- J4: n = 10 (exploratory).
- J5: 3 reps x 20 s with perf sc.

## Harness

- J1, J4 and J5 run on the current Jetson harness (run_paper.py as in E18, sha256 in each session's log).
- It has --perf-mode sc|cyc but no H1 per-row fields (schedstat ns, migrations, interrupts, smaps, shm size).
- J8 waits for the NUC harness with H1, which comes in the tar after C1.

## Update 14:58 JST (before any measurement): margin fixed to (a)

NUC answered (PREREG-2a, 14:57:40, sha256 bef25e5b...): x86 used each event's whole time,
1e6/89,614/2 = 5.580 us/event x 1.5 % = 0.0837. Orin: Dn-stock 15,776 rps -> 31.69 us/event,
x 2 % = **+-0.634 us/event**. This is equivalent to the +-2 % ratio criterion at Dn's throughput.
J8: every leg uses the s4 counting loader (@S4, no -stock), as in x86 C1 (PREREG-2 §9).
