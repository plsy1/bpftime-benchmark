# Review of the fixes after the e5ba4aa7 review (dev 3cb3b630..7670cee5), 2026-11-03 21:00-22:20

One Fable 5.1 subagent, read-only (code, history, kernel 6.8 `hashtab.c`); each finding checked
by the main session, R1 and R2 with tests. Fixes: dev 34521e66, d5279469.

| # | Finding | Verdict | Done |
|---|---|---|---|
| R1 | An update of a key behind the head links the new element at the head, then unlinks the old one: a lookup on the old element's predecessor reaches the end and misses a present key. | **Real, reproduced**: 4, 8 and 9 misses in 61-64 million lookups of two keys of one bucket updated at once (1.5 s each). Same as the kernel (`hlist_nulls_add_head_rcu`, then `hlist_nulls_del_rcu`). The in-place table never missed. | Kept as the kernel (user's choice); the header, `replace_locked`, CLAUDE.md and the churn test claimed "old or new" and are corrected. In-place splice (new element in the old one's place, acquire `next` loads) not taken. |
| R2 | Nothing tests the retirement of an old element behind the head. | **Real**: dropping it passed every test, also a two-key stress test (with and without a stall in the reader). | Deterministic test "hash map lookups never return a value reused for another key" (mutation: no retire -> the lookup accepts the reused element); stress test "hash map copies of keys updated behind the head are whole" ({key, x, x} values, misses counted). |
| R3 | Docs claiming no miss; `next_key` with updates while iterating can cycle between two keys of a bucket. | Real (kernel-same). | Documented. |
| R4 | Seed test "two seeded maps differ" can fail by chance (~1/4096 per comparison); per-CPU buffer of 256 values. | Real. | Three maps, not all equal; buffer from the CPU count. |
| R5 | XOR seed weaker than jhash + hashrnd (32 effective bits for 4 B keys). | Real, enough against the `j * phi^-1` construction. | Noted in `key_hash`. |
| R6 | The per-CPU hash allocates spares it never uses. | Real, harmless (at most 4096 small elements). | Not changed. |

Checked and found correct by the review: the spare's retirement -> reuse ordering (lock + release
fence), generations, head vs non-head replacement apart from R1, readers on the old element, value
pointers into a reused spare (kernel's window), delete/next_key/with_existing_value/lookup_read
interaction, capacity and E2BIG, a process killed mid-update, index bounds, 199e23ec's order and
test hook, the seed's placement (in the map object, in the segment) and the zero-seed switch.

Found by the gate on 34521e66 (not by the review): "Fixed hash lookup keeps a full-capacity chain
and misses past it" has failed intermittently since the seed (48767d88): one-byte keys repeat every
256 ids, and with a seed fewer than 8 distinct ones fall into its fixed bucket about 1 time in 100
(old test 7 of 300 runs failed, fixed 0 of 300). Another test found "the last bucket" in a separate
table, whose seed differs; it now uses the map's own table. Both in d5279469. Gates:
`gate-34521e66.log` (that failure), `gate-d5279469.log` (green).
