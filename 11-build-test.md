# 11. 构建、测试与示例地图

> 一句话：bpftime 怎么构建、编出来的每个文件是干什么的、单测怎么安全地跑、仓库里哪些示例和测试值得当学习材料。
> 读完你能回答：哪个 CMake 开关真的有用、哪个是摆设；`build/` 下那一堆 `.so` 谁用谁；
> 为什么跑单测前一定要换 shm 名；哪些示例 README 已经过时、该看什么代替。
>
> 前置：读过[第 0 章](00-architecture.md)即可；单测和示例部分会引用后面各章。预计用时：1 小时（查阅为主）。
> 本章是一张"地图"，大部分内容是表格，按需翻。

## 本章要回答的问题

1. 从零编一个能跑本书实验的 bpftime，最少要什么命令？哪些开关会改变实验结果？
2. 传了 `-DBPFTIME_ENABLE_FRIDA=OFF` 为什么没用？哪些选项根本没人读？
3. 编完后 `build/` 里的几个 `.so` 和可执行文件分别给谁用？
4. 为什么跑 `bpftime_runtime_tests` 之前要设 `BPFTIME_GLOBAL_SHM_NAME`？为什么直接整套跑会崩？
5. 想通过"读一个能跑的测试"来学某个模块，从哪 10 个开始？

---

## 11.1 构建系统一览

bpftime 用 CMake 构建，仓库根的 `Makefile` 只是几组 cmake 命令的包装。读构建脚本按这个顺序：

| 文件 | 管什么 |
|---|---|
| `cmake/StandardSettings.cmake` | 几乎所有 `option()`；按 CPU 架构强制设定 Frida / trap 两个后端 |
| 根 `CMakeLists.txt` | 顶层编排：C99 / C++20、全局 `-fno-omit-frame-pointer`、日志级别、按开关 `add_subdirectory`、安装 |
| `cmake/libbpf.cmake` | 用 ExternalProject 编 `third_party/libbpf` 和 `third_party/bpftool`；提供 `add_ebpf_program_target`（把 `.bpf.c` 编成 `.bpf.o`）和生成 skeleton 的函数 |
| `cmake/frida.cmake` | 下载 frida-core / frida-gum 的 devkit（固定版本，带 SHA256 校验；可预先放进 `third_party/frida/`） |
| `tools/CMakeLists.txt` | 三个命令行工具的编译条件 |
| `Makefile` | `make build`、`make release`、`make benchmark` 等快捷目标（默认 `JOBS := 1`，串行编译） |

两个全局约束要先知道：

- **禁止在源码目录内构建**（根 `CMakeLists.txt` 里直接 `FATAL_ERROR`），必须 `cmake -B build`。
- **安装位置写死为 `~/.bpftime`**，不受 `CMAKE_INSTALL_PREFIX` 控制，而且是在 **configure 时**按当时的 `$HOME`
  展开成绝对路径（生成的 `cmake_install.cmake` 里写的就是 `/home/<你>/.bpftime`）。普通用户 configure、再用
  `sudo cmake --install`，会以 root 身份写进**你自己的** `~/.bpftime`；只有用 sudo 跑 configure 才会落到 `/root/.bpftime`。
  反过来，`bpftime` 命令行**运行时**按当时的 `$HOME` 找 `.so`：`sudo bpftime …` 时通常去找 `/root/.bpftime`，
  可能和安装位置对不上，这时用它的 `-i` 选项指定目录。

---

## 11.2 CMake 选项总表

**直觉。** 选项分三类：真正改变构建内容的、只在某些架构上起作用（而且你改不了）的、以及声明了但没人读的"死开关"。
下表"作用"列写的是它在哪里被读、读了之后做什么。

| 选项 | 默认 | 作用 | 备注 |
|---|---|---|---|
| `CMAKE_BUILD_TYPE` | 未设时 Debug | Debug 时加 `-D_DEBUG` | 会打印 "Enabling ubsan for Debug builds"，但**没有加任何 ubsan 参数** |
| `BPFTIME_LLVM_JIT` | ON | 编 LLVM JIT/AOT（llvmbpf）；关掉时 runtime 默认 VM 改为 ubpf；是编 `bpftime-aot` 的前提 | |
| `BPFTIME_UBPF_JIT` | ON | 编 ubpf 后端，agent / syscall-server 整包链接它 | |
| `BPFTIME_BUILD_WITH_LIBBPF` | ON | 引入 libbpf / bpftool；daemon、aot、kernel-user map、单测、benchmark 都要求它；agent 里的 syscall trace 支持也包在这个宏里 | |
| `BPFTIME_ENABLE_UNIT_TESTING` | OFF | 编 `bpftime_runtime_tests`（还要求 LIBBPF）、daemon 测试、vm 测试 | Frida 和 syscall trace 两个 attach 测试不归它管，默认全量构建就会编 |
| `BUILD_BPFTIME_DAEMON` | ON | 编 `bpftime_daemon`（还要求 LIBBPF） | |
| `ENABLE_EBPF_VERIFIER` | OFF | 编 bpftime-verifier 并接进 syscall-server（[第 10 章](10-periphery.md)） | |
| `ENABLE_PROBE_READ_CHECK` / `ENABLE_PROBE_WRITE_CHECK` | ON / ON | 只给 runtime 定义宏：`bpf_probe_read*` / `bpf_probe_write_user` 走 `process_vm_readv/writev`，坏地址返回错误而不是崩溃（[第 7 章](07-helpers.md)） | 也决定单测里越界断言是否编入 |
| `SPDLOG_ACTIVE_LEVEL` | 未传时 TRACE | **编译期**日志级别；传了 INFO 之后，运行时设 `SPDLOG_LEVEL=debug` 也看不到 DEBUG 日志 | 不是 `option()`，用 `-D` 传 |
| `BPFTIME_ENABLE_LTO` | OFF | 打开链接期优化（LTO：编译时只产出中间代码，链接时再整体优化） | 会影响实验：静态库里是编译器私有格式的中间代码，自己链接时要用同一个编译器；也可能把被 hook 的小函数优化掉 |
| `BPFTIME_ENABLE_ASAN` | OFF | 全局 `-fsanitize=address,undefined` | |
| `BPFTIME_ENABLE_MPK` | OFF | 全局宏，用 Intel PKU 保护共享内存（[第 2 章](02-runtime-shm.md)） | |
| `BPFTIME_ENABLE_IOURING_EXT` | OFF | io_uring 扩展 helper（1006–1010），runtime 链接 liburing | |
| `BPFTIME_ENABLE_CUDA_ATTACH` | OFF | 全局宏；编 nv_attach_impl、GPU map、bpftimetool `run-on-cuda`（[第 9 章](09-gpu.md)） | |
| `BPFTIME_CUDA_ROOT` | "" | `find_cuda()` 拼路径用 | 用 `option()` 声明，缓存里会变成 `BOOL=OFF`，要显式传路径 |
| `BPFTIME_BUILD_STATIC_LIB` | OFF | 把各静态库解开重新打包成一个 `libbpftime.a`，装到 `~/.bpftime` | `example/libbpftime_example` 需要它 |
| `BPFTIME_WARNINGS_AS_ERRORS` | OFF | 加 `-Werror`，只作用于 runtime 及链接它的目标 | |
| `BPFTIME_ENABLE_FRIDA` | 非 riscv 上 ON | Frida uprobe 后端、text segment transformer、`bpftime` 命令行 | **FORCE**：每次 configure 按架构覆盖，命令行传 OFF 无效 |
| `BPFTIME_ENABLE_TRAP_UPROBE` | 只在 riscv 上 ON | trap（断点 + SIGTRAP）后端（[第 4 章](04-attach-uprobe.md)） | **FORCE**：x86 / arm64 上传 ON 也会被改回 OFF |
| `BPFTIME_ENABLE_CCACHE` | OFF | **不起作用**：只要系统里找得到 ccache 就会用 | 死开关（形同虚设） |
| `BPFTIME_BUILD_EXECUTABLE` | OFF | 没有任何地方读取 | 死开关 |
| `BPFTIME_USE_CATCH2` | OFF | 没有任何地方读取 | 死开关 |
| `BPFTIME_VERBOSE_OUTPUT` | ON | 没有任何地方读取 | 死开关 |
| `BPFTIME_BUILD_KERNEL_BPF` | ON | 没有任何地方读取（`installation.md` 和 Makefile 还在传它） | 死开关 |
| `BUILD_ATTACH_IMPL_EXAMPLE` | 未定义 | 编 `example/attach_implementation`（nginx 插件示例，[第 4 章](04-attach-uprobe.md)） | 不是 `option()` |
| `BPFTIME_ENABLE_GDRCOPY` | 未定义 | GPU map 的可选 GDRCopy 支持 | 不是 `option()` |
| `BPFTIME_BPF_CLANG` | 依次找 clang、clang-20、clang-18 | 编 `.bpf.c` 用的 clang | 只装了 `clang-19` 时要显式传 |
| `TEST_LCOV` | OFF | 覆盖率插桩 | 各子目录各自声明 |

