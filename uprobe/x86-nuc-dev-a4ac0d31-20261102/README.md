# x86 NUC：dev a4ac0d31 的编译和测试（2026-11-02 晚 – 11-03 凌晨）

Jetson 会话转达的用户请求：在 x86 上编译并测试开发分支 `dev/uprobe-official-177a1dc465ff` 的头 `a4ac0d31`（覆盖 `0ffe368b..a4ac0d31` 共 9 个提交），重点是 `[bpftime_hash_map]`、`[percpu-hash]` 及新加的 `[value-access]`、`[percpu-locks]`。按 NUC 的规矩只记录、不修开发分支。

- 机器：`debian-ichi-nuc`，i5-1240P，Debian 13，内核 `6.12.113+deb13-amd64`，glibc 2.41。
- 时间：21:38 构建，21:39–00:10 第一轮测试，00:19–01:20 用 60 秒限时重跑全部 runtime 用例，之后用 sudo 补跑需要 root 的用例。

## 0. 结论

- **构建通过**（`build/`）：配置和 6ab9cbfe 那次完全相同，源码树干净，警告还是那 10 条。
- **x86 上所有走 bpftime 自旋锁的路径都会卡死**，这是 `0ffe368b` 引入的，6ab9cbfe 和官方 177a1dc 都没有这个问题（第 2 节）。
  - runtime 单测 234 个用例，60 秒限时：165 通过、69 失败。其中 7 个是 6ab9cbfe 上就有的（要 root 或没开统计选项），其余 62 个全部是卡在锁上：60 个超时，2 个等 helper 就绪等了 30 秒失败（helper 里的 syscall server 卡在锁上）。
  - 请求的标签 `[bpftime_hash_map]`、`[percpu-hash]`、`[value-access]`、`[percpu-locks]`、`[home-stripe]`、`[spin-lock]` 全部超时。
  - 不经过这把锁的部分正常：Frida 19/19，x86 syscall 改写连跑 3 次 5/5，syscall trace 9/9（sudo），需要 root 的 4 个用例 sudo 下全过。
- **原因**：x86_64 glibc 的 `pthread_spin_lock` 用 1 表示"空闲"，aarch64 和通用实现用 0。`0ffe368b` 的内联锁按"0 = 空闲"写死，而锁是用 `pthread_spin_init` 初始化的（字值 1），于是第一次加锁就永远等下去。
- **已交给 Jetson**：Jetson 确认了原因，修复是 `49bc1fd9`（只在 `__GLIBC__ && __aarch64__` 下用内联锁，其他平台调回 `pthread_spin_lock/unlock`），上面还有 `42595149`。两个提交推送后我在 x86 上复测。
- **没有跑的**：`tests/run-repeat.sh`（新标签反复跑，看时序用例是否稳定）和 `uprobe-sanity/`（官方 uprobe 两轮 map 用例对照）。所有 map 操作都卡死，跑了没有意义，脚本留着等修复后用。

## 1. 构建（`build/`）

- `run-build.sh` 是 6ab9cbfe 那次的脚本，只改了输出目录，并加了"构建目录必须不存在"的检查。构建目录是 `/home/y/src/bpftime-wt-uprobe-177a1dc/build-a4ac0d31`。
- `source-state.txt`：`a4ac0d31`，`vm/llvm-jit` `87bac39a`，没有改动。`cmake-options.txt` 和上次一致。
- 21:38:24–21:39:44，退出码 0。警告 10 条，和 6ab9cbfe 一样：ODR 4、optnone 3、多行注释 2、构造函数优先级 1。

## 2. 自旋锁卡死（`spin-lock-protocol/`）

- `glibc-spinlock-disassembly.txt`：本机 glibc 2.41 的反汇编。`pthread_spin_init` 是 `pthread_spin_unlock` 的别名，都是 `movl $1,(%rdi)`；`pthread_spin_lock` 是 `lock decl`，减到 0 就算拿到锁。所以 1 = 空闲，0 或负数 = 被占。
- `repro.c` / `repro-output.txt`：不依赖 bpftime 的最小复现（`gcc -O2 -pthread`）。初始化后字值是 1，加锁后 0，解锁后 1；照 `0ffe368b` 写法的内联加锁拿不到一把空闲的锁；内联解锁写 0 之后 `pthread_spin_trylock` 返回 EBUSY。两个方向都不兼容。
- `gdb-enospc-backtrace.txt`：第一轮测试里卡住的 `bpf_perf_event_output returns -ENOSPC when the ring is full`。单线程，栈是 `bpftime_spin_lock`（map_common_def.hpp:43）← `bpftime_spin_lock_outlined` ← `bpftime_lock_guard` ← `map_update_elem`（perf_event_array），锁的字值是 1。
- `leftover-helpers-backtraces.txt`：60 秒限时只杀掉了测试主进程，留下 15 个 helper 和子进程，每个占约 98% CPU，最久的转了一个小时。杀掉前对 8 种各取一个栈，全部在 `bpftime_spin_lock`：
  - syscall server 自己的 context 锁（`syscall_context::handle_close/handle_open/handle_fopen`，`syscall_server_main.cpp:455/491/514`，字值 1），所以不只是 map 的锁；
  - `bpftime_hash_map::update_in_home_stripe`（fork 前后启动的两个 helper，以及"多进程共享 striped hash map"用例的子进程）。
- `other-timeouts-backtraces.txt`：另取 4 个不属于 hash 类的超时用例，重跑 5 秒后取栈。`get_stackid`、数组 JIT 混合 map、`-ENOSPC` 三个都在 `bpftime_spin_lock`；file logger 用例的 helper 停在 `read()` 等它的 syscall server，没有再往下追。
- 这些进程都已杀掉，测后没有残留。

## 3. 测试

见 `tests/README.md`。

## 4. 清理

- `/dev/shm`：测前的列表在 `tests/dev-shm-before.txt`。测试新留下的 82 个文件（共 3.6 GB，都属于用户 y，没有 root 的）已删，列表在 `tests/dev-shm-removed.txt`；现在只剩测前就有的 5 个。
- 没有起容器。`vm.mmap_min_addr` 测完恢复为 65536（`tests/mmap_min_addr.txt`）。
