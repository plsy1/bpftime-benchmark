# BPFtime x86 NUC 文档索引

本分支放用户 x86 NUC（`debian-ichi-nuc`：Intel i5-1240P，Debian 13，内核 6.12）上的 bpftime 工作的结论。
原始数据在 [`benchmark-results/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/x86-nuc)；
Jetson ARM64 的课题文档在 [`summry/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/summry/jetson)（研究总览、接力文档、真实程序报告）。

| 文档 | 内容 |
|---|---|
| [x86 报告（10/26）](x86-nuc/bpftime-x86-nuc-build-regression-20261026.md) | 构建与回归（LLVM 19）、交给 Jetson 会话的问题（第 4 节）、uprobe 完整十轮（第 5 节）、ssl-nginx 与 7 档扫测（第 6 节） |
| [新会话交接说明（Jetson 会话写，10/26）](x86-nuc/bpftime-x86-nuc-handoff-20261026.md) | 机器、构建选项、候选目标、用户定的规矩 |
| [系统调用追踪对照（10/29）](x86-nuc/bpftime-x86-nuc-syscall-20261029.md) | 开发分支 `7a7d29b8` 在 x86 上的构建、拆层测量、官方 benchmark/syscall，与 Jetson 对比 |
| [交给 Jetson 的发现（10/29）](x86-nuc/bpftime-x86-nuc-syscall-findings-20261029.md) | 没有要修的 bug；官方 benchmark 不能单核绑定；分发层和跳板的跨架构周期数 |
| [分发修复重测（10/29 下午）](x86-nuc/bpftime-x86-nuc-dispatch-fix-20261029.md) | 开发分支 `2e850c45` 修复前后对比：拆层、官方 benchmark/syscall、perf |
| [分发修复的发现（10/29 下午）](x86-nuc/bpftime-x86-nuc-dispatch-fix-findings-20261029.md) | 没有要修的 bug；剩下的 TLS 与 zpoline nop 滑道 |
| [syscount-nginx（10/29 晚）](x86-nuc/bpftime-x86-nuc-syscount-nginx-20261029.md) | 开发分支 `59e5e154`：pthread key 修复在 x86 上有效；官方脚本对照；与 Jetson、论文、用户 7 月结果对比 |
| [syscount-nginx 的发现（10/29 晚）](x86-nuc/bpftime-x86-nuc-syscount-nginx-findings-20261029.md) | **需要修**：x86 跳板不保留系统调用要保持的寄存器（上游 bug），nginx 因此起不来 |
| [x86 跳板寄存器修复的验证（10/29 夜）](x86-nuc/bpftime-x86-nuc-x86-hook-registers-findings-20261029.md) | **需要修**：`0a602133` 在 x86 上 GCC LTO 链接失败（`bpftime_xsave_area_size` 被删）；修复前测试在 LTO 下也链接不过 |
| [x86 跳板寄存器修复的验证（续，10/29 夜）](x86-nuc/bpftime-x86-nuc-x86-hook-registers-findings-2-20261029.md) | `e299cd3f` 修复有效（regcheck 0，nginx 正常，计数一致）；**需要修**：新测试没注册 capstone，必然失败；只加载跳板就 −11.9% |
| [x86 跳板寄存器修复的验证（续二，10/29 夜）](x86-nuc/bpftime-x86-nuc-x86-hook-registers-findings-3-20261029.md) | `c23442ac` 测试 3/3、变异后失败（但抓不到参数寄存器）；修复每次系统调用固定多约 255 周期（XSAVE+XRSTOR 约 230） |

## 一页结论（开发分支 `a7d54d7e` = 官方 177a1dc + 73 个提交）

- **构建与正确性**：官方和 `a7d54d7e` 用 LLVM 19 都能编；每日过滤器 110 个用例、Frida 19 个用例全过，含 x86 走 gum 回溯器的 uretprobe 取栈。
- **uprobe 完整十轮（CPU 2 钉 4.4 GHz）**：10 项 map 操作全部快于同轮 kernel（0.25–0.81×），官方只有 hash update/delete 快于 kernel；
  hash lookup/update/delete 倍率与 Jetson 几乎相同。x86 kernel uprobe 约 1.1 µs/次，bpftime 124 ns。
- **ssl-nginx**：相对 kernel +48.0%（16 B）/ +27.9%（256 KB），7 档扫测从 +50% 收窄到 +26%（Jetson +17% → +12%）；
  官方在 x86 上也快于 kernel，但从 1 KB 起就丢事件。
- **交给 Jetson 会话**：`bpf_get_stackid` 的错误在 64 位返回值里仍是正数（`d766e6ad`）；开发分支在只有 clang-19 的机器上配置失败。
- **这台机器固定频率的坑**：核心从 C6 及更深空闲状态醒来、以及改完频率设置后的头几秒会跑在 1.4 GHz；多个核同时忙时 4.4 GHz 撑不住。

## 系统调用追踪（10/29，开发分支 `7a7d29b8`）

- **构建与正确性**：x86 上能编译（含 `bpftime_text_segment_transformer`），每日过滤器 113/113，zpoline 计数 1,000,000 / 1,000,000。
- **分发层**：没有匹配程序也要约 220 周期（1.7 GHz 130 ns，4.4 GHz 51 ns），Jetson 349 周期（202 ns）；两边都是最大的一层。
- **跳板**：zpoline 约 110 周期，ARM64 逐处跳板 65 周期。
- **官方 benchmark/syscall**：1.7 GHz kernel +57.8 / bpftime +224.2 ns（3.9×），4.4 GHz +20.9 / +87.4 ns（4.2×），Jetson +84.8 / +286.6 ns（3.4×）。
- **分发修复（`2e850c45`）**：分发 221 → 41 周期（Jetson 349 → 48）；挂一个空程序时 bpftime 386 → 263 周期；
  官方 benchmark/syscall 倍数 3.9× → 2.8×（1.7 GHz）、4.2× → 2.9×（4.4 GHz）。剩下的大头是 zpoline 的 nop 滑道（每次 write 约 130 周期）。
- **syscount-nginx（`59e5e154`）**：pthread key 冲突在 x86 上也有，已修好；但 x86 跳板调用 C 代码后不恢复 rdi/rsi/rdx/r10/r8/r9 和 xmm
  （官方 177a1dc 也是），挂着程序时 nginx 启动即崩，正确跑法测不了。官方脚本原样：内核 −4.3% / −2.9%，bpftime −1.1% / −1.6%（什么都没追踪）。
- **x86 跳板寄存器修复（`0a602133`）**：在 x86 上编不过（GCC LTO 删掉了只被 asm 读的 `bpftime_xsave_area_size`），已交给 Jetson；测试、regcheck、nginx 和正式测量待修复后再做。
- **x86 跳板寄存器修复（`e299cd3f`）**：链接问题已解决，寄存器修复在 x86 上有效。syscount-nginx 正确跑法（10 轮，1.7 GHz）：
  内核 −5.3% / −3.6%，bpftime −18.7% / −17.1%（计数 = 内核），只加载跳板 −11.9%（Jetson −11.4% / −9.0% / −3.4%）。新测试因没注册 capstone 必然失败，已交给 Jetson。
- **寄存器修复的成本（`c23442ac`，写循环，1.7 GHz）**：每次系统调用固定多约 255 周期（≈150 ns），其中 XSAVE + XRSTOR 约 230；
  跳板层从约 110 周期涨到约 370 周期。nginx 里只加载跳板的 440 周期 ≈ 修复 255 + nop 滑道 118 + 分发 41。
