# R8 tests-examples（单测 / 示例 / benchmark 地图）源码笔记

> 基线 `177a1dc465ff`，仓库根 `/home/y/src/bpftime-wt-official-177a1dc`（下文路径均相对仓库根）。
> 本机：Debian 13 x86_64（NUC），16 核，现成构建目录 `build-jit/`（Ninja，`RelWithDebInfo`，
> `BPFTIME_LLVM_JIT=ON`，`BPFTIME_ENABLE_UNIT_TESTING=ON`，`BUILD_BPFTIME_DAEMON=OFF`，
> **`ENABLE_PROBE_READ_CHECK=OFF` / `ENABLE_PROBE_WRITE_CHECK=OFF`**，`BPFTIME_ENABLE_FRIDA=ON`，
> `BPFTIME_ENABLE_TRAP_UPROBE=OFF`，见 `build-jit/CMakeCache.txt:58,74,614,617`）。
> 本块是"地图"性质：第 2、3 节写得短，**三张总表与"活文档 Top 10"在第 9 节（9.1–9.4）**。

## 0. 一句话职责 + 范围

本块负责回答"仓库里有哪些可执行的学习材料、各自验证了什么、怎么跑、文档是否还对得上代码"。

读了的目录：

- `runtime/unit-test/`（全部 47 个文件，含 `assets/`）——Catch2 单测，编成一个二进制 `bpftime_runtime_tests`。
- `runtime/test/`（全部 19 个文件）——**已废弃**的旧测试目录（`runtime/test/README.md:3`），CMake 里测试源列表全部注释掉，什么都不编。
- `example/` 除 `gpu/`、`attach_implementation/` 外的每个子目录（只列出这两个）。
- `benchmark/` 除 `gpu/` 外的每个子目录（`gpu/` 只列出）。
- 为核对 README，附带查看了 `Makefile`（根）、`tools/cli/main.cpp` 的参数定义、`.github/workflows/test-runtime.yml` 的单测调用、若干 runtime 源码点（均在正文给出位置）。

行数：`runtime/unit-test` + `runtime/test` 共 10,378 行；`example/`（不含 gpu、attach_implementation、二进制、`sslsniff/.output` 构建产物）+ `benchmark/`（不含 gpu、二进制）文本文件共 31,683 行（其中 `hotpatch/redis/redis-server.h` 2746 行、`redis-durability-tuning/redis.conf` 2299 行是数据性文件）。

实跑概况（详见 9.1）：`bpftime_runtime_tests` 共 106 个 TEST_CASE；逐个独立运行 **104 个通过、2 个失败**：
`Test attach replace with ebpf`（默认顺序首跑时 SIGSEGV，**测试顺序依赖**，见 7.1）、
`Test tail calling from userspace to kernel`（需要内核 BPF 权限，非 root 下 `bpf_map_create` 失败）。
排除这两个后整体跑一次：`All tests passed (8380811 assertions in 104 test cases)`。

## 1. 文件清单

### 1.1 runtime/unit-test（全部逐个读过）

| 文件 | 行数 | 做什么（一句话） | 学习者是否必读 |
|---|---|---|---|
| `runtime/unit-test/CMakeLists.txt` | 158 | 定义 `bpftime_runtime_tests`、两个 helper 程序、Frida/CUDA/trap 条件源、现编 6 个 `.bpf.o` 并注入 `EBPF_PROGRAM_PATH_*` 宏 | 必读 |
| `runtime/unit-test/common_def.hpp` | 24 | 全局 `::shm_remove`（构造/析构都删同名 shm）与 `TOSTRING` 宏 | 必读（与 `bpftime::shm_remove` 的区别是坑，见 7.2） |
| `runtime/unit-test/test_config.cpp` | 60 | `runtime_config` 日志路径 setter；`BPFTIME_ALLOW_EXTERNAL_MAPS`、`BPFTIME_FRIDA_FUZZY_BACKTRACER` 环境变量解析 | 选读 |
| `runtime/unit-test/test_bpftime_shm_json.cpp` | 107 | hex 编解码；把 prog/tracepoint/link/map 写进 `bpftime_shm` 后导出 JSON 再导入 | 必读 |
| `runtime/unit-test/test_get_current_pid_tgid.cpp` | 86 | `bpf_get_current_pid_tgid` 缓存在 fork 后（含孙进程）刷新 | 选读 |
| `runtime/unit-test/test_get_current_uid_gid.cpp` | 67 | 用 `-Wl,--wrap=getuid/getgid` 注入凭据，验证 `bpf_get_current_uid_gid` 的打包与不缓存 | 选读（链接期 mock 技巧值得看） |
| `runtime/unit-test/test_get_stack.cpp` | 146 | 用 mock attach impl 覆盖弱符号 `get_global_attach_ctx`，测 `bpftime_get_stack` 的字节语义/skip/截断 | 推荐 |
| `runtime/unit-test/test_probe.cpp` | 254 | `bpftime_probe_read`/`probe_write_user`/`bpf_probe_read_str` 正常与越界；不篡改应用 SIGSEGV handler | 必读 |
| `runtime/unit-test/test_epoll.cpp` | 161 | `bpftime_epoll_wait` 对 ringbuf/perf 源"就绪只报一次、不消费数据、恢复信号掩码"，GENERATE 40 组组合 | 推荐 |
| `runtime/unit-test/test_ringbuf_tail_wrap.cpp` | 92 | ringbuf 记录头落在环最后 8 字节时，记录仍连续可消费 | 推荐 |
| `runtime/unit-test/test_software_perf_event.cpp` | 344 | perf 输出不改调用线程 CPU 亲和性；多线程分片；8 字节对齐回绕；`E2BIG` 上限；shm 耗尽 `ENOMEM` | 必读 |
| `runtime/unit-test/test_shm_allocation_failure.cpp` | 512 | fork+exec helper，`LD_PRELOAD` syscall-server/agent，验证 shm 太小时回落、不污染宿主 stdio、shm 所有权与清理 | 必读（"宿主进程契约"的活文档） |
| `runtime/unit-test/assets/shm_allocation_test_helper.c` | 233 | 上面测试的被测宿主程序：6 种模式（startup/startup-ok/startup-wait/mock-tmp/perf-mmap/check-sigusr1） | 必读（配合上一行） |
| `runtime/unit-test/assets/runtime_options_test_helper.c` | 22 | 直接 `bpf(BPF_PROG_LOAD)` 一条非法程序名 `bypass_me`；**单测二进制不用它**，只被 CI 脚本用（`.github/workflows/test-runtime.yml:240`） | 选读 |
| `runtime/unit-test/assets/filter.bpf.c` | 31 | uprobe 过滤程序：`str==NULL` 时 `bpf_set_retval(-22)` 并返回 `OP_SKIP`；调自定义 helper 40 | 推荐 |
| `runtime/unit-test/assets/helpers.bpf.c` | 23 | 最小程序：`bpf_printk` + `bpf_ktime_get_boot_ns`，返回 23 | 推荐 |
| `runtime/unit-test/assets/replace.bpf.c` | 33 | 用 ufunc 按名字调 `print_func`/`add_func`，返回值作为替换后的函数返回值 | 推荐 |
| `runtime/unit-test/assets/ufunc.bpf.c` | 40 | 用 ufunc 按 id（1、2）调宿主函数，期望返回 17 | 选读 |
| `runtime/unit-test/assets/ufunc.bpf.h` | 98 | ufunc 调用约定：helper 1000（dispatcher）/1001（按名查 id）+ `UFUNC_CALL_*` 宏 | 推荐 |
| `runtime/unit-test/assets/uprobe.bpf.c` | 39 | uprobe/uretprobe 程序，返回 `bpf_get_func_ip`；strdup 探针 | 推荐 |
| `runtime/unit-test/assets/uprobe_trap.bpf.c` | 34 | trap 后端用的 async-signal-safe 版本（不用 printk） | 选读（x86 上不编） |
| `runtime/unit-test/attach_with_ebpf/helper.hpp` | 15 | `create_bpftime_prog`：按名找程序→加 kernel helper 组→load | 推荐 |
| `runtime/unit-test/attach_with_ebpf/test_helpers.cpp` | 57 | 最小"打开 .o → 加 helper 组 → load → exec" | **必读（入门第一个）** |
| `runtime/unit-test/attach_with_ebpf/test_ufunc_register.cpp` | 102 | 按 id 注册 ufunc，解释器与 JIT 两种 load 都返回 17 | 推荐 |
| `runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf.cpp` | 115 | `frida_attach_impl` 在内部函数和 libc `strdup` 上挂 uprobe/uretprobe 并 detach | **必读** |
| `runtime/unit-test/attach_with_ebpf/test_attach_filter_with_ebpf.cpp` | 86 | `create_uprobe_override_at` 做"过滤"：按参数决定跳过原函数并改返回值；按 id 或按地址 detach | **必读** |
| `runtime/unit-test/attach_with_ebpf/test_attach_replace.cpp` | 122 | override + ufunc：整个函数被 eBPF 替换，返回 100；detach 后恢复 35 | 推荐（但有顺序依赖崩溃，见 7.1） |
| `runtime/unit-test/attach_with_ebpf/test_shm_progs_attach.cpp` | 270 | 手工在 shm 里造 handler_manager（prog/perf_event/link），`init_attach_ctx_from_handlers` 装配，fork 子进程再装一次 | **必读（agent 装配路径的缩影）** |
| `runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp` | 252 | trap（断点+SIGTRAP）后端：uprobe/uretprobe、未审计 helper 拒绝、未预编译拒绝 | 选读（整文件被 `#if BPFTIME_HAVE_TRAP_UPROBE_ATTACH` 包住，x86 上为空） |
| `runtime/unit-test/tailcall/test_user_to_user_tailcall.cpp` | 79 | 全局 shm 里建 PROG_ARRAY，helper 12 尾调用到另一个用户态程序；关闭目标后 lookup 为空 | **必读** |
| `runtime/unit-test/tailcall/test_user_to_kernel_tailcall.cpp` | 126 | 用户态程序 tail call 到内核程序（PROG_ARRAY 里放内核 prog fd） | 选读（需 root） |
| `runtime/unit-test/cuda/test_cuda_compile.cpp` | 98 | CUDA 构建下才编：PTX 编译冒烟、CUDA watcher 在 shm 析构前停止 | 跳过（非 CUDA） |
| `runtime/unit-test/maps/test_per_cpu_array.cpp` | 80 | per-CPU array：用 `ensure_on_certain_cpu` 在每个 CPU 上写，再从用户态一次读出所有 CPU 值 | 推荐 |
| `runtime/unit-test/maps/test_per_cpu_hash.cpp` | 99 | per-CPU hash 同上 | 选读 |
| `runtime/unit-test/maps/test_shm_hash_maps.cpp` | 231 | shm 里造 handler_manager + hash/array map，fork 子进程 `open_only` 读写；同名 map 互不干扰、引用计数共享 | **必读** |
| `runtime/unit-test/maps/test_external_map_ops.cpp` | 94 | `bpftime_register_map_ops` 注册外部 map 类型 2000（用 `std::map` 实现） | 推荐 |
| `runtime/unit-test/maps/test_bpftime_hash_map.cpp` | 188 | 底层 `bpftime_hash_map` 增删查/满/迭代 | 选读 |
| `runtime/unit-test/maps/test_fix_hash_map.cpp` | 53 | 桶数被调整为素数后，迭代仍能扫到最后一个桶 | 选读 |
| `runtime/unit-test/maps/test_map_in_map_bounds.cpp` | 38 | array-of-maps 越界 key 返回 NULL+ENOENT | 选读 |
| `runtime/unit-test/maps/kernel_unit_tests.cpp` | 805 | 移植自内核 `test_maps` 风格的 12 个用例（hash/percpu/array/大 key/100 线程压力/并行增删/RDONLY/WRONLY） | 推荐（与内核语义对照） |
| `runtime/unit-test/maps/test_stack_trace_map.cpp` | 101 | stack trace map：填栈得 id、哈希碰撞时 `REUSE`/替换语义 | 选读 |
| `runtime/unit-test/maps/test_queue_map.cpp` | 539 | QUEUE map：FIFO、BPF_EXIST 覆盖最旧、非法 flag、标准 map 接口映射为 peek/push/pop | 选读 |
| `runtime/unit-test/maps/test_stack_map.cpp` | 567 | STACK map：同上但 LIFO | 选读 |
| `runtime/unit-test/maps/test_bloom_filter_map.cpp` | 758 | bloom filter：无假阴性、假阳性率、hash 个数、DJB2 vs JHASH 计时、极端负载 | 选读 |
| `runtime/unit-test/maps/test_lpm_trie_map.cpp` | 1355 | LPM trie：与朴素参考实现 TLPM 对拍、IPv4/IPv6、删除、flags、满、多线程、迭代 | 选读 |
| `runtime/unit-test/maps/test_lru_hash_map.cpp` | 341 | LRU hash：淘汰、flags、容量、迭代 | 选读 |
| `runtime/unit-test/maps/test_lru_var_hash_map.cpp` | 100 | LRU var hash：未访问的 key=3 被淘汰 | 选读 |

