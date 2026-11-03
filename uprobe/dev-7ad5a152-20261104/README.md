# Hash map: updates in place, lookups re-check their link (dev 7ad5a152), 2026-11-04 00:20-01:25

Review R1 of `../dev-518a6b99-20261103/REVIEW.md`: an update of an existing key linked the new
element at the head and then unlinked the old one (as the kernel's `htab_map_update_elem`), so a
lookup on the old element's predecessor reached the end and missed a key present throughout.
The user first kept the kernel's behaviour, then, after the NUC showed up to 0.13% misses, asked
for the kernel's own rate on the NUC and for the fix.

x86 side (bpf's htab under `BPF_PROG_TEST_RUN`, bpftime harness, CPU placements, NUC tests and
full ten): `uprobe/x86-nuc-r1-miss-20261104` on the **benchmark-results/x86-nuc** branch.
Summary: the kernel's htab misses 0-0.63% of lookups per 2 s round under the same two-key load,
d5279469 0.001-0.31%, 7f35edec (in place, acquire next, word re-check) 1-25 per round, 7ad5a152
(plus the link re-check) 0 in 3.5 billion.

## Change (dev 7ad5a152)

- The new element takes the old one's place: written, then published by one release store of
  the old element's link (head or predecessor's next), then the old one retired, kept as spare.
- Lookups load next with acquire (relaxed + acquire fence after a memcmp'd key) and follow it
  only if the link they came through (reloaded, acquire) still points to the element and its
  word is unchanged: a retired element may be reused further down the same chain.
- Updated keys keep their place: next_key no longer returns them again.

## Tests

- "hash map lookups of keys updated at once never miss" requires 0 misses (d5279469's code: 2-7
  per run on the Orin); "hash map updates keep the chain's order" (d5279469: the tail key moved
  to the head). Map tests 47 cases green; candidate gate green (`gate-7ad5a152.log`).

## Cost on the Orin (cpu5 1728000 before and after)

Suite hash cases (`suite.jsonl`, `suite-summary.txt`, 518a6b99 vs 7ad5a152, 5 reps of 50000, ns
per probe of 1000 operations): hash update 35274 -> 32480 (-7.9%), lookup 18435 -> 17928
(-2.8%), delete 35449 -> 35955 (+1.4%); per-CPU hash within 2.2%; `read`/`write` bimodal.

Key-pattern matrix (`keys/summary.md`, 204 runs, all exact; lookup + delete of the oldest key +
insert of a new one through BPF programs): one thread 0.97-1.04x of 518a6b99, up to +11 ns at
90% load (102400 / rand32 0.281 -> 0.292 us), one cell 1.14x (10240 / 50% / shl12 0.139 ->
0.158 us); six threads at the probe path's level as before.
