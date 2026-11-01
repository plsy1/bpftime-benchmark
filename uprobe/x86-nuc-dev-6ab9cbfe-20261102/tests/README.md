# dev 6ab9cbfe 在 x86 上的构建与测试（2026-11-02 早，NUC）

只做正确性，不计时，频率没有固定。开发分支没有改动。

## 构建（`../build/`）

- 源码：`6ab9cbfe`（"ringbuf: submit back to one exchange; consumed slots free a lap later"），从 `1aa74c4c` 快进 60 个提交，快进前没有本地改动（`../build/source-state.txt`），`vm/llvm-jit` 干净的 `87bac39a`。
- 选项同 10/26（gcc-13、LLVM/clang 19.1.7、LTO、RelWithDebInfo，`run-build.sh`）。和 10/26 的 `cmake-options.txt` 相比只多了新选项 `BPFTIME_ENABLE_MAP_USAGE_STATS=OFF`（默认值）。
- 第一次构建因为脚本里写错了目标名（`bpftime-agent-transformer`，实际叫 `bpftime_text_segment_transformer`）在配置后立即退出，日志在 `../build/attempt1-wrong-target/`；改正后复用同一个只配置过的目录，**构建通过**（退出码 0）。
- 警告只有老的几类：`ebpf_inst` ODR（4）、`optnone` 被忽略（3）、多行注释（2）、构造函数优先级（1）。

## 测试（`run-tests.sh`，每项各有超时）

| 测试 | 结果 |
|---|---|
| 每日过滤器（`test-filter.txt`，与 10/26 相同） | 135 个用例：134 通过，1 跳过（需要 root，见下） |
| runtime 单测，每个用例单独一个进程（`runtime-each/`） | 228 个用例：221 通过，1 失败，6 跳过；失败和其中 3 个跳过都需要 root，用 sudo 补跑全部通过（`tests-runtime-root.log`） |
| 其中 map、perf、ringbuf 相关（名字含 map/hash/array/perf/ringbuf 等） | 156 个：151 通过，其余 5 个就是上面需要 root 或没开统计选项的用例；ringbuf 5/5，perf 16/16 |
| Frida 挂钩测试，逐用例（`frida-each/`） | 19/19 |
| syscall trace attach | 普通用户 6/9（3 个读不了 `/sys/kernel/tracing/events/syscalls`）；**sudo 下 9/9**（`tests-syscall-trace-root.log`） |
| x86 syscall 改写（`bpftime_text_segment_transformer_tests`，root，`vm.mmap_min_addr` 65536 → 0 → 65536） | **连跑 3 次都是 5/5**（13 个断言，`tests-transformer-{1,2,3}.log`，`mmap_min_addr.txt`） |

没有发现 6ab9cbfe 在 x86 上的测试失败。细节：

- runtime 里的 1 个失败是 `Test tail calling from userspace to kernel`，要建内核 map，普通用户 `map_fd = -1`，10/26 起就是这样；sudo 下通过。
- 6 个跳过（Catch2 退出码 4）：
  - 需要 root：`Producers in two pid namespaces with the same pid write apart`、`Kernel-user percpu array gives a program its CPU's value`、`Kernel-user hash lookups are private to the calling thread`，sudo 下都通过；
  - 没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`：`Map usage counters …` 两个和 `Map usage dump does not deadlock on nested operations`，按基线选项没有构建这个功能，这次没有测。
- 10/26 在 a7d54d7e 上逐用例 SIGSEGV 的 `Test attach replace with ebpf` 这次通过。
- 测试结束后 `/dev/shm` 里留下 3 个测试用的共享内存文件（`bpftime_hash_map_test`、`BPFTIME_TEST_ENTERNAL_MAP_SHM`、`BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT`，都是这次测试 05:15–05:16 创建的），测量前已删除。这是逐用例运行时测试没有清理，不影响结果。