### 1.2 runtime/test（已废弃，全部读过）

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `runtime/test/README.md` | 3 | "depreciated, use the unit-test instead" | 跳过 |
| `runtime/test/CMakeLists.txt` | 114 | `test_sources` 里所有条目都被注释（`:11-24`），`foreach` 不产生任何目标 | 跳过（知道它是空的即可） |
| `runtime/test/src/test_hash_map.c` | 82 | 旧 API（`bpftime_map_init`、`map/map_hash.h`），这些 API 在当前树里已不存在 | 跳过 |
| `runtime/test/src/test_shm_client.cpp` / `test_shm_server.cpp` | 41 / 32 | 旧的 server 建 shm、client `system()` 拉起；仍用当前 API 但未编译 | 跳过 |
| `runtime/test/src/test_ufunc.cpp` | 46 | 旧 API `bpftime_probe_create_ctx`/`bpftime_ufunc_ctx_from_btf`（已不存在） | 跳过 |
| `runtime/test/include/test_defs.h` | 64 | 旧测试公共定义（`add_func`/`print_func`） | 跳过 |
| `runtime/test/include/test_bpf_progs.h` | 122 | 手写字节码数组 | 跳过 |
| `runtime/test/include/test_minimal_bpf_host_ufunc.h` | 209 | 旧 ufunc 宿主定义 | 跳过 |
| `runtime/test/bpf/Makefile` | 75 | 把 `*.bpf.c` 编成 `.bpf.o`（`make unit-test-runtime` 会调用它） | 跳过 |
| `runtime/test/bpf/.gitignore` | 4 | — | 跳过 |
| `runtime/test/bpf/{btf-relo,ffi-btf,global,hash-map-test,map,patch,replace}.bpf.c`、`ufunc.bpf.h` | 42/34/24/32/18/72/32/97 | 旧测试用 eBPF 源；**没有任何现存测试读取它们的产物** | 跳过 |

### 1.3 example/（除 gpu、attach_implementation）——按目录列出全部文本文件（文件名(行数)）

| 目录 | 文件 | 合计行数 |
|---|---|---|
| `example/` | Makefile(13), README.md(163) | 176 |
| `example/minimal/` | attach_override.h(81), Makefile(141), README.md(128), syscall.bpf.c(13), syscall.c(70), uprobe.bpf.c(12), uprobe.c(69), uprobe-override.bpf.c(14), uprobe-override.c(70), victim.c(20) | 618 |
| `example/minimal/usdt_minimal/` | Makefile(141), README.md(3), usdt_minimal.bpf.c(14), usdt_minimal.c(71), victim.cpp(24) | 253 |
| `example/malloc/` | Makefile(141), malloc.bpf.c(41), malloc.c(119), README.md(70), victim.c(13) | 384 |
| `example/error-inject/` | attach_override.h(86), error_inject.bpf.c(24), error_inject.c(73), error_inject_syscall.bpf.c(25), error_inject_syscall.c(70), Makefile(141), README.md(19), victim.c(26) | 464 |
| `example/xdp-counter/` | Makefile(138), README.md(47), xdp-counter.bpf.c(72), xdp-counter.c(73)，另有二进制 base.btf | 330 |
| `example/libbpftime_example/` | ebpf.json(11), main.cpp(16), Makefile(27), README.md(38) | 92 |
| `example/otel-ebpf-profiler/` | README.md(56), config/otelcol.yaml(17), scripts/run.sh(99), victim-src/victim.c(13) | 185 |
| `example/hotpatch/` | Makefile(13) | 13 |
| `example/hotpatch/redis/` | Makefile(138), README.md(88), redis-patch.bpf.c(92), redis-patch.c(73), redis-server.h(2746), ufunc.bpf.h(97) | 3234 |
| `example/hotpatch/vim/` | poc.bpf.c(21), README.md(83)（**没有 Makefile**） | 104 |
| `example/bpf_features/` | Makefile(13) | 13 |
| `example/bpf_features/bloom_filter_demo/` | compat_libbpf.h(12), Makefile(116), README.md(175), target.c(88), test_correct_bloom_filter.sh(130), uprobe_bloom_filter.bpf.c(104), uprobe_bloom_filter.c(215) | 840 |
| `example/bpf_features/get_stack_id_example/` | get_stack_id_example.bpf.c(57), get_stack_id_example.c(132), get_stack_id_example.h(16), Makefile(141), README.md(70), victim.c(13) | 429 |
| `example/bpf_features/lpm_trie_demo/` | file_access_filter.bpf.c(127), file_access_monitor.c(184), file_access_target.c(151), Makefile(140), README.md(61), run_lpm_trie_demo.sh(249) | 912 |
| `example/bpf_features/queue_demo/` | Makefile(90), QUEUE_TEST_README.md(98), run_demo.sh(248), run_queue_demo.sh(281), run_stack_demo.sh(281), STACK_TEST_README.md(98), target.c(49), uprobe_queue.bpf.c(105), uprobe_queue.c(186), uprobe_stack.bpf.c(105), uprobe_stack.c(195) | 1736 |
| `example/bpf_features/tailcall_minimal/` | Makefile(141), README.md(30), tailcall_minimal.bpf.c(20), tailcall_minimal.c(239), victim.c(24) | 454 |
| `example/tracing/` | Makefile(13) | 13 |
| `example/tracing/bashreadline/` | Makefile(138), readline.bpf.c(39), readline.c(101), readline.h(12), README.md(3) | 293 |
| `example/tracing/bpftrace/` | bashreadline.bt(28), execsnoop.bt(27), gethostlatency.bt(47), killsnoop.bt(39), opensnoop.bt(46), README.md(80), sslsnoop.bt(69), statsnoop.bt(57), syncsnoop.bt(33), syscount.bt(37), threadsnoop.bt(26) | 489 |
| `example/tracing/funclatency/` | bits.bpf.h(31), funclatency.bpf.c(85), funclatency.c(663), funclatency.h(11), Makefile(141), victim.cpp(24) | 955 |
| `example/tracing/goroutine/` | goroutine.bpf.c(58), goroutine.c(219), goroutine.h(24), Makefile(137), README.md(125), LICENSE(122), go-server-http/main.go(34)，另有预编译二进制 go-server-http/main | 597 |
| `example/tracing/mountsnoop/` | Makefile(141), mountsnoop.bpf.c(134), mountsnoop.c(305), mountsnoop.h(40), victim.c(10) | 630 |
| `example/tracing/opensnoop/` | Makefile(141), opensnoop.bpf.c(136), opensnoop.c(330), opensnoop.h(26), README.md(3), victim.cpp(22) | 658 |
| `example/tracing/opensnoop_ring_buf/` | Makefile(141), opensnoop.bpf.c(110), opensnoop.c(325), opensnoop.h(26), README.md(35), victim.cpp(23) | 660 |
| `example/tracing/sigsnoop/` | Makefile(141), sigsnoop.bpf.c(116), sigsnoop.c(252), sigsnoop.h(16), victim.cpp(12) | 537 |
| `example/tracing/sslsniff/` | Makefile(138), README.md(31), sslsniff.bpf.c(191), sslsniff.c(521), sslsniff.h(26)（树里另有别人构建留下的 `sslsniff` 二进制和 `.output/`，未跟踪） | 907 |
| `example/tracing/statsnoop/` | Makefile(141), README.md(3), statsnoop.bpf.c(131), statsnoop.c(203), statsnoop.h(16), victim.cpp(30) | 524 |
| `example/tracing/syscount/` | errno_helpers.c(232), errno_helpers.h(7), Makefile(141), maps.bpf.h(42), README.md(3), syscall_helpers.c(850), syscall_helpers.h(12), syscount.bpf.c(89), syscount.c(481), syscount.h(15), victim.cpp(22) | 1894 |

只列出不细读（由别人负责）：

- `example/gpu/`：atomizer、cuda-counter、cudagraph、cutlass、directly_run_on_gpu、faiss-test、gpu_shared_map、host_map_test、kernelretsnoop、kernel_trace、launchlate、launchlate-kernel-gpu-shared-map、llama-cpp-test、mem_trace、pytorch-test、rocm-counter、threadhist、threadhist-gpu-kernel-shared-map、threadscheduling、threadscheduling_dynamic_hook，以及 Makefile、README.md。
- `example/attach_implementation/`：benchmark/、controller/、nginx_plugin/、nginx_plugin_adaptor/、CMakeLists.txt、README.md、index.html、nginx.conf（仅在 `BUILD_ATTACH_IMPL_EXAMPLE=ON` 时被根 `CMakeLists.txt:265-267` 加入）。

### 1.4 benchmark/（除 gpu）

| 目录 | 文件 | 合计行数 |
|---|---|---|
| `benchmark/` | CMakeLists.txt(42), Makefile(12), README.md(187), requirements.txt(4), test.c(136), test_embed.c(207) | 588 |
| `benchmark/uprobe/` | attach_override.h(81), benchmark.py(603), example_results.md(78), Makefile(139), README.md(281), uprobe.bpf.c(102), uprobe.c(69), uprobe-override.bpf.c(12), uprobe-override.c(70) | 1435 |
| `benchmark/syscall/` | benchmark.py(671), example_results.md(73), Makefile(141), README.md(43), syscall.bpf.c(21), syscall.c(90), victim.cpp(53) | 1092 |
| `benchmark/mpk/` | benchmark_mpk.py(62), benchmark.py(518), build.sh(25), example_results.md(85), Makefile(2), README.md(42) | 734 |
| `benchmark/ssl-nginx/` | benchmark.py(554), draw_figture.py(186), Makefile(11), nginx.conf(46), README.md(99)，证书 test.{cer,csr,key} | 896 |
| `benchmark/syscount-nginx/` | benchmark.py(937), Makefile(11), nginx.conf(39), README.md(20) | 1007 |
| `benchmark/deepflow/` | Makefile(2), README.md(128), benchmark/{collect_data.py(121), plot.py(125), README.md(36), run.py(336), test_run.py(11)}, scripts/{avg.py(101), drop.py(44)}, go-server/main.go(34), go-server-http/main.go(34)，另有预编译 go 二进制和 pem | 938 |
| `benchmark/fuse/` | bench.cpp(203), benchmark.py(181), Makefile(14), README.md(51), bpf/{fs-cache.bpf.c(429), fs-cache.c(90), fs-cache.h(36), Makefile(139), README.md(216)}, fs/{passthrough.c(408), passthrough.py(133)} | 1900 |
| `benchmark/redis-durability-tuning/` | attach_override.h(92), benchmark.py(285), Makefile(57), README.md(149), redis.conf(2299), batch_process/{Makefile(141), redis.bpf.c(72), redis.c(95)}, delayed-fsync/{Makefile(141), redis.bpf.c(57), redis.c(96)}, fsync-fast-notify/{Makefile(138), redis.bpf.c(57), redis.c(96)}, poc-iouring-minimal/{fsync_write.c(126), Makefile(138), redis.bpf.c(72), redis.c(93)} | 4204 |
| `benchmark/trap-uprobe/` | benchmark.py(192), CMakeLists.txt(10), Makefile(4), README.md(76), trap_bench.cpp(179) | 461 |

只列出：`benchmark/gpu/`（host/、micro/、nvbit/、workload/、Makefile、README.md、run_cuda_bench.py；`gpu/host/*_perf.cpp` 由 `benchmark/CMakeLists.txt:16-41` 在 CUDA 构建时编译）。

## 2. 核心数据结构（测试夹具，简写）

| 结构 | 位置 | 要点 / 生命周期 |
|---|---|---|
| `::shm_remove` | `runtime/unit-test/common_def.hpp:6-19` | 栈对象；构造和析构都 `shared_memory_object::remove(filename)`，删的是**同一个名字**。 |
| `bpftime::shm_remove` | `runtime/src/handler/handler_manager.hpp:69-85` | 同名但语义不同：`shm_remove(const char*)` 构造时删传入名字，**析构时删 `get_global_shm_name()`**（`BPFTIME_GLOBAL_SHM_NAME`，未设时为 `bpftime_maps_shm`，`handler_manager.hpp:51,62-64`）。见 7.2。 |
| `shm_initializer` | `maps/kernel_unit_tests.cpp:28-45`、`maps/test_lru_var_hash_map.cpp:11-29` | `create_only` 新建或 `open_only` 打开 managed_shared_memory，`unique_ptr` 持有；`test_map_stress` 用它让 100 个线程各自 `open_only` 同一段。 |
| `handler_manager`（测试中手工构造） | `test_shm_progs_attach.cpp:208-209`、`test_shm_hash_maps.cpp:111-112` | `segment.construct<handler_manager>("my_handler")(segment, max_fd)`，放在 shm 里，父子进程用 `segment.find` 拿同一个；子进程 `clear_all` + `destroy_ptr` 后父进程断言找不到。 |
| `helper_result` / `env_var_guard` / `parent_shm_guard` | `test_shm_allocation_failure.cpp:25-85` | 子进程退出码+合并后的 stdout/stderr；RAII 还原环境变量；父进程临时打开全局 shm 并登记 pid 到 alive agent / syscall-server 集合，析构时撤销。 |
| `mock_stack_attach_impl` + 全局 `mock_attach_ctx` | `test_get_stack.cpp:18-58` | 覆盖 runtime 里的弱符号 `get_global_attach_ctx`（`runtime/src/bpf_helper.cpp:58-59`），在类型 6 注册一个只实现 `generate_stack` 的 attach impl。**它是整个测试二进制的全局对象**，启动时那行 `bpf_attach_ctx constructed` 日志就来自它（推断）。 |
| `credentials` + `__wrap_getuid/getgid` | `test_get_current_uid_gid.cpp:8-33` | `thread_local` 指针决定 wrap 是否返回假凭据；链接选项见 `runtime/unit-test/CMakeLists.txt:69-73`。 |
| `tlpm_node` | `maps/test_lpm_trie_map.cpp:52-` | 朴素链表式最长前缀匹配，作为 LPM trie 的对拍参考实现（移植自内核 selftests 思路，推断）。 |

## 3. 主要流程与调用链（简写）

