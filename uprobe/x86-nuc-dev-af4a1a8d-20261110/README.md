# dev af4a1a8d（GPU 修复轮）在 x86 上的测试

Jetson 会话（bpftime-arm64环境-性能优化）在 NUC 上跑的检查，时间是 11/10 06:03–06:06。这个目录只收了它的输出，没有重跑。Jetson 那边也存了一份日志，在 `benchmark-results/jetson` 分支的 `gpu/fixes-20261109/verification/x86/`。

- 代码：GitHub 上 `dev/uprobe-official-177a1dc465ff` 的 45743763，加上 Jetson 用 git bundle 带过来、当时还没推送的 af4a1a8d。bundle 没有收进来。
- 和上次 x86 检查的 c9d4d974 相比多了 14 个提交（56 个文件，+5578/−2188）。大部分是 GPU（nv_attach）的改动，NUC 没有 CUDA，构建时关掉了，所以测不到。x86 上能测到的是：
  - e94e2c04：llvmbpf 里 bpf-to-bpf 调用移动 r10 时，把 64 个 8 字节槽当成了 64 字节，被调函数的栈会盖掉调用者的栈；改为移动整帧 512 字节。测试是 `[jit_load]` 里的 `A bpf-to-bpf callee's stack frame does not overlap its caller's`。
  - 45743763：GPU 栈帧数（llvmbpf 覆盖补丁的同一处）。
  - 17427685：syscall server 的共享内存段释放之后，再来的调用直接交给 libc。测试是 `The syscall server passes calls made after its segment is gone to libc`。
  - a2acb4f8：Frida 卸载测试改成先挂钩子、再启动调用线程。
  - 3695e13c：cmake。
- 编译：只编译 `bpftime_runtime_tests` 和 `bpftime_frida_uprobe_attach_tests`，用 daily 的设置：gcc-13，LLVM 19，RelWithDebInfo + LTO，验证器和 probe read/write 检查关闭，CUDA 关闭，nice 10，12 个并行任务。完整参数见 `jetson-check-af4a1a8d.sh`。
- 不计时，不钉频。整套 runtime 用控制台输出跑（junit 输出会因 double free 崩溃，见 `../x86-nuc-dev-9c088441-20261107/README.md`）。

## 结果：没有新的失败

| 项目 | 结果 |
|---|---|
| runtime 整套，一个进程（`runtime.whole.out.xz`） | 281 个：274 过，6 跳过，1 失败 |
| `[jit_load]` 连跑 3 次（`jit-load.*.txt`） | 每次 10 个用例全过（107 个断言） |
| syscall server 退出测试连跑 3 次（`server-exit.*.txt`） | 每次通过（5 个断言） |
| `[aot-cache]`（`aot-cache.txt`） | 6 个用例全过（113 个断言） |
| `[array-jit-lifetime],[array-jit-default],[array-jit-poc]`（`array-jit.txt`） | 34 个用例全过（4340 个断言） |
| Frida attach 整套连跑 3 次（`frida.*.txt`） | 每次 26 个用例全过（1113 个断言） |

- 失败和跳过都和 c9d4d974 那次相同：
  - 失败的 1 个是 `Test tail calling from userspace to kernel`，普通用户建不了内核 map，本来就过不了。
  - 6 个跳过：3 个要 root（perf event 一个，kernel-user map 两个）；另外 3 个是 map 使用统计用例，因为构建时没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`，不是因为要 root。
  - 这次没有用 sudo 重跑要 root 的用例。
- array JIT 这次 34 个用例，c9d4d974 那次 43 个，是因为那次还带了 `[jit_load]` 组（9 个）一起跑；这次 `[jit_load]` 单独跑，有 10 个，多出的是 e94e2c04 的新测试。
- 编译警告 14 条，和前两次从零编译时相同。

## 文件

| 文件 | 内容 |
|---|---|
| `jetson-check-af4a1a8d.sh`、`jetson-check-af4a1a8d.log` | Jetson 会话的脚本和输出 |
| `configure.log`、`build.log` | cmake 配置和编译日志 |
| `runtime.whole.out.xz` | runtime 整套的控制台输出（16 MB，xz 压缩） |
| `jit-load.*.txt`、`server-exit.*.txt`、`aot-cache.txt`、`array-jit.txt`、`frida.*.txt` | 各组的输出 |

运行结束时，脚本删掉了这次在 `/dev/shm` 建的段，没有 root 的段，也没有残留进程。
