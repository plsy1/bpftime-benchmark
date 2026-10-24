# Kernel panic on 2026-10-24 ~05:50 JST (stock 6.8.12-1021-tegra)

The Jetson rebooted at 05:54 after a kernel panic; logs saved by ramoops/systemd-pstore
(`/var/lib/systemd/pstore`), copied here.

- Uptime at the crash 921,690 s (boot 2026-10-13 13:42 -> 2026-10-24 ~05:50).
- `Unable to handle kernel NULL pointer dereference at virtual address 00000000000000a5`,
  `Internal error: Oops` in `percpu_ref_get_many` <- `refill_obj_stock` <- `obj_cgroup_uncharge` <-
  `__memcg_slab_free_hook` <- `kmem_cache_free` <- `radix_tree_node_rcu_free` <- RCU softirq, on an
  idle CPU (swapper/1). Then `Kernel panic - not syncing: Oops: Fatal exception in interrupt`.
- The memcg object-cgroup pointer read during a slab free was 0xa5 (a slab poison byte): slab
  metadata corruption or a use-after-free in kernel memcg accounting, not a user-space action.
- Machine was idle: the 03:34 daily job exited at once (official SHA unchanged); last log line
  before the Oops is a loop-device capacity change 30 s earlier. A Docker container
  (ssl-nginx-2x2) had been running since 2026-10-23; containers create and destroy memory cgroups.
- Earlier in the same boot: two `nvme nvme0: I/O tag ... timeout, completion polled` warnings
  (2026-10-23 ~14:12 and ~14:21).
- Effects: `jetson_clocks` was not re-applied after the reboot (see summry handoff item 12).

If it happens again, compare the call trace; repeated memcg/slab traces would point to a kernel
bug to report to NVIDIA (L4T R39.2.1), random traces to hardware (memory) problems.