1. **最小"加载并执行"**：`TEST_CASE("Test helpers")`（`attach_with_ebpf/test_helpers.cpp:29`）→ `bpftime_object_open(path, config)`（`:41`）→ `bpftime_object__next_program`（`:43`）→ `get_kernel_utils_helper_group().add_helper_group_to_prog`（`:48`）→ `bpftime_prog_load(false)`（`:51`）→ `bpftime_prog_exec(&memory, size, &ret)`（`:54`）。不涉及 shm、不涉及 attach，是理解 VM 封装的最短路径。
2. **直接用 Frida 后端挂 uprobe**：`TEST_CASE("Test probing internal functions")`（`test_attach_uprobe_with_ebpf.cpp:22`）→ `create_bpftime_prog`（`helper.hpp:5-14`）→ `attach::frida_attach_impl man`（`:44`）→ `man.create_uprobe_at(addr, cb)`（`:54`）/`create_uretprobe_at`（`:63`）→ 调被探测函数，回调里 `bpftime_prog_exec(&regs,…)` 并断言返回值等于 `bpf_get_func_ip` → `detach_by_id`（`:75-77`）。说明 attach impl 本身只认"地址+回调"，与 shm 无关。
3. **override / 过滤**：`test_attach_filter_with_ebpf.cpp:61` `create_uprobe_override_at` → 回调执行 `filter.bpf.c`，`str==NULL` 时程序里 `bpf_set_retval(-22)` 并返回 `OP_SKIP`（`assets/filter.bpf.c:24-26`）→ 原函数不执行、调用者拿到 -22（`:72-73`）。两个 SECTION 分别按 id、按函数地址 detach（`:74-83`）。
4. **从 shm handler 表装配 hook（agent 路径缩影）**：`TEST_CASE("Test shm progs attach")`（`test_shm_progs_attach.cpp:196`）→ 在 shm 里 `construct<handler_manager>`（`:208`）→ `attach_replace` 写 `bpf_prog_handler`(fd 0) + `bpf_perf_event_handler(BPF_TYPE_UREPLACE)`(fd 3) + `bpf_link_handler(0,3)`（`:118-146`）；`attach_uprobe` 写 uprobe perf handler(fd 4) + link（`:96-116`）→ `ctx.register_attach_impl({6,7,1008,1009}, frida_attach_impl, private_data 工厂)`（`:219-232`）→ `ctx.init_attach_ctx_from_handlers(manager, config)`（`:238`）→ 调函数 → `destroy_instantiated_attach_link`（`:252-253`）→ fork，子进程 `open_only` 同一段 shm 重复装配（`:148-194`）。
5. **全局 shm 上的 map/prog 与尾调用**：`test_user_to_user_tailcall.cpp:14` → `bpftime_initialize_global_shm(SHM_REMOVE_AND_CREATE)`（`:17-18`）→ `bpftime_maps_create(1001, PROG_ARRAY)`（`:20`）→ `bpftime_progs_create(1002, …)`（`:32`）→ `bpftime_map_update_elem`（`:43`）→ 调用者程序 `call 12` → 返回 0x1234（`:64-65`）→ `bpftime_close(1002)` 后 lookup 为空（`:69-72`）→ `destroy` 后再 `SHM_NO_CREATE` 初始化留一个空对象（`:74-78`）。
6. **示例的两进程运行（已实跑 malloc）**：loader 带 `LD_PRELOAD=libbpftime-syscall-server.so` 跑 libbpf skeleton（`example/malloc/malloc.c:101-104` 用 `bpf_program__attach_uprobe_opts` 挂 `libc.so.6:malloc`）→ syscall-server 把对象写入 `BPFTIME_GLOBAL_SHM_NAME` 指定的 shm → victim 带 `LD_PRELOAD=libbpftime-agent.so` 启动，agent 打开 shm 并装 hook → `bpf_printk` 输出到 victim 的 stdout，loader 周期性遍历 hash map 打印 `pid=… malloc calls: …`。

## 4. 跨模块接口

### 4.1 本块对外提供

- `bpftime_runtime_tests` 二进制：被 `Makefile:46-50`（`unit-test-runtime`）、`.github/workflows/test-runtime.yml:128,274` 调用；`add_test(NAME bpftime_runtime_tests …)`（`runtime/unit-test/CMakeLists.txt:140`）让 `ctest` 能整体跑它。
- `bpftime-runtime-options-test-helper`：只被 CI `test-runtime.yml:233-251` 用（`BPFTIME_RUN_WITH_KERNEL=true` + `BPFTIME_NOT_LOAD_PATTERN='bypass_me'`，需要内核 BPF 权限）。
- `bpftime-shm-allocation-test-helper`：只被 `test_shm_allocation_failure.cpp` 通过生成的头 `shm_allocation_test_paths.hpp`（`CMakeLists.txt:95-97`）使用。
- `test_get_stack.cpp:45` 定义强符号 `get_global_attach_ctx()`，**覆盖** `runtime/src/bpf_helper.cpp:59` 的弱定义——这对整个测试二进制生效。
- `simple-benchmark-with-embed-ebpf-calling`（`benchmark/CMakeLists.txt:1-14`）与 `trap-uprobe-bench`（`benchmark/trap-uprobe/CMakeLists.txt:1-10`，仅 trap 构建）。
- 示例/benchmark 不对外提供库接口；benchmark/ssl-nginx、syscount-nginx 的 Makefile 依赖 `example/tracing/sslsniff`、`example/tracing/syscount`（`benchmark/ssl-nginx/Makefile`、`benchmark/syscount-nginx/Makefile`）。

### 4.2 本块依赖外部（单测里的调用点）

| 外部接口 | 所属模块 | 测试中调用位置 |
|---|---|---|
| `bpftime_object_open` / `bpftime_object__next_program` / `bpftime_object_find_program_by_name` / `bpftime_object_close` | runtime/object | `test_helpers.cpp:41,43`、`test_attach_filter_with_ebpf.cpp:44,48`、`test_attach_uprobe_with_ebpf.cpp:30`、`test_shm_progs_attach.cpp:213,215`、`test_ufunc_register.cpp:75,77`、`helper.hpp:9` |
| `bpftime_helper_group::get_{kernel_utils,ufunc}_helper_group().add_helper_group_to_prog` | runtime helper | `test_helpers.cpp:48`、`test_attach_filter_with_ebpf.cpp:50-53`、`test_ufunc_register.cpp:82`、`tailcall/*.cpp:59,110` |
| `bpftime_prog::{bpftime_prog_load, bpftime_prog_exec, bpftime_prog_register_raw_helper, bpftime_prog_unload, prepare_for_signal_execution}` | runtime prog / VM | 各 attach 测试；`prepare_for_signal_execution` 仅 trap 测试 |
| `attach::frida_attach_impl::{create_uprobe_at, create_uretprobe_at, create_uprobe_override_at, detach_by_id, detach_by_func_addr}`、`attach::find_function_addr_by_name`、`attach::get_module_base_addr` | attach/frida | `test_attach_uprobe_with_ebpf.cpp:54-77,94-113`、`test_attach_filter_with_ebpf.cpp:61,76,80`、`test_attach_replace.cpp:57-70,97,113`、`test_shm_progs_attach.cpp:101-102,122` |
| `bpf_attach_ctx::{register_attach_impl, init_attach_ctx_from_handlers, destroy_instantiated_attach_link, find_instantiated_prog}` | runtime attach ctx | `test_shm_progs_attach.cpp:162,179,219,238,252`、`test_get_stack.cpp:48`、`test_bpftime_shm_json.cpp:105` |
| `handler_manager::{set_handler, set_handler_at_empty_slot, clear_id_at, clear_all}`、`bpf_map_handler`、`bpf_prog_handler`、`bpf_perf_event_handler`、`bpf_link_handler` | runtime handler | `test_shm_hash_maps.cpp:87,115-126,180,213,220,227`、`test_shm_progs_attach.cpp:111-144` |
| `bpftime_shm::{add_bpf_prog, add_tracepoint, add_bpf_link, add_bpf_map, attach_perf_to_bpf, is_*_fd}`、`bpftime_export_shm_to_json`/`bpftime_import_shm_from_json` | runtime shm/json | `test_bpftime_shm_json.cpp:66-100` |
| `bpftime_initialize_global_shm` / `bpftime_destroy_global_shm` / `bpftime_maps_create` / `bpftime_progs_create` / `bpftime_map_update_elem` / `bpftime_map_lookup_elem` / `bpftime_close` | runtime C API（`bpftime_shm.hpp`） | `tailcall/*.cpp`、`test_epoll.cpp:70-95`、`test_ringbuf_tail_wrap.cpp:33-58`、`test_software_perf_event.cpp:43-103` |
| `bpftime_epoll_create/wait`、`bpftime_add_{ringbuf,software_perf_event}_fd_to_epoll`、`bpftime_ringbuf_reserve/submit`、`bpftime_poll_from_ringbuf`、`bpftime_perf_event_output`、`bpftime_add_software_perf_event`、`bpftime_get_software_perf_event_raw_buffer` | runtime 输出通道 | `test_epoll.cpp:85-130`、`test_ringbuf_tail_wrap.cpp:65-86` |
| `software_perf_event_data::{ensure_mmap_buffer, output_data, has_data, producer_shards}` | `runtime/src/handler/perf_event_handler.hpp` | `test_software_perf_event.cpp:169-342` |
| helper 的 C 符号：`bpftime_probe_read`、`bpftime_probe_write_user`、`bpf_probe_read_str`、`bpftime_get_current_pid_tgid`、`bpf_get_current_uid_gid`、`bpftime_get_stack`、`bpf_perf_event_output`、`bpftime_set_retval` | `runtime/src/bpf_helper.cpp` | `test_probe.cpp:26-31`、`test_get_current_pid_tgid.cpp:13-15`、`test_get_current_uid_gid.cpp:18-21`、`test_get_stack.cpp:60-61`、`test_software_perf_event.cpp:20-21`、`test_attach_replace.cpp:18` |
| `bpftime_ufunc_resolve_from_info` / `bpftime_ufunc_register_ufunc` | `runtime/src/ufunc.cpp:52-72` | `test_attach_replace.cpp:57,69`、`test_shm_progs_attach.cpp:82,92`、`test_ufunc_register.cpp:45,56` |
| `bpftime_register_map_ops`、`bpftime_set_runtime_config` | runtime map ops / config | `test_external_map_ops.cpp:62,71,78` |
| 各 map 实现类（`per_cpu_array_map_impl`、`per_cpu_hash_map_impl`、`var_size_hash_map_impl`、`lru_var_hash_map_impl`、`fix_size_hash_map_impl`、`array_map_of_maps_impl`、`stack_trace_map_impl`、`queue_map_impl`、`stack_map_impl`、`bloom_filter_map_impl`、`lpm_trie_map_impl`、`bpftime_hash_map`）及 `ensure_on_certain_cpu` | `runtime/src/bpf_map/` | 各 `maps/*.cpp` |
| libbpf 内核接口 `bpf_map_create`、`bpf_prog_load`、`bpf_map_lookup_elem` | third_party/libbpf | `test_user_to_kernel_tailcall.cpp:24,61,122` |
| `trap_attach_impl` 等 | attach/trap_uprobe_attach_impl | `test_attach_uprobe_with_ebpf_trap.cpp`（x86 不编） |

### 4.3 示例依赖的外部能力（需要别的读者确认是否仍支持）

- `perf_event_attr.type = 1008`（`BPF_TYPE_UPROBE_OVERRIDE`）+ `bpf_prog_attach(..., BPF_MODIFY_RETURN)` 的私有约定：`example/minimal/attach_override.h:26-79`（`perf_event_open_with_override`、`bpf_prog_attach(…, BPF_MODIFY_RETURN)` 在 `:65`）、`example/error-inject/attach_override.h`、`benchmark/uprobe/attach_override.h`；runtime 侧识别在 `runtime/syscall-server/syscall_context.cpp:911-925`、`runtime/include/bpftime_shm.hpp:62-63`、`attach/frida_uprobe_attach_impl/include/frida_uprobe_attach_impl.hpp:29-31`。
- `elf_find_func_offset_from_file` 是 libbpf **内部**函数（`third_party/libbpf/src/libbpf_internal.h:742`），示例直接声明并链接静态 libbpf.a 来用。
- syscall tracepoint 上的 `bpf_override_return` 由 `attach/syscall_trace_attach_impl/src/syscall_trace_attach_impl.cpp:31-90` 的 `curr_thread_override_return_callback` 支撑（error_inject_syscall 示例依赖它）。

## 5. 配置项、环境变量、编译开关

