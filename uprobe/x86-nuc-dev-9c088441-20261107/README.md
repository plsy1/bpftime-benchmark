# dev 9c088441 在 x86 上的测试（对照 d46ceb8f）

Jetson 会话（bpftime-arm64环境-性能优化）在 NUC 上跑的检查，时间是 11/07 17:03–17:48。这个目录只收了它的输出，没有重跑。Jetson 那边的门禁和摘要在 `benchmark-results/jetson` 分支的 `uprobe/improve-20261106` 第 9 节。

- 分支：`dev/uprobe-official-177a1dc465ff`。9c088441 是新版，d46ceb8f 是对照，两者之间有 16 个提交：
  - ringbuf，6 个：reserve 锁内联、独占缓存行、等待者自旋一阵后让出，以及消费者在末尾填充处算作追上
  - perf 输出：按 perf_event_attr 唤醒消费者，回收有上限，加一个溢出分片
  - epoll：消费者读完记录后小睡，空闲时睡到调用者给的超时，信号交给内核
  - agent 启动时只找一次共享内存段，Frida 每个探针的模块每进程只解析一次
  - 字符串读取每块一次系统调用，以及 CLAUDE.md 和注释
- 编译：只编译 `bpftime_runtime_tests` 和 `bpftime_frida_uprobe_attach_tests`，cmake 参数和 11/04 那次相同（gcc-13，RelWithDebInfo + LTO，验证器和 probe read/write 检查关闭），8 个核。完整参数见 `jetson-check-9c088441.sh`。
- 跑法和本分支 11/03 那次相同：每日过滤器放一个进程；runtime 和 Frida 每个用例单独一个进程，各限时 60 秒（`tests/run-each-case.py`）。不计时，不钉频。

## 结果：没有新的失败

| 项目 | 9c088441 | d46ceb8f |
|---|---|---|
| 每日过滤器，一个进程（`filter.*.log`） | 195 个：194 过，1 跳过 | 184 个：183 过，1 跳过 |
| runtime 逐用例（`tests/runtime-each-*`） | 272 个：265 过，7 失败 | 261 个：254 过，7 失败 |
| Frida 逐用例（`tests/frida-each-*`） | 26/26 | 25/25 |
| `[ringbuf],[epoll],[perf_event],[allocation]` 连跑 3 次（`groups.*.txt`） | 每次 53 个：52 过，1 跳过 | — |
| 要 root 的 4 个，sudo，各 3 次（日志 part 3） | 都过 | — |

- 7 个失败两边相同，也和 11/03 的 d5279469 相同：
  - `Test tail calling from userspace to kernel`（rc=1）：普通用户建不了内核 map。
  - 3 个要 root 的用例被跳过（rc=4）。这 3 个和 tail call 那个在 sudo 下各 3 次都过。
  - 3 个 map 使用统计用例被跳过（rc=4），因为构建时没开 `BPFTIME_ENABLE_MAP_USAGE_STATS`。
- 编译警告都在这 16 个提交没改的文件里（ODR、optnone、`test_detach_while_called.cpp` 的 volatile `++`、`test_lpm_trie_map.cpp` 的多行注释）。`build.new.log` 是从零编译，所以比增量重编的 `build.base.log`、`build.new2.log` 多几条。

## 顺带发现：整套 runtime 用 junit 输出时 glibc 报 double free

不是这 16 个提交带来的，d46ceb8f 也一样。只记录，没查原因。

- 整套 runtime 放进一个进程、用 junit 输出（`-r junit -o x.xml`）跑：glibc 报 `double free or corruption (!prev)`，紧跟在 LPM trie 用例的日志之后，然后 abort、卡住或段错误，junit 报告没写完（`.xml` 只有 50 字节）。
  - x86：d46ceb8f 1/1（`runtime.base.out.xz`，被 timeout 结束，rc 124），9c088441 3/3（`runtime.new.out.xz` 段错误 rc 139，`rerun-junit.1/2.out.xz`）。
  - Jetson 会话说 Jetson 上 2/2 也这样。
- 用默认的控制台输出跑同一套就能跑完，3/3（`rerun-console.1/2.out.xz`、`runtime.whole-durations.out.xz`）：272 个里 265 过、6 跳过、1 失败。失败的是普通用户本来就过不了的 tail call 用例。
- `rerun-junit.1.out.xz` 里 double free 之后还有 `terminate called after throwing an instance of 'Catch::TestFailureException'`，tail call 用例的失败可能和崩溃有关，但没有验证。

## 文件

| 文件 | 内容 |
|---|---|
| `jetson-check-9c088441.sh`、`jetson-check2-…`、`jetson-check3-…` | Jetson 会话的三段脚本：编译和整套 junit 跑、过滤器和逐用例、sudo 用例和整套控制台跑 |
| `jetson-check-9c088441.log` | 三段脚本的输出 |
| `configure.log`、`build.new.log`、`build.base.log`、`build.new2.log` | cmake 配置；9c088441 从零编译、切到 d46ceb8f 重编、切回 9c088441 重编 |
| `filter.*.log`、`groups.*.txt`、`root-case.log` | 每日过滤器、四组标签 3 次、最后一个 sudo 用例的输出 |
| `frida.*.out`、`frida.*.xml` | Frida 整套，一个进程，junit |
| `runtime.*.out.xz`、`runtime.*.xml`、`rerun-*` | 整套 runtime 一个进程的 junit 和控制台输出（大的用 xz 压缩） |
| `tests/` | 逐用例结果（`results.json`）和失败用例的日志 |

检查结束后，克隆和构建目录（1.6 GB）已删除。`/dev/shm` 里 Jetson 建的段已删，没有 root 的段；`my_shm_attach_test`、`my_shm_maps_test` 原来就有，被测试重建过，属主还是 y，留着。
