# 在 x86 NUC 上跑 bpftime：给新会话的交接（2026-10-26）

写给在 NUC 上新开的 Claude 会话。Jetson 上的会话看不到你，你也发不了消息给它；两边只通过本仓库
（`plsy1/bpftime-benchmark`）交接。**回复用户用中文。**

## 0. 背景一段话

用户的课题是在 Jetson Orin（ARM64）上优化和验证 bpftime（用户态 eBPF 运行时，eunomia-bpf/bpftime，OSDI 论文），
处处与 kernel eBPF 对比。从 6 月到 10/26 做了：基础设施、ssl-nginx 端到端调查、uprobe map 诊断、C++ 实现层优化、
array lookup 的 JIT 内联、hash map 优化、真实工具兼容性审计与 18 个修复、BTF 内核上扩到 18 个工具的审计。
开发分支 `codex/uprobe-official-177a1dc465ff`（基于官方 177a1dc，HEAD `a7d54d7e`）在 ARM64 上 uprobe 微基准、
ssl-nginx、真实工具都快于 kernel。现在用户想在这台 x86 NUC 上也跑 bpftime。具体目标由用户定（见第 4 节的候选）。

## 1. 先读这些（`summry/jetson` 分支）

1. `bpftime-research-overview.md`：全过程与结论，按时间排。
2. `uprobe/bpftime-uprobe-handoff-20261021.md` 第 0 节（现状一页、提交表、结论、工作方式、上游 PR 计划）和第 2–3 节（JIT 设计边界、易踩坑）。
3. `realprog/bpftime-realprog-compatibility-20261026.md`：18 个修复逐项（现象、根因、哪些与 ARM 有关），第 8 节是 BTF 内核上的 18 工具审计。
4. 开发分支根目录 `CLAUDE.md`：每个修复和优化的设计约束（宿主进程契约、array JIT、fork、perf 输出、栈、printk、shm 大小等）。改代码前必读。

原始数据在 `benchmark-results/jetson` 分支 `uprobe/` 下（最近的：`btf-kernel-20261026`、`realprog-*`、`ssl-nginx-*`）。

## 2. 这台机器（10/26 从 Jetson 上 ssh 核对）

| 项 | 值 |
|---|---|
| 主机 | `debian-ichi-nuc`，局域网 192.168.1.46（与 Jetson 同一路由器），用户 `y`，sshd 端口 2222（只收密钥） |
| CPU | Intel i5-1240P：CPU 0–7 是 4 个 P 核（每核 2 线程，最高 4.4 GHz），CPU 8–15 是 8 个 E 核（3.3 GHz） |
| 内存 / 磁盘 | 62 GB / 433 GB（用了 5%） |
| 系统 | Debian 13 trixie，内核 6.12.113+deb13-amd64；BTF、`FTRACE_SYSCALLS`、`UPROBES` 都有 |
| 频率 | `intel_pstate`，governor `powersave`，turbo 开着 |
| 已有 | git、gcc/g++、make、python3、`/usr/lib/llvm-19`、`~/src`（`bpf-study`、`linux`）；免密 sudo 可用 |
| 没有 | cmake、ninja、clang 命令、bpftool、bpftrace、docker、Claude Code（PATH 里没有，若桌面端 SSH 连接需要请用户装） |

Jetson 的 `~/.ssh/id_ed25519` 已加入 `y` 的 `authorized_keys`，Jetson 上 `ssh nuc` 可登录。
8 月的 x64 对照数据（`*-x64-2026080*`）来自另一台 i7-8750H，不是这台。

**测量前固定频率**（大小核 + turbo 会让数字乱跳）：把被测进程和 bench 绑在同一个 P 核的一个线程上（不要用它的 SMT 兄弟线程，
也不要落到 E 核），关 turbo（`/sys/devices/system/cpu/intel_pstate/no_turbo`）、把该核 min/max 频率钉死，记录在结果里；
跑之前先确认，测完恢复。和 Jetson 的 `jetson_clocks` 同理。

## 3. 代码、分支、构建

- 仓库 `https://github.com/plsy1/bpftime-benchmark`（用户账号 plsy1）：
  - `codex/uprobe-official-177a1dc465ff`：开发分支（需 `git submodule update --init --recursive`）。
  - `automation/jetson-daily`：每日补丁栈，`tools/jetson-upstream/config.json` 的 `candidate.commits` 是 73 个提交，
    `build.test_filter` 是单测过滤器（Jetson 上按它跑出 110 个用例），`build.cmake_args` 是构建选项；用来在官方新版本上重放。
  - `benchmark-results/jetson`：原始数据；`summry/jetson`：文档；`docs/source-reading-guide`：按官方 177a1dc 写的源码阅读指南（9 章）。
