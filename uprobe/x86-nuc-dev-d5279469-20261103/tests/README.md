# dev d5279469 在 x86 上的测试

不计时，不钉频。`run-tests.sh`、`run-each-case.py` 和过滤器沿用 42595149 那次（`../../x86-nuc-dev-42595149-20261103/tests/`），只改了输出目录和标签；runtime 和 Frida 逐用例仍是每个用例限时 60 秒。

| 项目 | d5279469 | 42595149 |
|---|---|---|
| 请求的标签 `[bpftime_hash_map],[maps],[map-create],[percpu-hash],[hash-seed]`，一个进程（`tests-requested-tags.log`） | 61 个用例：59 过，2 跳过（kernel-user 两个，要 root）；11,481,114 个断言 | — |
| 每日过滤器（`tests-filter.log`） | 150 个：149 过，1 跳过（要 root） | 140 个：139 过，1 跳过 |
| runtime 逐用例，60 秒限时（`runtime-each/`，`tests-runtime-each.log`） | **245 个：238 过，7 失败** | 234 个：227 过，7 失败 |
| 需要 root 的 4 个，sudo（`tests-runtime-root.log`） | 4/4 | 4/4 |
| Frida 逐用例（`frida-each/`） | 19/19 | 19/19 |
| syscall trace attach | 6/9（3 个读不了 `/sys/kernel/tracing`）；sudo 9/9（`tests-syscall-trace-root.log`） | 同 |
| x86 syscall 改写（root，`vm.mmap_min_addr` 65536 → 0 → 65536，`mmap_min_addr.txt`） | 连跑 3 次 5/5（13 个断言） | 同 |

- 7 个失败和 42595149、6ab9cbfe 完全相同：`Test tail calling from userspace to kernel`（rc=1，普通用户建不了内核 map）和 3 个要 root 的跳过（rc=4），sudo 下全过；3 个 map 使用统计用例跳过（rc=4，构建时没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`）。**没有新的失败。**
- 新增的 11 个用例都过了，其中 4 个并发压力测试（每个 1.5 秒）：
  - `hash map lookups never miss keys present while their chains churn`
  - `hash map copies of a value being updated are whole`
  - `hash map copies of keys updated behind the head are whole`（漏查只计数，只有读到不完整的副本才算失败）
  - `hash map lookups never return a value reused for another key`
  - 以及 `Hash maps without a seed on BPF_F_ZERO_SEED or BPFTIME_HASH_ZERO_SEED`。

## 并发压力测试反复跑（`run-repeat.sh`，`repeat/`）

新的 hash map 依赖内存序，之前只在 aarch64 上跑过，所以虽然一次都没失败，还是在 x86 上把这 4 个各跑 20 次，再在所有 CPU 被 `stress-ng --cpu 16`（nice 19）占满时把前 3 个各跑 20 次。每次一个进程，限时 120 秒，用 `-s` 保留测试打印的计数（`repeat/<组>/run-<n>.log`，汇总在 `repeat/counts.md`，由 `repeat-counts.py` 生成）。

| 组 | 次数 | 通过 | 计数（20 次的最小–最大） |
|---|---:|---:|---|
| churn（链在变时查已有的键不漏） | 20 | 20 | 每次 4,800 万–6,400 万次查找 |
| value（并发更新时读到的副本完整） | 20 | 20 | 1.9 亿–4.3 亿份副本，不完整的副本 0；指针读到混合值 9–16,876 次 |
| behind（头后面的键被更新时副本完整） | 20 | 20 | 1.3 亿–3.4 亿次读，漏查 17–67,217 次，不完整 0 |
| reuse（不会返回被别的键复用的值） | 20 | 20 | — |
| churn，满载 | 20 | 20 | 3,900 万–5,600 万次查找 |
| value，满载 | 20 | 20 | 不完整的副本 0；指针读到混合值 643–40,547 次 |
| behind，满载 | 20 | 20 | 漏查 77–111,595 次，不完整 0 |

- "指针读到混合值"和"漏查"是测试设计里允许、只计数的情况：
  - 指针读：读者拿着旧元素的指针时，这个旧元素被下一次更新复用了。内核预分配 htab 的 `extra_elems` 也有同样的窗口。
  - 漏查：更新时新元素先挂到链头，再摘掉旧元素；站在旧元素前驱上的查找可能走到链尾而漏掉，内核也是这样（CLAUDE.md 的 bpftime_hash_map 一节）。
- x86 上 behind 的漏查每次 17–111,595 次（每次读的 0.000005%–0.13%）。CLAUDE.md 引用的复查结果是同一种情况在 6,100 万次查找中漏 4–8 次，x86 上多得多，而且 CPU 满载时更多。测试不对漏查设上限，这里只记录，交给 Jetson 判断。