| 名字 | 默认 | 定义 / 读取位置 | 对测试与示例的影响 |
|---|---|---|---|
| `BPFTIME_GLOBAL_SHM_NAME` | `bpftime_maps_shm` | `runtime/src/handler/handler_manager.hpp:51,62-64` | 所有用全局 shm 的测试（tailcall、epoll、ringbuf、perf affinity、`test_attach_uprobe_with_ebpf` 的 `shm_holder`）以及**所有 `bpftime::shm_remove` 析构**都落在这个名字上。不设就会动默认 shm（见 7.2）。示例两端必须一致。 |
| `BPFTIME_ENABLE_UNIT_TESTING` + `BPFTIME_BUILD_WITH_LIBBPF` | OFF / ON | `runtime/CMakeLists.txt:281-286` | 两者都开才加入 `runtime/test` 与 `runtime/unit-test`。 |
| `BPFTIME_ENABLE_FRIDA` / `BPFTIME_ENABLE_TRAP_UPROBE` | 按架构 **FORCE** | `cmake/StandardSettings.cmake:76-82` | x86/arm64：Frida ON、trap OFF（命令行 `-DBPFTIME_ENABLE_TRAP_UPROBE=ON` 也会被覆盖）。Frida 关时 4 个 attach 测试不编（`runtime/unit-test/CMakeLists.txt:54-61`）；trap 关时 trap 测试文件内容为空（`:118-122`）。 |
| `ENABLE_PROBE_READ_CHECK` / `ENABLE_PROBE_WRITE_CHECK` | ON | `cmake/StandardSettings.cmake:110,112`；测试二进制宏 `runtime/unit-test/CMakeLists.txt:75-81` | 关掉时 `test_probe.cpp:46-54,92-122,186-196` 的越界断言被编译掉。**本机 build-jit 两个都是 OFF**，所以"Test bpftime_probe_read"只跑 5 个断言。 |
| `BPFTIME_ENABLE_CUDA_ATTACH` | OFF | `runtime/unit-test/CMakeLists.txt:63-65,124-129` | 决定是否编 `cuda/test_cuda_compile.cpp`。 |
| `TEST_LCOV` | OFF | `runtime/unit-test/CMakeLists.txt:79,101-103,131-136` | 覆盖率插桩。 |
| `BPFTIME_LLVM_JIT` | ON | `cmake/StandardSettings.cmake:10` | 开时测试链接加 `-Wl,--export-dynamic`（`runtime/unit-test/CMakeLists.txt:107-111`）。多数测试 `config.set_vm_name("llvm")` 写死。 |
| `BPFTIME_VM_NAME` | 无（用默认 VM） | `runtime/src/bpftime_config.cpp:130` | bloom 示例 README 要求设 `ubpf`，脚本却设 `llvm`（见 7.4）。`Makefile:48` 的 `export BPFTIME_VM_NAME=llvm` 单独一行，对下一行无效。 |
| `BPFTIME_LOG_OUTPUT` | `~/.bpftime/runtime.log` 一类默认路径 | `runtime/src/bpftime_config.cpp:158`、`runtime/agent/agent.cpp:133-135,957-958`、`runtime/syscall-server/syscall_context.cpp:200` | agent 启动早期用自己的环境变量，装配后改用 shm 里 loader 写入的路径（已实跑 malloc：victim 的后半段日志进了 loader 指定的 `server.log`）。 |
| `BPFTIME_SHM_MEMORY_MB` / `BPFTIME_MAX_FD_COUNT` | — | `runtime/src/bpftime_config.cpp:119-128` | `test_shm_allocation_failure.cpp:121-134` 用 `64MB + 1048576 fd` 构造"shm 放不下 handler 表"的场景。 |
| `BPFTIME_ALLOW_EXTERNAL_MAPS` / `BPFTIME_FRIDA_FUZZY_BACKTRACER` | 未设=关 | `runtime/src/bpftime_config.cpp:110-117` | `test_config.cpp:25-60`。 |
| `HOME` | — | 测试 `test_shm_allocation_failure.cpp:123,208,295` 设成 `/proc/bpftime-unwritable-home` | 确保默认日志文件写不进去时也不回落到宿主 stdio。 |
| `AGENT_SO` | — | `attach/text_segment_transformer/agent-transformer.cpp:56` | 示例 README 中 `start -s`/syscall 跟踪的手工写法。 |
| `-Wl,--wrap=getuid,--wrap=getgid` | Linux 上恒加 | `runtime/unit-test/CMakeLists.txt:69-73` | uid/gid 测试的 mock 机制。 |
| Catch2 版本 | v3.4.0 | 根 `CMakeLists.txt:145-153`（子模块）或 `runtime/unit-test/CMakeLists.txt:1-12`（开 verifier 时 FetchContent） | 运行时 banner 显示 "Catch2 v3.4.0"（已实跑）。 |

## 6. 并发、生命周期与错误处理（测试视角）

- **fork 类测试**：`test_shm_hash_maps.cpp:137-148`、`test_shm_progs_attach.cpp:255-269`、`test_probe.cpp:132-170,229-253`、`test_get_current_pid_tgid.cpp:58-85`、`test_shm_allocation_failure.cpp:110-145`。子进程用 `_exit` 而非 `REQUIRE` 报告结果，父进程用 `waitpid` + `WEXITSTATUS` 断言——这是 Catch2 下测试多进程的正确姿势。**例外**：`test_shm_progs_attach.cpp:179` 在子进程里用 `REQUIRE`，`:156,186` 用 `assert`（Release/RelWithDebInfo 下 `NDEBUG` 使其失效，见 7.3）。
- **多线程**：`test_software_perf_event.cpp:177-205`（4 线程 × 256 事件，靠 per-thread shard 无锁）；`kernel_unit_tests.cpp:582-605`（100 线程各自 open 同一 shm 段并各建 map）、`:708-769`（20 线程并行增删同一 map，**测试自己加 `pthread_spinlock`**）；`test_queue_map.cpp:267-360`、`test_stack_map.cpp:304-`（同样外加自旋锁）；`test_lpm_trie_map.cpp:946`。
- **信号**：`test_probe.cpp:125-171` 验证 probe 访问不替换/不吞掉应用自己的 SIGSEGV handler、`:227-253` 验证 SIG_DFL 被保留；`test_epoll.cpp:44-52,128-143` 验证 `bpftime_epoll_wait` 返回后信号掩码与调用前一致；`shm_allocation_test_helper.c:200-207` 验证 agent 不改 SIGUSR1。
- **失败行为（宿主契约）**：`test_shm_allocation_failure.cpp:343-512` 系统地覆盖"shm 放不下→回落到 libc（退出码 100 表示宿主正常跑完）、日志 sink 失败不写 stdio、默认不写 stdio、只在自己创建时才删 shm、仍有存活 agent/peer server 时不删 shm、mock 文件不留 /tmp 垃圾"。与 `CLAUDE.md` 的 "Injected Agent Host-Process Contract" 一一对应。
- **全局状态复位**：多个测试在结束时 `bpftime_destroy_global_shm()` 再 `bpftime_initialize_global_shm(SHM_NO_CREATE)`（例如 `test_user_to_user_tailcall.cpp:74-78`、`test_epoll.cpp:70-72`），注释解释是为了不给后续测试留下指向已 unmap 段的悬空对象。说明测试之间**共享进程级全局状态**，顺序会影响结果（7.1 就是例子）。

## 7. 不直观的行为、疑似 bug、代码与注释不一致

### 7.1 `Test attach replace with ebpf` 在默认顺序下必然 SIGSEGV（测试顺序依赖）——确认（实跑）

- 现象：单独跑或整套默认顺序跑，第 2 个断言前崩溃（`FAILED … SIGSEGV`，`assertions: 2 | 1 passed | 1 failed`），连续 3 次复现。
- 位置：`runtime/unit-test/attach_with_ebpf/test_attach_replace.cpp:82-84`：先 `register_ufunc_for_print_and_add`（内部 `bpftime_ufunc_resolve_from_info(…, &attach::find_function_addr_by_name)`，`:57,69`），再构造 `frida_attach_impl`（`:95`）。`find_function_addr_by_name` 直接调 `gum_find_function`（`attach/frida_uprobe_attach_impl/src/frida_attach_utils.cpp:137-145`），而 `gum_init_embedded()` 在 `frida_attach_impl` 构造函数里（`frida_uprobe_attach_impl.cpp:40`）。
- 验证：用 `--order rand --rng-seed N` 让 "Test probing internal functions"（会先构造 `frida_attach_impl`）排在前面，两例都过（`44 assertions in 2 test cases`）；它排在后面时 rc=139。对比 `test_shm_progs_attach.cpp:219-233` 是先注册 attach impl 再 resolve ufunc，所以不崩。
- 结论：Frida 未初始化就调 gum 查符号。测试用例的 Catch2 默认（注册）顺序里它排第 2（`--list-tests` 输出），因此整套跑必崩。CI 是否同样失败未验证（CI 用 Debug，见 `.github/workflows/test-runtime.yml:123-128`）。按 NUC 规则只记录不修。
- 绕过命令：`./bpftime_runtime_tests "Test probing internal functions,Test attach replace with ebpf" --order rand --rng-seed 3`（已实跑通过）。

### 7.2 `bpftime::shm_remove` 析构删的是"全局 shm 名"而不是传入的名字——确认（代码 + 实跑现象）

- 位置：`runtime/src/handler/handler_manager.hpp:75-84`。构造 `shm_remove(name)` 删 `name`，析构删 `get_global_shm_name()`。
- 受影响的测试：`test_shm_progs_attach.cpp:202`（显式 `bpftime::shm_remove`）、`test_external_map_ops.cpp:50`（`struct bpftime::shm_remove`）、`test_shm_hash_maps.cpp:103`（没 include `common_def.hpp`，`using namespace bpftime` 下解析到 `bpftime::shm_remove`，推断）、`test_user_to_kernel_tailcall.cpp:69`。
- 后果 1（泄漏）：这些测试自己的段（`my_shm_attach_test`、`BPFTIME_TEST_ENTERNAL_MAP_SHM`、`my_shm_maps_test`）跑完留在 `/dev/shm`（已实跑观察到，且机器上本来就残留着别人留下的同名段）。
- 后果 2（误删）：析构时会删掉 `BPFTIME_GLOBAL_SHM_NAME`（未设时就是 **`bpftime_maps_shm`**），如果同机有真实 bpftime 会话在用默认名，跑单测会把它的 shm 名字 unlink 掉。
- 另一个相关点：tailcall 两个测试用 `shm_remove remover(SHM_NAME)` 删的是一个**从未创建的名字**（`"BPFTIME_…_TAIL_CALL_TEST_SHM"`），真正创建的是 `bpftime_initialize_global_shm(SHM_REMOVE_AND_CREATE)` 的全局名（`test_user_to_user_tailcall.cpp:10,16-18`、`test_user_to_kernel_tailcall.cpp:17,69-71`），而 `bpftime_destroy_global_shm` 只析构对象、不 unlink（`runtime/src/bpftime_shm_internal.cpp:59-71`、`:1257-1284`）。所以：不设 `BPFTIME_GLOBAL_SHM_NAME` 跑单测，会 **REMOVE_AND_CREATE 默认名 `bpftime_maps_shm`** 并留下它。
- 其它泄漏：`test_bpftime_shm_json.cpp:61` 的 `BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT`（50 MB）从不删除，且写 `/tmp/bpftime_test_shm_json.json`（`:66-67,88-89,96-97`）；`test_bpftime_hash_map.cpp:27-28` 用 `open_or_create` 的 `bpftime_hash_map_test` 从不删除。共享内存的锁文件 `/tmp/bpftime-shm-<name>.lock` 也会残留（实跑观察）。
- 建议给学习者的固定姿势：`export BPFTIME_GLOBAL_SHM_NAME=notes_$RANDOM`，跑完 `ls /dev/shm` 对比清理。

### 7.3 `Test shm progs attach` 对"替换生效"几乎没有断言——确认

- 父进程调用被替换函数后只打日志，不检查返回值（`test_shm_progs_attach.cpp:242-251`）；子进程里检查 `res == 35/100` 用的是 `assert`（`:156,186`），在 `NDEBUG`（RelWithDebInfo/Release）下被编译掉。子进程里还用了 `REQUIRE`（`:179`），失败时行为依赖 Catch2 在 fork 子进程中的处理（推断：会以非零退出，父进程 `WEXITSTATUS==0` 断言能兜住）。因此这个测试"通过"并不证明 UREPLACE 真的生效，只证明装配流程不崩。

### 7.4 其它测试层面的小问题

- `test_hashmap_sizes (kernel)` 没有任何断言（实跑输出 `assertions: - none -`；`kernel_unit_tests.cpp:148-163`）。
- `Test cuda compile` 没有断言、不检查 `bpftime_prog_load(true)` 的返回值（`cuda/test_cuda_compile.cpp:73-82`）。
- `Queue/Stack Map Conceptual Concurrency Test` 的注释说"主要依赖 queue_map_impl 内部互斥防崩溃"，但测试自己在每次 push/pop 外面包了 `pthread_spin_lock`（`test_queue_map.cpp:278-360`、`test_stack_map.cpp` 同构），实际上没有测到 map 自身的并发安全。
- `Queue Map Constructor Validation` 注释说"Test invalid max_entries (0)"，但用的是 `(shm, 0, 0)`（value_size 也是 0），没单独测"value_size 合法、max_entries=0"（`test_queue_map.cpp:53-58`）。
- 36/106 个用例没有 tag（全部 attach、probe、get_stack、tailcall、config、json 等），只能按名字或通配符过滤。
- `runtime_options_test_helper.c` 被 `add_dependencies` 拉进单测构建（`runtime/unit-test/CMakeLists.txt:90-94`），但没有任何 TEST_CASE 用它。

### 7.5 构建/脚本层面

- `Makefile:46-50` 的 `unit-test-runtime`：`make -C runtime/test/bpf && cp … build/runtime/test/` 和 `cd build/runtime/test && make && ctest -VV` 都针对**已废弃且为空**的 `runtime/test`（`runtime/test/CMakeLists.txt:11-24` 全注释）；用 Ninja 生成器时 `build/runtime/test` 下没有 Makefile，这一行会失败（本机 `build-jit/runtime/test/` 只有 `CMakeFiles/ cmake_install.cmake CTestTestfile.cmake`，推断在 Ninja 下 `make` 报错，未实跑）。`export BPFTIME_VM_NAME=llvm`（`:48`）单独一行无效。
- `example/Makefile`、`example/tracing/Makefile` 等用 `find . -maxdepth 2 -name Makefile` 枚举子目录，`make -C example` 会进入 `gpu/`（需要 CUDA），推断会失败（未实跑）。`benchmark/Makefile:5-12` 同理会进 `gpu/`。
- `Makefile:82-103` 的 `benchmark` 目标与 `benchmark/mpk/build.sh` 把 `LLVM_DIR` 写死为 `/usr/lib/llvm-15/cmake`。

