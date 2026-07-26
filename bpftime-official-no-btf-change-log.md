# bpftime official-no-btf 修改清单（change log）

分支 `codex/official-no-btf`（远端 plsy1/bpftime-benchmark）上本轮调查产生的全部代码修改，
以及配套的基础设施变更。按提交顺序排列；"验证"列注明每项的证据强度。

## 一、代码修改（runtime / benchmark）

| # | Commit | 日期 | 文件 | 问题 | 修复 | 验证 |
|---|---|---|---|---|---|---|
| 1 | `0fcdb0e` | 07-22 | `runtime/src/handler/perf_event_handler.cpp` + 单测 | software perf record 未按 8 字节对齐：record header 可落在环最后一字节，libbpf 读到跨界 `size=0` → 消费者空转、producer shard 填满、隐藏丢事件、吞吐双峰（CV 8.7–16.6%） | record 总长向上对齐 8B 并写入 `header.size`、padding 清零、u16 上限拒绝、消费侧拒绝非对齐记录、新增环边界回归测试 | 单测 8,358 断言；reader task-clock/指令 −95.5%/−96.1%；drop 0%；七档 CV 14.2%→2.6%；2026-07-27 代码复核确认正确（含不变量归纳） |
| 2 | `99adf3a` | 07-27 | `example/tracing/sslsniff/sslsniff.bpf.c` | 分支 HEAD 残留 V2 消融版（probe_read/output 被注释）+ V1 开关注释痕迹——曾污染 07-23/24 的 CI 数据 | 恢复为上游 `c796f45` 干净完整版 | 与上游字节级一致 |
| 3 | `076e3e4` | 07-27 | `runtime/src/bpf_helper.cpp` | `bpf_perf_event_output` 每事件执行 affinity 绑核序列（getcpu+getaffinity+2×setaffinity）：无任何保护作用（cpu 值先于绑核已快照、ring 写入走 per-(pid,tid) shard、绑核非锁），Jetson 上 3.1–6.7 µs/事件 = ④段成本 72%；附带两个 bug——错误路径（`:520/:532`）漏恢复 mask 将线程永久钉死单核、restore 用陈旧 mask 与应用自身 setaffinity 竞态 | 无条件删除绑核序列，cpu 值保留 `my_sched_getcpu()` 快照（rseq，3.5 ns） | 单测通过；16B 短测 bpftime 由 −8% 变 +8.8% vs kernel；**fair 口径 3 轮 ×2 payload：16B +7.58% / 256KB +9.81%，六轮全同向**；事件守恒 2.001/请求零丢失；CV 收敛至 kernel 水平 |
| 4 | `7bd421b` | 07-27 | `benchmark/ssl-nginx/{benchmark,draw_figture}.py` | benchmark 参数硬编码（NUM_RUNS/wrk/payload 列表），CI 与短测无法调参 | 环境变量 `SSL_NGINX_{SIZES,NUM_RUNS,WRK_DURATION,WRK_CONNECTIONS}`（默认行为不变；非法 size 报错列出有效值；补 32kb/64kb 档） | 语法与行为测试（子集选择、非法值拒绝） |
| 5 | `ead56c9` | 07-27 | `runtime/src/bpf_helper.cpp` | `probe_read`/`probe_write_user` 的"是否需装 SIGSEGV handler"判断读**未初始化局部变量**（UB，首个调用后恒真）→ 每次调用重装 handler，热路径每次多 2 个 `rt_sigaction` syscall | `thread_local bool *_handler_installed` 标志替换坏判断（read/write 两路径） | 单测（既有失败 23/24 不变，A/B 确认与本改动无关）；单样本 16B：bpftime impact 24.5%→22.9%（+1.7%，n=1 方向性）|

## 二、基础设施与工具（不在代码分支）

| 位置 | 内容 |
|---|---|
| `main` 分支 workflows | 20 个旧 workflow 整合为 3 个：`benchmark.yml`（多 suite/双 arch/参数化/同 VM A/B `ref_b`/prebuilt 复用/job summary 渲染+ci-assets 图表）、`build-prebuilt.yml`（预编译产物 artifact）、`build-jetson-no-btf-image.yml`；删除 v0.2.0 源码树；ssl-nginx 测量直跑 `draw_figture.py`，编排脚本仅负责依赖+构建 |
| `bpftime-offical-no-btf/v4-ablation/` | ④段 runtime 消融变体（no-affinity/no-mapops/no-ringwrite/noop-helper/no-sigaction + original 参照），`build-variant.sh`（pristine 校验+还原+sha256）、`short-test.sh`、`event-audit.sh`、`fair-test.sh`、`run-all-variants.sh` |
| `bpftime-offical-no-btf/v1-v4-ablation/` | V1–V4 `sslsniff.bpf.c` 变体（V2/V3 取自 git 原件，V1 重构），整目录 mount 即可复跑 |
| 分支重组 | `benchmark-results/jetson`（纯数据，本地 `~/src/benchmark-results` 即 checkout）、`summry/jetson`（分析文档，本地 `~/src/summry`）、`docs/source-reading-guide`（阅读指南 9 文件）；删除 3 个废弃分支 |

## 三、已知未修 / 上游候选清单

| 项 | 性质 | 状态 |
|---|---|---|
| `bpf_perf_event_output` 忽略 `flags`（`BPF_F_INDEX_MASK` 显式索引被当 current-cpu） | 功能缺陷 | 未修，建议独立 PR/issue |
| ring 满时 `output_data` 恒返回 0 + 全 runtime 无 `PERF_RECORD_LOST` | 丢弃对消费者不可见 | 未修，建议 issue（含临时 lost 计数器方案） |
| aarch64 `text_segment_transformer` 蹦床未实现但**静默构建成功**（.so 带未定义符号，运行时才炸；反汇编器写死 x86） | 构建系统缺陷 | 未修，建议 issue（加 `#error`/CMake 平台判断） |
| `per_cpu_hash_map::elem_delete` 清零范围疑似有误（`[begin, begin+cpu*value_size)`） | 疑似 bug | 未验证，待查 |
| `*probe*` 单测既有失败（mocked `get_global_attach_ctx` 异常，23/24） | 测试缺陷 | 与本轮改动无关，待查 |
| `ENABLE_PROBE_READ_CHECK` 本地 ON / CI OFF 不一致 | 配置漂移 | 跨平台对比前应统一 |
| bpftime 残余偶发离群低值（fair 60 样本中 4 个 −10~−15%） | 性能抖动 | 来源未定位（P3） |

## 更新记录

- 2026-07-27：首版，覆盖 `0fcdb0e`…`ead56c9` 五项代码修改与全部基础设施变更。
