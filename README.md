# bpftime-benchmark

bpftime 的 benchmark 工具仓库：测试脚本与说明在 `main`，被测代码与数据在其他分支。

## 分支布局

| 分支 | 内容 |
|---|---|
| `main` | benchmark 脚本（`benchmark/`）、Jetson 镜像说明（`docker/`）、安装与使用文档 |
| `dev/uprobe-official-4708f4092d1e` | 开发分支：official 4708f409 + 每日列表的提交（2026-11-10 起） |
| `archive/uprobe-official-177a1dc465ff`、`archive/uprobe-official-8da867d541f8` | 之前的两条开发分支；每日列表里的提交只在它们上面，不能删 |
| `automation/jetson-daily` | Jetson 每日任务：跟踪 official、重放每日列表、跑 official uprobe benchmark |
| `official-latest` | official bpftime 的镜像，只做快进 |
| `benchmark-results/jetson` | Jetson 本地实验的结果数据（checkout 即数据目录；2026-10-19 以前的旧结果在 tag `jetson-results-before-cleanup-20261110`） |
| `benchmark-results/x86-nuc` | x86 NUC 上的结果数据 |
| `summry/jetson`、`summry/x86-nuc` | 分析文档（消融报告、runbook、修复记录、阅读指南单文件版） |
| `docs/source-reading-guide` | bpftime 源码阅读指南（多文件版） |

## 关于 CI

这个仓库以前有一套 GitHub Actions workflow（ssl-nginx、uprobe、syscall、syscount-nginx、mpk，
以及预编译、Jetson no-BTF 镜像和 nginx 路径 perf 诊断），2026-11-10 已全部删除，`ci-assets`
分支（run 页面图表）也一并删除；需要时可从 main 的历史里找回（删除前的最后一个提交是
`ed4d6681`）。现在所有测量都在 Jetson 和 NUC 本机上做，结果和做法记在各结果分支的 README 里。

## 本地运行

- 各 suite 的脚本在 `benchmark/<suite>/` 下，直接运行其中的 py 脚本；构建用被测仓库根目录的
  `make benchmark`（该官方目标固定 LLVM 15，并将 `ENABLE_PROBE_READ/WRITE_CHECK` 设为 `0`）。
- `setup.sh`、`INSTALL.md`、`usage.md`：环境准备与用法。
- Jetson 镜像：`docker/jetson-repro/README.md`。