`cmake/rocm.cmake` 没有被任何地方 include，`BPFTIME_ROCM_ROOT` 也就不起作用。这些死开关与文档出入记在
[附录 B](B-known-issues.md#b-build-test)。

**看自己的构建实际用了什么值**：`grep -E '^(BPFTIME|ENABLE|BUILD|SPDLOG)_' build/CMakeCache.txt`。
以你自己的 `CMakeCache.txt` 为准，别以为"默认值就是我的值"。

**要点**：影响本书实验的主要是这几个：`BPFTIME_LLVM_JIT`、`BPFTIME_ENABLE_UNIT_TESTING`、
两个 probe check、`SPDLOG_ACTIVE_LEVEL`、`BPFTIME_ENABLE_LTO`。Frida / trap 由架构决定，你改不了。

---

## 11.3 最小构建命令

官方 `installation.md` 的最小流程：装依赖、取子模块，然后

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO
cmake --build build --target install -j"$(nproc)"
```

默认会打开 LLVM JIT、ubpf、Frida（非 riscv）、libbpf、daemon，所以还需要：LLVM ≥ 15 的开发包、
clang（编 `.bpf.c`）、能联网下载 Frida devkit。注意 `install` 目标会写 `~/.bpftime`。

**为学习推荐的命令**（带单测，保留 DEBUG 日志，probe check 保持默认 ON）：

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBPFTIME_ENABLE_UNIT_TESTING=ON \
  -DBUILD_BPFTIME_DAEMON=OFF
cmake --build build -j"$(nproc)"
# 找不到 LLVM 时加 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm（按你的版本改）
# 只装了 clang-19 时加 -DBPFTIME_BPF_CLANG=/usr/bin/clang-19
```

不传 `SPDLOG_ACTIVE_LEVEL`，是为了让 DEBUG 日志在编译期保留下来（第 6、7 章有些现象只在 DEBUG 日志里看得到）。

**本书实验用的 `build-jit` 和上面不一样。** 实验室机器上现成的 `build-jit` 是这样配的（从它的
`CMakeCache.txt` 反推，命令行传入的项）：

```bash
cmake -B build-jit -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBUILD_BPFTIME_DAEMON=OFF \
  -DBPFTIME_ENABLE_LTO=ON \
  -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
  -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-19
```

三处差别会影响实验：**probe check 关**（`bpf_probe_read` 是裸 `memcpy`，单测里越界断言没编进来）、
**LTO 开**（静态库里存的是 gcc-13 私有格式的中间代码，自己写小程序链接它时必须用 g++-13；
LTO 还可能把被 hook 的小函数内联或改掉参数传递，单测源码里有注释提醒这一点）、**日志级别 INFO**（看不到 DEBUG 日志）。
另外它只构建了部分目标：没有 `bpftime` 命令行（`tools/cli`）、没有 `bpftime-aot`、没有 daemon 和 verifier。
[第 12 章](12-practice.md)的实验都按这些限制写。

**Makefile 目标对照**（都会 `cmake -Bbuild`；除 `make benchmark` 外默认串行，记得加 `JOBS=$(nproc)`）：

| make 目标 | 构建类型和主要开关 | 备注 |
|---|---|---|
| `make build` | Debug，单测 + daemon | |
| `make release` | RelWithDebInfo，daemon，并安装到 `~/.bpftime` | `release-with-llvm-jit` 和它效果一样（LLVM JIT 默认就开） |
| `make release-with-static-lib` | 加 `BPFTIME_BUILD_STATIC_LIB` | |
| `make build-unit-test` | Debug，显式打开两个 probe check，只编 `bpftime_runtime_tests` 和 `bpftime_daemon_tests` | |
| `make unit-test` | 跑上面两个测试 | 见 11.5，里面有遗留步骤 |
| `make benchmark` | RelWithDebInfo + LTO + INFO 日志 + 关 probe check，另编一份 MPK 版本（`build-mpk`）和 nginx attach 示例，最后 `make -C benchmark` | `LLVM_DIR` 写死成 llvm-15；用不限并发的 `-j`，不读 `JOBS` |
| `make build-wo-libbpf` | 不带 libbpf | 注释误写成 "iouring" |

---

## 11.4 构建产物表

编完之后，`build/` 下这些东西值得认识（`BUILD_SHARED_LIBS` 未设时）：

| 产物（相对 build 目录） | 是什么 | 谁用 | 编译条件 |
|---|---|---|---|
| `runtime/syscall-server/libbpftime-syscall-server.so` | 控制面库：拦截 `bpf()`、`perf_event_open()` 等 | `LD_PRELOAD` 给 loader（如 sslsniff）；`bpftime load` | 总会编 |
| `runtime/agent/libbpftime-agent.so` | 数据面库：读 handler 表、JIT、装钩子 | `LD_PRELOAD` 给目标进程（如 nginx）；`bpftime start/attach` | 总会编 |
| `attach/text_segment_transformer/libbpftime-agent-transformer.so` | 系统调用改写器，再用 `AGENT_SO` 加载真正的 agent（[第 5 章](05-syscall-trace.md)） | `bpftime start -s` / `attach -s` | FRIDA |
| `tools/cli/bpftime` | 注入器命令行（[第 3 章](03-entrypoints.md)） | 用户 | FRIDA |
| `tools/bpftimetool/bpftimetool` | shm 检修工具（[第 10 章](10-periphery.md)） | 用户 | 总会编 |
| `tools/aot/bpftime-aot` | AOT 编译 / 运行（[第 10 章](10-periphery.md)） | 用户 | LLVM_JIT 且 LIBBPF |
| `daemon/bpftime_daemon` | daemon（[第 10 章](10-periphery.md)） | root 用户 | DAEMON 且 LIBBPF |
| `runtime/libruntime.a` | 核心库：shm、handler、map、helper、prog | 链进上面所有 | 总会编 |
| `vm/vm-core/libbpftime_vm.a`、`vm/compat/llvm-vm/*.a`、`vm/compat/ubpf-vm/*.a` | VM 抽象与两个后端（[第 6 章](06-vm-jit.md)） | runtime | 各自开关 |
| `attach/*/lib*_attach_impl.a` | 各 attach 后端（Frida、syscall trace、trap、CUDA） | agent | 各自开关 |
| `libbpf/libbpf.a`、`bpftool/…` | ExternalProject 编出的 libbpf 和 bpftool | 链接、生成 skeleton | LIBBPF（每次全量构建都会编） |
| `runtime/unit-test/bpftime_runtime_tests` 及 6 个 `*.bpf.o` | runtime 单测（11.5） | 开发者 | UNIT_TESTING 且 LIBBPF |
| `attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests`、`attach/syscall_trace_attach_impl/bpftime_syscall_trace_attach_tests` | Frida 后端、syscall trace 后端的单测 | 开发者 | 各自后端开着就编，**不受** UNIT_TESTING 控制（默认 `all` 目标就有） |
| `benchmark/simple-benchmark-with-embed-ebpf-calling` | 不 hook、直接在进程内调用 JIT 好的程序的基准 | 开发者 | LIBBPF |
| `libbpftime.a`（根） | 单个静态大包 | 把 bpftime 当库用 | STATIC_LIB |

`make install` 会把 agent、syscall-server、transformer、`bpftime`、`bpftimetool`、`bpftime-aot`、daemon
装到 `~/.bpftime`。

---

## 11.5 单测：怎么跑、为什么要隔离

### 测试二进制

全部用 **Catch2 v3.4.0**。主力是 `bpftime_runtime_tests`：`runtime/unit-test/` 下所有 `test_*.cpp`
编成一个二进制，共 **106 个用例、50 个 tag**（本机 `--list-tests` / `--list-tags` 实测）。

| 二进制 | 源码 | 覆盖 | 本机 build-jit 里有吗 |
|---|---|---|---|
| `bpftime_runtime_tests` | `runtime/unit-test/` | maps、perf / ringbuf / epoll、probe、get_stack、shm 分配失败与宿主契约、attach + eBPF、tail call | 有 |
| `bpftime_frida_uprobe_attach_tests` | `attach/frida_uprobe_attach_impl/` | Frida 后端本身（17 个用例） | 有 |
| `bpftime_syscall_trace_attach_tests` | `attach/syscall_trace_attach_impl/` | syscall trace 分发 | 无（这份构建只编了部分目标） |
| `bpftime_trap_uprobe_attach_tests` | `attach/trap_uprobe_attach_impl/` | trap 后端（只在 riscv 上编） | 无 |
| `bpftime_daemon_tests` | `daemon/test/` | daemon 用户态 driver | 无（没编 daemon） |
| `bpftime_verifier_tests` | `bpftime-verifier/test/` | verifier 封装 | 无（没开 verifier） |

`runtime/test/` 是**已废弃**的旧测试目录（它自己的 README 就这么说），CMake 里测试源全被注释，什么都不编。
`bpftime_runtime_tests` 用到的 6 个 `.bpf.o` 由 CMake 从 `runtime/unit-test/assets/*.bpf.c` 现编到构建目录，
和 `runtime/test/bpf` 无关。

### 为什么一定要先换 shm 名

**直觉。** 很多测试直接用"全局共享内存"那套 API，而全局 shm 的名字来自 `BPFTIME_GLOBAL_SHM_NAME`，
没设时就是 `bpftime_maps_shm`——和你真实跑 bpftime 用的是**同一个名字**。测试会建它、删它。

两处具体的代码：

- user→user tail call 测试用 `SHM_REMOVE_AND_CREATE` 初始化全局 shm：**删掉同名旧段、新建一个 50MB 的段**，跑完不 unlink。
  （user→kernel 那个用例也这样建段，但它用的 `bpftime::shm_remove` 在用例结束时会删掉全局名。）
- `bpftime::shm_remove` 这个小工具析构时删的不是传给它的名字，而是**全局 shm 名**。
  `Test shm progs attach`、外部 map ops 等测试都用它。

`runtime/src/handler/handler_manager.hpp` 的 `struct shm_remove`：

```cpp
struct shm_remove {
        shm_remove(const char *name)
        {
                boost::interprocess::shared_memory_object::remove(name);   // 构造：删传入的名字
        }
        ~shm_remove()
        {
                boost::interprocess::shared_memory_object::remove(
                        get_global_shm_name());   // 析构：删的是 BPFTIME_GLOBAL_SHM_NAME（默认 bpftime_maps_shm）
        }
        /* ... */
};
```

所以：同一台机器上有一组 bpftime 会话在用默认名时，不设 `BPFTIME_GLOBAL_SHM_NAME` 跑单测，
可能把它的 shm 删掉或换成一个空段。本机实测：先在 `/dev/shm` 放一个占位文件、把 shm 名指向它，
跑完 `Test shm progs attach` 后占位文件**没了**；跑完 user→user tail call 测试后占位文件变成了一个 50MB 的新段。
整套跑完后你的 shm 名在不在，取决于最后动它的是哪个用例（本机整套跑完时它已被后面的用例删掉）。
另外有几个测试用固定名字的段（`my_shm_attach_test`、`BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT` 等）跑完不删，
还会留下 `/tmp/bpftime_test_shm_json.json`。锁文件 `/tmp/bpftime-shm-<名>.lock` 只由 syscall-server 和 agent 创建，
所以只有 `[allocation]` 一组（用 `LD_PRELOAD` 拉起它们的子进程）会按各自的名字留下锁文件，普通固定名的段不产生。
详见[附录 B](B-known-issues.md#b-build-test)。

固定姿势：

```bash
cd build/runtime/unit-test
export BPFTIME_GLOBAL_SHM_NAME=lab_$RANDOM     # 必须
ls /dev/shm > /tmp/shm_before.txt              # 跑之前记一下
T=./bpftime_runtime_tests
# ... 跑测试 ...
diff /tmp/shm_before.txt <(ls /dev/shm)        # 跑完对比，删掉多出来的
```

### 为什么默认顺序整套跑会崩

直接 `$T` 不加过滤，会在第 2 个用例 `Test attach replace with ebpf` 处 **SIGSEGV**（退出码 139），后面全部没跑。

原因是测试之间**共享进程级全局状态**：这个用例先用 Frida 的 `gum_find_function` 按名字查符号，再构造
`frida_attach_impl`；而 Frida 的初始化（`gum_init_embedded`）在 `frida_attach_impl` 的构造函数里。
它单独跑或排在第一个 Frida 用例之前，Frida 还没初始化，于是崩溃（按配对、换序实验和读码推断，本机没有 gdb 取栈）。
只要先有一个构造过 `frida_attach_impl` 的用例（比如 `Test probing internal functions`）跑在它前面，它就能过。
这是测试的顺序依赖，不是 runtime 的 bug。在 CI 上是否同样崩溃，本机无法验证；但 CI 的 runtime job
（Debug 构建、privileged 容器）是不加过滤直接跑整套的，如果 Debug 构建也必崩，那个 job 会一直是红的——
所以崩溃更可能只在 `build-jit` 这类配置（RelWithDebInfo + LTO + gcc-13）下出现。

另一个用例 `Test tail calling from userspace to kernel` 要真内核 BPF 权限，非 root 下失败。

**推荐的整套跑法**（本机实测 `All tests passed (… assertions in 104 test cases)`，约 15 秒，断言数每次略有不同）：

```bash
$T "~Test attach replace with ebpf" "~Test tail calling from userspace to kernel"
```

想单独验证被排除的那个替换用例，让 Frida 先初始化：

```bash
$T "Test probing internal functions,Test attach replace with ebpf" --order rand --rng-seed 3
# 本机：All tests passed (44 assertions in 2 test cases)；换 --rng-seed 1 时顺序反过来，rc=139
```

### 按名字或 tag 只跑一部分

Catch2 的过滤参数可以是用例名（支持 `*` 通配）、`[tag]`，多个用逗号分隔，`~` 表示排除。
106 个用例里 36 个没有 tag（attach、probe、get_stack、tail call、config、json 等），只能按名字过滤。

```bash
$T --list-tests                  # 所有用例名
$T --list-tags                   # 所有 tag 及每个 tag 下的用例数
$T "[software_perf_event]"       # 按 tag
$T "get_stack*"                  # 按名字通配
$T "[kernel]~[large_memory]"     # tag 组合：避开大内存用例
```

### 单测总表

"本机结果"列：在 `build-jit` 上按"过滤串"列单独跑的结果（2026-11-01）。本机 probe check 关闭，
`test_probe.cpp` 里的越界断言没有编进来。

| 测试文件 | 测什么 | 过滤串 | 本机结果 | 对应章 |
|---|---|---|---|---|
| `test_config.cpp` | 运行时配置的 setter 与两个环境变量 | `"Test bpftime runtime_config,Allow external maps*,Select Frida*"` | 3 例过 | 2 |
| `test_bpftime_shm_json.cpp` | hex 编解码；prog / tracepoint / link / map 写进 shm 再导出导入 JSON | `"*hex*"`、`"Test bpftime shm json*"` | 过（json 用例 7 断言）；留下固定名 shm 和 `/tmp` 下的 json | 2、10 |
| `test_get_current_pid_tgid.cpp` | pid/tid 缓存在 fork 后刷新 | `"bpf_get_current_pid_tgid*"` | 过 | 7 |
| `test_get_current_uid_gid.cpp` | 用链接期 `--wrap` mock `getuid/getgid` | `"[uid_gid]"` | 过 | 7 |
| `test_get_stack.cpp` | `bpf_get_stack` 的字节语义、截断、skip；用 mock attach impl 隔离 | `"get_stack*"` | 6 例过 | 7 |
| `test_probe.cpp` | probe_read / write / read_str；不篡改应用的 SIGSEGV 处理 | `"Test bpftime_probe*,Test Probe*,Test repeated*,Test bpf_probe_read_str,Test probe access*,Test default SIGSEGV*"` | 7 例过（53 断言） | 7 |
| `test_software_perf_event.cpp` | perf 输出：分片、8 字节对齐回绕、`E2BIG`、`ENOMEM`；不改调用线程的 CPU 亲和性 | `"[software_perf_event]"`、`"[affinity]"` | 3 例过（8307 断言）；1 例过 | 7、8 |
| `test_ringbuf_tail_wrap.cpp` | ringbuf 记录跨环尾仍连续可读 | `"[ringbuf]"` | 过（1002 断言） | 8 |
| `test_epoll.cpp` | loader 侧 epoll：就绪只报一次、不消费数据、恢复信号掩码 | `"[epoll]"` | 过（6558 断言） | 8 |
| `test_shm_allocation_failure.cpp` + `assets/shm_allocation_test_helper.c` | `LD_PRELOAD` 注入宿主时的失败回落、不写宿主 stdio、shm 所有权与清理 | `"[allocation]"` | 13 例过（87 断言） | 3 |
| `attach_with_ebpf/test_helpers.cpp` | 最短的"打开 .o → 加 helper 组 → load → exec" | `"Test helpers"` | 过 | 6 |
| `attach_with_ebpf/test_ufunc_register.cpp` | ufunc：按 id 注册宿主函数，解释与 JIT 都返回 17 | `"Test ufunc register"` | 过 | 7 |
| `attach_with_ebpf/test_attach_uprobe_with_ebpf.cpp` | Frida uprobe / uretprobe、同址多挂、detach | `"Test probing internal functions"` | 过（35 断言） | 4 |
| `attach_with_ebpf/test_attach_filter_with_ebpf.cpp` | override：跳过原函数并改返回值；按 id / 地址 detach | `"Test attaching filter*"` | 过（34 断言） | 4 |
| `attach_with_ebpf/test_attach_replace.cpp` | override + ufunc 整体替换函数 | 见上文"为什么会崩" | 单独跑 SIGSEGV；配对跑过 | 4 |
| `attach_with_ebpf/test_shm_progs_attach.cpp` | 手工在 shm 里造 handler 表，用 `init_attach_ctx_from_handlers` 装配，fork 后再装一次 | `"Test shm progs attach"` | 过（8 断言）；注意它对"替换生效"几乎没有断言 | 3 |
| `attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp` | trap 后端 | — | x86 上整个文件被宏包掉 | 4 |
| `tailcall/test_user_to_user_tailcall.cpp` | 全局 shm C API + PROG_ARRAY + helper 12 | `"*tail calling from userspace to userspace"` | 过（8 断言） | 7、8 |
| `tailcall/test_user_to_kernel_tailcall.cpp` | 用户态程序 tail call 进内核程序 | — | 非 root 失败（需要 root，本机未跑） | 7 |
| `maps/kernel_unit_tests.cpp` | 移植自内核 `test_maps` 风格：flags、errno、并发 | `"[kernel]~[large_memory]"` | 10 例过 | 8 |
| `maps/test_shm_hash_maps.cpp` | 父子进程经 shm 共享 handler 表与 map；同名 map 互相独立 | `"Test shm hash maps*,Hash maps with colliding*"` | 2 例过 | 2、8 |
| `maps/test_per_cpu_array.cpp` / `test_per_cpu_hash.cpp` | per-CPU map 在用户态的模拟 | `"Test basic operations of array map"` / `"…hash map"` | 过 | 8 |
| `maps/test_external_map_ops.cpp` | 注册外部 map 类型 | `"*external hash map ops"` | 过；留下固定名 shm | 8 |
| 其余 `maps/*.cpp`（LRU、LPM、bloom、queue、stack、stack trace、map-in-map、底层 hash） | 各 map 实现的语义 | `"[lru_hash]"`、`"[lpm_trie]"`、`"[lpm_trie_map]"`、`"[bloom_filter]"`、`"[queue_map]"`、`"[stack_map]"`、`"Test stack trace map*"`、`"[map_in_map]"`、`"[bpftime_hash_map]"` | 均过 | 8 |
| `cuda/test_cuda_compile.cpp` | CUDA 构建专用 | — | 本构建不含 | 9 |

**要点**：

- 跑单测前必设 `BPFTIME_GLOBAL_SHM_NAME`，跑完对比 `/dev/shm` 清理。
- 不加过滤整套跑会在第 2 个用例 SIGSEGV；排除替换用例和需要 root 的用例后全过。
- `runtime/test/` 已废弃；测试用的 `.bpf.o` 由 CMake 现编。

---

## 11.6 示例总表

仓库的 `example/` 是学习材料，也是文档最容易过时的地方。"README"列：✅ 与当前代码一致，
⚠ 能用但有偏差，❌ 按 README 无法复现。"本机"列是本次复核的结果。

**两个通用前提**：

1. 示例两端（loader 和目标进程）的 `BPFTIME_GLOBAL_SHM_NAME` 必须一致；先起 loader，再起目标进程。
2. SEC 里写相对路径的示例（如 `SEC("uprobe/./victim:target_func")`）要在示例目录里运行。
   `make -C example/<目录>` 会在示例目录下生成 `.output/` 并现编一份 libbpf / bpftool。

| 目录 | 演示什么 | README | 本机 | 对应章 |
|---|---|---|---|---|
| `malloc/` | uprobe libc `malloc` + HASH 计数，loader 每秒打印 | ✅ 命令对；正文讲解过时（说用 cgroup id 作 key，实际是 pid） | **已跑**：loader 每秒打印 `pid=… malloc calls: 10` | 0、1 |
| `minimal/`（uprobe） | 最小用户态 uprobe + `bpf_printk` | ⚠ 命令可用，正文讲的是另一个程序 | 未跑 | 1、4 |
| `minimal/`（syscall） | 系统调用 tracepoint，需要 transformer | ✅ | 未跑（需要 `vm.mmap_min_addr=0`，即 root） | 5 |
| `minimal/`（uprobe-override） | `bpf_override_return` 改返回值（私有 perf type 1008） | ⚠ 无单独说明 | 未跑 | 4 |
| `minimal/usdt_minimal/` | USDT | ⚠ 只有一句话；victim 需要 `sys/sdt.h` | 未跑 | 4 |
| `error-inject/` | 用户函数和系统调用上的错误注入 | ✅（syscall 版没写运行方式） | 未跑 | 4、5 |
| `tracing/sslsniff/` | **全书主线**：`SSL_read/SSL_write` 明文抓取 | ❌ 命令断行、路径过时；以[第 12 章](12-practice.md)的命令为准 | **已跑**（[第 12 章实验 1](12-practice.md#exp-1)） | 1 |
| `tracing/opensnoop/` | open 系列系统调用，PERF_EVENT_ARRAY | ⚠ 根 README 误称它用 ringbuf | 未跑 | 5、8 |
| `tracing/opensnoop_ring_buf/` | 同上，用 RINGBUF | ⚠ 路径过时 | 未跑 | 8 |
| `tracing/bashreadline/`、`funclatency/`、`mountsnoop/`、`sigsnoop/`、`statsnoop/`、`syscount/` | bcc 风格的追踪工具 | ⚠ README 基本只有一句链接（statsnoop 的链接还指错了）；funclatency 的 SEC 写的是 kprobe，实际由 loader 挂成 uprobe | 未跑 | 4、5 |
| `tracing/goroutine/` | 追踪 Go 运行时；`bpftime attach` 远程注入 | ❌ 相对路径少一层 `../` | 未跑 | 3 |
| `tracing/bpftrace/` | 用户态跑 bpftrace | ⚠ 需要 bpftrace 和已安装的 `bpftime` | 未跑 | 3 |
| `bpf_features/bloom_filter_demo/`、`lpm_trie_demo/`、`queue_demo/` | 各种 map 类型 | ❌ README 和脚本里的 `../../build` 应为 `../../../build`；bloom 的 README 和脚本对 VM 的要求互相矛盾；lpm 的 README 脚本名拼错 | 未跑 | 8 |
| `bpf_features/get_stack_id_example/` | `bpf_get_stackid` + STACK_TRACE map | ❌ README 是 malloc README 的复制品 | 未跑 | 7、8 |
| `bpf_features/tailcall_minimal/` | 用户态程序 tail call 进**内核**程序 | ⚠ 没提需要 root | 未跑（需要 root） | 7 |
| `hotpatch/redis/`、`hotpatch/vim/` | 热补丁（override + ufunc 调宿主函数） | ❌ 依赖不存在的压缩包、`workloads/` 目录和旧 CLI；redis loader 的 attach 代码被注释掉；vim 没有 Makefile | 只适合读代码 | 4、7 |
| `xdp-counter/` | XDP 程序在用户态 runtime 加载 | ✅ 参数顺序与代码一致 | 未跑（建 veth 需要 root） | 3 |
| `libbpftime_example/` | 把 bpftime 当库：初始化 shm、JSON 导入导出 | ⚠ 需要 `BPFTIME_BUILD_STATIC_LIB=ON` 并安装 | 未跑 | 2 |
| `otel-ebpf-profiler/` | daemon 镜像 cilium/ebpf 的 perf link（兼容性验证） | ✅ 脚本自己用独立 shm 名 | 未跑（需要 root + daemon） | 10 |
| `attach_implementation/` | 自定义 attach：nginx 插件 | 需 `BUILD_ATTACH_IMPL_EXAMPLE` | 未跑 | 4 |
| `gpu/` | CUDA 示例（cuda-counter 等） | 见[第 9 章](09-gpu.md) | 未跑（需要 GPU） | 9 |

另外：`example/README.md` 指向的 `documents/` 目录不存在，说 VM CLI 在 `vm/cli`（实际在 `vm/llvm-jit/cli`）；
`make -C example` 会进入 `gpu/`，没有 CUDA 时推断会失败。逐条位置见[附录 B](B-known-issues.md#b-build-test)。

---

## 11.7 benchmark 总表

| 目录 | 测什么 | 怎么跑 | README / 脚本的问题 | 本机 |
|---|---|---|---|---|
| `benchmark/`（`test.c`、`test_embed.c`） | `test.c`：17 个几乎不做事的 `__bench_*` 函数（`return a[b] + c`）的调用耗时，被 hook 后就是 hook 开销；`test_embed.c`：不 hook，直接在进程内调用 JIT 好的程序 | `make -C benchmark` 编 `test`；`build/benchmark/simple-benchmark-with-embed-ebpf-calling <uprobe.bpf.o> <uretprobe.bpf.o>` | `make -C benchmark` 会进 `gpu/` | **已跑**（[第 12 章实验 4](12-practice.md#exp-4)、[实验 6](12-practice.md#exp-6)） |
| `benchmark/uprobe/` | 用户态 uprobe / uretprobe、probe_read / write、4 种 map 操作的开销 | `python3 benchmark/uprobe/benchmark.py`（内核组要 sudo）；手工：loader `benchmark/uprobe/uprobe` + 目标 `benchmark/test`，**在仓库根运行**（SEC 里写的是相对路径） | README 的基线函数名、`benchmark/uretprobe` 目录过时；脚本靠"当前目录名叫 bpftime"拼路径 | **已跑**用户态部分 |
| `benchmark/syscall/` | 系统调用 tracepoint 开销 | `sudo python benchmark/syscall/benchmark.py` | ✅ | 未跑（需要 root） |
| `benchmark/mpk/` | 开 / 关 MPK 的开销对比 | `build.sh` 后跑 `benchmark.py` | `build.sh` 写死 llvm-15；需要支持 PKU 的 Intel CPU | 未跑 |
| `benchmark/ssl-nginx/` | sslsniff 对 nginx HTTPS 吞吐的影响 | `make -C benchmark/ssl-nginx`（编 sslsniff）；`python3 benchmark/ssl-nginx/benchmark.py` | README 把"一键脚本"写成画图脚本 `draw_figture.py`，sslsniff 路径旧；`benchmark.py` 也用旧路径 | 场景本身已跑（实验 1） |
| `benchmark/syscount-nginx/` | syscount 对 nginx 的影响 | `python3 benchmark/syscount-nginx/benchmark.py` | ⚠ 脚本里 syscount 路径过时（`example/libbpf-tools/…`，实际在 `example/tracing/syscount/`），不改的话会打印 "Skipping" 并静默跳过所有追踪组，只剩无追踪的基线 | 未跑 |
| `benchmark/deepflow/`、`fuse/`、`redis-durability-tuning/` | 外部系统上的端到端实验 | 见各自 README | deepflow README 自称 TODO；redis 的一个程序写死了作者机器上的绝对路径 | 未跑（需要 root / 外部依赖） |
| `benchmark/trap-uprobe/` | trap 后端每次命中的延迟 | riscv 交叉构建 + qemu | x86 上目标不会生成（FORCE OFF） | 未跑 |
| `benchmark/gpu/` | CUDA map / host 性能 | — | — | 未跑（需要 GPU） |

---

## 11.8 活文档 Top 10

"活文档"指能跑、有断言、而且比 README 更不会过时的代码。按学习顺序：

| # | 材料 | 学什么 | 对应章 |
|---|---|---|---|
| 1 | `example/malloc/`（`malloc.bpf.c` 41 行 + `malloc.c` 119 行） | 亲眼看到"loader + 目标进程 + 共享内存"三件套跑起来，再去读源码 | 0、1 |
| 2 | `runtime/unit-test/attach_with_ebpf/test_helpers.cpp` + `assets/helpers.bpf.c` | 不涉及 shm、不涉及 attach 的最短"打开 → 加 helper 组 → load → exec"，理解 VM 封装 | 6 |
| 3 | `runtime/unit-test/maps/test_shm_hash_maps.cpp` | shm 里构造 handler 表，fork 出的子进程直接读到父进程写的 map；同名 map 独立、内存完全回收 | 2、8 |
| 4 | `runtime/unit-test/test_bpftime_shm_json.cpp` | prog / tracepoint / link / map 四类 handler 怎么写进 shm、fd 怎么编号、导出的 JSON 长什么样 | 2、10 |
| 5 | `runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf.cpp` | attach impl 只认"地址 + 回调"；uprobe / uretprobe、同址多挂、`bpf_get_func_ip` | 4 |
| 6 | `runtime/unit-test/attach_with_ebpf/test_attach_filter_with_ebpf.cpp`（配 `example/error-inject/`） | override 怎么让原函数不执行并改返回值；自定义 helper | 4、7 |
| 7 | `runtime/unit-test/attach_with_ebpf/test_shm_progs_attach.cpp` | agent 从 handler 表装配钩子的缩影（prog → perf event → link → `init_attach_ctx_from_handlers`）；注意断言很弱 | 3 |
| 8 | `runtime/unit-test/tailcall/test_user_to_user_tailcall.cpp` | 用全局 shm 的 C API 拼出完整小场景，顺带理解 tail call helper | 2、7 |
| 9 | `test_software_perf_event.cpp` → `test_ringbuf_tail_wrap.cpp` → `test_epoll.cpp` | 事件输出通道三件套：perf 分片与 8 字节对齐、ringbuf 回绕、loader 侧 epoll | 7、8 |
| 10 | `runtime/unit-test/test_shm_allocation_failure.cpp` + `assets/shm_allocation_test_helper.c` | "注入代码是宿主的客人"写成了 13 个可执行断言：失败回落、不写宿主 stdio、shm 所有权与清理 | 3 |

进阶加读：`maps/kernel_unit_tests.cpp` 是"bpftime map 与内核 errno 语义"的对照表；
`benchmark/uprobe/` 是量化 hook 开销的现成实验。

---

## 小结

- 构建用 CMake；`Makefile` 只是包装。禁止源码内构建，安装位置写死 `~/.bpftime`。
- 影响实验的主要是 LLVM JIT、单测、两个 probe check、`SPDLOG_ACTIVE_LEVEL`、LTO。Frida / trap 按架构 FORCE，命令行改不了；还有五个死开关。
- 本书用的 `build-jit` 关了 probe check、开了 LTO、日志级别 INFO，没有 `bpftime` 命令行、aot、daemon、verifier。
- syscall-server 和 agent 两个 `.so` 是主角；bpftimetool、bpftime-aot、daemon 是外围工具。
- 跑单测前必设 `BPFTIME_GLOBAL_SHM_NAME`：测试会删除或重建全局 shm 名；`bpftime::shm_remove` 析构删的是全局名。
- 默认顺序整套跑会在第 2 个用例 SIGSEGV（Frida 未初始化的顺序依赖）；排除它和需要 root 的用例后全过。
- 示例 README 大面积过时；优先看单测和本书验证过的命令。

## 动手验证

1. **看你的构建实际用了哪些值**：`grep -E '^(BPFTIME|ENABLE|BUILD|SPDLOG)_' build/CMakeCache.txt`，对照 11.2 的表。
2. **整套单测的安全跑法**与**顺序依赖复现**：命令见 11.5，完整步骤和预期见[第 12 章实验 11](12-practice.md#exp-11)。

## 自测题

1. **你在 x86 机器上想关掉 Frida 后端，传了 `-DBPFTIME_ENABLE_FRIDA=OFF`，结果 `bpftime` 命令行还是编出来了。为什么？**
   <details><summary>答案</summary>

   `cmake/StandardSettings.cmake` 按 CPU 架构用 `set(... CACHE BOOL ... FORCE)` 设定 `BPFTIME_ENABLE_FRIDA`
   和 `BPFTIME_ENABLE_TRAP_UPROBE`：非 riscv 上 Frida 强制 ON、trap 强制 OFF。FORCE 会覆盖命令行和缓存里的值，
   所以每次 configure 都被改回去。
   </details>

2. **同学说"我的程序在 bpftime 下读坏指针会返回 -EFAULT，不会崩"。你在 `build-jit` 上试，目标进程却直接 SIGSEGV 了。最可能的原因？**
   <details><summary>答案</summary>

   `ENABLE_PROBE_READ_CHECK` 在 CMake 里默认 ON，但 `build-jit` 配置时显式关掉了。关掉后 `bpf_probe_read*`
   就是裸 `memcpy`，坏地址会让宿主进程崩溃。看 `CMakeCache.txt` 里这个选项的实际值，而不是看默认值。
   </details>

3. **你的机器上正跑着一组用默认 shm 名的 bpftime 实验。这时直接 `./bpftime_runtime_tests "*tail calling from userspace to userspace"` 会发生什么？**
   <details><summary>答案</summary>

   这个测试用 `SHM_REMOVE_AND_CREATE` 初始化全局 shm，名字来自 `BPFTIME_GLOBAL_SHM_NAME`，没设时就是
   `bpftime_maps_shm`。它会删掉你的 shm 名、建一个新的空段并留着不删。已经映射旧段的进程还能继续用旧段，
   但之后新启动的 agent / bpftimetool 看到的是测试留下的空段。所以跑单测前要设一个独立的 shm 名。
   </details>

4. **为什么 `Test attach replace with ebpf` 单独跑会崩，和 `Test probing internal functions` 一起、且排在它后面就能过？这说明单测有什么特点？**
   <details><summary>答案</summary>

   替换用例在构造 `frida_attach_impl` 之前就调用了 Frida 的符号查找，而 Frida 的初始化在
   `frida_attach_impl` 构造函数里。前面先有一个用例构造过 `frida_attach_impl`，Frida 就已经初始化好了。
   这说明所有用例跑在同一个进程里，共享全局状态（Frida 初始化、全局 shm、mock 的 attach ctx），顺序会影响结果。
   </details>

5. **`make unit-test-runtime` 里有一步 `make -C runtime/test/bpf` 和 `cd build/runtime/test && ctest`。这两步测了什么？**
   <details><summary>答案</summary>

   什么也没测。`runtime/test/` 是已废弃的旧测试目录，CMake 里测试源全被注释，不产生任何测试目标；
   `runtime/test/bpf` 编出的 `.bpf.o` 也没有现存测试读取。真正的单测是 `bpftime_runtime_tests`，
   它用的 `.bpf.o` 由 CMake 从 `runtime/unit-test/assets/` 现编。用 Ninja 生成器时，`cd build/runtime/test && make`
   这一步还会失败（本机实测：`No targets specified and no makefile found`）。
   </details>

6. **你想学"agent 怎么从共享内存里的 handler 表把钩子装起来"，但不想起 nginx。读哪个测试？读的时候要注意什么？**
   <details><summary>答案</summary>

   `runtime/unit-test/attach_with_ebpf/test_shm_progs_attach.cpp`。它在 shm 里手工构造 prog、perf event、link
   三类 handler，注册 Frida attach impl，再调 `init_attach_ctx_from_handlers` 装配，然后 fork 子进程打开同一段 shm
   再装一次——正是 agent 启动时做的事。要注意它对"替换真的生效"几乎没有断言（父进程只打日志，子进程的检查用的是
   `assert`，Release 类构建下被编译掉），测试通过只说明装配流程不崩。
   </details>

## 深入参考

<details>
<summary>代码位置速查（按 177a1dc465ff 行号）</summary>

| 概念 / 函数 | 位置 |
|---|---|
| 选项定义 | `cmake/StandardSettings.cmake`（LLVM / ubpf `:10-11`，单测 `:32`，LTO `:52-61`，ccache `:63-68`，ASAN `:70-74`，Frida / trap FORCE `:76-82`，MPK `:84-87`，verifier `:95`，daemon `:98`，static lib `:104`，libbpf `:107`，probe check `:110,112`） |
| 禁止源码内构建 | `CMakeLists.txt:129-131` |
| ubsan 提示（无实际参数） | `CMakeLists.txt:92` |
| `BPFTIME_BUILD_WITH_LIBBPF` 宏与 include | `CMakeLists.txt:104-106`、`:133-135` |
| verifier 或 Catch2 子模块 | `CMakeLists.txt:144-154` |
| `SPDLOG_ACTIVE_LEVEL` 默认 TRACE | `CMakeLists.txt:159-165` |
| probe check 宏只给 runtime | `CMakeLists.txt:189-195` |
| daemon 编译条件 | `CMakeLists.txt:259-261` |
| 安装到 `~/.bpftime` | `CMakeLists.txt:274-280` |
| `bpftime` 运行时按 `$HOME` 找安装目录（`-i` 覆盖） | `tools/cli/main.cpp:893-900` |
| 两个 attach 测试的定义（不受 UNIT_TESTING 控制） | `attach/frida_uprobe_attach_impl/CMakeLists.txt:42`、`attach/syscall_trace_attach_impl/CMakeLists.txt:32` |
| 锁文件 `shm_lifecycle_lock` 的使用者 | `runtime/syscall-server/syscall_server_utils.cpp:79,157`、`runtime/agent/agent.cpp:935` |
| syscount-nginx 的过时路径 | `benchmark/syscount-nginx/benchmark.py:22` |
| 三个工具的编译条件 | `tools/CMakeLists.txt` |
| `add_ebpf_program_target` | `cmake/libbpf.cmake:110` |
| 单测目录加入条件 | `runtime/CMakeLists.txt:281-286` |
| Frida attach 测试条件 | `runtime/unit-test/CMakeLists.txt:52-61` |
| 测试用 `.bpf.o` 现编 | `runtime/unit-test/CMakeLists.txt:143-158` |
| `get_global_shm_name` / `struct shm_remove` | `runtime/src/handler/handler_manager.hpp:60-67` / `:69-85` |
| tail call 测试用 `SHM_REMOVE_AND_CREATE` | `runtime/unit-test/tailcall/test_user_to_user_tailcall.cpp:16-18` |
| 使用 `bpftime::shm_remove` 的测试 | `test_shm_progs_attach.cpp:202`、`maps/test_external_map_ops.cpp:50` |
| 替换用例的顺序依赖 | `test_attach_replace.cpp:83`（先注册 ufunc）、`:95`（后构造 `frida_attach_impl`）；`gum_init_embedded` 在 `attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:40` |
| 废弃的旧测试目录 | `runtime/test/README.md:3`、`runtime/test/CMakeLists.txt:11-24` |
| Makefile 目标 | `Makefile`：`build-unit-test` `:35`、`unit-test-daemon` `:43`、`unit-test-runtime` `:46-50`、`unit-test` `:52`、`build` `:54`、`build-wo-libbpf` `:62`、`release` `:66`、`release-with-llvm-jit` `:71`、`release-with-static-lib` `:77`、`benchmark` `:82-103` |
| benchmark 的 embed 程序 | `benchmark/CMakeLists.txt:1-14`、`benchmark/test_embed.c` |
| uprobe benchmark 的程序与被测函数 | `benchmark/uprobe/uprobe.bpf.c`、`benchmark/test.c:7-29` |

</details>

<details>
<summary>细节与坑</summary>

**构建**

- 根 `CMakeLists.txt` 给所有 C/C++ 编译加 `-fno-omit-frame-pointer`，全局定义 `LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1`，设 `CMAKE_POSITION_INDEPENDENT_CODE`。
- `vm/compat/CMakeLists.txt` 会再 include 一次 `StandardSettings.cmake`，FORCE 又执行一遍。
- libbpf 的 ExternalProject 设了 `BUILD_ALWAYS TRUE`，bpftool 不是 `EXCLUDE_FROM_ALL`，每次全量构建都会编。
- 静态大包的自定义命令里用了不存在的变量 `${CMAKE_CURRENT_BUILD_DIR}`，因为工作目录恰好是构建目录才碰巧能用（推断）。
- `installation.md` 的几处不符：工具名写成 "bpftime-vm"（实为 `bpftime-aot`）；"(With ubpf jit)" 两节其实也默认开着 LLVM JIT；`ENABLE_PROBE_READ_CHECK` 的说明写成了 "bpf_probe_write"；说 ccache "Default to OFF"；`BPFTIME_BUILD_KERNEL_BPF` 不起作用。
- `build-jit` 的 `BPFTIME_BPF_CLANG=/usr/bin/clang-19` 一定是命令行（或预置缓存）传入的：本机没有 `/usr/bin/clang`，而 `find_program` 的候选名里没有 clang-19。

**单测**

- `Makefile` 的 `unit-test-runtime` 里 `export BPFTIME_VM_NAME=llvm` 单独占一行，make 每行起一个新 shell，对下一行无效。
- `test_get_stack.cpp` 定义了强符号 `get_global_attach_ctx()`，覆盖 runtime 里的弱定义，对整个测试二进制生效。同一文件里的命名空间级全局对象 `mock_attach_ctx` 在程序启动时构造，测试二进制启动时（连 `--list-tests` 也是）那行 `bpf_attach_ctx constructed` 日志就来自它；退出时的 `Destructor: bpf_attach_ctx` 也是它，而且打在 stdout 最后一行。
- fork 类测试的正确写法是子进程用 `_exit(code)` 报告、父进程 `waitpid` 断言；`test_shm_progs_attach.cpp` 在子进程里用了 `REQUIRE` 和 `assert`（后者在 `NDEBUG` 下失效）。
- `test_hashmap_sizes (kernel)` 没有任何断言；`Queue/Stack Map Conceptual Concurrency Test` 自己在外面加了自旋锁，测不到 map 本身的并发安全（queue / stack 实现内部也没有锁，靠 handler 层加锁）。
- `runtime_options_test_helper.c` 被拉进单测构建，但没有任何 TEST_CASE 用它，只被 CI 脚本用。
- `test_bpftime_shm_json.cpp` 会留下 50MB 的 `BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT` 和 `/tmp/bpftime_test_shm_json.json`；`test_bpftime_hash_map.cpp` 用 `open_or_create` 的段从不删。锁文件只由 syscall-server / agent 创建，单测里只有 `[allocation]` 一组会按子进程的名字留下 `/tmp/bpftime-shm-*.lock`。

**示例与 benchmark**

- `elf_find_func_offset_from_file` 是 libbpf 的**内部**函数，示例直接声明并链接静态 `libbpf.a` 来用。
- `perf_event_attr.type = 1008` + `bpf_prog_attach(..., BPF_MODIFY_RETURN)` 是 bpftime 私有的 override 约定（`example/minimal/attach_override.h`）。
- 把 victim 用 `timeout` 之类的包装命令启动时，`LD_PRELOAD` 被包装进程继承，agent 也会注入包装进程本身。
- `lpm_trie_demo` 的 Makefile 用 `-mllvm -bpf-stack-size=8192`，超出内核 512 字节栈限制，可用来观察"用户态不受内核栈限制"。

**CI 怎么测**（`.github/workflows/`，只略读）

| workflow | 构建配置 | 跑什么 |
|---|---|---|
| `test-runtime.yml` | Debug，LLVM JIT，单测，static lib，probe check 开；另有 MPK 组 | `bpftime_runtime_tests`；`example/malloc` 端到端 |
| `test-attach.yml` | Debug，不编 daemon | Frida 和 syscall trace 两个 attach 测试 |
| `test-examples.yml` | RelWithDebInfo，JIT 开 / 关两套 | 编译并运行 `example/*` |
| `test-bpftrace.yml` | RelWithDebInfo + JIT | bpftrace + syscall-server 跑 openat、malloc uprobe |
| `test-tools.yml` | RelWithDebInfo + JIT | `tools/aot/test_run.py`、`tools/bpftimetool/test_load.py` |
| `test-verifier.yml` | Release，LLVM JIT 关，verifier 开 | 只跑 `bpftime_verifier_tests` |
| `build-gcc13.yml` | gcc-13，LLVM JIT 关；无 libbpf 一组；daemon 一组 | 只编译；`bpftime_daemon_tests` |
| `test-riscv64.yml` | 交叉编译 riscv64，trap | qemu 下跑 trap 测试和 runtime 测试 |
| `test-vm.yml` | `make build-vm` | `ctest --test-dir vm/build` |
| `test-nginx-attach.yml` | `BUILD_ATTACH_IMPL_EXAMPLE` | nginx attach 示例 |
| `test-ptxpass.yml`、`test-gpu-examples.yml` | CUDA，自托管 GPU 机器 | PTX pass 和 GPU 示例 |
| `benchmarks.yml` | `make benchmark` | uprobe、syscall、mpk 基准 |

</details>

<details>
<summary>上游变更与我们的工作</summary>

- 单测里与本书相关的新增：#612 带了 perf 记录 8 字节对齐的回归用例（"…wrap on aligned boundaries after resize"）；#678 加了 `[perf_event][affinity]`；#706 加了 `test_ringbuf_tail_wrap.cpp`；#665 加了 trap 后端测试，#674 让 CI 在 riscv64 上用 qemu 跑。来龙去脉见[附录 A](A-research-notes.md)。
- daemon 测试比 7 月多了三个真用例（cookie 保留 #586；快照容量上下界 #662）和 `test_daemon_fd.cpp`（tag `[daemon-fd]`，#687）。
- 对照旧版第 7、8 章修正的说法：旧版说"完整跑 `make unit-test` 前要先 `make -C runtime/test/bpf` 并拷 `.bpf.o`"，实际这些 `.o` 没人用，`bpftime_runtime_tests` 的 `.bpf.o` 由 CMake 现编；旧版没提跑单测要隔离 shm 名、默认顺序会崩。
- 本章表格的"本机"结果都在实验室 x86 NUC 的 `build-jit` 上跑出（Debian 13，16 核，2026-11-01）。
- 实验室 NUC 上已有一份官方 `177a1dc` 的现成构建：`/home/y/src/bpftime-wt-official-177a1dc/build-jit`（只读，不重新配置）。

</details>

---
[← 返回目录](README.md)
