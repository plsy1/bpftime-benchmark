# x86 NUC BPFtime benchmark 结果

本分支只放用户 x86 NUC（`debian-ichi-nuc`：Intel i5-1240P，4 个 P 核 + 8 个 E 核，Debian 13，内核 6.12）上的原始结果、脚本和环境记录。
Jetson ARM64 的结果在 [`benchmark-results/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/jetson)；
x86 的结论在 [`summry/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/summry/x86-nuc)。

对比的三方：kernel eBPF、bpftime 官方 177a1dc、开发分支 `dev/uprobe-official-177a1dc465ff` 的 `a7d54d7e`（官方 + 73 个提交）。
开发分支 10/27 由 Jetson 会话从 `codex/uprobe-official-177a1dc465ff` 改名；本分支的测量都在 `a7d54d7e` 上，之后的提交（含 x86 发现的修复）未在 x86 上重测。

| 目录 | 内容 |
|---|---|
| [uprobe/x86-nuc-build-regression-20261026](uprobe/x86-nuc-build-regression-20261026/) | LLVM 19 下两棵树的构建、每日过滤器单测、Frida 测试、逐用例的完整 runtime 单测 |
| [uprobe/x86-nuc-full10-20261026](uprobe/x86-nuc-full10-20261026/) | 官方 uprobe benchmark 完整十轮（CPU 2 钉 4.4 GHz），10 项 map 操作与探针开销 |
| [uprobe/x86-nuc-ssl-nginx-20261026](uprobe/x86-nuc-ssl-nginx-20261026/) | ssl-nginx 16 B / 256 KB，2 轮 × 10 次 |
| [uprobe/x86-nuc-ssl-nginx-sweep-20261026](uprobe/x86-nuc-ssl-nginx-sweep-20261026/) | ssl-nginx 7 档 payload 扫测，每档 5 次，含图 |

说明：
- 各 README 里提到的 Jetson 目录（`btf-kernel-20261026`、`ssl-nginx-2x2-20261023`、`ssl-nginx-sweep-bca2ebce-20261024`、
  `baseline-177a1dc-jit-llvm16-20261021` 等）都在 `benchmark-results/jetson` 分支的 `uprobe/` 下。完整十轮汇总用到的 Jetson 数据复制在
  `uprobe/x86-nuc-full10-20261026/jetson-reference/`。
- 这些结果 10/26 先提交在 `benchmark-results/jetson`（提交 `86fadc91`..`77a1df2f`），当天按用户要求搬到本分支；
  当时脚本运行在该分支的工作区 `/home/y/src/bpftime-wt-results`，搬家后脚本里的绝对路径改成了本分支工作区 `/home/y/src/bpftime-wt-results-x86`，内容不变。
- 冻结的运行时产物留在 NUC 本机 `/home/y/src/bpftime-x86-runs/`，未提交；sslsniff 原始抓包文本（21 GB）10/26 已删除，仓库里有首尾片段。