### 7.6 示例/benchmark 文档与代码不一致（逐条，确认）

| 文档位置 | 文档说法 | 实际 |
|---|---|---|
| `example/README.md:42` | 指向 `documents/avaliable-features.md` | 仓库里没有 `documents/` 目录 |
| `example/README.md:69-70` | opensnoop "demonstrate … with ring buffer output" | `example/tracing/opensnoop/opensnoop.bpf.c` 用 `BPF_MAP_TYPE_PERF_EVENT_ARRAY` + `bpf_perf_event_output`；ringbuf 的是 `opensnoop_ring_buf` |
| `example/README.md:162` | VM CLI 在 `vm/cli` | 实际在 `vm/llvm-jit/cli`（`vm/example/` 仍存在） |
| `example/malloc/README.md:5-55` | 程序用 `bpf_get_current_cgroup_id()` 作 key、u64 key | `example/malloc/malloc.bpf.c:9,32-36` 用 pid（`bpf_get_current_pid_tgid()>>32`）、u32 key，并且 `bpf_printk` |
| `example/minimal/README.md:43-89` | "uprobe trace" 讲的是 cloudflare 的 cgroup 计数程序 | `example/minimal/uprobe.bpf.c:6-11` 只是 `SEC("uprobe/./victim:target_func")` + `bpf_printk` |
| `example/bpf_features/get_stack_id_example/README.md:1-70` | 全文复制自 malloc README，运行命令指向 `example/malloc/malloc` | 实际程序用 `BPF_MAP_TYPE_STACK_TRACE` + `bpf_get_stackid` + ringbuf，挂 `libc.so.6:malloc`/`free` |
| `example/bpf_features/*/README.md` 与 `*.sh`（bloom、queue、stack、lpm） | `LD_PRELOAD=../../build/runtime/…` | 这些目录在 `example/bpf_features/<demo>/` 下，`../../build` 指向 `example/build`，应为 `../../../build`。对应 Makefile 已改成 `../../../third_party`，脚本/README 没跟着改 |
| `example/bpf_features/bloom_filter_demo/README.md:27-33,156-158` vs `test_correct_bloom_filter.sh:8-12` | README 说必须 `BPFTIME_VM_NAME=ubpf`；脚本说默认 llvm 推荐、ubpf 可能有问题 | 互相矛盾；`build-jit` 是 LLVM 构建 |
| `example/bpf_features/lpm_trie_demo/README.md:17` | 脚本名 `run_lmp_trie_demo.sh` | 实际 `run_lpm_trie_demo.sh` |
| `example/bpf_features/queue_demo/run_*_demo.sh:189,192` | 路径 `example/queue_demo/` | 实际 `example/bpf_features/queue_demo/` |
| `example/tracing/statsnoop/README.md:3` | 链接 bcc 的 `bashreadline.bpf.c` | 应为 statsnoop |
| `example/tracing/opensnoop_ring_buf/README.md:10,24` | `example/opensnoop/opensnoop`、日志里 "helper groups ffi" | 路径已移到 `example/tracing/…`；日志文本是旧版 |
| `example/tracing/goroutine/README.md:14,21` | 在 `example/goroutine#` 下用 `../../build/…` | 目录已是 `example/tracing/goroutine`，应为 `../../../build` |
| `example/tracing/sslsniff/README.md:6-7` | `libbpftime-syscal` / `l-server.so` 被断行；`example/sslsniff/sslsniff` | 断行使命令不可复制；路径应为 `example/tracing/sslsniff/sslsniff` |
| `example/hotpatch/redis/README.md:3,69` | 有 tar.gz 预编译、用 `build/tools/cli/bpftime-cli workloads/…/poc.json` 注入 | 树里没有 tar.gz、没有 `workloads/`；当前 CLI 是 `bpftime`，子命令 `load/start/attach/detach/trace`（`tools/cli/main.cpp:914-981`），没有 JSON 注入。且 `redis-patch.c:55-64` 的 attach 代码整段注释，只 load 不 attach |
| `example/hotpatch/vim/README.md:6,46,57` | `workloads/ebpf-patch-dev/poc5-vim`、`make`、`poc5.json` | 目录里只有 `poc.bpf.c` 和 README，没有 Makefile、`ufunc.bpf.h`、json、预编译 vim |
| `example/libbpftime_example/README.md` | 需要 `~/.bpftime/libbpftime.a` | 需要 `-DBPFTIME_BUILD_STATIC_LIB=ON`（`cmake/StandardSettings.cmake:104`）构建并 install；命令本身与代码一致（未实跑） |
| `example/bpf_features/tailcall_minimal/README.md` | `bpftime load` / `bpftime start`，未提 root | loader 用真实 `syscall(__NR_bpf)` 把程序加载进内核（`tailcall_minimal.c:113-178`），需要 root/CAP_BPF |
| `benchmark/uprobe/README.md:28-45` | baseline 函数是 `__benchmark_test_function3`；`make -C benchmark/uretprobe` | `benchmark/test.c:7-29` 是 17 个 `__bench_*`；没有 `benchmark/uretprobe/` |
| `benchmark/uprobe/benchmark.py:14-20`、`benchmark/mpk/benchmark.py:17-22` | README 说在仓库根跑 | 脚本判断 `cwd.name == "bpftime"`，否则拼 `cwd/bpftime/benchmark`。仓库目录不叫 `bpftime`（例如本机 worktree）时路径会错（推断，未实跑） |
| `benchmark/ssl-nginx/README.md` | "one click" 用 `draw_figture.py`；`make -C example/sslsniff` | 跑测的是 `benchmark.py`，`draw_figture.py` 画图；sslsniff 在 `example/tracing/sslsniff`（Makefile 已用新路径） |
| `benchmark/redis-durability-tuning/README.md` | 提到第三种实现 `bpf-sync-kernel` | 目录里只有 batch_process、delayed-fsync、fsync-fast-notify、poc-iouring-minimal；且 `batch_process/redis.bpf.c:26,65` 把 redis 路径写死为 `/root/zys/bpftime-evaluation/redis/src/redis-server` |
| `benchmark/trap-uprobe/README.md` | `-DBPFTIME_ENABLE_TRAP_UPROBE=ON` | 只在 riscv 上有效；x86 上被 `cmake/StandardSettings.cmake:79-81` FORCE 成 OFF |

## 8. 对照现有指南

对照了 `07-periphery.md`（7.4 单测及其"细节与坑/上游变更"）、`08-practice.md`（8.2、8.3 实验 6.1–7.1、深入参考）。

### 错误

1. `08-practice.md` 实验 7.1 末尾："完整跑全部用例（`make unit-test`）还需要先编出 `runtime/test/bpf` 下的 `.bpf.o` 文件；按标签跑上面这些不需要"，深入参考里也说"完整跑 `make unit-test` 前要先 `make -C runtime/test/bpf` 并把 `.bpf.o` 拷到 `build/runtime/test/`"。
   → 实际：`bpftime_runtime_tests` 用的 6 个 `.bpf.o` 全部由 CMake 从 `runtime/unit-test/assets/` 现编到 `build/runtime/unit-test/`（`runtime/unit-test/CMakeLists.txt:143-158`），与 `runtime/test/bpf` 无关；`runtime/test` 已废弃且没有任何测试目标（`runtime/test/CMakeLists.txt:11-24`、`runtime/test/README.md:3`）。`make unit-test-runtime` 里那两步是遗留，Ninja 下 `cd build/runtime/test && make` 还会失败（推断）。
2. `07-periphery.md` "细节与坑·单测"："`make unit-test-runtime` 除了跑 `bpftime_runtime_tests`，还会 `make -C runtime/test/bpf` 产出另一批测试用 `.o`，并在 `build/runtime/test` 下跑 `ctest`。" → 描述的是 Makefile 文字，但没说明那批 `.o` 没人用、`build/runtime/test` 下 ctest 没有测试（同上）。容易让读者以为存在第二套测试。
3. `08-practice.md` 8.2.2 说 `ENABLE_PROBE_READ_CHECK/WRITE_CHECK` "默认 ON"——这对 CMake 选项默认值正确（`cmake/StandardSettings.cmake:110,112`），但本机现成的 `build-jit/` 是 OFF。若写作者让读者直接用 `build-jit`，实验 6.3 的"读空指针返回 -EFAULT 的断言会执行"不成立。建议指南明确"以你自己 CMakeCache 为准"（不是指南的错，是交叉核对提醒）。

### 遗漏（学习者应该知道）

- **跑单测前必须设 `BPFTIME_GLOBAL_SHM_NAME`**：不设时 tailcall 等测试会 `SHM_REMOVE_AND_CREATE` 默认名 `bpftime_maps_shm`，`bpftime::shm_remove` 析构也会删它（7.2）。07/08 都让读者直接 `$T "[tag]"`，单独按 tag 跑那几个是安全的，但整套跑或跑 tailcall/map 测试会动默认 shm。
- **整套跑在默认顺序下会因 `Test attach replace with ebpf` 崩溃**（7.1），以及 `Test tail calling from userspace to kernel` 需要 root。指南说 `make unit-test` 会把测试都跑一遍，没提醒这两个。推荐命令：`$T "~Test attach replace with ebpf" "~Test tail calling from userspace to kernel"`（已实跑全过）。
- 36 个用例没有 tag，按名字通配更实用（`"get_stack*"`、`"Bloom*"`、`"*hash map*"` 等，已实跑，见 9.1）。
- 测试二进制里有个**全局 mock attach ctx**（`test_get_stack.cpp:45` 覆盖弱符号），解释了启动时那行 `bpf_attach_ctx constructed`。
- `test_shm_allocation_failure.cpp` + helper 是 `CLAUDE.md` "宿主进程契约"的可执行版本，值得在 03 章或 07 章点名。
- 示例 README 大面积过时（7.6 表），尤其 `bpf_features/` 的相对路径全错、`get_stack_id_example` README 是复制品、hotpatch 两个示例无法按 README 复现。08 章若要引用示例做实验，应给出自己验证过的命令（见 9.2 的 malloc 实跑命令）。
- `runtime/test/` 已废弃——07 章"测试二进制"表没有列它，这是对的，但应一句话说明，免得读者被 `Makefile` 误导。

### 讲得好、可以保留

- 07 的"按名字或 `[tag]` 过滤"、`--list-tests/--list-tags` 用法，实跑 `[software_perf_event]`（3 例）、`[affinity]`（1 例）均为 `All tests passed`，与 08 实验 6.1 的预期一致。
- 07 对 trap/Frida 按架构 FORCE、trap 测试文件被 `#if BPFTIME_HAVE_TRAP_UPROBE_ATTACH` 包住的说明正确（`cmake/StandardSettings.cmake:76-82`、`test_attach_uprobe_with_ebpf_trap.cpp:2`）。
- 07 "attach 类测试依赖现编 eBPF 程序与后端"正确。
- 08 实验 6.1 对三个 `[software_perf_event]` 用例的解释与源码一致（`test_software_perf_event.cpp:157-264,266-321,323-344`）。
- 08 指出 `Makefile:48` 的 `export BPFTIME_VM_NAME=llvm` 无效、`make benchmark` 写死 llvm-15、ssl-nginx README 路径过时——均核实属实。
- 08 的 `make -C benchmark/ssl-nginx` 构建 sslsniff 的说法与 `benchmark/ssl-nginx/Makefile` 一致。

## 9. 学习建议

所有单测命令的公共前缀（**本机已实跑**，以 `build-jit` 为准）：

```bash
cd /home/y/src/bpftime-wt-official-177a1dc/build-jit/runtime/unit-test
export BPFTIME_GLOBAL_SHM_NAME=notes_$RANDOM     # 必须：避免动默认 bpftime_maps_shm
T=./bpftime_runtime_tests
# 跑完：ls /dev/shm ; rm -f /dev/shm/$BPFTIME_GLOBAL_SHM_NAME
```

### 9.1 单测总表

"已实跑"列：逐个用例独立运行（每例一个随机 shm 名），记录结果与耗时（2026-11-01，本机）。

