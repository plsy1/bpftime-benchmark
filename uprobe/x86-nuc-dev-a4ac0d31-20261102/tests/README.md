# dev a4ac0d31 在 x86 上的测试

不计时，不钉频。脚本和过滤器沿用 6ab9cbfe 那次（`../../x86-nuc-dev-6ab9cbfe-20261102/tests/`），只改了输出目录；`run-each-case.py` 多了第三个参数，用来设每个用例的超时（默认 600 秒）。

| 项目 | a4ac0d31 | 6ab9cbfe（11/02 早） |
|---|---|---|
| 每日过滤器（`test-filter.txt`，`tests-filter.log`） | 退出码 124：总超时 1800 秒前只跑了 7 个用例，6 过，第 7 个 `Perf output preserves the calling thread's CPU affinity` 卡住被杀 | 135 个：134 过，1 个要 root |
| runtime 逐用例，600 秒限时（`runtime-each/`） | 退出码 124：总超时 7200 秒内只跑到第 118 个左右，11 个各卡满 600 秒，没有 `results.json` | 228 个：221 过 |
| **runtime 逐用例，60 秒限时**（`runtime-each-60s/`，`tests-runtime-each-60s.log`） | **234 个：165 过，69 失败**（下面分类） | — |
| 需要 root 的 4 个，sudo，60 秒限时（`tests-runtime-root.log`） | 4/4 | 4/4 |
| Frida 逐用例（`frida-each/`） | 19/19 | 19/19 |
| syscall trace attach | 普通用户 6/9（3 个读不了 `/sys/kernel/tracing`，同上次）；sudo 9/9（`tests-syscall-trace-root.log`） | 6/9，sudo 9/9 |
| x86 syscall 改写（root，`vm.mmap_min_addr` 65536 → 0 → 65536） | 连跑 3 次 5/5（13 个断言） | 5/5 ×3 |

## 60 秒限时那一轮的 69 个失败

用例编号是 `runtime-each-60s/results.json` 里的顺序（从 1 起）；日志文件 `case-NNN.log` 的编号从 0 起，比它小 1。

- **7 个和 6ab9cbfe 相同**：
  - `Test tail calling from userspace to kernel`（rc=1，建不了内核 map）和 3 个跳过（rc=4）：pid 命名空间生产者、2 个 kernel-user 用例。都要 root，sudo 下全过。
  - 3 个 map 使用统计用例跳过（rc=4，构建时没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`）。
- **60 个超时**，全部和锁有关：
  - 所有 hash、per-cpu hash、fixed hash、striped hash 用例，包括新加的 `[spin-lock]`、`[value-access]`（2 个）、`[percpu-locks]`（3 个）、`[home-stripe]`（1 个）；
  - perf：CPU 亲和性、`-ENOSPC`；`get_stackid`；
  - syscall server：perf mmap 耗尽、file logger、自己创建的和只打开的共享内存、mock 临时文件、启动前后 fork、map 分配失败；
  - `test_hashmap_percpu (kernel)`、`test_map_stress (kernel)`；
  - 用到 hash map 的数组 JIT 用例（222、223、227、228、229、231、232）。
- **2 个 rc=1**：`Syscall server keeps shared memory while an agent pid is alive` 和 `...while a peer server pid is alive`，都在 `test_shm_allocation_failure.cpp:228` 的 `REQUIRE(wait_for_file(ready_file))` 等满 30 秒失败。原因是 helper 里的 syscall server 卡在锁上，一直没写就绪文件。
- 卡住的进程的栈见 `../spin-lock-protocol/`。

## 准备了但没跑

- `run-repeat.sh`：`[percpu-locks]` 跑 50 次，`[value-access]`、`[home-stripe]`、`[spin-lock]` 各 20 次，再在全部 CPU 被 `stress-ng`（nice 19）占满的情况下跑 `[percpu-locks]` 20 次，只保留失败那次的日志。目的是看 Jetson 提到的 3 个时序用例在 x86 上稳不稳。现在每次都会卡死，等修复后再跑。
