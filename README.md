# BPFtime x86 NUC 文档索引

本分支放用户 x86 NUC（`debian-ichi-nuc`：Intel i5-1240P，Debian 13，内核 6.12）上的 bpftime 工作的结论。
原始数据在 [`benchmark-results/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/x86-nuc)；
Jetson ARM64 的课题文档在 [`summry/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/summry/jetson)（研究总览、接力文档、真实程序报告）。

| 文档 | 内容 |
|---|---|
| [x86 报告（10/26）](x86-nuc/bpftime-x86-nuc-build-regression-20261026.md) | 构建与回归（LLVM 19）、交给 Jetson 会话的问题（第 4 节）、uprobe 完整十轮（第 5 节）、ssl-nginx 与 7 档扫测（第 6 节） |
| [新会话交接说明（Jetson 会话写，10/26）](x86-nuc/bpftime-x86-nuc-handoff-20261026.md) | 机器、构建选项、候选目标、用户定的规矩 |

## 一页结论（开发分支 `a7d54d7e` = 官方 177a1dc + 73 个提交）

- **构建与正确性**：官方和 `a7d54d7e` 用 LLVM 19 都能编；每日过滤器 110 个用例、Frida 19 个用例全过，含 x86 走 gum 回溯器的 uretprobe 取栈。
- **uprobe 完整十轮（CPU 2 钉 4.4 GHz）**：10 项 map 操作全部快于同轮 kernel（0.25–0.81×），官方只有 hash update/delete 快于 kernel；
  hash lookup/update/delete 倍率与 Jetson 几乎相同。x86 kernel uprobe 约 1.1 µs/次，bpftime 124 ns。
- **ssl-nginx**：相对 kernel +48.0%（16 B）/ +27.9%（256 KB），7 档扫测从 +50% 收窄到 +26%（Jetson +17% → +12%）；
  官方在 x86 上也快于 kernel，但从 1 KB 起就丢事件。
- **交给 Jetson 会话**：`bpf_get_stackid` 的错误在 64 位返回值里仍是正数（`d766e6ad`）；开发分支在只有 clang-19 的机器上配置失败。
- **这台机器固定频率的坑**：核心从 C6 及更深空闲状态醒来、以及改完频率设置后的头几秒会跑在 1.4 GHz；多个核同时忙时 4.4 GHz 撑不住。
