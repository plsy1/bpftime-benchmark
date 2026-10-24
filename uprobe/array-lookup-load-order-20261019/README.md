# Ordinary array lookup load-order candidate

Baseline: `fefe2f6edf276e1b2957711203b5022dd1be8d7c`.
Frozen baseline: `/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors`.
Frozen candidate root: `/tmp/bpftime-array-load-order-20261019.Qbnxir`.

Implementation-only experiment: materialize `data.data()` and `_value_size`
before the original bounds check; calculate the indexed pointer only after
the check, avoiding out-of-range pointer arithmetic for invalid keys. Continue
using the array implementation's own `_max_entries`. No fields, layout,
checks, error behavior, helper dispatch or CPU semantics change. No runtime
assumptions, compiler barriers, raw-pointer caches or alignment assertions.

Considered but not implemented: using the handler's duplicate capacity to
shorten the dependent load chain. Its constructors/standard duplication path
agree with the implementation, but `share_map_impl_from` itself does not
enforce matching metadata; this experiment does not introduce that assumption.

First inspect actual production helper assembly. If the normalized helper
instructions are unchanged, stop without spending time on an A/B. Otherwise
run array/hash correctness tests and freeze artifacts, then six alternating
short array A/B rounds under tmux. Gate: >=2% target median improvement,
>=5/6 paired wins, no other array median >3% slower. Passing candidates get a
hash regression screen and manual review before full ten-round validation.
No full run or acceptance follows from static instruction counts alone.

Regression tests include capacities 1/7/65/1024, byte widths 1/3/8/24/257,
first/middle/last elements, exact byte strides, invalid indices including
UINT32_MAX, unchanged success errno and ENOENT on failure.

## Initial assembly inspection

The actual production helper changed: it remains 47 static instructions with
no stack references or calls on the successful path. Loads of the vector
offset, capacity and element width now precede the key/capacity comparison;
the final indexed pointer is still formed only after that check succeeds.
Manager/fd/type checks and generic fallback remain present. This is a genuine
scheduling change, not an instruction-count reduction, and is not yet evidence
of a performance improvement. See `assembly.diff` and both helper disassemblies.

Background session: `uprobe-array-load-order-20261019`. The pipeline builds and
runs correctness tests before freezing the candidate and starting balanced
six-round array A/B. Failure of any stage stops the pipeline; short-screen
success is not automatic acceptance or permission to update the ten-item table.

## Decision

Six-round array screening completed; correctness passed 106,942 assertions in
18 cases. Target control-subtracted medians: 6.48735104 -> 6.41466336 ns/helper
(-1.12045%), six paired wins. Other array median changes were +0.126%, -0.222%,
and +0.705%. The 2% minimum target gate stopped further work (pipeline exit 1,
not a correctness failure). No hash screen or full ten-round run was started.
This is an unconfirmed small gain, not an accepted optimization. Following
user review, the leaf and test edits were removed before the independent
offset-pointer-branch candidate. Its archived source and frozen binaries remain
available; no source commit or daily replay-list change was made.
