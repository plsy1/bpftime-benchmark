# x86 NUC BPFtime benchmark 结果

本分支只放用户 x86 NUC（`debian-ichi-nuc`：Intel i5-1240P，4 个 P 核 + 8 个 E 核，Debian 13，内核 6.12）上的原始结果、脚本和环境记录。
Jetson ARM64 的结果在 [`benchmark-results/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/jetson)；
x86 的结论在本分支的 [`reports/`](reports/)，见下面"报告"一节。

对比的三方：kernel eBPF、bpftime 官方 177a1dc、开发分支 `dev/uprobe-official-177a1dc465ff` 的 `a7d54d7e`（官方 + 73 个提交）。
开发分支 10/27 由 Jetson 会话从 `codex/uprobe-official-177a1dc465ff` 改名，11/10 再改名为 `archive/uprobe-official-177a1dc465ff`；
11/10 起的开发分支是 `dev/uprobe-official-4708f4092d1e`（每日补丁栈重放到官方 4708f409），本分支 11/10 之前的结果都不在它上面。
10/26 的四个 `uprobe/` 目录在 `a7d54d7e` 上，11/02 起的 `uprobe/` 目录各自在 README 里写明提交；`syscall/` 下的在 `7a7d29b8` 上（`x86-nuc-dispatch-fix-20261029` 在 `2e850c45` 上，`x86-nuc-syscount-nginx-20261029` 在 `59e5e154` 上）。

| 目录 | 内容 |
|---|---|
| [uprobe/x86-nuc-build-regression-20261026](uprobe/x86-nuc-build-regression-20261026/) | LLVM 19 下两棵树的构建、每日过滤器单测、Frida 测试、逐用例的完整 runtime 单测 |
| [uprobe/x86-nuc-full10-20261026](uprobe/x86-nuc-full10-20261026/) | 官方 uprobe benchmark 完整十轮（CPU 2 钉 4.4 GHz），10 项 map 操作与探针开销 |
| [uprobe/x86-nuc-ssl-nginx-20261026](uprobe/x86-nuc-ssl-nginx-20261026/) | ssl-nginx 16 B / 256 KB，2 轮 × 10 次 |
| [uprobe/x86-nuc-ssl-nginx-sweep-20261026](uprobe/x86-nuc-ssl-nginx-sweep-20261026/) | ssl-nginx 7 档 payload 扫测，每档 5 次，含图 |
| [syscall/x86-nuc-build-20261029](syscall/x86-nuc-build-20261029/) | 开发分支 `7a7d29b8`（ARM64 系统调用改写）在 x86 上的构建与每日过滤器单测（113/113） |
| [syscall/x86-nuc-breakdown-20261029](syscall/x86-nuc-breakdown-20261029/) | 每次系统调用开销拆层（Jetson 的 `run_layers.py`），1.7 / 4.4 GHz，与 Jetson 对比 |
| [syscall/x86-nuc-official-syscall-20261029](syscall/x86-nuc-official-syscall-20261029/) | 官方 benchmark/syscall，1.7 / 4.4 GHz（含一次因单核绑定作废的记录） |
| [syscall/x86-nuc-syscall-session-20261029](syscall/x86-nuc-syscall-session-20261029/) | 上面两项的驱动脚本、钉频与 `vm.mmap_min_addr` 记录 |
| [syscall/x86-nuc-dispatch-fix-20261029](syscall/x86-nuc-dispatch-fix-20261029/) | 分发修复 `2e850c45` 的重测：构建与单测、拆层、官方 benchmark/syscall、perf，与修复前对比 |
| [syscall/x86-nuc-syscount-nginx-20261029](syscall/x86-nuc-syscount-nginx-20261029/) | syscount-nginx（`59e5e154`）：pthread key 检查、审计、x86 跳板寄存器问题、官方脚本对照（容器，CPU 0–7 钉 1.7 GHz） |
| [uprobe/x86-nuc-dev-6ab9cbfe-20261102](uprobe/x86-nuc-dev-6ab9cbfe-20261102/) | dev `6ab9cbfe` 的官方 uprobe 十轮和 ssl-nginx |
| [uprobe/x86-nuc-dev-a4ac0d31-20261102](uprobe/x86-nuc-dev-a4ac0d31-20261102/) | dev `a4ac0d31` 的编译和测试 |
| [uprobe/x86-nuc-dev-42595149-20261103](uprobe/x86-nuc-dev-42595149-20261103/) | dev `42595149` 复测 |
| [uprobe/x86-nuc-dev-d5279469-20261103](uprobe/x86-nuc-dev-d5279469-20261103/) | dev `d5279469`（hash map 重写）的测试和测量 |
| [uprobe/x86-nuc-r1-miss-20261104](uprobe/x86-nuc-r1-miss-20261104/) | hash map 更新期间漏查（审查 R1），bpf 对照与 bpftime 修复 |
| [uprobe/x86-nuc-realtools-20261104](uprobe/x86-nuc-realtools-20261104/) | 多线程真实工具（bpftime 示例 malloc / funclatency） |
| [uprobe/x86-nuc-frida-park-f0954773-20261104](uprobe/x86-nuc-frida-park-f0954773-20261104/) | dev `f0954773` 的 Frida 测试（钩子停放），Jetson 会话在 NUC 上跑 |
| [uprobe/x86-nuc-dev-9c088441-20261107](uprobe/x86-nuc-dev-9c088441-20261107/) | dev `9c088441` 的测试，对照 `d46ceb8f`；顺带记下 junit 输出时的 double free |
| [uprobe/x86-nuc-dev-c9d4d974-20261108](uprobe/x86-nuc-dev-c9d4d974-20261108/) | dev `c9d4d974`（AOT 缓存）的测试 |
| [uprobe/x86-nuc-dev-af4a1a8d-20261110](uprobe/x86-nuc-dev-af4a1a8d-20261110/) | dev `af4a1a8d`（GPU 修复轮）的测试 |

## 报告

x86 的结论在 [`reports/`](reports/)，入口是 [`reports/README.md`](reports/README.md)（文档索引和一页结论）；报告正文在 [`reports/x86-nuc/`](reports/x86-nuc/)。
这些文档原来在单独的 `summry/x86-nuc` 分支上，11/10 用子树合并并进本分支（合并提交保留了它的历史）。
Jetson ARM64 的报告同样并进了 [`benchmark-results/jetson` 的 `reports/`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/jetson/reports)。

说明：
- 各 README 里提到的 Jetson 目录（`btf-kernel-20261026`、`ssl-nginx-2x2-20261023`、`ssl-nginx-sweep-bca2ebce-20261024`、
  `baseline-177a1dc-jit-llvm16-20261021` 等）都在 `benchmark-results/jetson` 分支的 `uprobe/` 下。完整十轮汇总用到的 Jetson 数据复制在
  `uprobe/x86-nuc-full10-20261026/jetson-reference/`。
- 这些结果 10/26 先提交在 `benchmark-results/jetson`（提交 `86fadc91`..`77a1df2f`），当天按用户要求搬到本分支；
  当时脚本运行在该分支的工作区 `/home/y/src/bpftime-wt-results`，搬家后脚本里的绝对路径改成了本分支工作区 `/home/y/src/bpftime-wt-results-x86`，内容不变。
- 冻结的运行时产物留在 NUC 本机 `/home/y/src/bpftime-x86-runs/`，未提交；sslsniff 原始抓包文本（21 GB）10/26 已删除，仓库里有首尾片段。