| 测试文件 | TEST_CASE（tag） | 测哪个模块的什么行为 | 学习者能学到什么 | 单独跑（命令里的过滤串） | 已实跑结果 |
|---|---|---|---|---|---|
| `test_config.cpp` | Test bpftime runtime_config；Allow external maps from the environment；Select Frida's fuzzy backtracer from the environment（无 tag） | `runtime_config` setter；`construct_runtime_config_from_env` 解析两个环境变量 | 运行时配置从哪来 | `$T "Test bpftime runtime_config,Allow external maps*,Select Frida*"` | 3 例均过 |
| `test_bpftime_shm_json.cpp` | Test buffer_to_hex_string function；Test hex_string_to_buffer function；… with invalid input；Test bpftime shm json import/export（无 tag，后者 2 个 SECTION） | hex 工具；`bpftime_shm` 的 add_bpf_prog/add_tracepoint/add_bpf_link/add_bpf_map/attach_perf_to_bpf 与 JSON 导入导出 | handler 表里有哪几类对象、fd 如何编号；`bpftimetool export` 的格式来源 | `$T "*hex*"`；`$T "Test bpftime shm json*"` | `*hex*` 3 例过；json 1 例过（7 断言）。**会留下 `/dev/shm/BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT` 和 `/tmp/bpftime_test_shm_json.json`，需手删** |
| `test_get_current_pid_tgid.cpp` | bpf_get_current_pid_tgid reports the calling process after fork | helper 的 pid/tid 缓存在 fork（两层）后刷新 | helper 有进程内缓存；fork 安全 | `$T "bpf_get_current_pid_tgid*"` | 过 |
| `test_get_current_uid_gid.cpp` | bpf_get_current_uid_gid reports current unsigned credentials `[uid_gid]` | 返回值 = gid<<32 \| uid，且不缓存 | 用 `--wrap` 在链接期 mock libc | `$T "[uid_gid]"` | 过（8 断言） |
| `test_get_stack.cpp` | get_stack rejects build IDs / uses byte-sized buffers… / truncates only at complete frame boundaries / rejects a partial frame buffer / applies frame skips before the byte limit / rejects skips beyond the available trace（无 tag） | `bpftime_get_stack`：BUILD_ID→`-ENOTSUP`，按字节计长、整帧截断、skip、出错时清零缓冲 | 用 mock attach impl 隔离测 helper；helper 通过 `call_attach_specific_function("generate_stack")` 向 attach 层要栈 | `$T "get_stack*"` | 6 例过 |
| `test_probe.cpp` | Test bpftime_probe_read；Test repeated valid probe memory access；Test bpf_probe_read_str；Test probe access preserves application SIGSEGV handlers；Test bpftime_probe_write_user；Test Probe read/write size valid or not ；Test default SIGSEGV handler is preserved（无 tag；注意第 6 个名字末尾有空格） | probe_read/write/read_str 的正常路径、负长度 `-EFAULT`；不篡改应用信号处理 | 用户态 probe 的安全语义；`ENABLE_PROBE_*_CHECK` 对断言的影响 | `$T "Test bpftime_probe*,Test Probe*,Test repeated*,Test bpf_probe_read_str,Test probe access*,Test default SIGSEGV*"` | 7 例过（53 断言）。本机 CHECK=OFF，越界断言未编入 |
| `test_epoll.cpp` | Epoll returns each ready registration once and restores signals `[epoll]` | `bpftime_epoll_wait` 对 ringbuf / software perf 源：容量截断、超时、不消费数据、信号掩码还原（2×2×10 组 GENERATE） | loader 侧 epoll 等待的实现约束 | `$T "[epoll]"` | 过（6558 断言，0.38 s） |
| `test_ringbuf_tail_wrap.cpp` | ringbuf keeps tail-straddling records contiguous and consumable `[ringbuf]` | 48 字节记录让第 74 条的头落在 4096 环最后 8 字节，仍连续可读 | ringbuf 物理布局与回绕 | `$T "[ringbuf]"` | 过（1002 断言） |
| `test_software_perf_event.cpp` | Perf output preserves the calling thread's CPU affinity `[perf_event][affinity]`（3 个 SECTION）；Software perf event buffers shard concurrent producers by thread；…records wrap on aligned boundaries after resize；…mmap reports shared memory exhaustion（后三个 `[perf_event][software_perf_event]`） | `bpf_perf_event_output` 不改亲和性；per-thread shard；8 字节对齐；`E2BIG`；`ENOMEM` | perf buffer 的完整语义，第 6 章主干 | `$T "[software_perf_event]"`；`$T "[affinity]"` | 3 例过（8307 断言）；1 例过（38 断言） |
| `test_shm_allocation_failure.cpp` + `assets/shm_allocation_test_helper.c` | 13 例：falls back when startup shared memory is too small；perf mmap reports shared memory exhaustion；keeps logger sink failures off host stdio；keeps default logging off host stdio；Global log level cannot enable default host stdio；File logger is installed before syscall startup；preserves the console logger level name；Agent preload keeps missing shared memory off host stdio；removes owned shared memory on normal exit；preserves shared memory it only opened；keeps shared memory while an agent pid is alive；…while a peer server pid is alive；mock files do not leave tmp entries（tag 组合自 `[allocation][syscall_server][logging][cleanup][agent]`） | syscall-server/agent 以 `LD_PRELOAD` 注入宿主时的失败回落、stdio 洁净、shm 所有权、`/sys/bus/event_source/...` mock 文件 | 宿主进程契约；syscall-server 何时删 shm | `$T "[allocation]"`（或 `[logging]`/`[cleanup]`） | 13 例过（87 断言）；`Agent preload…` 单例约 3 s（agent 等 shm 重试）。会在 `/tmp` 留 `bpftime-shm-*-<pid>.lock` |
| `attach_with_ebpf/test_helpers.cpp` | Test helpers | 打开 `helpers.bpf.o`、kernel helper 组、load、exec 返回 23 | 最短的"加载+执行" | `$T "Test helpers"` | 过 |
| `attach_with_ebpf/test_ufunc_register.cpp` | Test ufunc register | 按 id 注册宿主函数，解释器与 JIT 都返回 17 | ufunc（helper 1000）机制；load(false/true) | `$T "Test ufunc register"` | 过 |
| `attach_with_ebpf/test_attach_uprobe_with_ebpf.cpp` | Test probing internal functions（2 SECTION：内部函数、libc strdup） | Frida uprobe/uretprobe 多次挂同一地址、`bpf_get_func_ip`、detach | attach impl 的最小用法 | `$T "Test probing internal functions"` | 过（35 断言） |
| `attach_with_ebpf/test_attach_filter_with_ebpf.cpp` | Test attaching filter program with ebpf, and reverting（2 SECTION） | override 回调里 `bpf_set_retval` 跳过原函数；自定义 raw helper 40；按 id / 按地址 detach | "过滤/错误注入"的实现路径 | `$T "Test attaching filter*"` | 过（34 断言） |
| `attach_with_ebpf/test_attach_replace.cpp` | Test attach replace with ebpf | override+ufunc 整体替换函数（返回 100），detach 恢复 | 函数替换（UREPLACE 的原型） | 必须让 Frida 先初始化：`$T "Test probing internal functions,Test attach replace with ebpf" --order rand --rng-seed 3` | **单独跑 SIGSEGV（3/3 次）**；按上面命令 2 例过（44 断言）。见 7.1 |
| `attach_with_ebpf/test_shm_progs_attach.cpp` | Test shm progs attach | 手工在 shm 造 handler 表 → `init_attach_ctx_from_handlers` → fork 子进程重装 | agent 从 shm 装配 hook 的缩影 | `$T "Test shm progs attach"` | 过（8 断言）。留下 `/dev/shm/my_shm_attach_test`（7.2） |
| `attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp` | Test probing internal functions with the trap backend；Trap BPF admission rejects unaudited helper implementations；Trap runtime links reject unsafe programs before patching code；Trap BPF execution cannot lazily compile an unprepared program | trap 后端；信号安全 helper 准入；未预编译拒绝 | `CLAUDE.md` 里 SIGTRAP 契约的测试 | x86 上不存在（`--list-tests` 中无这 4 例） | 未跑（需 riscv64 / trap 构建） |
| `tailcall/test_user_to_user_tailcall.cpp` | Test tail calling from userspace to userspace | PROG_ARRAY + helper 12；目标关闭后槽位清空 | 全局 shm C API 的完整小例子 | `$T "*tail calling from userspace to userspace"` | 过（8 断言） |
| `tailcall/test_user_to_kernel_tailcall.cpp` | Test tail calling from userspace to kernel | 用户态程序 tail call 进内核程序，内核 map 被更新 | 用户态↔内核混合 | `sudo` 下才可能过（未跑 root） | **非 root 失败**：`REQUIRE(map_fd >= 0)` → `-1 >= 0` |
| `cuda/test_cuda_compile.cpp` | Test cuda compile；CUDA watcher stops before shared-memory teardown | CUDA 构建专用 | — | 本构建不含 | 未跑 |
| `maps/test_per_cpu_array.cpp` | Test basic operations of array map（2 SECTION） | per-CPU array 按 CPU 写入、用户态整行读出 | per-CPU 在用户态的模拟方式 | `$T "Test basic operations of array map"` | 过（385 断言） |
| `maps/test_per_cpu_hash.cpp` | Test basic operations of hash map（2 SECTION） | per-CPU hash 同上（随机 key，种子来自 Catch rng） | 同上 | `$T "Test basic operations of hash map"` | 过（3265 断言） |
| `maps/test_shm_hash_maps.cpp` | Test shm hash maps with sub process；Hash maps with colliding public names remain independent `[maps][map_name]` | 父子进程经 shm 共享 handler 表与 map；同名 map 独立、共享 impl 的引用计数、`clear_all` 后内存完全归还 | **跨进程共享的核心证据** | `$T "Test shm hash maps*,Hash maps with colliding*"` | 2 例过。前者留下 `/dev/shm/my_shm_maps_test` |
| `maps/test_external_map_ops.cpp` | Test basic operations of external hash map ops | `bpftime_register_map_ops` 外部 map 类型 | 扩展点：自定义 map | `$T "*external hash map ops"` | 过。留下 `/dev/shm/BPFTIME_TEST_ENTERNAL_MAP_SHM` |
| `maps/test_bpftime_hash_map.cpp` | bpftime_hash_map basic operations `[bpftime_hash_map]` | 底层哈希表 | — | `$T "[bpftime_hash_map]"` | 过（80 断言）；`open_or_create` 的段不删除 |
| `maps/test_fix_hash_map.cpp` | Fixed hash iteration scans adjusted bucket count `[maps][hash][iteration]` | 素数桶数下迭代覆盖最后一桶 | — | `$T "[iteration]"` | 过 |
| `maps/test_map_in_map_bounds.cpp` | Array-of-maps rejects out-of-range lookup keys `[maps][map_in_map]` | 越界返回 ENOENT | — | `$T "[map_in_map]"` | 过 |
| `maps/kernel_unit_tests.cpp` | test_hashmap / test_hashmap_sizes / test_hashmap_percpu / test_hashmap_walk / test_arraymap / test_arraymap_percpu / test_arraymap_percpu_many_keys / test_map_large `[large_memory]` / test_map_stress / test_map_parallel `[large_memory]` / test_map_rdonly / test_map_wronly（全部 `[kernel]`） | 与内核 map 语义对齐：flags、errno、E2BIG、迭代、RDONLY/WRONLY、并发 | bpftime map 与内核行为的对照表 | `$T "[kernel]"`；避开大内存：`$T "[kernel]~[large_memory]"` | 12 例过（8,378,168 断言）；最慢 test_map_parallel 8.5 s、test_map_large 1.6 s；test_hashmap_sizes 0 断言 |
| `maps/test_stack_trace_map.cpp` | Test stack trace map 1；Test stack trace map 2 | 填栈得 id、删除；哈希碰撞时 REUSE/替换 | `bpf_get_stackid` 背后的 map | `$T "Test stack trace map*"` | 2 例过 |
| `maps/test_queue_map.cpp` | Queue Map Constructor Validation / Core Operations / Conceptual Concurrency Test / Kernel Style Test / Standard Interface Kernel Style（`[queue_map]` + 子 tag） | FIFO、BPF_EXIST 覆盖、push/peek/pop 与标准接口映射 | — | `$T "[queue_map]"` | 5 例过（289 断言） |
| `maps/test_stack_map.cpp` | Stack Map 同上 5 例（`[stack_map]`） | LIFO | — | `$T "[stack_map]"` | 5 例过（310 断言） |
| `maps/test_bloom_filter_map.cpp` | Bloom Filter Constructor Validation / Basic Operations / False Positive Test / Different Hash Counts / Hash Algorithm Benchmark / Hash Distribution Quality / Large Scale False Positive Test / Extreme Load Test（`[bloom_filter]` + 子 tag） | 无假阴性、假阳性率、DJB2 vs JHASH | — | `$T "[bloom_filter]"` | 8 例过（32,111 断言） |
| `maps/test_lpm_trie_map.cpp` | TLPM Basic Operations；TLPM Order Independence；LPM Trie Map vs TLPM Comparison；LPM Trie IP Address Operations / Deletion Operations / Update Flags / Full Map / Multi-thread / String Iteration（`[lpm_trie]`）；LPM Trie Map Constructor Validation / IPv4 Operations / Longest Prefix Match（`[lpm_trie_map]`） | 与参考实现对拍 | 对拍式测试写法 | `$T "[lpm_trie],[lpm_trie_map]"` | `[lpm_trie]` 9 例过（31,876 断言）；`[lpm_trie_map]` 3 例逐个跑均过 |
| `maps/test_lru_hash_map.cpp` | LRU Hash Map Basic Operations / Update Flags / Capacity Limits / Iteration（`[lru_hash]`） | 淘汰与 flags | — | `$T "[lru_hash]"` | 4 例过 |
| `maps/test_lru_var_hash_map.cpp` | Test basic lru map operations | 未访问 key 被淘汰 | — | `$T "Test basic lru map operations"` | 过（4017 断言） |

整套（推荐的安全写法）：`$T "~Test attach replace with ebpf" "~Test tail calling from userspace to kernel"` → **已实跑 `All tests passed (8380811 assertions in 104 test cases)`**，约 15 s。
不加排除直接 `$T`：在第 2 个用例 SIGSEGV 退出（rc=139），后面全部没跑（已实跑）。

### 9.2 示例总表

公共前提：本机没有安装 `~/.bpftime`，也没有构建 `bpftime` CLI（`build-jit/tools/cli/` 下无二进制）；`clang` 只有 `/usr/lib/llvm-19/bin/clang`。示例 Makefile 默认会在示例目录下生成 `.output/` 并现编 libbpf/bpftool（会写官方树），所以我只在 scratch 里手工构建了两个（malloc、benchmark/uprobe）。"README 命令核对"列：✅=与当前代码一致，⚠=能用但有偏差，❌=按 README 无法复现。

