# 当前稳定版本 array lookup 重新 profile

日期：2026-10-19。基线：`fefe2f6edf276e1b2957711203b5022dd1be8d7c`。
本次只诊断，没有新的生产代码改动，也没有提交或推送。

## 结论

1. 当前普通 array lookup 的成功路径已经是无调用、无栈帧的专用 helper；
   没看到早期临时对象、堆分配、`std::function` 间接调用那类开销。
2. 剩余明显热点是 **map 定位到元数据读取的串行依赖链**，并不只是
   `key * value_size` 的取址算术。两次普通 array 采样中，约 40% 的目标样本
   落在 `cmp w2,w0`；其前一条是通过刚解码的 map implementation 指针加载
   `_max_entries`。这支持“检查附近存在数据依赖等待”的假设，不能证明
   `cmp` 本身耗费了 40% 时间，也不能据此删除边界检查。
3. per-CPU 还有 CPU 查询与调用约定开销：libc `sched_getcpu` 函数内占
   18.60–18.95% 的目标样本，PLT 占 1.69–1.74%，helper 调用点占
   0.72–0.79%。寄存器保存/恢复与函数进出区域占 9.51–9.78%。
   这些指令区域不能当成可独立相加、可全部消除的优化收益。
   两次 libc 函数内的样本均落在 TLS 快路径，未采到 syscall fallback 区域；
   因此不能将当前这部分开销简单描述成“每次都陷入内核”。
4. 两个采样周期下的热点排序与占比基本一致。下一步应优先审查
   **普通 array lookup 的指针/元数据依赖链及 C++ 表达、内联后的调度**，
   而不是继续重复已经失败的 per-CPU 拆函数、提前计算地址路线。
   目前定位到了方向，还没有验证一个确定有效的新补丁。

保持约束：不改共享内存布局，不缓存可能失效的裸指针，不跳过 fd、类型、
key 范围检查，不固定 CPU 或更改迁核语义。任何候选都应独立做正确性检查和
短轮 A/B，成功后才进入完整十轮。

## 真实生产路径采样

使用不可变的已接受 agent/server，未修改 BPF 对象或 victim。CPU 5 固定
1728 MHz。每次 2,000,000 次 BPF invocation，每次循环 1000 个 helper，
key 为 0–999；延迟 5 秒，再采 5 秒。普通/per-CPU 各两次，采样周期分别为
1,728,000 和 1,572,863 cycles。按目标 PID 的 top-frame PC 计数，结合实际
进程映射消除 ASLR，再对照相同 ELF 的反汇编。

| 观测项 | 普通 array 第 1 / 2 次 | per-CPU array 第 1 / 2 次 |
| --- | ---: | ---: |
| 目标 top-frame 样本数 | 4,975 / 5,463 | 4,972 / 5,461 |
| manager / fd / 类型解析指令区域 | 23.32% / 22.42% | 33.99% / 34.68% |
| key / bounds 指令区域 | 43.06% / 43.03% | 13.11% / 11.88% |
| data 指针及地址计算区域 | 12.62% / 13.67% | 9.84% / 10.62% |
| libc CPU 查询函数内 | — | 18.95% / 18.60% |

这是选取的指令区域，不是完整分解表；完整数据见
[profiles-analysis.md](profiles-analysis.md)、[profiles-summary.json](profiles-summary.json)
和 [profile-instructions.csv](profile-instructions.csv)。ARM sampling skid、
乱序执行与跨 JIT 栈回溯限制都需要保留。采样期间的 wall time 不用于证明优化。

## 独立分层诊断：支持证据，不是逐层精确耗时

在源码树之外复制现有 harness，补上当前真正使用的专用 helper `l4`。
每项 6 轮，每轮 2000 万操作，轮换测量顺序；以下为未扣 control 的均值。

| 独立诊断入口 | 普通 array（ns/op） | per-CPU array（ns/op） |
| --- | ---: | ---: |
| Control：间接调用及循环对照 | 2.911 | 2.910 |
| l0：直接 map implementation | 3.117 | 6.738 |
| l1：通用 handler | 8.116 | 12.659 |
| l2：通用 shared-memory API | 21.160 | 24.652 |
| l3：通用 helper | 20.096 | 23.435 |
| l4：当前专用 helper | 7.759 | 11.398 |

per-CPU 合成对照：固定 CPU 取址 3.300 ns、查询 CPU 后取址 7.091 ns、
通过当前模板 callable 取址 6.920 ns。固定 CPU 只用于诊断，绝不能作为生产替代。

以上说明：旧的通用 helper 分层数据不代表当前专用路径；当前模板封装也没有
显示出早期 `std::function` 一类明显额外开销。独立程序的代码布局、调用边界、
循环与生产 JIT 不同，静态链接时亦有既存 `ebpf_inst` LTO ODR 警告；只作交叉线索，
不能把这些值与正式 benchmark 混用或用相邻层之差声称某层的精确成本。
原始数据：[layers/measurements.csv](layers/measurements.csv)。

## 实验状态和复现注意事项

- 正式 runtime 源码保持干净；原有 benchmark 诊断文件的用户修改完整保留。
- 生产 agent/server、loader/victim/BPF object 的采样前后校验均通过。
- 4 份 SQLite 均含有效目标 CPU 5 样本，6 轮独立诊断全部完成。
- 前两次 capture 脚本收尾返回 1：这版 nsys 在窗口结束后先返回，victim
  尚未刷新最终 CSV，导致立即检查误报失败；报告已生成，CSV 随后正常输出。
  已改成等待 CSV 后再清理。原始失败状态没有改写，另由
  `verify-captures.sh` 复核全部报告、输出和二进制校验。
- resume 脚本另一次校验因工作目录错误未找到相对路径，已修正；独立复核通过。
- `run-profiles.sh` 是首次采样入口；中断恢复过程使用 `resume-profiles.sh`
  与 `verify-captures.sh`。重跑应新建输出目录，不能覆盖已有报告。
- 当前没有继续运行的 benchmark。tmux 会话 `uprobe-array-reprofile-20261019`
  留存日志；所有本轮生产 loader/目标及诊断进程均已退出，默认共享内存已清理。

这次没有新优化，因此仍沿用已接受十轮结果：普通 array lookup
**7.47 ns / kernel 3.81 ns = 1.96×**；per-CPU array lookup
**10.76 ns / kernel 4.40 ns = 2.45×**。其他八项未重新测量，不能更新十项大表。
