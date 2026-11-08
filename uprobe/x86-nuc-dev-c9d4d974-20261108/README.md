# dev c9d4d974（AOT 缓存）在 x86 上的测试

Jetson 会话（bpftime-arm64环境-性能优化）在 NUC 上跑的检查，时间是 11/08 23:44–23:47。这个目录只收了它的输出，没有重跑。

- 代码：GitHub 上 `dev/uprobe-official-177a1dc465ff` 的 8a0afd36，加上 Jetson 用 git bundle 带过来、还没推送的 4 个提交（25 个文件，+1277/−78）：
  - 33bba8fe：attach ctx 用一个共享函数为程序选 helper
  - a90d8d0b：AOT 缓存。"AOT 缓存"的意思是：工具加载程序时把它编译成机器码对象，只编译一次；agent 直接链接这个对象，不再各自 JIT。
  - 9898ed19：绑定到 map 的程序里的地址改成按进程填的符号，每个进程链接时填自己的 map 地址
  - c9d4d974：CLAUDE.md
  - bundle 本身没有收进来；这几个提交推送到 GitHub 之前，这里只有提交号。
- 编译：只编译 `bpftime_runtime_tests` 和 `bpftime_frida_uprobe_attach_tests`，用 daily 的设置：gcc-13，LLVM 19，RelWithDebInfo + LTO，验证器和 probe read/write 检查关闭，nice 10，12 个并行任务。完整参数见 `jetson-check-c9d4d974.sh`。
- 不计时，不钉频。整套 runtime 用控制台输出跑：用 junit 输出时整套会因 double free 崩溃，见 `../x86-nuc-dev-9c088441-20261107/README.md`。

## 结果：没有新的失败

| 项目 | 结果 |
|---|---|
| runtime 整套，一个进程（`runtime.whole.out.xz`） | 278 个：271 过，6 跳过，1 失败 |
| `[aot-cache]` 连跑 3 次（`aot-cache.*.txt`） | 每次 6 个用例全过（113 个断言） |
| `[array-jit-lifetime],[array-jit-default],[array-jit-poc],[jit_load]`（`array-jit.txt`） | 43 个用例全过（4429 个断言） |
| Frida attach 整套连跑 3 次（`frida.*.txt`） | 每次 26 个用例全过（1113 个断言） |

- 失败和跳过都和 9c088441 那次相同：
  - 失败的 1 个是 `Test tail calling from userspace to kernel`，普通用户建不了内核 map，本来就过不了。
  - 6 个跳过：3 个要 root（perf event 一个，kernel-user map 两个）；3 个 map 使用统计用例，构建时没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`。
  - 这次没有用 sudo 重跑要 root 的用例。
- 新增的 6 个 `[aot-cache]` 用例：
  - `Agents link the AOT object the server compiled for a program`
  - `A program that differs from what its AOT object was compiled from is compiled`
  - `Bound programs' AOT objects use the maps of the process running them`
  - `An unbound program's map values are this process's in its AOT object`
  - `A program whose map was replaced after compiling is compiled again`
  - `The server caches no program with other addresses in its code`
- 编译警告 14 条，和 9c088441 从零编译时相同，都不在这 4 个提交改的文件里。

## 文件

| 文件 | 内容 |
|---|---|
| `jetson-check-c9d4d974.sh`、`jetson-check-c9d4d974.log` | Jetson 会话的脚本和输出 |
| `configure.log`、`build.log` | cmake 配置和编译日志 |
| `runtime.whole.out.xz` | runtime 整套的控制台输出（16 MB，xz 压缩） |
| `aot-cache.*.txt`、`array-jit.txt`、`frida.*.txt` | 各组的输出 |

运行结束时，脚本删掉了这次在 `/dev/shm` 建的段（包括被测试重建的 `my_shm_attach_test`、`my_shm_maps_test`），没有 root 的段，也没有残留进程。