| 示例目录 | 演示 bpftime 的什么能力 | attach 类型 / map / helper | 构建与运行（README 命令 → 核对） | 适合指南哪一章当实验 |
|---|---|---|---|---|
| `minimal/`（uprobe） | 最小用户态 uprobe，无 map | `SEC("uprobe/./victim:target_func")`；`bpf_printk` | `make -C example/minimal`；server `LD_PRELOAD=~/.bpftime/libbpftime-syscall-server.so ./uprobe`，client `LD_PRELOAD=~/.bpftime/libbpftime-agent.so ./victim`。⚠ 命令可用，但 README 正文讲解的是另一个程序（7.6） | 第 1 章（最小两进程模型）、第 4 章 |
| `minimal/`（syscall） | 系统调用 tracepoint（需 text segment transformer） | `tracepoint/syscalls/sys_exit_write`；`bpf_printk` | README 用 `sudo bpftime load …` / `bpftime start -s …` 或 `AGENT_SO=… LD_PRELOAD=…/libbpftime-agent-transformer.so`。✅ 路径与 `build/attach/text_segment_transformer/` 产物一致（`build-jit` 中存在该 .so）；sudo 非必需（推断，未实跑） | 第 4 章（syscall trace 与 Frida uprobe 的区别） |
| `minimal/`（uprobe-override） | `bpf_override_return` 改返回值 | 私有 perf type 1008 + `BPF_MODIFY_RETURN`（`attach_override.h`）；helper 58 | 同 uprobe 模式。⚠ README 无单独运行说明 | 第 4、6 章 |
| `minimal/usdt_minimal/` | USDT | `SEC("usdt")` + `BPF_USDT`（依赖 libbpf usdt spec map、`bpf_get_attach_cookie`=174，runtime 有实现 `bpf_helper.cpp:662`） | README 只有一句说明，无命令；victim 需 `sys/sdt.h`（systemtap-sdt-dev）。⚠ | 第 4 章选做 |
| `malloc/` | uprobe libc + hash map 计数，loader 周期读 map | `uprobe/libc.so.6:malloc`；HASH；`bpf_get_current_pid_tgid`、`bpf_map_*`、`bpf_printk` | README：`LD_PRELOAD=build/runtime/syscall-server/… example/malloc/malloc` / `LD_PRELOAD=build/runtime/agent/… example/malloc/victim`。✅ 命令对（正文讲解过时）。**已实跑**（scratch 手工构建，非 root）：loader 每秒打印 `pid=419326  malloc calls: 10`，victim 端打印 `malloc called from pid 419326` | **第 1 章锚点之外最好的入门实验**；第 2 章（看 shm）、第 6 章（hash map） |
| `error-inject/` | 用户函数与系统调用上的错误注入（随机一半） | uprobe override（type 1008）；`tracepoint/syscalls/sys_enter_write` + override；`bpf_get_prandom_u32`、`bpf_override_return` | `LD_PRELOAD=~/.bpftime/libbpftime-syscall-server.so ./error_inject` / agent `./victim`。✅（README 未写 syscall 版本的运行方式） | 第 4 章（override 回调）、第 6 章 |
| `tracing/bashreadline/` | uretprobe 读返回字符串 | `uretprobe//bin/bash:readline`；PERF_EVENT_ARRAY；`bpf_probe_read_user_str` | README 只有一句链接 bcc。⚠ 需按 malloc 模式自行组合 | 第 6 章（perf 输出） |
| `tracing/funclatency/` | 函数延迟直方图 | 源里写 `kprobe/dummy_*`，loader 用 `bpf_program__attach_uprobe` 改挂到用户函数（`funclatency.c:515-556`）；HASH；`bpf_ktime_get_ns` | 无 README。⚠ | 第 4 章（SEC 名与实际 attach 无关的例子） |
| `tracing/goroutine/` | 追踪 Go 运行时 `runtime.casgstatus`；`bpftime attach` 远程注入 | `uprobe/./go-server-http/main:runtime.casgstatus`；RINGBUF；`bpf_probe_read_user` | README 的 LD_PRELOAD 路径 `../../build` 过时（应 `../../../build`）；README 自己说 Go 程序 LD_PRELOAD 不生效、要用 `bpftime attach`（需 ptrace 权限）。❌/⚠ | 第 3 章（attach 注入模式） |
| `tracing/mountsnoop/` | mount/umount 系统调用 | 4 个 syscalls tracepoint；RINGBUF+HASH；`bpf_get_current_task`（用户态语义需核对） | 无 README。⚠ | 第 4 章 syscall trace |
| `tracing/opensnoop/` | open/openat 系统调用 | 4 个 tracepoint；PERF_EVENT_ARRAY+HASH；`bpf_get_stack` | README 一句链接。⚠（根 README 误称 ringbuf） | 第 6 章（perf 与 get_stack） |
| `tracing/opensnoop_ring_buf/` | 同上但用 ringbuf | RINGBUF | README 路径 `example/opensnoop/…` 过时 ⚠ | 第 6 章（ringbuf vs perf 对照实验） |
| `tracing/sigsnoop/` | kill/tkill/tgkill | 6 个 tracepoint；PERF_EVENT_ARRAY+HASH | 无 README ⚠ | 第 4 章 |
| `tracing/sslsniff/` | **全书锚点**：`SSL_read/SSL_write/do_handshake` 明文抓取 | `uprobe/…`、`uretprobe/…`（loader 用 `bpf_program__attach_uprobe_opts` 按库路径挂）；PERF_EVENT_ARRAY、PERCPU_ARRAY、HASH | README 命令断行、路径 `example/sslsniff` 过时 ❌；以 08 章 8.2.4 的命令为准（`make -C benchmark/ssl-nginx` 构建） | 第 1 章（主线）；第 6 章 |
| `tracing/statsnoop/` | stat 系列系统调用 | 5 对 tracepoint；PERF_EVENT_ARRAY+HASH | README 链接错 ⚠ | 第 4 章 |
| `tracing/syscount/` | 按系统调用计数 | `raw_syscalls/sys_enter|sys_exit`；HASH ×2 | README 一句链接 ⚠ | 第 4 章；benchmark/syscount-nginx 的工具本体 |
| `tracing/bpftrace/` | 用户态跑 bpftrace（10 个 .bt） | uprobe / tracepoint（由 bpftrace 生成） | `sudo ~/.bpftime/bpftime load bpftrace -e …` / `bpftime start [-s] …`。⚠ 需要 bpftrace 和已安装的 CLI；README 说测过的 bpftrace 是 0.9.4 与某 commit | 第 3 章（syscall-server 兼容性） |
| `bpf_features/bloom_filter_demo/` | BLOOM_FILTER map | `uprobe/./target:{user_access,admin_operation,system_event}`；BLOOM_FILTER+ARRAY | README/脚本 `../../build` 路径错 ❌；VM 说法自相矛盾 | 第 6 章（map 类型扩展） |
| `bpf_features/get_stack_id_example/` | `bpf_get_stackid` + STACK_TRACE map | `uprobe/libc.so.6:malloc|free`；STACK_TRACE+RINGBUF | README 是 malloc 的复制品 ❌ | 第 6 章（与单测 "Test stack trace map*" 对照） |
| `bpf_features/lpm_trie_demo/` | LPM_TRIE 前缀匹配做访问控制 | `uprobe/./file_access_target:test_file_access`；LPM_TRIE+QUEUE+ARRAY；`bpf_map_push_elem`；Makefile 用 `-mllvm -bpf-stack-size=8192`（超出内核 512 栈） | 脚本路径 `../../build` 错；README 脚本名拼错 ❌/⚠ | 第 6 章；可做"用户态不受内核栈限制"的观察 |
| `bpf_features/queue_demo/` | QUEUE / STACK map | `uprobe/./target:{target_function,secondary_function}`；QUEUE、STACK、ARRAY | README/脚本 `../../build` 路径错、脚本提示路径旧 ❌ | 第 6 章（对照 `[queue_map]`/`[stack_map]` 单测） |
| `bpf_features/tailcall_minimal/` | 用户态 uprobe 程序 tail call 进**内核**程序 | `uprobe/./victim:add_func`；PROG_ARRAY；`bpf_tail_call` | `bpftime load ./tailcall_minimal` / `bpftime start ./victim`；⚠ 实际需 root（loader 直接 `syscall(__NR_bpf)` 进内核，`tailcall_minimal.c:113-178`），看 `trace_pipe` | 第 6 章选做（对照单测 `Test tail calling from userspace to kernel`） |
| `hotpatch/redis/` | 热补丁 redis CVE（override + ufunc 调宿主函数） | `uprobe/redis-server:xgroupCommand`；`bpf_override_return`、`UFUNC_CALL_NAME_2` | ❌ README 依赖不存在的 tar.gz、`workloads/`、`bpftime-cli … poc.json`；loader 的 attach 代码被注释 | 只读代码：第 4 章（UREPLACE/override 思想） |
| `hotpatch/vim/` | 热补丁 vim | `uprobe/vim:openscript`；ufunc `check_secure` | ❌ 无 Makefile、无 json、无预编译 vim | 只读 |
| `xdp-counter/` | XDP 程序在用户态 runtime 加载（配 DPDK/AF_XDP 驱动） | `SEC("xdp")`；ARRAY；自定义 `base.btf` 做用户态 `xdp_md` 重定位（`runtime/extension/userspace_xdp.h` 存在 ✅） | `LD_PRELOAD=…syscall-server.so example/xdp-counter/xdp-counter .output/xdp-counter.bpf.o veth1 base.btf`；参数顺序与 `xdp-counter.c:16-30` 一致 ✅；创建 veth 需 root（未跑） | 第 3 章（非 uprobe 程序类型如何被 syscall-server 接住） |
| `libbpftime_example/` | 把 bpftime 当库：初始化全局 shm、JSON 导入导出 | 无 attach；`bpftime_import/export_global_shm_*_json`、`bpftime_remove_global_shm`（均存在于 `runtime/include/bpftime_shm.hpp:230-240`） | 需 `-DBPFTIME_BUILD_STATIC_LIB=ON` 并 install 到 `~/.bpftime`；`ebpf.json` 的 `attach_fds:[2,3]` 指向不存在的 perf fd（导入时行为未验证）⚠ | 第 2 章（handler 表 JSON；对照单测 `Test bpftime shm json*`） |
| `otel-ebpf-profiler/` | daemon 镜像 cilium/ebpf 的 `BPF_LINK_CREATE` perf link（兼容性验证，程序实际跑在内核） | uprobe `libc:malloc/free`（由上游 profiler 生成） | 需 root、需 daemon（`BUILD_BPFTIME_DAEMON` 默认 ON 但本机 build-jit 关）、需 Go 1.25 构建上游。✅ 脚本自身用独立 `BPFTIME_GLOBAL_SHM_NAME` 并清理 | 第 7 章（daemon） |
| `gpu/`、`attach_implementation/` | （由别人负责，仅列出） | — | — | — |

**本机已实跑的 malloc 命令**（不需要 root，不写官方树）：

```bash
R=/home/y/src/bpftime-wt-official-177a1dc
W=$SCRATCH/malloc; mkdir -p $W && cd $W
CL=/usr/lib/llvm-19/bin/clang
BT=$R/example/tracing/sslsniff/.output/bpftool/bootstrap/bpftool   # 树里现成的 bpftool（别人构建的）
$CL -g -O2 -target bpf -D__TARGET_ARCH_x86 -I$R/build-jit/libbpf -I$R/third_party/libbpf/include/uapi \
    -I$R/third_party/vmlinux/x86 -c $R/example/malloc/malloc.bpf.c -o malloc.tmp.bpf.o
$BT gen object malloc.bpf.o malloc.tmp.bpf.o && $BT gen skeleton malloc.bpf.o > malloc.skel.h
gcc -g -I. -I$R/build-jit/libbpf -I$R/third_party/libbpf/include/uapi $R/example/malloc/malloc.c \
    $R/build-jit/libbpf/libbpf.a -lelf -lz -o malloc
gcc -g $R/example/malloc/victim.c -o victim
export BPFTIME_GLOBAL_SHM_NAME=notes_$RANDOM
BPFTIME_LOG_OUTPUT=$W/server.log LD_PRELOAD=$R/build-jit/runtime/syscall-server/libbpftime-syscall-server.so ./malloc &
sleep 2
LD_PRELOAD=$R/build-jit/runtime/agent/libbpftime-agent.so ./victim
```

关键输出（已实跑）：loader `pid=419326  malloc calls: 10`（每秒一行）；victim 侧 `malloc called from pid 419326`；`server.log` 里有 `Attach successfully`、`Executable path: …/victim`。
附带观察：我用 `timeout 4 ./victim` 包了一层，`LD_PRELOAD` 被 `timeout` 进程本身继承，agent 也注入了 `timeout`，于是出现一行 `pid=419323 malloc calls: 425`——这是教学上很好的"LD_PRELOAD 会传染给子进程/包装进程"的例子。

### 9.3 benchmark 总表

