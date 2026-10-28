# x86 构建与每日过滤器回归：开发分支 7a7d29b8（2026-10-29，NUC）

目的：Jetson 在 `7a7d29b8`（ARM64 系统调用改写）里给 `text_segment_transformer` 的 x86 部分只加了 `#if` 隔离，
没在 x86 上编译过。这里确认它在 x86 上能编译，并且每日过滤器的单测没有回归。

- 机器：`debian-ichi-nuc`，i5-1240P，Debian 13，内核 6.12.113+deb13-amd64。
- 源码：开发分支 `dev/uprobe-official-177a1dc465ff` 快进到 `7a7d29b8`（`source-state.txt`，干净），
  `vm/llvm-jit` 子模块 `87bac39a`。
- 构建：`run-build.sh`，选项与 10/26 相同（`../../uprobe/x86-nuc-build-regression-20261026/run-build.sh`：
  RelWithDebInfo、LTO、LLVM JIT + ubpf、单测开、probe 检查关；gcc-13/g++-13，LLVM 19.1.7，BPF clang 19），
  构建目录 `build-7a7d29b8`（新目录，不复用 10/26 的 `build-jit`）。目标比 10/26 多了
  `bpftime_text_segment_transformer`，少了 uprobe 的两个 benchmark/Frida 测试目标。`cmake-options.txt` 是实际生效的选项。
- 结果：构建退出码 0（`build-exit-status.txt`，`build.log`）。`libbpftime-agent-transformer.so` 正常链接，
  transformer 的两个源文件没有任何警告；全部 10 条警告和 10/26 同类（ODR、`optnone`、`-Wcomment`、构造函数优先级）。
  （`bpftime_text_segment_transformer_tests` 只在 aarch64 上生成，x86 没有这个目标。）
- 每日过滤器：`test-filter.txt` 取自 `automation/jetson-daily` 的 `tools/jetson-upstream/config.json`
  （提交见 `test-filter-source-commit.txt`），比 10/26 多一项 `get_stackid returns errors as negative 64-bit values`。
  **113/113 通过**（`tests-filter.log`，退出码 0）。`tests-filter-names.txt` 是选中的用例；和 10/26 的构建
  （同一过滤器）相比多出的 3 个都是新测试：`get_stackid returns errors as negative 64-bit values`、
  `per-cpu hash program updates follow kernel flags and size limits`、
  `per-cpu hash stays consistent under concurrent inserts and deletes`。10/26 的 110 个照旧通过。
- zpoline 路径没被改坏：见 `../x86-nuc-breakdown-20261029`（计数程序 1,000,000 / 1,000,000）。
