# dev f0954773（含 25f4134f）的 Frida 测试在 x86 上

Jetson 会话（bpftime-arm64环境-性能优化）在 NUC 上跑的检查，时间是 11/04 22:19–22:23。这个目录只收了它的输出，没有重跑。

- 分支：`dev/uprobe-official-177a1dc465ff`，提交 f09547739cdb4f68269018cf8d929d8afedd55bd。
- 25f4134f 的改动：程序运行期间不再卸载 Frida（Gum）钩子，改为"停放"。停放的意思是钩子留在函数上，但不再调用探针回调；卸载要等正在执行的回调都结束才返回。
- 编译：只编译了 `bpftime_frida_uprobe_attach_tests`，用 gcc-13 13.3.0，RelWithDebInfo + LTO，cmake 参数和本分支其他 x86 构建相同。完整参数见 `jetson-check-25f4134f.sh`。没有固定 CPU 和频率，所以下面的调用速度只能看量级。

| 文件 | 内容 |
|---|---|
| `jetson-check-25f4134f.sh` | Jetson 会话写的脚本：克隆、配置、编译、跑测试 |
| `jetson-check-25f4134f.log` | 脚本输出 |
| `configure.log`、`build.log` | cmake 配置和编译日志；有 1 个警告（`test_detach_while_called.cpp:156`，对 volatile 用 `++` 已被弃用） |
| `suite.1.txt`–`suite.3.txt` | 整套测试跑 3 遍的输出 |

## 结果

- 整套测试：3 遍都是 23 个用例全过（1102 个断言）。
- 新增的 3 个用例各单独跑 5 遍，全过：
  - `Detaching probes while threads call the function keeps them running`
  - `No probe callback runs after its detach returned`
  - `Probes attached again after a detach run`
- `Hook cost attached and parked` 测的测试函数调用速度（次/秒）：

| 状态 | 1 线程 | 6 线程 |
|---|---:|---:|
| 不挂钩子（原生） | 4.08 亿 | 19.0 亿 |
| 挂着空的 uprobe | 753 万 | 1334 万 |
| 卸载之后 | 911 万 | 1329 万 |

- 日志里是 `parked=0`。按 Jetson 会话的说法，x86 上没有走停放。但卸载之后，函数仍然比原生慢约 45 倍（1 线程），和挂着空 uprobe 时差不多，说明卸载后调用还在经过钩子。这里只记录，交给 Jetson 判断。

检查结束后，克隆和构建目录（640 MB）已删除。