| 目录 | 测什么 | 怎么跑（README / 脚本） | README 与当前代码一致吗 |
|---|---|---|---|
| `benchmark/`（根：test.c、test_embed.c、CMakeLists、Makefile） | `test.c`：17 个 `__bench_*` 空函数的调用耗时（被 uprobe 后测 hook 开销）；`test_embed.c`：不 hook，直接把 `__bench_uprobe` 程序 JIT 后在进程内调用 | `make -C benchmark` 编 test；`simple-benchmark-with-embed-ebpf-calling <uprobe elf> <uretprobe elf>`（CMake 产物 `build/benchmark/`） | ⚠ 根 README 的 "make benchmark / make run-all-benchmark" 存在（`Makefile:82,105`），但写死 llvm-15；`make -C benchmark` 会进 gpu/ |
| `benchmark/uprobe/` | 内核 vs 用户态 uprobe/uretprobe、probe_read/write、4 种 map 的 lookup/update/delete | `python3 benchmark/uprobe/benchmark.py`（会 sudo 跑内核组）；手工：server `LD_PRELOAD=…syscall-server.so benchmark/uprobe/uprobe`，client `LD_PRELOAD=…agent.so benchmark/test`（SEC 里是相对路径 `benchmark/test`，必须在仓库根运行） | ⚠ README 基线函数名 `__benchmark_test_function3`、`make -C benchmark/uretprobe` 过时；脚本按 `cwd.name=="bpftime"` 拼路径。**已实跑用户态部分**（scratch 里重建 `benchmark/test` 相对布局，非 root）：`__bench_uprobe` 124.6 ns、`__bench_uretprobe` 182.1 ns、`__bench_uprobe_uretprobe` 211.5 ns、`__bench_read`（1000 次 probe_read）1703 ns、`__bench_write` 1510 ns、`__bench_array_map_lookup`（1000 次）5802 ns、`__bench_hash_map_lookup` 20978 ns、`__bench_hash_map_delete` 100825 ns；未 hook 基线约 1.1–1.7 ns。embed：`avg function elapse time: 62.9 ns` |
| `benchmark/syscall/` | 系统调用 tracepoint 开销（read/write 出口） | `sudo python benchmark/syscall/benchmark.py`；手工 `bpftime load`/`start -s` 或 `AGENT_SO=… LD_PRELOAD=…transformer.so` | ✅ 路径与产物一致（脚本 `:15-19`）；需 sudo（内核组） |
| `benchmark/mpk/` | 开/关 MPK 时的 uprobe/map 开销对比 | `build.sh` 构建 `build/` 与 `build-mpk/`，再 `python benchmark/mpk/benchmark.py` | ⚠ build.sh 写死 llvm-15；`benchmark.py:17-22` 同样依赖目录名 `bpftime`；需要支持 PKU 的 Intel CPU |
| `benchmark/ssl-nginx/` | sslsniff 对 nginx HTTPS 吞吐的影响（内核 vs 用户态） | `make -C benchmark/ssl-nginx`；`python3 benchmark/ssl-nginx/benchmark.py`；需 nginx + wrk | ⚠ README 的"一键脚本"写成 draw_figture.py，sslsniff 路径旧；Makefile 用新路径 ✅（本机无 nginx，未跑） |
| `benchmark/syscount-nginx/` | syscount 对 nginx 的影响（5 种配置） | `python3 benchmark/syscount-nginx/benchmark.py`；需 nginx + wrk | ✅（未跑） |
| `benchmark/deepflow/` | DeepFlow 在内核/用户态 uprobe 下的 Go HTTP(S) 服务吞吐 | `run.py -t {no-probe,kernel-uprobe,no-uprobe,user-uprobe}`；需 deepflow 的 rust_example、`bpftime attach`、wrk、root | ⚠ README 自称 TODO，依赖外部仓库（未跑） |
| `benchmark/fuse/` | 用 syscall tracepoint 缓存 FUSE 元数据（getdents64/newfstatat/open 等），对 passthrough FS 加速 | `make -C benchmark/fuse`（需 libfuse）；`sudo …passthrough`；`sudo BPFTIME_SHM_MEMORY_MB=1024 LD_PRELOAD=…syscall-server.so benchmark/fuse/bpf/fs-cache`；agent 用 transformer | ✅ 路径与产物一致；需 root 挂 FUSE（未跑） |
| `benchmark/redis-durability-tuning/` | 用 uprobe+override（及 iouring 扩展 helper 1006–1010）改变 redis AOF fsync 策略 | `make redis`（clone eunomia-bpf/redis）、`make build`、`benchmark.py` | ⚠ README 提到不存在的 `bpf-sync-kernel`；`batch_process/redis.bpf.c:26,65` 写死 `/root/zys/...` 路径；`poc-iouring-minimal` 需 `BPFTIME_ENABLE_IOURING_EXT`（未跑） |
| `benchmark/trap-uprobe/` | trap 后端每次命中延迟（riscv64） | riscv 交叉构建 `trap-uprobe-bench`，`qemu-riscv64 …` 或 `benchmark.py --qemu` | ✅ `cmake/riscv64-toolchain.cmake` 存在；x86 上该目标不会生成（FORCE OFF）；`Makefile` 只是给 `make -C benchmark` 的空壳 |
| `benchmark/gpu/` | （只列出）CUDA map/host 性能 | — | — |

### 9.4 "活文档" Top 10（按学习顺序）

1. **`example/malloc/`**（`malloc.bpf.c` 41 行 + `malloc.c` 119 行）——先亲眼看到"loader 进程 + 目标进程 + 共享内存"三件套跑起来（9.2 的命令已实跑），再去读源码。对应第 0/1 章。
2. **`attach_with_ebpf/test_helpers.cpp` + `assets/helpers.bpf.c`**——不涉及 shm、不涉及 attach 的最短"打开→加 helper→load→exec"。对应第 5 章。
3. **`maps/test_shm_hash_maps.cpp`**——shm 里构造 `handler_manager`，fork 出的子进程直接读到父进程写的 map；第二个用例展示同名 map 独立、引用计数共享与内存完全回收。对应第 2 章。
4. **`test_bpftime_shm_json.cpp`**——一次性看到 prog / tracepoint / link / map 四类 handler 如何写进 shm、fd 怎么编号、导出的 JSON 长什么样。对应第 2 章、第 7 章 bpftimetool。
5. **`attach_with_ebpf/test_attach_uprobe_with_ebpf.cpp`**——attach impl 只认"地址+回调"；uprobe/uretprobe、同址多挂、`bpf_get_func_ip`。对应第 4 章。
6. **`attach_with_ebpf/test_attach_filter_with_ebpf.cpp`（配 `example/error-inject/`）**——override 如何让原函数不执行并改返回值；自定义 raw helper。对应第 4、6 章。
7. **`attach_with_ebpf/test_shm_progs_attach.cpp`**——agent 从 shm handler 表装配 hook 的缩影（prog → perf_event → link → `init_attach_ctx_from_handlers`），并在 fork 子进程里重做一遍。读时注意它的断言很弱（7.3）。对应第 3 章。
8. **`tailcall/test_user_to_user_tailcall.cpp`**——用全局 shm C API（`bpftime_maps_create`/`bpftime_progs_create`/`bpftime_map_update_elem`/`bpftime_close`）拼出完整小场景，顺带理解 helper 12。对应第 2、6 章。
9. **`test_software_perf_event.cpp` → `test_ringbuf_tail_wrap.cpp` → `test_epoll.cpp`**——事件输出通道三件套：perf 分片与 8 字节对齐、ringbuf 回绕、loader 侧 epoll。对应第 6 章（可接 08 章实验 6.1/6.2）。
10. **`test_shm_allocation_failure.cpp` + `assets/shm_allocation_test_helper.c`**——把 `CLAUDE.md` 的"注入代码是宿主的客人"写成了 13 个可执行断言：失败回落、不写 stdio、shm 所有权与清理。对应第 3 章收尾或第 7 章。

（进阶加读：`maps/kernel_unit_tests.cpp` 作为"bpftime map 与内核 errno 语义对照表"；`benchmark/uprobe/` 作为量化实验。）

### 9.5 适合学习者的动手实验（均已实跑，除注明）

1. **整套单测的安全跑法**：见 9.1 末尾命令。预期 `All tests passed (8380811 assertions in 104 test cases)`；再不加排除跑一次，观察 rc=139 停在第 2 个用例——引出"测试共享进程全局状态/顺序依赖"的讨论（7.1）。
2. **复现并理解顺序依赖**：`$T "Test probing internal functions,Test attach replace with ebpf" --order rand --rng-seed 1`（rc=139）对比 `--rng-seed 3`（2 例过）。让学习者去 `frida_uprobe_attach_impl.cpp:40` 找 `gum_init_embedded`。
3. **观察 shm 泄漏/误删**：先 `ls /dev/shm`，跑 `$T "Test shm progs attach"`，再 `ls /dev/shm` 看到多出 `my_shm_attach_test`；读 `handler_manager.hpp:75-84` 解释原因。
4. **malloc 两进程实验 + LD_PRELOAD 传染**：9.2 的命令；把 victim 用 `timeout` 包一层，观察 loader 多出一个 pid（`timeout` 进程本身被注入）。
5. **用户态 uprobe 开销量化**：在 scratch 里建 `benchmark/test` + `benchmark/uprobe/uprobe` 的相对布局（SEC 用相对路径），先跑无 hook 的 `./benchmark/test`（每次 ~1.2 ns），再带 agent 跑（`__bench_uprobe` ~125 ns）；再跑 `build-jit/benchmark/simple-benchmark-with-embed-ebpf-calling uprobe.bpf.o uprobe.bpf.o`（~63 ns），讨论 hook 开销 vs 纯 JIT 执行。
6. **按 tag/通配跑**：`$T --list-tags`（本机 50 个 tag）；`$T "get_stack*"`（6 例）、`$T "*hash map*"`（8 例）、`$T "[kernel]~[large_memory]"`。

### 9.6 学习者最容易误解的点

1. **以为 `runtime/test/` 是第二套测试**：它已废弃、什么都不编；`make unit-test-runtime` 里相关两步是遗留。
2. **以为单测都是自包含的**：很多用例共享进程全局状态（全局 shm、Frida 初始化、mock attach ctx），顺序会影响结果；而且默认会动 `bpftime_maps_shm`。
3. **以为 `shm_remove` 就是"删这个名字"**：`::shm_remove` 和 `bpftime::shm_remove` 析构语义不同。
4. **照抄示例 README**：大部分 tracing/bpf_features/hotpatch 的 README 路径或内容已过时；`SEC("uprobe/./victim:…")` 这类相对路径要求在示例目录运行，`benchmark/uprobe` 要求在仓库根运行。
5. **以为 SEC 名决定 attach 点**：funclatency 的 SEC 写 `kprobe/dummy_kprobe`，实际由 loader 用 `attach_uprobe` 挂到用户函数；redis 热补丁 loader 甚至根本不 attach。

## 10. 未读完/未确定的地方

- **示例 loader 与脚本只做了结构性阅读**：`tracing/*/` 下各 `*.c` loader（opensnoop.c 330 行、syscount.c 481 行 + syscall_helpers.c 850 行、funclatency.c 663 行、sslsniff.c 521 行、mountsnoop.c、sigsnoop.c、statsnoop.c、goroutine.c、readline.c）、`bpf_features/*` 的 loader 与 `run_*.sh`、`benchmark/*/benchmark.py`（共约 4,000 行 Python）以 grep 关键路径/attach 调用/SEC/map/helper 的方式核对，没有逐行精读。表中对它们的结论限于"做什么、用什么、命令是否对得上"。
- `maps/test_lpm_trie_map.cpp`（1355 行）、`test_bloom_filter_map.cpp`（758 行）、`test_queue_map.cpp`/`test_stack_map.cpp` 只精读了结构、夹具与并发部分，未逐断言核对每个期望值（R2-maps 应有更细的核对）。
- 未用 root 跑：`Test tail calling from userspace to kernel`、tailcall_minimal、xdp-counter、syscall tracepoint 类示例（`start -s`）、所有 nginx/wrk/FUSE/redis/deepflow benchmark、otel 示例。
- `Test attach replace with ebpf` 在 CI（Debug 构建）是否同样崩溃未验证；崩溃点"gum 未初始化"是基于顺序实验和源码的推断，没用 gdb 取栈（本机无 gdb）。
- `test_shm_hash_maps.cpp:103` 的 `shm_remove` 解析到 `bpftime::shm_remove` 是推断（依据：未 include `common_def.hpp` + 实跑后 `my_shm_maps_test` 残留）。
- `libbpftime_example/ebpf.json` 的 `attach_fds:[2,3]` 指向不存在的 perf fd，导入时是成功还是失败未验证（`bpftime_shm::add_bpf_prog_attach_target` 只检查 prog fd，`runtime/src/bpftime_shm_internal.cpp:430-445`）。
- `make -C example`、`make -C benchmark` 会进入 `gpu/` 而失败、Ninja 下 `unit-test-runtime` 最后一步失败——均为推断，未实跑（避免在官方树产生构建产物）。
- 需要别的读者确认的跨模块点：（1）R5/attach：`find_function_addr_by_name` 在 gum 未初始化时的行为、以及是否应在 `bpftime_ufunc_resolve_from_info` 前保证初始化；（2）R2/maps：`queue_map_impl`/`stack_map_impl` 是否真有内部锁（测试注释声称有，但测试外加了锁）；（3）R3/agent：agent 从 shm config 切换日志路径（`agent.cpp:957-958`）的时机；（4）R7/periphery：`Makefile:46-50` 的遗留步骤与 CI 的实际单测入口差异。

清理说明：所有运行均设置了 `BPFTIME_GLOBAL_SHM_NAME=notes_R8_*`；我产生的 `/dev/shm/BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT`、`/dev/shm/my_shm_attach_test`、`/tmp/bpftime_test_shm_json.json` 及我进程号对应的 `/tmp/bpftime-shm-*.lock` 已删除。`/dev/shm` 中 `bpftime_hash_map_test`、`BPFTIME_TEST_ENTERNAL_MAP_SHM`、`my_shm_maps_test` 在我开始前就已存在（他人遗留，被我的运行覆写），按原状保留。scratch 构建产物在 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R8/`。