- 官方基线：`eunomia-bpf/bpftime` 提交 `177a1dc4`（"fix(ringbuf): keep a record's header and payload contiguous..."）。
- Jetson 的 CMake 选项（`benchmark-results/jetson` 的 `uprobe/baseline-177a1dc-jit-llvm16-20261021/cmake-options.txt`）：
  `CMAKE_BUILD_TYPE=RelWithDebInfo`、`BPFTIME_LLVM_JIT=ON`、`BPFTIME_UBPF_JIT=ON`、`BPFTIME_ENABLE_LTO=ON`、`BPFTIME_BUILD_WITH_LIBBPF=ON`、
  `BPFTIME_BUILD_KERNEL_BPF=ON`、`BPFTIME_ENABLE_UNIT_TESTING=ON`、`BPFTIME_ENABLE_FRIDA=ON`、gcc-13，LLVM 16（每日回放用 LLVM 15）。
  x86 上尽量用同样的选项，LLVM 版本写进结果；Debian 13 自带 LLVM 19，若 llvmbpf 不兼容再装 16。
- **Boost 版本要记录**：Boost 1.83 创建共享内存时 `posix_fallocate` 整段提交，`BPFTIME_SHM_MEMORY_MB` 是常驻内存（开发分支默认 128 MB）。
- 开发分支的 llvmbpf 修改是父仓库的补丁（`vm/compat/llvm-vm/*.patch`，配置时生成到构建目录），不改子模块；补丁冲突会让配置失败，属于预期。

## 4. 可以做的事（用户还没定，按价值排的建议）

1. **x86 上的构建与回归**：编官方 177a1dc 和开发分支，跑单测（过滤器取自 `config.json`）和 frida 测试。确认 18 个修复和各项优化
   在 x86 上也成立：其中只有 aarch64 帧指针取栈（`64da197c`）和 PAC 去签名（`f2596ce8`）是 ARM 专有路径，x86 仍走 gum 回溯器。
2. **真实工具审计搬到 x86**：脚本和工作负载在 `benchmark-results/jetson` 的 `uprobe/btf-kernel-20261026/realprog-audit/src/`
   （`harness/audit.py`、`measure.py`，`workloads/*.c`、`GcLoop.java`；路径常量在文件头，需改）。x86 有系统调用改写跳板，
   **syscall tracepoint 类工具（opensnoop、syscount）在 x86 上应该能跑**，ARM64 上是静默无事件；javagc 需要 `bpftime attach`（frida CLI，需另编）。
3. **性能对照**：uprobe 完整十轮（`benchmark/uprobe/benchmark.py --iter 10`，含 kernel 腿）、真实工具吞吐、ssl-nginx（Jetson 用 docker 镜像
   `bpftime-ssl-nginx:20261023`，x86 需自己建）。看开发分支相对官方、相对 kernel 的倍率在 x86 上是否与 ARM64 一致。
4. **为 ARM64 的缺口做参照**：x86 的系统调用跳板（`attach/text_segment_transformer`）是将来写 aarch64 版本的参照。

## 5. 用户定下的规矩（必须遵守）

- 回复用中文；自起的标签（如 "dev2"、"mixed"）第一次出现要用白话解释；汇报用提交号加描述。
- **推送前先问用户**（用户明确说推送时才推）；补丁栈改动攒几轮再加，也要先问。
- 每个修复配一个"不修就失败"的测试，并在官方原版上确认问题是否已存在；BPF 程序和工具原样不动，问题在 bpftime 里修。
- 测量：同轮测 kernel；固定 CPU 和频率；配对 A/B、轮换顺序；**测量时不跑任何其他 CPU 密集任务，两个测量不同时跑**；
  测量用的 docker 容器跑完即删（镜像保留）；异常差值用 `perf stat`（每次调用指令数 + IPC）区分真实开销和代码布局。
- 任何 benchmark 先审计 bpftime 那一组确实收到了事件，再采信吞吐。
- 经常同步 GitHub（Jetson 的 SD 卡坏过）：日志、配置、脚本也要备份；大于 1 MB 的原始输出裁成首尾片段再提交。
- 不对变量或相对路径用 `rm -rf`（用明确的绝对路径或 `"${VAR:?}"`）。
- 上游 PR 暂缓（计划见接力文档 0.6），先做眼前的工作。

## 6. 结果放哪

- 原始数据放 `benchmark-results/jetson` 分支，新目录用 `x86-nuc-<主题>-<日期>`（例如 `uprobe/x86-nuc-baseline-20261027`），README 写明机器、
  频率设置、构建选项和提交号。
- 结论写进 `summry/jetson` 分支：新建 `x86-nuc/` 下的报告，并在 `README.md` 和研究总览里加入口。
- Jetson 会话会从仓库读你的结果；需要跨机器对比时，请写清楚两边的条件差异（CPU 架构、频率、内核版本、LLVM 版本）。
