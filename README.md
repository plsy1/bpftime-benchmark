# bpftime-benchmark

bpftime 在 Jetson Orin Nano（aarch64）和 x86 NUC 上的开发、测量与分析记录。`main` 只放这份说明：
被测代码、每日任务、结果和分析各在下面的分支里。

## 分支

| 分支 | 内容 |
|---|---|
| `dev/uprobe-official-4708f4092d1e` | 开发分支：official 4708f409 + 每日列表的提交（2026-11-10 起）；测试脚本在其中的 `benchmark/` |
| `archive/uprobe-official-177a1dc465ff`、`archive/uprobe-official-8da867d541f8` | 之前的两条开发分支；每日列表里的提交只在它们上面，不能删 |
| `automation/jetson-daily` | Jetson 每日任务：跟踪 official、重放每日列表、跑 official uprobe benchmark（`tools/jetson-upstream`） |
| `official-latest` | official bpftime 的镜像，只做快进 |
| `benchmark-results/jetson` | Jetson 上的结果数据，按主题索引见其 README；2026-10-19 以前的旧结果在 tag `jetson-results-before-cleanup-20261110` |
| `benchmark-results/x86-nuc` | x86 NUC 上的结果数据 |
| `summry/jetson`、`summry/x86-nuc` | 分析文档（消融报告、runbook、修复记录、阅读指南单文件版） |
| `docs/source-reading-guide` | bpftime 源码阅读指南（多文件版） |

## 历史

`main` 以前放着 benchmark 脚本的副本、Jetson no-BTF 镜像说明和一套 GitHub Actions workflow。
2026-11-10 起测量都在本机上做，workflow 和 `ci-assets` 分支已删除，`main` 精简为这份说明；
旧内容可从 `main` 的历史里找回（精简前的最后一个提交是 `dd1a9cf3`，删除 workflow 前是 `ed4d6681`）。
