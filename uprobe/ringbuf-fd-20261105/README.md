# Problem 9 fixed: ringbuf helpers find the map again, dev 6a15a271 (2026-11-05)

Problem 9 of `uprobe/harness-gaps-20261104`: since 5920d5f9 (10-31) the record header's second
word held the reservation's owner slot, while `bpf_ringbuf_submit` and `bpf_ringbuf_discard` read
it as the map fd. dev 6a15a271 (on f0954773, local) makes the word a tag: the fd above the low 9
bits, the owner slot plus one in them (0: none); the helpers decode the fd
(`bpftime_ringbuf_record_fd`), submit and the reclaim the slot. Frozen build:
`uprobe/ssl-nginx-2x2-20261023/builds/dev-6a15a271` (with its CLI and transformer).

Test "Ringbuf helpers submit and discard through the sample's map" (`[ringbuf]`): two ringbufs
after another map, reserve twice, submit one and discard the other through the helpers, the fd
decoded from each sample. Passes; with the helpers reading the raw word (the old behaviour) both
records stay busy (`0x80000000`). Gate (`gate.log`): runtime 171 passed, 1 skipped (one new);
Frida 22/22; text segment transformer 49/50 (1 fails as expected); real-tool smoke funclatency
7,756,000 of 7,756,000, memleak rc 0, detach rc 0.

get_stack_id example, 3 rounds (`run.sh dev-6a15a271`, `stackid/`; victim mallocs counted from its
pid, sh's before the exec included):

| backend | reported | with a stack | fd errors | discarded records | workload |
|---|---|---|---|---|---|
| bpf | 64 / 64 / 64 | 0 (`-EEXIST`: the map fills system-wide) | - | - | rc 124 3/3 |
| official | 1,304 / 1,308 / 1,304 | all, depth 2 | 0 | 0 | SIGSEGV 3/3 |
| dev-25f4134f | 0 / 0 / 70 | 0 / 0 / 70 | 132 / 132 / 36 | 208 / 203 / 34 | SIGSEGV 1/3 |
| dev-6a15a271 | 70 / 70 / 70 | 67 / 69 / 69, depth 7 | 0 | 0 | rc 124 3/3 |

- official counts the agent's own allocations too (the agent's work is visible to its probes
  there; dev keeps it out since 4d9b8a61), hence ~1,300.
- 1-3 of 70 without a stack on dev-6a15a271: two of the ~70 distinct stacks share a bucket of the
  example's 1000 and the second gets `-EEXIST`, as the kernel does (expected ~2.4).
- Problem 10 (logging at exit) did not show: with no fd error nothing logs.
- Others once (`other/`): gethostlatency 350,200 of 350,200; opensnoop still 0 (problem 11).

Not measured: the ringbuf output cost (the reserve stores the tag with one shift and or; the helpers
decode it with one shift, where they read the word before).
