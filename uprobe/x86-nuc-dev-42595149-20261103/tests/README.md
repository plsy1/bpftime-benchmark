# dev 42595149 在 x86 上的测试

不计时，不钉频。脚本和过滤器沿用 a4ac0d31 那次（`../../x86-nuc-dev-a4ac0d31-20261102/tests/`），只改了输出目录；`run-tests.sh` 里 runtime 和 Frida 逐用例改成每个用例限时 60 秒（Jetson 的要求）。

| 项目 | 42595149 | a4ac0d31 | 6ab9cbfe |
|---|---|---|---|
| 每日过滤器（`tests-filter.log`） | 140 个：139 过，1 跳过（要 root） | 卡住，只跑了 7 个 | 135 个：134 过，1 跳过 |
| runtime 逐用例，60 秒限时（`runtime-each/`，`tests-runtime-each.log`） | **234 个：227 过，7 失败** | 234 个：165 过，69 失败 | 228 个：221 过，7 失败 |
| 需要 root 的 4 个，sudo（`tests-runtime-root.log`） | 4/4 | 4/4 | 4/4 |
| Frida 逐用例（`frida-each/`） | 19/19 | 19/19 | 19/19 |
| syscall trace attach | 6/9；sudo 9/9（`tests-syscall-trace-root.log`） | 6/9；sudo 9/9 | 6/9；sudo 9/9 |
| x86 syscall 改写（root，`vm.mmap_min_addr` 65536 → 0 → 65536） | 连跑 3 次 5/5（13 个断言） | 5/5 ×3 | 5/5 ×3 |

- 7 个失败和 6ab9cbfe 完全相同：
  - `Test tail calling from userspace to kernel`（rc=1，普通用户建不了内核 map）和 3 个跳过（rc=4）：pid 命名空间生产者、2 个 kernel-user 用例。都要 root，sudo 下全过。
  - 3 个 map 使用统计用例跳过（rc=4，构建时没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`）。
- `tests-runtime-each.log` 和 `tests-frida-each.log` 里的标签写的是 `dev-a4ac0d31`：这是从上次复制的 `run-each-case.py` 里写死的名字，跑完才发现并改掉。二进制确实是 `build-42595149` 的（见 `run-tests.sh` 和 `../build/source-state.txt`）。

## 指定标签（`tests-tags.log`，每个标签一个进程，限时 300 秒）

| 标签 | 结果 |
|---|---|
| `[spin-lock]`（a4ac0d31 上卡死的"不修就失败"测试） | 1 个用例，11 个断言，通过 |
| `[bpftime_hash_map]` | 16 个用例，10,122,526 个断言，通过 |
| `[percpu-hash]` | 15 个用例，5,471 个断言，通过 |
| `[value-access]` | 2 个用例，35 个断言，通过 |
| `[percpu-locks]` | 3 个用例，33 个断言，通过 |
| `[home-stripe]` | 1 个用例，53 个断言，通过 |
| `[bpftime_hash_map][capacity]` | 1 个用例，49,244 个断言，通过 |

## 反复跑（`run-repeat.sh`，`repeat/summary.txt`）

每次一个进程，限时 300 秒，只保留失败那次的日志（没有失败，所以 `repeat/` 里只有汇总、最后一次的日志和 stress-ng 的日志）。01:32–01:34，退出码 0。

| 内容 | 次数 | 通过 |
|---|---:|---:|
| `[percpu-locks]` | 50 | 50 |
| `[value-access]` | 20 | 20 |
| `[home-stripe]` | 20 | 20 |
| `[spin-lock]` | 20 | 20 |
| `[bpftime_hash_map][capacity]` | 20 | 20 |
| `[percpu-locks]`，所有 CPU 被 `stress-ng --cpu 16`（nice 19）占满 | 20 | 20 |
