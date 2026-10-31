# 新版阅读指南大纲（草案，待用户确认）

基线 `177a1dc465ff`。读者：略懂 eBPF。写法沿用学习版规范（先直觉后代码；正文无行号/PR 号；每章末尾折叠的「深入参考」）。
素材：`notes/R1–R8` + `notes/X-crosscheck.md` + 现有章节中写得好的部分。

## 章节

| 文件 | 章 | 内容要点 | 主要素材 | 相对现在 |
|---|---|---|---|---|
| `00-architecture.md` | 0 全景 | eBPF 预备知识；bpftime 是什么；组件图；仓库地图（含 GPU、syscall trace）；术语表 | 全部笔记 §0/§4 | 修正术语表错误（假 fd、槽位分配、ufunc 默认值），补全组件 |
| `01-lifecycle.md` | 1 一个事件的生命周期 | sslsniff + nginx 的 13 步主线 | R1 R3 R4 R5 | 修正启动时机、启动顺序；补 trap 后端的对照一句 |
| `02-runtime-shm.md` | 2 共享内存与 handler 表 | 布局、7 种 handler、假 fd 的两套分配器、会话/epoch（及其局限）、生命周期与残留、json 导出导入、MPK | R1 | 改正 epoch 过度承诺、link 级联删除；新增 json、MPK、锁文件 |
| `03-entrypoints.md` | 3 两个入口与 cli | syscall-server 拦截总表与 bpf cmd 总表；agent 两种启动方式、扫表（只扫一次）、刷新、卸载；cli 各子命令；宿主契约及其违反点 | R3 | 原 03 改名；新增 cli、IPC/auto-refresh、启动顺序正解 |
| `04-attach-uprobe.md` | 4 uprobe 类 attach | base 接口；inline hook；四种 attach 对比；两层实体；x86/aarch64 寄存器转换；override/replace（含 fail-open）；trap 后端；自定义 attach（simple_attach、nginx 示例） | R5 | 原 04 改名；改正 ureplace/返回地址说法；补互斥检查单向问题 |
| `05-syscall-trace.md` | 5 系统调用追踪 | **新章**：syscall_trace attach；text_segment_transformer 的 zpoline 式改写；dlmopen 隔离；哪些 syscall 不触发；`bpftime start -s` | R5 §3.5–3.7 | 新增 |
| `06-vm-jit.md` | 6 VM 与 JIT | 三层结构；指令→IR 对照；lddw 各 src；helper 绑定；bpf-to-bpf；惰性 JIT；ubpf 后端；AOT 产物 | R4 | 原 05 改名；补 AOT、ubpf；修 is_gpu 说法 |
| `07-helpers.md` | 7 helper | helper 三组与注册；39 个 helper 总表；perf 输出与 ringbuf 输出；probe_read/write；ufunc；tail call | R4 R2 | 从原 06 拆出 |
| `08-maps.md` | 8 map | 分发与锁（每 fd 一把）；全部 map 类型总表；per-CPU 模拟；ringbuf 与 perf_event_array 生产/消费协议；kernel-user 共享 map | R2 | 从原 06 拆出；补 ringbuf、全类型 |
| `09-gpu.md` | 9 GPU/CUDA 概览 | **新章**：CUDA/PTX 最少背景；整体架构（拦 CUDA 运行时 → 找 kernel → eBPF 编 PTX 插桩 → 重编模块）；"kprobe=CUDA 探针"；GPU↔CPU helper 通信；GPU map；局限 | R6、X#4 | 新增（概览深度） |
| `10-periphery.md` | 10 外围工具 | daemon（机制与限制）、verifier（谁会被验证）、bpftimetool、bpftime-aot | R7 | 原 07 改名；补 aot、daemon 限制；改正 verifier 说法 |
| `11-build-test.md` | 11 构建、测试与示例地图 | CMake 选项总表、构建产物表、最小构建命令；单测总表与怎么跑（含隔离 shm 的必要性、默认顺序崩溃）；示例与 benchmark 总表；"活文档 Top 10" | R7 R8 | **新章** |
| `12-practice.md` | 12 动手实践 | 学习路线（一周计划）；按章编号的实验（全部在本机复核；注明 build-jit 配置限制） | R1–R8 §9 | 原 08 改名重写 |
| `A-research-notes.md` | 附录 A 研究笔记 | 原 09 原样迁移（七月修复、PR 状态、注释不一致） | 现有 09 | 改名 |
| `B-known-issues.md` | 附录 B 已知问题清单 | **新**：通读中发现的疑似 bug / 文档与代码不符，按模块分组，每条：现象、位置、确定程度（已实跑/读码确认/推断）、影响 | 全部笔记 §7、X | 新增 |

正文提到"坑"时只写一句并链到附录 B，不在正文罗列 bug。

## 阶段三分工（7 个写作者）

| 写作者 | 负责 | 素材 |
|---|---|---|
| W1 | 00、01 | 全部笔记的 §0/§3/§4、X |
| W2 | 02、03 | R1、R3、X |
| W3 | 04、05 | R5、X |
| W4 | 06、07 | R4（helper 输出路径参考 R2）|
| W5 | 08、09 | R2、R6、X |
| W6 | 10、11、12 | R7、R8，及各笔记 §9 |
| W7 | 附录 A（改名迁移）、附录 B、README | 全部笔记 §7/§8、X |

## 阶段四核查（3 个，不读笔记，只看成稿 + 源码）

- C1 逐条核事实（00–06）、C2 逐条核事实（07–12、B），各自对每个事实陈述标 正确/错误/无法确认。
- C3 实跑所有"动手验证"与第 12 章实验（本机；nginx 用 Docker 镜像 `x86-nginx-wrk:20261029`），并以初学者身份通读标出读不懂处。
