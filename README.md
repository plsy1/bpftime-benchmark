# bpftime-benchmark

bpftime 的 benchmark 工具仓库：workflow、脚本与说明在 `main`，被测代码与数据在其他分支。

## 分支布局

| 分支 | 内容 |
|---|---|
| `main` | benchmark harness：GitHub Actions workflow、runner 脚本、说明文档 |
| `codex/official-no-btf` | 被测的 bpftime 代码（含全部修复；bug 清单见 summry 分支的 change-log） |
| `benchmark-results/jetson` | Jetson 本地实验的结果数据（checkout 即数据目录） |
| `summry/jetson` | 分析文档（消融报告、runbook、修复记录、阅读指南单文件版） |
| `docs/source-reading-guide` | bpftime 源码阅读指南（多文件版） |
| `ci-assets` | CI 自动维护：run 页面内嵌图表的存放处，勿手动改动 |

## Benchmark workflows（Actions 页面手动触发）

每个 benchmark 一个 workflow，公共逻辑在 `bench-core.yml`（reusable workflow）：
checkout 或取预编译产物 → 装依赖 → 只构建该 suite 的目标 → **直接运行
`benchmark/<suite>/` 下的 py 脚本** → 结果文件与图表渲染到 run 页面 Summary →
完整产物上传 artifact。

| Workflow | 平台（`arch`） | 专属参数 | 备注 |
|---|---|---|---|
| `ssl-nginx` | both / arm64 / x64（默认 both） | `ssl_sizes`、`ssl_num_runs`、`wrk_params` | 主力 suite |
| `uprobe` | 默认 both | `iter` | |
| `syscall` | 默认 **x64** | — | aarch64 的 syscall 改写蹦床未实现 |
| `syscount-nginx` | 默认 both | — | |
| `mpk` | 默认 **x64** | `iter` | MPK 是 x86 专属特性 |

所有 workflow 共有的参数：

- **`prebuilt_run_id`（默认 `latest`）**：使用 Build Prebuilt 的预编译产物直接跑，
  免去 ~30 分钟编译。填具体 run id 可钉住某次构建；**清空则从源码现编**。
  使用预编译时 **`ref` 参数不生效**（代码版本由预编译产物决定，run 页面会有
  notice 标明其 commit）。
- **`ref`**（仅在现编源码时生效）：分支名、tag 或**完整 40 位 SHA**
  （`actions/checkout` 不接受短 hash；本地 `git rev-parse <短hash>` 展开）。

### 典型用法

```text
改了代码：
  1. 跑一次 Build Prebuilt（arch=both, ref=改动所在分支）
  2. 之后任意 benchmark workflow 直接跑（默认 prebuilt=latest，免编译）

测某个特定 commit 的源码：
  清空 prebuilt_run_id，ref 填完整 SHA

快速冒烟（ssl-nginx）：
  ssl_sizes=16b, ssl_num_runs=1
```

### 结果去哪看

- **run 页面 Summary**：本次产出的全部结果文件（txt/csv/小 json）内联展示、
  吞吐图内嵌（存于 `ci-assets` 分支，历史 run 的图长期有效）、run.log 尾部
- **Artifact**：完整结果包（`results-<arch>.tar.gz`）

### 辅助 workflows

| Workflow | 用途 |
|---|---|
| `build-prebuilt` | 预编译整棵树并按 arch 上传 artifact（保留 30 天），供 `prebuilt_run_id` 复用 |
| `build-jetson-no-btf-image` | 构建 Jetson 用的 no-btf Docker 镜像 |
| `official-no-btf-nginx-path-perf-x64` | perf 计数诊断（nginx 路径 CPU 归因），独立于吞吐 benchmark |

## 注意事项

- 托管 runner 存在**硬件抽签**（不同 run 分到的 CPU 不同，baseline 可差 30%+）：
  跨 run 的数值对比只看大方向，几个 pp 的差异不作数；同一 run 内
  baseline/kernel/bpftime 三腿的对比（同 VM 同 session）才可靠。
- CI 构建固定 LLVM 15、`ENABLE_PROBE_READ/WRITE_CHECK=1`（与 Jetson 本地
  构建对齐）。
- ssl-nginx 的负载参数通过环境变量进入 `benchmark.py` / `draw_figture.py`
  （`SSL_NGINX_SIZES` / `SSL_NGINX_NUM_RUNS` / `SSL_NGINX_WRK_DURATION` /
  `SSL_NGINX_WRK_CONNECTIONS`），本地跑同样适用，例如：
  `SSL_NGINX_SIZES=16b SSL_NGINX_NUM_RUNS=1 python3 benchmark/ssl-nginx/draw_figture.py`。

## 辅助脚本

- `run_x64_syscount_smoke.sh`、`setup.sh`
- Jetson 本机的消融/短测/审计工具在代码分支的 `v4-ablation/`、`v1-v4-ablation/`
  目录，用法见 summry 分支的 runbook
