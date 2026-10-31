# 附录 B 已知问题清单

> 通读 bpftime 源码时发现的疑似 bug、和内核 eBPF 语义不同的行为、代码与注释或文档不一致的地方。
> 正文各章遇到这些坑只写一句话，细节都在这里。

## 这份清单是什么

- **来源**：8 位读者把官方源码（基线 `177a1dc465ff`）逐文件读了一遍，各自记在 `notes/R1`–`R8` 的笔记里；
  跨模块的问题由汇总者回源码裁定（`notes/X-crosscheck.md`，下文简称 X）。原附录 A.6「代码与注释不一致清单」
  的 9 条也并进来了，条目里注明"原附录 A.6"。
- **状态**：只是记录。**没有向上游报告，也没有修**。上游后来的提交可能已经改了其中一些。
- **收录标准**：满足任一条就收：可能崩溃、泄漏、数据竞争或写坏数据；和内核 eBPF 语义不同，用户会意外；
  代码与注释、README、帮助文字说的不一样；构建或测试脚本按说明跑不通。纯风格问题不收。
- **每条写什么**：现象、位置（`路径:行号`，行号按 `177a1dc465ff`）、确定程度、影响。多位读者独立发现的同一问题合并成一条，
  并注明是谁发现的（R1–R8 对应 `notes/` 下的八份笔记）。
- **确定程度**三档：
  - **已实跑**：在本机（x86-64 Debian，内核 6.12）用官方树的 `build-jit/` 或 scratchpad 里的小程序复现过。
  - **读码确认**：代码路径直接、没有歧义，但没有运行。
  - **推断**：从代码推出来的后果，没有构造用例验证，可能有遗漏的条件。

用的时候按模块找对应分节；正文各章链接的就是这些分节锚点。

**最可能踩到的几条**（按读者做实验时碰上的概率排）：

| 条目 | 一句话 |
|---|---|
| [B.1.1](#b-shm) | 跑单测会删掉默认的共享内存段 `bpftime_maps_shm` |
| [B.10.6](#b-build-test) | 整套单测按默认顺序跑，第 2 个用例必然段错误 |
| [B.2.4](#b-entry) | 用 LD_PRELOAD 启动的 agent 只扫一次表，必须先起 loader |
| [B.1.3](#b-shm) | 第二个 loader 用同一个 shm 名会清空第一个的全部对象 |
| [B.5.1](#b-vm) | eBPF 程序运行时除以 0 会让宿主进程收到 SIGFPE |
| [B.7.3](#b-maps) | 默认 HASH map 不认 flags、满了也返回成功 |
| [B.6.2](#b-helpers) | perf buffer 满了静默丢事件，helper 还返回 0 |
| [B.1.6](#b-shm) | `bpftimetool export` 出来的 JSON 导不回去 |

---

<a id="b-shm"></a>

## B.1 共享内存、handler 表、会话与 json 导出导入

**B.1.1 `bpftime::shm_remove` 析构时删错了段**（已实跑；R1、R8 独立发现）
- 现象：带名字构造时删的是传入的名字，析构时却删全局段名。没设 `BPFTIME_GLOBAL_SHM_NAME` 时，全局段名就是 `bpftime_maps_shm`。
- 位置：`runtime/src/handler/handler_manager.hpp:69-85`。单测里的同名结构 `runtime/unit-test/common_def.hpp:6-19` 是对的。
- 影响：用 `bpftime::shm_remove` 的单测（`test_external_map_ops.cpp:50`、`test_shm_progs_attach.cpp:202`、`test_shm_hash_maps.cpp:103`、`tailcall/test_user_to_kernel_tailcall.cpp:69`）
跑完会把自己的段留在 `/dev/shm`，还会删掉同机正在用默认名的真实会话（核查时实跑：跑 `Test shm progs attach` 后全局名指向的段被删）。
user→user 的 tailcall 测试用的是单测自己那份正确的 `shm_remove`，但它会以 REMOVE_AND_CREATE 重建全局段，见 [B.10.7](#b-build-test)。跑单测前一定设 `BPFTIME_GLOBAL_SHM_NAME`。

**B.1.2 关掉一个 link 会连带删掉它的 perf event，以及挂在同一 perf event 上的其他 link**（已实跑）
- 现象：删 link 时级联 `clear_id_at(target)`；删 perf event 时又把所有指向它的 link 清掉。
- 位置：`runtime/src/handler/handler_manager.cpp:105-121`（perf 级联删 link）、`:139-146`（link 级联删 perf）。
- 影响：和内核不同。内核里关 link 不会让用户手里的 perf fd 失效，也不影响别的 link。之后对这个 perf fd 的 ioctl 会退回真内核。
非 perf 型 link（如 XDP 的 `target_ifindex`）也会去清同号格子，可能误删别的 handler（推断）。

**B.1.3 第二个 syscall-server 启动会清空第一个的整个会话**（已实跑；R1、R3 独立发现）
- 现象：每个 syscall-server 的 `start_up` 都调 `begin_new_session`，哪怕它只是打开已有的段，不是段的主人。
`begin_new_session` → `reset_server_state` 清空整张 handler 表，随后还用自己的环境变量覆盖共享的 `runtime_config`。
- 位置：`runtime/syscall-server/syscall_server_utils.cpp:172,216`；`runtime/src/bpftime_shm_internal.cpp:1017-1031`。
- 影响：两个 loader 不能共用一个 shm 名。开了 auto-refresh 的 agent 会因 epoch 变化拆掉全部 link 重挂；
没开的 agent（LD_PRELOAD、`bpftime attach`）留着旧 link 继续跑，它引用的 map fd 已被清掉或被新对象复用（推断）。
头文件注释（`bpftime_shm_internal.hpp:162-169`）表明设计上就是"一次只有一个 trace 会话"。
核查时实跑：第一个 loader 持有 map、epoll、软件 perf event，第二个 loader 用同一个 shm 名启动后，表里只剩第二个 loader 新建的 map；第一个 loader 再对自己的 map fd 做 lookup 仍然"成功"，命中的是别人的 map。

**B.1.4 `map_init` 失败被吞，留下"有 fd、没容器"的 map**（已实跑；R1、R2 独立发现，X#5 裁定）
- 现象：`set_handler` 调 `map_init` 不看返回值，照样返回 fd。参数非法时（RINGBUF 的 `max_entries` 不是 2 的幂、BLOOM 的 `key_size != 0`、未知类型）
`map_init` 返回 -1 且不构造实现对象。
- 位置：`runtime/src/handler/handler_manager.cpp:81-86`；`runtime/src/handler/map_handler.cpp:813-818,862-866,1137-1140`。
- 影响：之后对这个 map 的任何操作都会对空指针做 `static_cast` 再调用，例如 `bpftime_ringbuf_reserve` 直接解引用（`runtime/src/bpftime_shm.cpp:391-393`，崩溃未实跑）。
另一面：实现类构造函数**抛异常**时，异常会穿过 `set_handler`，在 syscall-server 被接住后退回真内核的 `bpf(2)`。
所以同样是"参数非法"，有的类型拿到一个坏的假 fd，有的被转去真内核创建。

**B.1.5 `bpf_prog_handler::type` 从不初始化**（已实跑；R1、R7 独立发现）
- 现象：构造函数不用 `prog_type` 参数，`type` 字段是垃圾值。
- 位置：`runtime/src/handler/prog_handler.cpp:9-19`；导出处 `runtime/src/bpftime_shm_json.cpp:302`。
- 影响：只影响 JSON 导出。实跑导出过 `"type": -1388388152`；官方示例 `daemon/test/asserts/malloc.json` 里也是 `1516411092`。

**B.1.6 JSON 导出和导入不对称，不能当备份用**（已实跑；R1、R7 独立发现）
现象（都在 `runtime/src/bpftime_shm_json.cpp`）：
- 导出写 `epoll_handler`、`memfd_handler`（`:336-357`），导入不认识（`:216-219`），整份导入返回 -1 并**中途停下**，已导入的部分不回滚。
- 软件 perf event 导出时不写 `pid`（`:87-95`），导入无条件读 `value["attr"]["pid"]`（`:148`），nlohmann 抛 `type_error`，异常穿过 `extern "C"` 的 `bpftime_import_*`。
- kprobe、custom、UREPLACE(1009) 类型的 perf event 导入不支持（`:154-195`）。
- link 只导出 prog/target（`:342-347`），导入时 `attach_type=0`，cookie 丢失，也绕过了 perf target 校验（`:205-215`）。
- prog 导入读 `attach_fds`，导出从不写（`:130` 对比 `:302-311`），这个循环永远空转。
- map 只导出结构，不导出内容；`gpu_thread_count` 不导出；`offset`、`ref_ctr_off` 以 `int` 读回（`:156-157`），超过 2^31 会截断。
- 影响：`bpftimetool export` 的结果只能看，不能原样 `import` 回去。

**B.1.7 epoch 只防"换会话"，不防会话内的增删**（读码确认；后果为推断）
- 现象：`set_handler`、`clear_id_at` 都不碰 `epoch_seq`，只有 `begin_new_session` 会改它。
- 位置：`runtime/src/bpftime_shm_internal.cpp:1017-1031`。
- 影响：agent 的 auto-refresh 扫表时，syscall-server 正在加 handler，agent 可能读到写了一半的格子，而且不会重试（推断）。

**B.1.8 假 fd 有两套分配器，混用同一张表可能撞号**（推断）
- 现象：syscall-server 用调用者进程的真实内核 fd 号（`open_fake_fd`），daemon 用表里最小的空格（`find_minimal_unused_idx`）。
`add_bpf_map` 遇到目标格非空时会**直接清掉原来的 handler**（`runtime/src/bpftime_shm_internal.cpp:1070-1074`）。
- 影响：`bpftimetool import`、daemon、第二个 syscall-server 写入的号和本进程 `open_fake_fd` 的号冲突时，`set_handler` 返回 `-EEXIST`，
或者把别人的 prog/map 清掉。

**B.1.9 `PERF_EVENT_IOC_ENABLE` / `DISABLE` 不起作用**（读码确认；X#6）
- 现象：两个 ioctl 只改 handler 里的 `enabled` 标志。除了 JSON 导入导出，没有任何代码读这个标志。
- 位置：`runtime/src/handler/perf_event_handler.hpp:193-205`；读写点只有 `runtime/src/bpftime_shm_json.cpp:200,333`。
- 影响：对已经挂上的程序 `DISABLE` 不会让它停下来。（CUDA 部分的 `enabled` 是另一个字段。）

**B.1.10 `bpftime_prog` 带 config 的构造函数会改写全局配置**（读码确认；R1、R4 独立发现，X#14）
- 现象：构造一个程序对象时调 `bpftime_set_runtime_config(std::move(config))`。
- 位置：`runtime/src/bpftime_prog.cpp:140`。`bpftime_object_open(path, cfg)` 走这条（`runtime/object/bpf_object.cpp:109`）。
- 影响：进程已经打开全局 shm 时，这会覆盖 shm 里所有进程共享的配置。

**B.1.11 MPK 模式大概率不可用**（推断；pkey 语义已实跑，bpftime 的 MPK 构建未验证）
- 现象：实跑 `pkey_alloc(0, PKEY_DISABLE_WRITE)` 后，调用线程对该 pkey 只读；之后新建的线程和之前已有的线程读写都会 SEGV。对照代码：
构造 shm 的线程紧接着就要写 shm（登记存活 pid、`begin_new_session`，agent 侧见 `runtime/agent/agent.cpp:1138`）；
其他线程在 `bpftime_prog_exec` 之外读 shm 也会出错；`bpftime_prog_exec` 结束时总是恢复保护，嵌套执行会提前恢复。
trap 后端的信号执行路径不切换 MPK（`runtime/src/bpftime_prog.cpp:241-247`）。
- 影响：打开 `BPFTIME_ENABLE_MPK` 的构建很可能一启动就崩，需要单独验证。

**B.1.12 配置里两个定长字符串用 `strcpy` 写入**（读码确认）
- 现象：`set_logger_output_path`、`set_vm_name` 直接 `strcpy` 进 1024 和 128 字节的数组。
- 位置：`runtime/include/bpftime_config.hpp:106-113`。
- 影响：`BPFTIME_LOG_OUTPUT` 超过 1023 字节或 `BPFTIME_VM_NAME` 超过 127 字节就会溢出（配置对象可能在 shm 里）。

**B.1.13 `injected_pids` 没有初值**（读码确认；影响为推断）
- 现象：成员声明缺 `= nullptr`，`SHM_NO_CREATE` 路径不给它赋值，placement new 也不清零。
- 位置：`runtime/src/bpftime_shm_internal.hpp:95`。
- 影响：对以 `SHM_NO_CREATE` 打开的实例调 `add/remove_pid_from_alive_agent_set` 会用到不确定的值。单测清理时常这样做。

**B.1.14 未初始化全局 shm 时 `get_runtime_config` 解引用空 optional**（推断）
- 现象：`runtime_config == nullptr` 时返回 `*local_runtime_config`，而它从没被设置过。
- 位置：`runtime/src/bpftime_shm_internal.cpp:1165-1172`。`runtime/unit-test/test_bpftime_shm_json.cpp:105` 就在没初始化全局 shm 的情况下调用了它。
- 影响：未定义行为，测试能过是靠运气或靠同一二进制里先跑的测试。

**B.1.15 共享内存锁文件永不删除，打开失败时静默不加锁**（读码确认；R1、R3、R8 都观察到残留）
- 现象：每个 shm 名对应一个 `/tmp/bpftime-shm-<名>.lock`，用完不删；`open` 或 `flock` 失败时直接返回，不报错也不加锁。
- 位置：`runtime/src/bpftime_shm_internal.cpp:195-209`。
- 影响：`/tmp` 里越积越多；锁文件权限不对时，生命周期互斥悄悄失效。

**B.1.16 `open_fake_fd` 的 dup 循环会占住低号 fd**（读码确认）
- 现象：拿到的 fd ≤ 2 时循环 `dup`，直到号 > 2（最多 4 次），中间拿到的号都不关。例如拿到 0 时会依次占住 0、1、2，最后返回 3。
- 位置：`runtime/src/bpftime_shm_internal.cpp:674-688`。
- 影响：宿主若关过 0/1/2，这些号会被 `/dev/null` 永久占住。对宿主可能反而无害，但不是"借一个号"。

**B.1.17 注释与实现不符**（读码确认）
- `read_stable_epoch_seq` 的注释说 epoch 对象缺失时返回 0（`runtime/src/bpftime_shm_internal.hpp:171`），实现返回 `BPFTIME_EPOCH_SEQ_MISSING`（即 `UINT64_MAX - 1`，`.cpp:999-1000`）。
- `runtime/src/bpftime_shm_internal.cpp:891` 注释 "Allocate 20M bytes of memory by default"，实际默认 50 MB（`runtime/include/bpftime_config.hpp:79`）。

**B.1.18 小问题与死代码**（读码确认）
- `get_software_perf_event_raw_buffer` 失败时 `return nullptr;` 构造的是"有值的空指针"（`runtime/src/bpftime_shm_internal.cpp:1190`），调用方 `.value_or(nullptr)` 恰好无害。
- `runtime/object/bpf_object.cpp`：`find_program_by_secname` 在没有程序时对空 vector 取 `front()`（`:119`）；`name || "<NULL>"` 打印的是 bool（`:84,106`）；
  `bpftime_object_open` 打开失败也返回非空对象（`:153-157`）。
- `runtime/include/spinlock.hpp:37` 的 `yield` 是 ARM 指令，x86 上不能编；只在没有 POSIX spinlock 的平台（macOS）启用（`spinlock_wrapper.hpp:12`），影响很小。
- 只有声明没有定义或没人用：`global_shm_open_type`（`runtime/include/bpftime_shm.hpp:181`）、`bpftime_object_load_relocate_btf`（`runtime/object/bpftime_object.hpp:14`）、
  `libbpf_print_fn`（`bpf_object.cpp:198-205`）、`runtime_config::debug`、不在 handler variant 里的 `bpf_btf_handler`、
  `handler_manager.hpp:25-27` 引入却没用的锁头文件、非 CUDA 构建里 `~bpftime_shm` 唯一的一个未用局部变量（`bpftime_shm_internal.cpp:1263`）。

**B.1.19 只在建段时生效的配置**（读码确认，不直观）
`BPFTIME_SHM_MEMORY_MB` 和 `BPFTIME_MAX_FD_COUNT` 只在创建共享内存段时起作用。段已经存在时改环境变量没用，要先 `bpftimetool remove`。
复用已有段时还有一层误导：`start_up` 末尾 `bpftime_set_runtime_config` 用本进程环境变量的配置整体覆盖 shm 里的 `runtime_config`
（`runtime/syscall-server/syscall_server_utils.cpp:216`，实现在 `runtime/src/bpftime_shm_internal.cpp:1152-1163`），
于是配置里的 `max_fd_count`、shm 大小变成新值，与真实表长（`find_or_construct` 拿到的旧表，`:845-847`）不一致。
建表之后没有代码再读这两个字段，所以没有功能后果，只是字段不可信（W2 写作时发现）。

**B.1.20 nginx 式 daemonize 后，loader 退出可能删掉 worker 还在用的段**（推断，未实测；C1 核查时发现）
- 现象：agent 只登记最初那个进程的 pid。最初进程 `exit()` 时 `__destruct_shm` 把这个 pid 从存活集合摘掉，fork 出的 worker 不在集合里，集合可能变空；之后 owner loader 退出时按"没有其他活着的 loader 或 agent"删段。
- 位置：`runtime/agent/agent.cpp:1138-1139`（只登记 `getpid()`，agent 里没有 fork 处理）；`runtime/src/bpftime_shm_internal.cpp:90-111`（退出时摘掉本进程 pid）；`runtime/syscall-server/syscall_server_utils.cpp:55-100`（owner 退出时检查存活集合）。
- 影响：worker 仍映射着旧段继续跑（已映射的内存不会消失），但新的 loader / agent 再也连不上它。

---

<a id="b-entry"></a>

## B.2 syscall-server、agent 与 cli

**B.2.1 IPC refresh 中挂钩抛异常 → 宿主进程被 `std::terminate` 杀掉**（已实跑）
- 现象：agent 的 IPC 线程收到 `refresh` 后直接调 `refresh_attach_session`，里面的 `init_attach_ctx_from_handlers` 没有 try；Frida 挂钩失败会抛 `std::runtime_error`。
- 位置：`runtime/agent/agent.cpp:343,698-699`；抛出点 `attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:140,166`。
- 影响：实跑时目标进程退出码 134，stderr 出现 `terminate called after throwing ...`，违反"不终止宿主"的契约。
`bpftime trace` 对已有 agent 的进程优先走 IPC refresh（`tools/cli/main.cpp:1173-1178`），`bpftime attach` 也会转发过去（`agent.cpp:883`），都能触发。
同类风险（推断）：`perform_detach` 在 worker 线程和 IPC 线程里也没有 try（`agent.cpp:357,521`）；auto-refresh 线程只接 `std::exception`。

**B.2.2 一个 link 挂钩抛异常 → 整个 agent 初始化回滚**（已实跑）
- 现象：扫表循环只容忍负返回值（`runtime/src/attach/bpf_attach_ctx.cpp:109-119`），异常会穿出去，在 `runtime/agent/agent.cpp:1073-1078` 走 `init_fail`。
- 影响：同一张表里本可以挂上的其他 link 全都不挂。"单个 handler 失败只记日志"只对返回错误码成立。
Frida attach 失败用的就是异常，`create_attach_with_ebpf_callback` 只接 `bad_cast`。

**B.2.3 agent 装 SIGUSR1 处理器时不保存、不链式调用原来的**（已实跑）
- 位置：`runtime/agent/agent.cpp:1158`，裸 `signal(SIGUSR1, ...)`。
- 影响：宿主自己的 SIGUSR1 处理器被顶掉。单测只覆盖了失败路径保持 `SIG_DFL`（`runtime/unit-test/assets/shm_allocation_test_helper.c:200-207`）。

**B.2.4 LD_PRELOAD 和 `bpftime attach` 启动的 agent 只扫一次表**（已实跑，不直观）
- 现象：只有带 `auto_refresh_ms` 的注入（`bpftime trace`）或收到 IPC `refresh` 时才会重扫。
`bpftime start` 和 `bpftime attach` 传给 agent 的参数是空串（`runtime/agent/agent.cpp:539`、`tools/cli/main.cpp:1046`）。
- 影响：agent 先起时分两种情况。shm 还不存在时，agent 打不开 shm，直接放弃初始化（日志 `Unable to initialize shared memory`）；
shm 已经存在（上次残留，或 loader 刚建好段、还没建 handler）时，agent 扫到空表，日志照样打 `Attach successfully`，但什么都没挂上，计数器一直是 0。
必须先启动 loader 并等它加载完，再启动目标进程。另外 auto-refresh 只增量加新 link，表里删掉的 link 不会被拆（除非 epoch 变了）。

**B.2.5 回退到真 mmap 时每次泄漏一段匿名映射**（已实跑；R3 发现，原附录 A.6 第 7 条）
- 现象：兜底路径连调两次 `orig_mmap64_fn`，第一次的结果 `ptr` 直接丢掉。
- 位置：`runtime/syscall-server/syscall_context.cpp:1005-1007`。
- 影响：loader 直接调 `mmap`/`mmap64` 时，每次走到兜底路径都漏一段。实跑 1000 次 VmSize 涨了 4000 kB。看不出第一次调用的意图。
glibc malloc 的大块分配内部走 `__mmap`，不经过这个钩子，不受影响（核查时实跑确认）。

**B.2.6 宿主自己的 epoll 被破坏**（已实跑）
- 现象：`epoll_create1` 无条件返回 bpftime 的 epoll 假 fd；对普通 fd 的 `epoll_ctl` 落到 `/dev/null` 上，返回 `EINVAL`。
- 位置：`runtime/syscall-server/syscall_context.cpp:1070-1078`。
- 影响：loader 如果自己也用 epoll（网络、timerfd 等），会失效。

**B.2.7 `syscall(__NR_memfd_create)` 一律被换成 `/dev/null` 假 fd**（已实跑）
- 位置：`runtime/syscall-server/syscall_context.cpp:1216-1228`。
- 影响：宿主用它建真 memfd 时会出错。

**B.2.8 `BPF_OBJ_GET_INFO_BY_FD` 对不认识的 fd 返回 0 且不填输出**（已实跑）
- 现象：map、prog 以外的分支落到最后的 `return 0`，既不转给内核也不报错；prog 分支也只填了 `id`。
- 位置：`runtime/syscall-server/syscall_context.cpp:743-786`（prog 分支 `:778-783`）。
- 影响：loader 拿到一份全零的 info 结构却以为成功。

**B.2.9 `PERF_EVENT_IOC_SET_BPF` 返回的是新 link 的 fd，不是 0**（读码确认；R1 提出，X#9 裁定）
- 现象：`bpftime_attach_perf_to_bpf` 新开一个假 fd 当 link，syscall-server 把这个正数直接当 ioctl 返回值。
- 位置：`runtime/syscall-server/syscall_context.cpp:1051-1061`；`runtime/src/bpftime_shm_internal.cpp:444-451`。
- 影响：内核语义是返回 0。libbpf 只检查 `< 0`，功能上没问题，但这个 link 的 `/dev/null` fd 没有人关。

**B.2.10 `try_startup` 每次调用都把 `enable_mock` 临时置 false**（推断）
- 现象：几乎每个 `handle_*` 都先调 `try_startup`，它先把开关置 false，结束时再无条件置 true。
- 位置：`runtime/syscall-server/syscall_context.cpp:239-246`。
- 影响：多线程 loader 里，线程 A 处在这个窗口时，线程 B 的 `bpf()`、`mmap()` 看到开关是 false，会直接走真内核。另外别人设的 false 会被覆盖。

**B.2.11 syscall-server 给 verifier 的 helper 列表加错了组**（读码确认；R3、R7 独立发现，X#13）
- 现象：`enable_ufunc_helper_group` 分支里加的仍是 **shm maps 组** 的 id，ufunc 组的 id 从没加进 `helper_ids`。
- 位置：`runtime/syscall-server/syscall_server_utils.cpp:200-206`。
- 影响：只影响打开 `ENABLE_EBPF_VERIFIER` 的构建：ufunc helper 有原型却不在可用集合里，会被判成 "invalid helper function id"。

**B.2.12 uprobe 程序其实不进用户态 verifier**（读码确认，含 libbpf 源码；端到端未跑；R3、R7 独立发现）
- 现象：注释说 "uprobe/uretprobe -> BPF_PROG_TYPE_SOCKET_FILTER"，但 libbpf 给 `SEC("uprobe")` 设的是 KPROBE（`third_party/libbpf/src/libbpf.c:9832-9836`）。
syscall-server 只验证 TRACEPOINT 和 SOCKET_FILTER。
- 位置：`runtime/syscall-server/syscall_context.cpp:642-656`。
- 影响：真正会被验证的只有 tracepoint 程序和 libbpf 做特性探测时加载的小程序。

**B.2.13 kernel 模式下的几处问题**（只在 `BPFTIME_RUN_WITH_KERNEL` 时相关）
- `create_kernel_bpf_map` 发现 map 已存在时返回 0，调用方把 0 当 map fd 交给 libbpf；失败日志打印的是还没填的 `info.id`（`runtime/syscall-server/syscall_context.cpp:387-391,453-456`）。读码确认。
- `/sys` 下 uprobe/kprobe type 文件的 mock 不看 `run_with_kernel`，libbpf 读到 9/8 后原样传给真 `perf_event_open`（`:277-328,818-826,877-885`）。
  在动态 PMU 号不是 9/8 的内核上会建错类型（推断；本机真值恰好是 9/8，无法实测）。
- SOFTWARE perf event 总是建假 fd（`:905-910`），而 mmap、epoll 在 kernel 模式下直接走原函数，perf buffer 大概率用不了（推断）。

**B.2.14 attach 上下文里的几个竞争与悬空指针**（推断）
- `instantiate_bpf_link_handler_at` 用 `operator[]` 取 perf 实例（`runtime/src/attach/bpf_attach_ctx.cpp:323-324`）：target 没实例化时插入默认项（type 0），
  随后找不到 attach impl 返回 `-ENOTSUP`；若真有人注册了 type 0，会解引用空的 private data。
- `perform_detach` 在 `destroy_all_attach_links` 失败后（它遇到第一个错误就停，`bpf_attach_ctx.cpp:556-561`）仍然 `reset_instantiated_state`（`runtime/agent/agent.cpp:439-446`），
  没拆掉的钩子回调里捕获的是裸 `prog` 指针（`bpf_attach_ctx.cpp:398-404`），成了悬空指针。`refresh_attach_session` 同样忽略 destroy 的返回值（`agent.cpp:695-696`）。
- `find_instantiated_prog` 不加锁（`bpf_attach_ctx.cpp:190-195`），auto-refresh 线程却在 `ctx_mutex` 下修改同一个 `std::map`，tail call helper 并发读写有数据竞争。
- `mocked_mmap_values` 没有锁（`runtime/syscall-server/syscall_context.hpp:92`），多线程 loader 有数据竞争。
- agent 持 `shm_lifecycle_lock` 的时间覆盖整个扫表、JIT、挂钩（`agent.cpp:934-1145`），期间新 loader 的启动和退出清理都会阻塞。

**B.2.15 CLI 的几个行为**
- `bpftime detach` 对同一个 agent 先发 IPC `detach` 再发 SIGUSR1，detach 两次；而且是在持有 shm 的 pid 集合锁的回调里做的（`tools/cli/main.cpp:1268-1283`；`runtime/src/bpftime_shm_internal.cpp:1312-1330`）。
  alive 集合里若残留被 SIGKILL 的 agent 的 pid、又被系统复用，会给无关进程发 SIGUSR1（默认动作是终止）。推断。
- `bpftime trace` 总会对目标可执行文件及其命令行里出现的文件跑 `cuobjdump --extract-ptx all`，不管目标是不是 CUDA 程序（`tools/cli/main.cpp:603-755,1101`）。读码确认。
- `bpftime trace` 的返回值是注入的结果，不是 loader 的退出码（`tools/cli/main.cpp:1259`）。读码确认。

**B.2.16 不直观但不算 bug 的行为**（读码确认）
- syscall-server 的"一次性启动"由**任一**被拦截函数第一次进入 `handle_*` 触发，包括普通的 `close`、`open`、`read`；实跑 `close(-1)` 就建出了 shm。
- 被拦截的 `open`/`openat` 打开 `/sys` 下那几个固定路径时返回的临时文件是**空的**，内容只经被拦截的 `read()` 从内存给出；只有 `fopen` 路径真写了内容。
  `openat(AT_FDCWD, 相对路径)` 不会命中 mock。
- 每一次普通 ioctl 都打一条 WARN（`runtime/syscall-server/syscall_context.cpp:1066`）。
- agent 拿到 shm 里的配置后立刻把日志切到 **loader** 配置的路径（`runtime/agent/agent.cpp:956-957`，X#11）。agent 自己的 `BPFTIME_LOG_OUTPUT` 只管最初几行。
- agent 不清除 `LD_PRELOAD`，目标进程 exec 出来的子进程也会被注入。

**B.2.17 文档与代码不一致**（读码确认）
- `BPFTIME_NOT_LOAD_PATTERN`：代码在 kernel 模式下用一个 2 条指令的平凡程序代替原程序送进内核，日志写 "By pass kernel verifier"（`runtime/syscall-server/syscall_context.cpp:405-426`）。
  `runtime/syscall-server/README.md:40-44` 说 "skip verification" 基本对，只是没写只在 kernel 模式生效；`usage.md:162` 说 "skip loading the eBPF program into the kernel" 是错的。
- `usage.md:149-153` 的 `BPFTIME_USE_JIT` 在 runtime 里没有任何代码读取，已过时（只有 `benchmark/deepflow/benchmark/run.py:85` 还在设）。
- `BPFTIME_RUN_WITH_KERNEL`、`BPFTIME_DISABLE_JIT` 是"设了就算真"，设成 `false` 也会生效。

**B.2.18 `bpf_trace_printk` 写宿主 stdout**（读码确认）
违反"不碰宿主 stdio"的宿主契约，详见 [B.6.7](#b-helpers)。

**B.2.19 小问题与死代码**（读码确认）
- `_IO_new_fopen` 的原函数回退查的是 `"fopen"` 符号（`runtime/syscall-server/syscall_server_main.cpp:458-460`），无害。
- 没人用的：`runtime/src/attach/demo_ptx_prog.hpp`、`bpf_attach_ctx::current_id`（只写不读）、`mocked_file_provider::replacement_file`（`syscall_context.hpp:48`）、
  agent 内的 `injected_with_frida`（只写不读）、CLI 的 `--dry-run`（不生效）、`handle_sysbpf` 里的 `errmsg`（`syscall_context.cpp:491`）。

---

<a id="b-attach"></a>

## B.3 uprobe 类 attach（Frida、trap）

**B.3.1 先挂 uprobe 再挂 override：attach 返回成功，override 却永远不生效**（已实跑）
- 现象：同一函数已有 uprobe 时再挂 override，返回正常 id，但钩子不会被替换；之后再挂 uprobe 反而被 `-EEXIST` 拒绝。
`has_override` 只检查了单向；`ensure_listener` 对 override 类型什么都不做；`has_uprobe_or_uretprobe()` 定义了却没人调用。
- 位置：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:65-83`；`attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:115-118,209-218`。
- 影响：实跑输出 `id1=1 id2=2 ret=102 up=1 ov=0`，随后 `id3=-17`。用户以为 override 挂上了。

**B.3.2 Frida `detach_by_id` 对不存在的 id 返回 0，并把 `errno` 设成负数**（已实跑）
- 位置：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:144-150`（`errno = -ENOENT`）。
- 影响：调用方按返回值判断成功；errno 也不该是负数。实跑 `ret=0 errno=-2`。

**B.3.3 override 没覆盖返回值时，用 5 个参数重放原函数，第 6 个参数丢失**（已实跑）
- 位置：`attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:255,270-275,288-289`。
- 影响：x86-64 的第 6 个参数在 r9，被丢掉。实跑 `f6(1..6)` 期望 6015，得到一个垃圾值。浮点参数、栈上参数、浮点返回值也都不保留（推断）。

**B.3.4 ureplace 在内部被登记成 1008**（已实跑）
- 现象：`iterate_attaches` 和 `get_type()` 报的是 1008（UPROBE_OVERRIDE），不是 1009（UREPLACE）。
- 位置：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:264-265`；`attach/trap_uprobe_attach_impl/src/trap_uprobe_attach_impl.cpp:1279`。
- 影响：对外查询拿不到真实类型。

**B.3.5 ureplace 程序执行失败时，原函数照常执行**（读码确认，不直观）
- 现象：wrapper 只在 eBPF 回调 `err >= 0` 时 `bpftime_set_retval`；失败时不覆盖返回值，原函数照常跑（fail-open）。
- 位置：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:246-262`；`attach/trap_uprobe_attach_impl/src/trap_uprobe_attach_impl.cpp:1271-1276`。
- 影响："原函数从不执行"只在程序成功时成立。

**B.3.6 override 处理器设置的线程局部回调从不复位**（残留已实跑；悬空后果为推断）
- 现象：`curr_thread_override_return_callback` 设好后不清。
- 位置：`attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:267-268`。
- 影响：之后同一线程误调 `bpf_override_return` 不会抛异常，而是写到旧 entry 上；entry 析构后就是悬空引用。

**B.3.7 Frida 自定义 helper 与内核语义不同**（读码确认）
- `bpf_get_retval`（186）在这里返回被 hook 函数的返回值；内核里 186 是 cgroup 程序用的。
- `bpf_get_func_arg` 不检查 n 的上限，也忽略 ctx（内核里 n ≥ 参数个数返回 `-EINVAL`）。
- `bpf_get_func_ret` 在 uprobe（入口）程序里也能调，拿到的是此时 rax 里的垃圾（推断）；trap 版用 `phase != 2` 拒绝了（`trap_uprobe_attach_impl.cpp:1325`）。
- `bpf_get_func_ip` 在 override 程序里返回 0（`frida_attach_entry.hpp:34-39`；trap 同，`trap_uprobe_attach_impl.cpp:61-63`）。
- 位置：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:286-291`、`attach/frida_uprobe_attach_impl/src/frida_attach_utils.cpp:176-201`。

**B.3.8 UPROBE / URETPROBE 的注释写反了**（读码确认；R5 发现，原附录 A.6 第 1 条）
- 现象：`ATTACH_UPROBE = 6` 顶着 "invoked when the attached function return" 的注释，`ATTACH_URETPROBE = 7` 顶着 "was invoked"。
- 位置：`attach/frida_uprobe_attach_impl/include/frida_uprobe_attach_impl.hpp:20-25`。
- 影响：实现是对的（`ensure_listener` 给 uprobe 挂 `on_enter`、给 uretprobe 挂 `on_leave`），以实现为准。

**B.3.9 private data 的解析和序列化**（读码确认 / 推断）
- Frida 版 `initialize_from_string` 遇到非数字直接 `std::stoul` 抛异常；`addr` 没有初值；也没有 `to_string`（`attach/frida_uprobe_attach_impl/src/frida_attach_private_data.cpp:15,36`）。trap 版有 try/catch 和初值。读码确认。
- trap 版 `to_string()` 有 module 时输出 `module:<绝对地址>`，而 `initialize_from_string` 把冒号后当偏移，往返会加两次基址（`attach/trap_uprobe_attach_impl/src/trap_attach_private_data.cpp:41-43,54-59`）。
  目前 runtime 只在 kprobe 路径调 `to_string`，推断暂时踩不到。
- runtime 自己传进来的字符串总是合法的（`module:偏移`，冒号总在；`runtime/src/attach/bpf_attach_ctx.cpp:446-468`），上面的异常只有直接调这些 API 时才会碰到。

**B.3.10 attach 层的不直观约定**（读码确认）
- attach id 是**每个 attach impl 实例**各自递增的，不同 impl 之间会重号；runtime 用 `(id, impl 指针)` 来区分。
- `register_custom_helpers` 直接覆盖 attach 上下文里同号的 helper，没有提示（`runtime/src/attach/bpf_attach_ctx.cpp:276-284`）。
- `bpftime_pt_regs.hpp` 与 `frida_register_def.hpp` 故意共用一个 include guard，两份定义靠人工保持一致；一处改了另一处没改会静默选错布局。
- 出错日志 `"Function {} was already attached ..."` 没传参数，打印出字面的 `{}`（`frida_uprobe_attach_impl.cpp:77-78`，已实跑）。

**B.3.11 trap 后端只在 riscv64 上可用**（读码确认；推断部分已注明）
- 现象：trap 后端只有 riscv64 的实现，`trap_arch_riscv64.cpp` 没有架构宏保护；`cmake/StandardSettings.cmake:76-82` 按架构 FORCE 设置开关（见 [B.10.1](#b-build-test)）。
trap README 说 "defaults to ON on riscv64"，其实是强制。
`handle_return` 用 `frame.sp + sizeof(uintptr_t) < sp` 判断被遗弃的帧，只对 riscv 成立（`trap_uprobe_attach_impl.cpp:983-985`），将来加 x86 实现时要重审（推断）。

**B.3.12 nginx 自定义 attach 示例用错了格式符**（推断）
- 现象：`snprintf(buf, sizeof(buf), "%*s", (int)r->uri.len, r->uri.data)` 用的是**宽度**而不是精度，会一直读到 NUL，再靠"截掉最后一个空格"补救。
- 位置：`example/attach_implementation/nginx_plugin/ngx_http_bpftime_module.c:72-79`。应为 `%.*s`。

**B.3.13 `attach/README.md` 过时**（读码确认）
组件树漏了 trap；说 "Linux: All attach types supported"（实际 trap 只在 riscv64、syscall trace 只在 x86-64）；`syscall_trace_private_data` 名字写错；
没说 CUDA 的 8/9 与 KPROBE/KRETPROBE 同号（`attach/README.md:16-24,112-113,146,178`）。


**B.3.14 解析"模块:偏移"时，Frida 会把目标进程没加载的模块加载进来再挂钩**（已实跑；W1 写作时发现）
- 现象：`get_module_base_addr` 先调 `gum_module_load` 再取基址。loader 登记了目标进程根本不用的库的探针时，agent 会把那个库加载进目标进程并挂上钩子。
- 位置：`attach/frida_uprobe_attach_impl/src/frida_attach_utils.cpp:147-151`。
- 影响：sslsniff 默认也登记 GnuTLS、NSS 探针，注入 agent 的 `openssl s_client` 里多出 `libgnutls.so.30`、`libnspr4.so` 映射（未注入的进程没有）。"模块未映射返回 -ENOENT"只对 `/proc/<pid>/map_files/...` 形式的路径成立。

**B.3.15 没有对应 attach impl 的 perf handler 只打一条 ERROR 就被跳过**（读码确认）
- 现象：在非 CUDA 构建里，kprobe（8/9）类型的 perf handler 找不到 attach impl，`instantiate_perf_event_handler_at` 打 "Unable to lookup attach implementation of attach type 8" 并返回 `-ENOENT`，对应 link 不生效。
- 位置：`runtime/src/attach/bpf_attach_ctx.cpp:432-439`。
- 影响：loader 侧 attach 成功，目标进程里静默不触发，只能从日志看出来。

---

<a id="b-syscall-trace"></a>

## B.4 系统调用追踪（syscall_trace 与 text_segment_transformer）

**B.4.1 普通 LD_PRELOAD 的 agent 里，syscall trace link 能建成功但永远不触发**（读码确认；X#10）
- 现象：`_bpftime__setup_syscall_trace_callback` 只被 text_segment_transformer 调用。
- 位置：`runtime/agent/agent.cpp:1200`；调用方 `attach/text_segment_transformer/agent-transformer.cpp:90`。
- 影响：不经 `bpftime start -s` / `attach -s` 启动时，tracepoint 程序挂上了也收不到任何事件，也没有报错。

**B.4.2 transformer 多数失败直接 `exit(1)` 杀掉宿主**（exit 已实跑；空指针为读码确认；R3、R5 独立发现）
- 现象：`dlmopen`、`mprotect`、capstone 打开、第 0 页 mmap 失败都直接 `exit(1)`；找不到 `injected_with_frida` 符号时打一条 WARN 后仍然解引用空指针；它的 logger 固定写 stderr。
  例外：没设 `AGENT_SO`、找不到入口符号时只打日志后 `return`（`agent-transformer.cpp:57-65,92-96`）。
- 位置：`attach/text_segment_transformer/agent-transformer.cpp:50-52,79,84-88`；`attach/text_segment_transformer/text_segment_transformer.cpp:163,171,202,243,283`。
- 影响：违反"不终止宿主、不写宿主 stderr"的契约。

**B.4.3 enter 回调 override 后，原系统调用不执行、exit 回调也不跑；exit 回调也能改返回值**（已实跑）
- 位置：`attach/syscall_trace_attach_impl/src/syscall_trace_attach_impl.cpp:69-94`。
- 影响：和内核 tracepoint 不同（内核 tracepoint 不能改返回值）。实跑：`enter-override: ret=55 orig_calls=0 exit_runs=0`；`exit-override: ret=66 orig_calls=1`。

**B.4.4 clone、clone3、rt_sigreturn、exit、exit_group 不会触发 syscall trace 程序**（读码确认）
- 位置：`attach/text_segment_transformer/text_segment_transformer.cpp:61-75`；`attach/syscall_trace_attach_impl/src/syscall_trace_attach_impl.cpp:24-27`。
- 影响：跟踪这几个系统调用的程序永远收不到事件。

**B.4.5 跳板汇编的三个疑点**（推断，未验证；需要 root 把 `vm.mmap_min_addr` 设为 0 才能跑）
- `syscall_hooker_asm` 把 6 个参数寄存器压栈后调 C 函数，返回前 `leave; ret`，不恢复这些寄存器；真的 `syscall` 只破坏 rax/rcx/r11。
  glibc 等内联 syscall 的 asm 只声明 rcx/r11 被破坏，编译器可能依赖 rdi/rsi/rdx/r8/r9/r10 不变。
- `call *%rax` 和跳板里的 push 会写到调用者栈指针下方，覆盖 128 字节的 red zone；叶子函数可能把局部变量放在那里。
- rt_sigreturn 走 `handle_sigreturn` 时栈上还多着 `call *%rax` 压的返回地址（跳板的 `push %rax` 和 `pop %rax` 正好抵消），源码里 `addq $8, %rsp` 被注释掉了（`:59-63,108-111`）。
  x86-64 内核按 `regs->sp - 8` 找 sigframe，会偏 8 字节；扫描会改写所有可执行段，glibc 的 `__restore_rt` 也在内，所以信号处理函数一返回就大概率出错。读码支持这个疑点，仍未实测。
- 位置：`attach/text_segment_transformer/text_segment_transformer.cpp:57-112`。

**B.4.6 越界与扫描的边界情况**（推断）
- `dispatch_syscall` 对 `sys_nr` 不检查边界；第 0 页的 nop 滑板只有 512 字节（`syscall_trace_attach_impl.cpp:55,81`；`text_segment_transformer.cpp:51,246-276`）。号 ≥ 512 或负数会越界。
- 线性反汇编遇到解码失败就 `break`，整段停止扫描；数据混在代码段里时可能误改（`text_segment_transformer.cpp:178-183`）。
- 第 0 页最后设成 `PROT_EXEC`，注释说"保持空指针解引用的正常行为"；x86 没有 PKU 时 exec-only 页通常仍可读，读 NULL 不再段错误，会掩盖空指针 bug（`:277-279`）。

**B.4.7 只支持 x86-64**（推断）
- 现象：aarch64 分支只有 `// TODO`，capstone 写死 `CS_ARCH_X86`；但 `attach/CMakeLists.txt:15-17` 在开了 Frida 的所有 Linux 上都编 transformer。
- 位置：`attach/text_segment_transformer/text_segment_transformer.cpp:142-146,167`。
- 影响：aarch64 上 syscall trace 不可用；这个 .so 在 aarch64 上能否链接、加载未验证。

**B.4.8 syscall 号表的生成**（推断）
- 号表用构建机的 `cpp` 生成（`attach/syscall_trace_attach_impl/generate_syscall_id_table.sh:1-5`），交叉编译时得到的是构建机架构的号。
- `while (ss) { ss >> name >> id; ... }` 最后一轮读失败时仍插入一条空名和未初始化的 id（`attach/syscall_trace_attach_impl/src/syscall_table.cpp:29-42`）。

**B.4.9 private data 的错误处理**（读码确认）
找不到 syscall 时返回 `-EEXIST`（应为 ENOENT）；`stoi` 遇到非数字抛异常没人接（`attach/syscall_trace_attach_impl/src/syscall_trace_attach_private_data.cpp:11,55,61`）。
runtime 传进来的是 `std::to_string(tracepoint_id)`，总是数字，只有直接调这个 API 时才会抛。

**B.4.10 名字混乱与过时文档**（读码确认）
- `_bpftime__setup_syscall_hooker_callback` 和 `_bpftime__syscall_dispatcher` 是死代码（`syscall_trace_attach_impl.cpp:170-192`）；
  transformer 找的是 `_bpftime__setup_syscall_trace_callback`，报错文案却写 `..._hooker_callback`（`agent-transformer.cpp:89-95`）。`nm -D` 核对过 agent 只导出前者。
- `attach/text_segment_transformer/README.md:29-31` 说"用 NOP 填充以保持空指针解引用行为"，NOP 实际是滑板；也没提需要 `vm.mmap_min_addr=0`。

---

<a id="b-vm"></a>

## B.5 VM、JIT、ubpf 与 AOT

**B.5.1 运行时除以 0 让宿主进程 SIGFPE（x86）**（已实跑）
- 现象：DIV/MOD 用 `select(src==0, …, udiv(...))` 生成，除法指令仍无条件执行；LLVM 里除零是未定义行为。
- 位置：`vm/llvm-jit/src/compiler.cpp:681-764,851-875`。
- 影响：除数从内存读入 0 时，udiv64/umod64/sdiv64 都被 signal 8 杀死，O0 也一样。内核语义是结果为 0 或保持 dst。
除数是常量 0 时会被常量折叠，所以测试用例里的除零用例能过。aarch64 硬件除零不陷入，但 IR 层仍是 UB（推断）。

**B.5.2 64 位有符号除法和取模把寄存器除数截成 32 位**（已实跑）
- 现象：为了"立即数要符号扩展"做的 trunc + sext，对寄存器操作数也做了。
- 位置：`vm/llvm-jit/src/compiler.cpp:698-702,867-871`。
- 影响：实跑 `0x200000000 sdiv 0x100000003` 得 `0xaaaaaaaa`（应为 1）。除数来自运行时（如从内存读入）且低 32 位为 0、高位非 0 时，截断后变成除零，`sdiv64`、`smod64` 都被 SIGFPE 杀死（核查时已实跑）：`src_is_zero` 比较的是截断前的值（`:681-684`），漏过了检查。除数是 lddw 常量时会被常量折叠成垃圾值，不崩。

**B.5.3 bpf-to-bpf 调用的栈帧只隔 64 字节，会互相覆盖**（已实跑）
- 现象：单位错了。`STACK_SIZE = (EBPF_STACK_SIZE + 7) / 8 = 64`，单位是 8 字节；分配数据栈时按 i64 用它，每帧 512 B（`:404-410`），调用和返回时却按字节 `r10 -= STACK_SIZE`，每帧只移 64 B。
- 位置：`vm/llvm-jit/src/compiler.cpp:42`、`:404-410,548-554,1455-1461`。（`:43` 的 `CALL_STACK_SIZE` 是返回地址栈的深度，和这个问题无关。）
- 影响：调用方写 `[r10-72]=0x11`，被调方写 `[r10-8]=0x22`，返回后调用方读到 0x22。

**B.5.4 第 0 条指令就是局部调用时 JIT 代码崩溃**（已实跑）
- 现象：判断"上一条是局部调用"时用了 `i > 1`，漏掉了 pc 0。
- 位置：`vm/llvm-jit/src/compiler.cpp:482`。
- 影响：llvm 后端 SIGSEGV；同一程序把调用挪到 pc 1 就正常。ubpf 后端没有这段 `i > 1` 判断，但带局部调用的程序在 ubpf 上另有 B.5.5 的问题，W4 实跑两次得到的返回值不确定（0x1 和一个垃圾值）。

**B.5.5 ubpf 适配器把局部调用当成 helper 调用改写 imm**（已实跑）
- 现象：只看 `code == EBPF_OP_CALL`，不看 `src_reg`。
- 位置：`vm/compat/ubpf-vm/compat_ubpf.cpp:83-104`。`runtime/src/bpftime_prog.cpp:316-321` 的注释也承认只有 LLVM 兼容层能区分局部调用。
- 影响：带 bpf-to-bpf 调用的程序在 ubpf 后端跳到错误位置；imm 不在映射表里时直接加载失败。

**B.5.6 tail call 失败后不继续往下执行，两个后端行为不同**（已实跑）
- 现象：bpftime 的 `bpf_tail_call` 在 helper 里同步跑完目标程序、结果放进线程局部变量（`runtime/src/bpf_helper.cpp:622-629`，`runtime/src/bpftime_prog.cpp:270-271`），所以成功后马上退出是有意设计（注释见 `compiler_utils.cpp:360-361`）。
  问题在失败路径：llvm 后端失败时也直接跳到出口、返回 -1；ubpf 后端是"返回 0 才退出"。内核语义是失败则继续执行下一条。
- 位置：`vm/llvm-jit/src/compiler_utils.cpp:359-364`。
- 影响：实跑 helper 12 返回 -1 后，后面的 `r0 = 7` 没执行，程序返回 `0xffffffffffffffff`。

**B.5.7 `unload_code()` 不清 JIT 缓存**（已实跑）
- 现象：只清指令，不清已编译的函数；头文件注释说会清。
- 位置：`vm/llvm-jit/src/vm.cpp:140-143`；注释 `vm/llvm-jit/include/llvmbpf.hpp:76`。
- 影响：加载 A → 执行 → unload → 加载 B → 执行，跑的还是 A。`bpftime_prog_unload` 在未 JIT 时会调它。

**B.5.8 `generate_ptx(target_cpu)` 的参数落到了 bool 形参上**（已实跑；R4、R6 独立发现）
- 现象：`vm.cpp` 调 `jit_ctx->generate_ptx(target_cpu)`，而该函数第一个形参是 `bool main_with_arguments`；指针转成 true，目标 CPU 恒为默认的 sm_60。
- 位置：`vm/llvm-jit/src/vm.cpp:350-353`；`vm/llvm-jit/src/llvm_jit_context.hpp:66-69`。
- 影响：请求 sm_80，输出 `.target sm_60`。bpftime 的 GPU 主线直接调内部接口、参数齐全，不受影响；受影响的是 llvmbpf 的公共 API。

**B.5.9 `!err` 判断写反**（读码确认）
- 现象：非 PRELOAD 构建（默认，`BPFTIME_ENABLE_LLVM_PRELOAD` 为 OFF）里，`mainDylib.define(...)` 成功时打 "failed"（DEBUG 级，默认看不到），失败时 `llvm::Error` 没被检查。PRELOAD 分支的判断是对的。
- 位置：`vm/llvm-jit/src/llvm_jit_context.cpp:804-806,855-857`。
- 影响：本树定义了 `LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING`，所以不会 abort；但真失败时没有任何提示。

**B.5.10 LLVM 初始化或 PTX 生成失败会 `exit()` 整个进程**（读码确认）
- 现象：`create_and_initialize_lljit_instance` 与 `generate_ptx/spirv` 用 `static ExitOnError`。
- 位置：`vm/llvm-jit/src/llvm_jit_context.cpp:688,742,750,907,938,1002,1033`。
- 影响：宿主目标探测失败或 PTX 生成失败时宿主直接退出。另外 `create_vm_instance` 抛的 `std::runtime_error` 会穿过 `extern "C"` 的 `ebpf_create`。

**B.5.11 原子操作的语义比内核弱**（推断，需要并发测试）
CMPXCHG 的 r0 来自 cmpxchg 之前一次非原子的 load，并发下可能与真正比较的旧值不一致；所有原子操作用 `Monotonic` 序，比内核 BPF_FETCH 类原子的全序语义弱。
- 位置：`vm/llvm-jit/src/compiler.cpp:1698-1718`；`vm/llvm-jit/src/compiler_utils.cpp:388`。

**B.5.12 编译器对异常输入缺少检查**（推断，未构造用例）
- 程序计数器用 `uint16_t`（`compiler.cpp:279-282,314,593`），指令数超过 65535 时循环不终止、跳转目标回绕；`EBPF_MAX_INSTS` 没被检查。
- `JA_IMM`（gotol）切块时 `blockBegin[i + curr.imm + 1] = true` 不检查越界（`compiler.cpp:322-325`），恶意 imm 会堆越界写。
- 局部调用的目标不参与切块（`compiler.cpp:311-331`），只有前一条恰好是跳转或 exit 时才成为块首，否则报 "jump to an illegal position"。clang 产物一般满足。

**B.5.13 两类指令的静默处理**（读码确认）
- lddw 的 `src_reg ≥ 7` 时不写目标寄存器，也不报错（`compiler.cpp:1093-1338` 的 if 链没有 else）；ubpf 适配器同样情形会报错。
- `CALL | 0x8`（0x8d，CALLX）被当成按 imm 调 helper（`compiler.cpp:1365-1369`），内核的 callx 用的是寄存器；信号执行路径的准入检查明确拒绝 0x8d（`runtime/src/bpftime_prog.cpp:312-313`）。

**B.5.14 惰性 JIT 的几个后果**（读码确认 / 推断）
- attach 私有 helper 在程序编译之后才注册（`runtime/src/attach/bpf_attach_ctx.cpp:304` 先 load，`:310-312` 后注册），靠惰性 JIT"碰巧"再编一次才成功。读码确认。
- `bpftime_prog_load(true)` 编译失败只 WARN 并返回 0（fail-open）；之后每次执行都重编、都失败，程序体永远不执行。读码确认。
- 两个线程同时首次 `exec` 会各编一次，后者替换 LLJIT，旧 LLJIT 析构后先拿到的函数指针悬空（`vm/llvm-jit/src/vm.cpp:148-171`）。推断。

**B.5.15 `bpftime_prog` 的小问题**（读码确认；部分 R4、R6 独立发现）
- `is_cuda()` 的 if/else 两个分支设置的 lddw 回调完全相同，日志却说 "Do not set lddw helpers due to cuda program"（`runtime/src/bpftime_prog.cpp:154-162`）。
- `load_aot_object` 不置 `program_loaded`（`:342-354`），之后 `prepare_for_signal_execution` 返回 `-EINVAL`（AOT 本来也被信号路径拒绝）。
- `errmsg` 每次 `strdup` 不释放；`ufunc_ctx` 成员没用到也没初始化。
- 构造函数改写全局配置见 [B.1.10](#b-shm)。

**B.5.16 注释与实现不符**（读码确认；R4 发现，含原附录 A.6 第 2 条）
- `vm/vm-core/include/ebpf-vm.h:151` 说 `ebpf_exec` "using the interpreter"，llvm 后端下实为惰性 JIT（`ebpf_exec` → `bpftime_llvm_vm::exec` → `llvmbpf_vm::exec`，首调用时 compile 并缓存）。`vm/llvm-jit/include/llvmbpf.hpp:79-82` 的注释反而是对的。
- `ebpf-vm.h:64` 说 `vm_name` 用 `""` 表示默认，实际空串会抛异常（`vm/compat/include/bpftime_vm_compat.hpp:231-234`）。

**B.5.17 其他不直观之处**（读码确认 / 推断）
- 有两份 `ebpf_inst` 定义，字段名不同（runtime 里是 `code/src_reg`，llvmbpf 里是 `opcode/src`）却共用一个 include guard。读码确认。
- AOT 产物里的 map fd 是编译时烧进去的；`get_compiled_code` 会再编一次。读码确认。
- ubpf 只有 63 个 helper 槽，默认三组共 39 个再加 attach 私有 helper，接近上限；超出后注册失败被忽略，`helper_id_map` 却已写入（`vm/compat/ubpf-vm/compat_ubpf.cpp:60-63`）。推断。
- `register_vm_factory` 用 `constructor(0)`（`vm/compat/llvm-vm/compat_llvm.cpp:82`），落在 GCC 保留的 0–100 区间。推断。
- `vm/example/main.c`：`res = ebpf_exec(vm, &m, sizeof(m), &res)` 用返回码覆盖了结果；程序调用 helper 3 却没注册，llvm 下编译必失败。推断（未运行）。
- llvmbpf cli 的 CMake 把 libbpf 克隆进源码树 `vm/llvm-jit/cli/libbpf_project`（`vm/llvm-jit/cli/CMakeLists.txt:18-31`）；cli README 描述的 argparse 界面与手写解析不符。读码确认。

**B.5.18 `vm/README.md` 大面积过时**（读码确认；R4、R6 独立发现）
提到的 `optimizer.cpp`、`native_code_gen.cpp`、`ptx_code_gen.cpp`、`llvm_bpf_jit.hpp`、`ebpf_unregister`、`ebpf_aot_compile`、`set_inline_map`、`do_get_ptx()`、`LLVM_JIT_DUMP_IR` 都不存在；
`ebpf_exec` 签名和 lddw 回调返回类型写错；`example/main.cpp` 实为 `main.c`。读这个 README 时以源码为准。


**B.5.19 ubpf 后端上用 attach 私有 helper 的程序加载失败**（读码确认，本机未实跑；W4 写作时发现）
- 现象：ubpf 适配器在 `load_code` 时按 `helper_id_map` 改写 `call` 的 imm，找不到映射就返回 `-EINVAL`（不论 imm 大小，只是报错文案不同）。
  `load_code` 在 `bpftime_prog_load` → `ebpf_load` 时就执行（`runtime/src/bpftime_prog.cpp:170-182`），而 Frida/trap 注册的私有 helper（183 `bpf_get_func_arg`、184 `bpf_get_func_ret`、186 `bpf_get_retval`）在这之后才登记（`runtime/src/attach/bpf_attach_ctx.cpp:304` 先 load，`:310-312` 后注册）。
  所以用到它们的程序在 ubpf 上加载即失败；llvm 后端靠惰性重编躲过了这个顺序问题。
- 位置：`vm/compat/ubpf-vm/compat_ubpf.cpp:80-101`；`runtime/src/attach/bpf_attach_ctx.cpp:304-312`。

---

<a id="b-helpers"></a>

## B.6 helper

**B.6.1 `bpf_perf_event_output` 忽略 `flags` 和 `ctx`**（读码确认；R4 发现，含原附录 A.6 第 6 条）
- 现象：`flags` 在函数体里没有任何使用，总是写当前 CPU 那一格；`BPF_F_INDEX_MASK` 显式索引被当成 current-cpu。每个事件还要 3 次亲和性系统调用（getaffinity + 2 次 setaffinity）。
- 位置：`runtime/src/bpf_helper.cpp:487-553`（亲和性调用在 `:499,502,549`）。
- 影响：用显式 CPU 索引的程序结果不对；热路径多了 3 个绑核系统调用（成功写入时另有找分片的 `getpid`、`gettid`，每次共 5 个，见[第 7 章](07-helpers.md)）。

**B.6.2 perf buffer 满了静默丢弃，helper 还返回 0**（读码确认；R1 发现，原附录 A.6 第 5 条）
- 现象：`output_data` 不看 `append_record_parts` 的返回值，一律返回 0；全 runtime 不生成 `PERF_RECORD_LOST`（只在 `runtime/include/bpftime_epoll.h:314` 有枚举）。
未 mmap 时同样静默丢弃。唯一会报错的是 payload 过大（`E2BIG`）。
- 位置：`runtime/src/handler/perf_event_handler.cpp:385-387`。
- 影响：BPF 程序和消费者都看不到丢事件。内核语义是 helper 返回 `-ENOSPC` 且消费者能收到 lost 记录。
附录 A 记录过在 Jetson 上的实测后果：大响应下 bpftime 只交付约一半事件（见 [附录 A.6](A-research-notes.md#a6)）。

**B.6.3 `bpf_tail_call` 固定拷贝 ctx 的前 64 字节**（读码确认；后两点为推断）
- 现象：不管 ctx 多大都 `memcpy` 64 字节到局部数组；被调程序看到的是副本，修改不回传。
- 位置：`runtime/src/bpf_helper.cpp:613-619,634-639`。
- 影响：ctx 小于 64 字节时越界读（单测 `test_user_to_user_tailcall.cpp:63-64` 传的就是 4 字节的 `int`）。
内核程序的新 fd 若恰好和某个 bpftime 假 fd 同号，会被误判为用户态程序（推断）；嵌套 tail call 里内层执行结束会提前恢复 MPK 保护（推断）。

**B.6.4 ringbuf helper 的返回值和文案**（读码确认）
失败返回 `(uint64_t)-1` 而不是 `-errno`；`flags` 只打 WARN 不处理；`bpf_ringbuf_discard` 的 WARN 文案写成了 "submit"（`runtime/src/bpf_helper.cpp:432-485`，文案在 `:480-481`）。

**B.6.5 map helper 返回 -1 而不是 -errno**（读码确认）
BPF 程序看到的 map helper 失败返回值大多是 `-1`，错误码在 errno 里；内核 helper 直接返回负的错误码。按内核习惯写的 `if (ret == -ENOENT)` 判断会失效。

**B.6.6 `bpf_get_current_comm` 返回的是可执行文件名**（读码确认）
- 现象：读 `/proc/self/exe` 取 basename，不是线程的 comm；`strncpy` 名字长于 size 时不补 NUL；失败返回 1（内核返回负 errno）。
- 位置：`runtime/src/bpf_helper.cpp:276-297`。函数内的 `static std::string` 惰性填充还有数据竞争。

**B.6.7 `bpf_trace_printk` 的实现**（读码确认；Apple arm64 部分为推断）
- 现象：`va_start(args, fmt_str)` 用的是局部变量（警告被 pragma 关掉），GCC 实际以最后一个具名参数 `fmt_size` 为界，所以格式参数恰好从 r3 开始取；忽略 `fmt_size`；直接 `vprintf` 到宿主进程的 stdout（违反"不碰宿主 stdio"的契约，`runtime/src/bpf_helper.cpp:78-90`）；恒返回 0（内核返回写入的字节数）。
- 位置：`runtime/src/bpf_helper.cpp:78-90`；AOT 示例 `tools/aot/example/main.c:8-20` 的 `_bpf_helper_ext_0006` 复制了同样的写法（`tools/aot/README.md:155` 也有一份）。
- 影响：能用是巧合。在变参走栈的平台（Apple arm64）上会读错（推断）。

**B.6.8 其他 helper 的语义差异**（读码确认，第 3 条为推断）
- `bpf_get_prandom_u32` 用 `rand()`：没播种，只有 31 位（`runtime/src/bpf_helper.cpp:179-182`）。
- helper 45 的注册名写成 `"bpf_probe_str"`（`:1309`）；名字只对 ubpf 判断 `bpf_tail_call` 有意义，无功能影响。
- `bpf_xdp_adjust_head` 头部空间不足时的 `memmove` 只有原 headroom 为 0 时位置才对；`data_end` 没跟着移；没检查 `data_end + d ≤ buffer_end`（`:730-746`）。内核此时直接返回 `-EINVAL`。
- `bpf_probe_read_str` 只有**读**失败才把 dst 清零（`:396-399`），写 dst 失败（`:410-411`）直接返回 `-EFAULT` 不清零。
- 没开 `ENABLE_PROBE_READ_CHECK` 时 probe_read 就是裸 `memcpy`，坏指针直接崩。CMake 默认是开的，但本机现成的 `build-jit/` 是关的，做实验时以自己的 `CMakeCache.txt` 为准。

**B.6.9 ufunc 的问题**（读码确认；ABI 部分为推断）
- 位置：`runtime/src/ufunc.cpp`。
- 只向原生函数传 5 个参数，却允许 `num_args` 到 6（`:96-108`）。
- DOUBLE 参数按位放进整型寄存器传递，返回值只取整型返回寄存器；原生函数真声明了 `double` 形参或返回值会拿错（x86-64/aarch64 浮点走 XMM/V 寄存器，推断）。
- INT8/16/32 不做截断或符号扩展。
- `bpftime_ufunc_register_ufunc` 不查 id 上界、不增加 `ufunc_func_cnt`，按名字查找（1001）找不到这样注册的函数。
- `resolve_from_info` 的容量检查差一（最多 MAX-1 项）；`is_attached` 为真或找不到函数信息时打一条 ERROR 后返回 0（不是错误码，`:83-91`），BPF 程序分不清失败和返回值 0。

**B.6.10 死代码**（读码确认）
定义了却没注册：`bpftime_ktime_get_coarse_ns`（`runtime/src/bpf_helper.cpp:184-194`，注册的是 `bpf_ktime_get_coarse_ns`）、`bpftime_xdp_load_bytes`（`:760-770`）。
`add_helper_group_to_prog` 忽略单个注册的返回值，恒返回 0（`:1165-1167`）。

**B.6.11 trap 后端只准入信号安全的 helper**（读码确认；有意设计，不直观）
- 现象：trap 后端在 attach 时调 `prepare_for_signal_execution()` 逐条检查程序里的 helper 调用，不在白名单里的直接拒绝，返回 `-ENOTSUP`，日志 "Trap attach rejected … not async-signal-safe"。
  白名单只有 58、173、174、187 和 trap 自己的 183、184、186，而且函数指针也要对得上。
- 位置：`runtime/src/attach/bpf_attach_ctx.cpp:391-395`；`runtime/src/bpftime_prog.cpp:289-340`；白名单 `runtime/src/bpf_helper.cpp:895-910`；单测 `runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp:146-160`（1、2、6、12、27、67 等都要求被拒）。
- 影响：这是有意设计（信号处理函数里只能做异步信号安全的事），不是 bug，但后果比"只准入白名单"听上去大：在 riscv64 的 trap 后端上，**用了 map helper（1/2/3）、printk、tail call、栈 helper 的程序都 attach 不上**。详见[第 4 章](04-attach-uprobe.md)的 trap 一节。

Frida 后端注入的自定义 helper 见 [B.3.7](#b-attach)；GPU 上能用的 helper 见 [B.8.7](#b-gpu)。

---

<a id="b-maps"></a>

## B.7 map

**B.7.1 `map_init` 失败留下空 map**：见 [B.1.4](#b-shm)。

**B.7.2 dup 出来的 fd 和原 fd 共享实现对象，却各用各的锁**（推断；锁是两把已读码确认）
- 位置：`runtime/src/handler/map_handler.hpp:251-255,278`；`runtime/src/bpftime_shm_internal.cpp:1109-1115`。
- 影响：锁在 handler（每个 fd）上，不在 map 上。对需要加锁的 map（hash、lru、queue…），经两个 fd 并发操作不互斥。

**B.7.3 默认 HASH（fix_size 实现）不实现 flags、容量和删除语义**（已实跑）
- 现象：`elem_update` 忽略 flags，也不看底层返回值，恒返回 0；`elem_delete` 恒返回 0；`elem_lookup` 未命中不设 errno。
- 位置：`runtime/src/bpf_map/userspace/fix_hash_map.cpp:26-45`；`runtime/src/bpf_map/bpftime_hash_map.hpp:142-158`。
- 影响：实跑 `max=2` 时，插第 3 个返回 0 但查不到；对已存在的 key 用 `BPF_NOEXIST` 返回 0；删除不存在的 key 返回 0；对不存在的 key 用 `BPF_EXIST` 照样插入成功。

**B.7.4 内核兼容单测测的不是默认 HASH**（读码确认）
- 现象：`runtime/unit-test/maps/kernel_unit_tests.cpp` 全部用 `var_size_hash_map_impl`（`:55,157,299` 等），而 `BPF_MAP_TYPE_HASH` 默认走 fix_size（`runtime/src/handler/map_handler.cpp:54-58`）。
切换用的宏 `BPFTIME_USE_VAR_HASH_MAP` 在任何 CMake 文件里都没有定义入口。
- 影响："内核 test_maps 通过"不代表默认 HASH map 有内核语义。

**B.7.5 STACK_TRACE 的内容比较写反了**（已实跑）
- 现象：`equals = (长度相等); if (!equals) equals = equals && std::equal(...)`，内容永远不比较。
- 位置：`runtime/src/bpf_map/userspace/stack_trace_map.cpp:115-120`。
- 影响：同桶、同长度的不同栈被当成同一个，返回旧 id；`!discard_old_one` 时不同栈也返回同一个 id（内核返回 `-EEXIST`）。
实跑 `id1=637 id2=637 stored==a? 1 stored==b? 0`。单测 `test_stack_trace_map.cpp:57-101` 没覆盖这种情形。

**B.7.6 STACK_TRACE 经 syscall lookup 可能越界读**（推断）
- 现象：value vector 长度等于实际帧数（`stack_trace_map.cpp:103-108`），而 syscall 路径按 `value_size` 做 `memcpy`（`runtime/syscall-server/syscall_context.cpp:575-576`）。
- 影响：帧数少于 `value_size/8` 时读出 shm 里后面的数据（内核会补零）。

**B.7.7 per-CPU hash 的 helper 视角 delete 清错范围、不删 key**（已实跑；R2 发现，原附录 A.6 第 4 条）
- 现象：`std::fill(begin, begin + cpu * value_size, 0)` 清的是 CPU `0..cpu-1` 的份，不是当前 CPU 的 `[cpu*value_size, (cpu+1)*value_size)`；在 CPU 0 上调用什么都不清。key 也不删除。
- 位置：`runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp:96-107`；对照 `elem_update` 的 `:83-84` 可知是笔误。
- 影响：实跑（4 核，在 CPU1 上）删除后 `key still present=1 values=[0,22,33,44]`。

**B.7.8 per-CPU hash 的 helper 视角 update 不执行 NOEXIST/EXIST 语义，也不查容量**（读码确认；后果为推断）
- 现象：只用 `check_update_flags` 检查 flags 取值是否合法，然后无条件写入或插入；loader 侧的 `elem_update_userspace`（`:162-182`）才做 NOEXIST/EXIST 判断和 `max_entries` 检查。
该实现 `should_lock = false`，新 key 插入时无锁改 `unordered_map`。
- 位置：`runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp:68-94`；`per_cpu_hash_map.hpp:52`。
- 影响：多线程同时插不同的新 key 有数据竞争（推断）。

**B.7.9 per-CPU hash 的一条注释两处失实**（读码确认；原附录 A.6 第 3 条）
注释说 "Allocate as a local variable to make it thread safe, since we use sharable lock"，实际用的是共享成员 `key_templates[0]`，而且全类不加锁。
- 位置：`runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp:122-124`。

**B.7.10 LPM trie 的跨进程引用**（推断，高置信）
- 现象：结构里存了一个 `managed_shared_memory&`（`runtime/src/bpf_map/userspace/lpm_trie_map.hpp:98`），`create_node` 用它分配（`lpm_trie_map.cpp:119-129`）。
- 影响：map 由 syscall-server 创建、agent 进程里的程序做 update 时，这个引用指向别的进程里的对象地址 → 未定义行为。lookup、delete 不用它。

**B.7.11 LPM trie 的其他语义差异**
- lookup 返回线程局部的副本（`lpm_trie_map.cpp:254-263`），程序通过返回的指针写 value 不会落进 map（内核里会）。已实跑。
- `get_next_key` 基本没实现：`key == NULL` 只返回从根沿左优先路径遇到的第一个非中间节点，否则一律 ENOENT（`:583-612`）。实跑 2 个条目只遍历到 1 个。
- 删除是逻辑删除（标成 intermediate，不回收节点，`:553-559`）；之后对同一前缀 `BPF_NOEXIST` 插入返回 EEXIST（`:349-355`）。实跑 `ret=-1 errno=17`，内核此时应成功。
- `matchlen == _max_prefixlen` 分支把当前节点直接当 found，即使它是中间节点（`:221-225`），不会回退到更短的已匹配前缀。推断（边界情况）。
- handler 层加一次锁（`should_lock = true`，`lpm_trie_map.hpp:118`），实现内部又加一次 `mapSpin`（声明在 `:102`，各操作入口如 `lpm_trie_map.cpp:194`）。无害但多余。读码确认。

**B.7.12 BLOOM filter 经 syscall 的 LOOKUP 不是 peek**（读码确认）
- 现象：`elem_lookup` 恒返回空指针并设 EOPNOTSUPP，文件头的注释却写 "BPF_MAP_LOOKUP_ELEM -> Peek"。经 syscall 时 errno 又被改写成 ENOENT（`runtime/syscall-server/syscall_context.cpp:571-572`，核查时实跑 `errno=2`）。
  另外 `elem_delete`、`map_get_next_key` 的注释说返回 `-EOPNOTSUPP`，实际是 `-1` + errno（`bloom_filter.cpp:324-335`）。
- 位置：`runtime/src/bpf_map/userspace/bloom_filter.cpp:283-289`；`bloom_filter.hpp:9,99-101,117,125`。
- 影响：内核的 syscall LOOKUP 对 bloom 走的是 peek，用 libbpf `bpf_map_lookup_elem` 查 bloom 的程序会失败。

**B.7.13 QUEUE/STACK 的 update 与 delete**（读码确认）
- 现象：`elem_update` 不检查 flags（和 `map_push_elem` 不同），满时返回 E2BIG；注释说非法 flags 返回 `-EINVAL`、满时返回 `-EBUSY`（`queue.hpp:98-104`）。`elem_delete` 被当成 pop 用，而内核 syscall DELETE 对 queue/stack 返回 EINVAL。
- 位置：`runtime/src/bpf_map/userspace/queue.cpp:66-120`。

**B.7.14 `BPF_MAP_LOOKUP_AND_DELETE_ELEM` 只对 queue/stack 有效，其他类型静默假成功**（已实跑，C3 核查时）
- 现象：`bpftime_map_lookup_and_delete_elem` 直接调 pop，其他类型落到 `-ENOTSUP`（-95）。syscall-server 把 -95 直接当返回值，errno 却是开头清成的 0。
- 位置：`runtime/src/bpftime_shm.cpp:155-166`；`runtime/src/handler/map_handler.cpp:1439-1443`；`runtime/syscall-server/syscall_context.cpp:490,613-614`。
- 影响：裸 syscall 返回 -95 且 errno 为 0；经 libbpf 的 `bpf_map_lookup_and_delete_elem` 对 HASH 返回 0，被当成成功，value 没写、key 也没删。

**B.7.15 ARRAY_OF_MAPS 的 lookup 把内层 map id 当指针返回**（已实跑，C3 核查时：经 syscall lookup 时 SIGSEGV）
- 现象：`return (void *)((u_int64_t)map_id)`。helper 视角下程序把它当 map 句柄继续用，说得通；但 syscall 路径会 `memcpy(value, (void*)id, ...)`，解引用一个小整数地址。
存的 id 为 0 时 lookup 返回空指针，被当成"不存在"。
- 位置：`runtime/src/bpf_map/userspace/map_in_maps.hpp:25-31`；`runtime/syscall-server/syscall_context.cpp:575`。

**B.7.16 PROG_ARRAY 的内核分支每次 lookup 都新开一个 fd**（推断）
- 现象：内核程序的条目每次 lookup 都 `BPF_PROG_GET_FD_BY_ID`（`runtime/src/bpf_map/userspace/prog_array.cpp:136-144`）；tail call 路径会关掉，syscall 路径不关。
- 影响：经 syscall 反复查 prog array 会泄漏 fd。

**B.7.17 kernel-user 共享 map 把进程私有的 fd 和 mmap 地址存进 shm**（推断，高置信）
- 现象：第一个调 `init_map_fd` 的进程写入后，其他进程看到 `map_fd >= 0` 或 `mmap_ptr != nullptr` 就直接用，而这个 fd 号、地址在本进程无效。
- 影响：只有"始终只有一个 agent 进程访问"时才正确。

**B.7.18 KERNEL_USER_ARRAY 的元素步长没按 8 字节对齐**（推断，需要 root）
- 现象：mmap 大小按 `roundup(value_sz, 8) * max_entries` 算，访问却用 `key * _value_size`。内核 array 的元素步长是 `round_up(value_size, 8)`。
- 位置：`runtime/src/bpf_map/shared/array_map_kernel_user.cpp:27-35,142,176,202`。
- 影响：`value_size` 不是 8 的倍数时读写错位。另外 delete 把元素清零并返回 0（`:183-208`，内核返回 EINVAL），mmap 路径的 update 忽略 flags（`:169-178`），这两点读码确认。

**B.7.19 KERNEL_USER_HASH 的查找缓冲是 shm 里的一份共享缓冲**（读码确认）
- 现象：`value_vec` 是对象成员，放在 shm 里，`should_lock = false`，并发 lookup 互相覆盖；注释说 "Allocate as a local variable to make it thread safe, since we use sharable lock"，既不是局部变量也没有锁。`init_map_fd` 失败被忽略。
- 位置：`runtime/src/bpf_map/shared/hash_map_kernel_user.hpp:26-35`；`hash_map_kernel_user.cpp:16-37,49-53`。

**B.7.20 KERNEL_USER_PERCPU_ARRAY 完全不可用**（读码确认）
- 现象：`init_map_fd` 只有定义，全仓库没人调用；`kernel_map_fd` 恒为 -1，所有操作都对 fd -1 调 libbpf；`value_data` 只有 1 字节，`_value_size`、`_max_entries` 没初始化。
- 位置：`runtime/src/bpf_map/shared/percpu_array_map_kernel_user.cpp:12,34-63`。

**B.7.21 KERNEL_USER_PERF_EVENT_ARRAY 的多处问题**（位置均在 `runtime/src/bpf_map/shared/perf_event_array_kernel_user.cpp`）
- `sample_period = duration_ms * 1000`，对纳秒计的 CPU_CLOCK 实际是 10 µs（`:446-458`）。读码确认，未实测。
- `pid=0, cpu=-1` 的 perf event 只在创建它的进程自己占用 CPU 时才计时触发，daemon 或 syscall-server 空闲时搬运程序可能长时间不跑。推断。
- `reserve_lock` 从不使用（`:52,249,277-284`）；`output_data_into_kernel` 失败返回正的 errno，被调用方当成功（`:81-84`，`runtime/src/bpftime_shm.cpp:612-617`）。读码确认。
- `user_rb_map` 是无锁的进程全局 `std::map`（`:39-47,229-245`），多线程首次输出有竞争。推断。
- 构造函数里 `kernel_perf_fd`、`btf_fd`、`btf` 对象、`user_rb_fd` 都没关闭或释放（`:109,133,464,486`）；早期失败 `return` 留下半初始化对象（`:112-126`），后期失败却抛异常（`:138-155`）。读码确认。
- 对象带 virtual 析构（`perf_event_array_kernel_user.hpp:42`），放在 shm 里就带着进程私有的 vptr。推断：在创建进程以外通过 vptr 调用会跳错地址，取决于编译器是否去虚化。

**B.7.22 ringbuf 和内核 / libbpf 的差异**（读码确认）
- 记录头的第二个 u32 存的是 map fd，不是页偏移（`runtime/src/bpf_map/userspace/ringbuf_map.cpp:258-261,323`）；`bpf_ringbuf_submit` 靠 `ptr[-1]` 取 fd（`runtime/src/bpf_helper.cpp:465,478`）。
- 跨环尾时用 DISCARD 填充记录，不是内核的双重映射（`ringbuf_map.cpp:302-320`）。
- `max_entries` 只要求 2 的幂，不要求页对齐（`runtime/src/handler/map_handler.cpp:806-818`）。
- `BPF_RB_NO_WAKEUP` 等 flags 被忽略；没有 `bpf_ringbuf_query`、`reserve_dynptr`。

**B.7.23 LRU 是"全局 LRU"**（读码确认）
lookup 会把节点移到链表头（`runtime/src/bpf_map/userspace/lru_var_hash_map.cpp:33`），所以只读操作也必须加锁；淘汰时先 erase 再 evict（`:66-71`）。
没有内核 LRU 的 per-CPU 本地列表和批量淘汰，`BPF_F_NO_COMMON_LRU` 等 flag 不认。

**B.7.24 外部 map 的 `map_free` 回调永远不会被调用**（读码确认；R1 发现）
- 现象：外部类型的 `map_init` 把 `map_impl_ptr` 置空（`runtime/src/handler/map_handler.cpp:1130`），`map_free` 开头见空就 return（`:1148-1150`），走不到 default 分支。外部 map ops 表也是进程本地的。

**B.7.25 关掉 libbpf 构建时 STACK_TRACE 和 PROG_ARRAY 的 `#ifdef` 不对称**（读码确认；R1、R2 独立发现）
- 现象：两者的 `map_init`/`map_free` 包在 `BPFTIME_BUILD_WITH_LIBBPF` 里（`map_handler.cpp:919-932,1194-1197,1214-1216`），STACK_TRACE 的 lookup/update/delete/get_next_key 分支却不在（`:159-163,320-324,474-478,635-639`）。
get_next_key 的 kernel_user 分支条件是 `__linux__ && LIBBPF`（`:492`），其他三个操作只有 `LIBBPF`。
- 影响：`BPFTIME_BUILD_WITH_LIBBPF=OFF` 时 STACK_TRACE 创建失败被吞（见 B.1.4），之后访问空指针（推断）；PROG_ARRAY 退化为"不支持"。

**B.7.26 `bpftime_hash_map` 的墓碑不回收**（推断，性能）
没有 rehash，`EMPTY` 槽只减不增（`runtime/src/bpf_map/bpftime_hash_map.hpp:186-188,226`）。长期增删后所有槽都是 FILLED 或 DELETED，未命中的查找退化成全表扫描（`:146-157`）。

**B.7.27 死代码**（读码确认）
`ensure_on_certain_cpu` 的主模板对零参 `std::function<T()>` 传了参数，一实例化就编译失败，`currcpu` 也从不赋值（`runtime/src/bpf_map/map_common_def.hpp:33-49`）。

queue/stack 单测的注释问题见 [B.10.8](#b-build-test)；GPU map 见 [B.8](#b-gpu)；daemon 能用的 map 类型见 [B.9.4](#b-periphery)。

---

<a id="b-gpu"></a>

## B.8 GPU / CUDA

本节全部来自读代码。本机没有 GPU，标"推断"的运行时后果都没有验证过（可在 Jetson 上验证）。

**B.8.1 bpftime 里的 "kprobe" 就是 CUDA kernel 探针**（读码确认；X#4，不直观）
`ATTACH_CUDA_PROBE = 8`、`ATTACH_CUDA_RETPROBE = 9`（`attach/nv_attach_impl/nv_attach_impl.hpp:43-44`）有意和 `BPF_TYPE_KPROBE = 8`、`KRETPROBE = 9`（`runtime/include/bpftime_shm.hpp:59-60`）同号。
kprobe 类型的 perf event 会交给注册在 8/9 上的 nv_attach_impl。CPU 侧没有 kprobe 实现。

**B.8.2 GPU→CPU 调用的参数在拿锁之前就写进了共享区**（读码确认；后果为推断）
- 现象：helper 0001/0002/0003/0006/0501 先 `simple_memcpy(&req…)`，再调 `make_helper_call`，后者才 `spin_lock`。
- 位置：`attach/nv_attach_impl/trampoline/default_trampoline.cu:241-245,298-305,317-320,330-341,416-427`。
- 影响：多个 GPU 线程并发调用普通 map 的 lookup/update 时，key/value 可能被别的线程覆盖，CPU 处理的是别人的 key。
`example/gpu/README.md:290-296` 宣称"先拿锁再写参数"，与实现相反。

**B.8.3 trampoline 源码与提交的生成物不同步**（读码确认）
- 现象：`.cu` 里 `GPU_HELPER_MAX_BUF = 1 << 24`（`default_trampoline.cu:21`），host 侧是 `1 << 20`（`runtime/include/bpf_attach_ctx.hpp:43`），提交的 `trampoline_ptx.h` 里的偏移 2097184 对应的也是 `1 << 20`（`attach/nv_attach_impl/trampoline_ptx.h:121`）。
`trampoline/README.md` 说 sm_60，Makefile 用 sm_61。
- 影响：按 Makefile 重新生成 trampoline 会让 GPU 和 CPU 对 `CommSharedMem` 的布局不一致。

**B.8.4 per-thread map 的线程数越界只打印、不拦截**（读码确认；整数溢出为推断）
- 现象：`array_map_offset` 在线程号 ≥ `max_thread_count` 时 `printf` 一行警告，仍返回越界地址；默认容量只有 1024（`runtime/include/bpftime_shm.hpp:45`）。
- 位置：`attach/nv_attach_impl/trampoline/default_trampoline.cu:189-199`。`getGlobalThreadId` 用 `int` 算 x/y/z（`:180-188`），大 grid 下溢出（推断）。
- 影响：超过 1024 线程的 kernel 用 1502/1512 类 map 会越界写显存或 host 内存。GPU ringbuf（0025）在同样情形下正确返回 1。

**B.8.5 GPU ringbuf 的问题**（读码确认；后两点为推断）
- GPU 侧写入不检查 `data_size <= value_size`，会写进下一个槽（`default_trampoline.cu:398-401`）。
- `drain_data` 遇到任一线程 `dirty` 就整体返回 0（`runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp:64-66`），而且先 `head++` 再回调读数据（`:70-76`），回调期间生产者可能覆盖这个槽（推断）。
- `elem_*` 把 `errno` 设成负数 `-ENOTSUP`（`:90-112`）。
- GPU 侧 `dirty = 0` 之前没有 `membar.sys`，CPU 可能先看到 tail/dirty 再看到数据（推断）。

**B.8.6 GPU_HASH_MAP（1501）的实现问题**（读码确认；锁的可靠性为推断）
- 位置：`runtime/src/bpf_map/gpu/nv_gpu_shared_hash_map.cpp`。
- 桶数是 `next_prime(max_entries)`（`:19`），值却存在容量为 `max_entries` 的 array 里，用桶下标当 array 的 key（`:58,86,99`）。落到下标 ≥ `max_entries` 的键插入失败（E2BIG）。
- 插新键时把用户的 `flags` 透传给 array，array 对 `BPF_NOEXIST` 恒返回 EEXIST（`nv_gpu_shared_array_map.cpp:133-136`），所以 `BPF_NOEXIST` 插新键必然失败。
- 删除直接 `set_empty`（`:123`），破坏了线性探测链，后面的键可能查不到。
- GPU 侧对 1501 没有快速路径，全部走 CPU 调用；lookup 结果放在 shm 里唯一的暂存区，并发互相覆盖。
- 头文件注释说 array 由 handler 加锁，实际 `should_lock = false`（`nv_gpu_shared_hash_map.hpp:26-27,68-70`），靠一把 `PTHREAD_PROCESS_PRIVATE` 的 spinlock，放在 shm 里跨进程用不可靠（推断）。

**B.8.7 GPU 上能用的 helper 很有限，506 不可用**（读码确认）
- 现象：trampoline 定义了 `_bpf_helper_ext_0506`（membar），但编 PTX 时没注册 506（`attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp:205-223`）。
实际注册的只有 16 个：1、2、3、6、14、25、501–505、507–511。`example/gpu/README.md:275` 把 506 列为可用，与此不一致。
- 影响：用了这 16 个之外的 helper（如 506、`bpf_ktime_get_ns`、tail call）时，llvmbpf 报 `Ext func not found`，`generate_ptx` 里的 `ExitOnError` 随即让**目标进程直接 `exit(1)`**，不是 attach 失败（`vm/llvm-jit/src/compiler_utils.cpp:366-369`、`vm/llvm-jit/src/llvm_jit_context.cpp:907,936-939`；核查时实跑调用 506，退出码 1）。
  syscall-server 的 verifier 不检查 kprobe 类程序，拦不住。

**B.8.8 一旦有 CUDA link，fatbin 里所有 kernel 都改由 bpftime 重编的副本运行**（读码确认）
- 现象：没被修改的 PTX 也会编译加载（`attach/nv_attach_impl/nv_attach_fatbin_record.cpp:119-188,204-279`），`__cudaRegisterFunction` 只要在新模块里找到同名 kernel 就重定向。
- 影响：失去 nvcc 预编 SASS 的优化，行为差异可能出现。

**B.8.9 PTX 版本与目标架构的处理**（读码确认；兼容性为推断）
三个 pass 都写死 `"sm_61"`（kprobe_entry `main.cpp:55`、kretprobe `:43`、memcapture `:141`）；合并后保留 trampoline 的 `.version 8.1 / .target sm_61`，原 PTX 的 `.version` 被删（`core.cpp:165-190`）；
最后 `rewrite_ptx_target` 只改 target，仅对 sm ≥ 100 改 version（`attach/nv_attach_impl/nv_attach_utils.cpp:24-60`）。应用 PTX 用了 ISA 8.1 之后的特性时，合并后可能编不过（推断）。

**B.8.10 register guard 默认关闭，也没有途径打开**（读码确认；作用为推断）
- 现象：kprobe_entry 只有 `save_strategy == "full"` 才加 guard（`attach/nv_attach_impl/pass/ptxpass_kprobe_entry/main.cpp:119`），默认配置设的是 `"minimal"`（`:36`），配置文件不会被读取；kretprobe、memcapture 固定不加。
guard 保存的是 eBPF 函数自己声明的寄存器（`nv_attach_impl_register_guard_patcher.cpp:198-318`），而 PTX 虚拟寄存器本来就按函数隔离，作用存疑。README 却把它当主要机制描述。

**B.8.11 memcapture 插在 ld/st 之后，且作用于整份 PTX**（读码确认；重名为推断）
- 现象：先输出原指令再插 `call`（`attach/nv_attach_impl/pass/ptxpass_kprobe_memcapture/main.cpp:55-148`），对所有函数生效，不只目标 kernel。
同一 PTX 被处理两次会生成重名的 `__memcapture__1`；两个 kprobe 程序挂同一个 kernel 会生成两个同名 `__probe_func__<k>`，编译失败（推断）。

**B.8.12 GPU 程序没有 ctx**（读码确认，不直观）
编 PTX 时 `with_arguments = false`，生成的函数没有参数，r1/r2 未初始化（`vm/llvm-jit/src/compiler.cpp:356-361`）。GPU 探针**看不到 kernel 的实参**。每线程的 eBPF 栈约 16 KB，放在 local memory。

**B.8.13 越界访问和空指针**（读码确认；R3、R6 独立发现第一条）
- `create_map_basic_info` 先取 `local_basic_info[i]` 的引用（`runtime/src/attach/bpf_attach_ctx_cuda.cpp:340`），到 `:350` 才检查 `i >= size`。handler 表默认 6144 项，map fd ≥ 1024 时 vector 越界。
- GPU 侧 `map_info[fd]` 同样没有边界检查（`default_trampoline.cu:207`）。
- ptx_compiler 动态库加载失败后仍然解引用（`attach/nv_attach_impl/nv_attach_impl.cpp:558`）。

**B.8.14 shm 里存放进程私有的句柄和指针**（读码确认；后果为推断）
- `nv_gpu_shared_array_map_impl` 的 `agent_cuda_context`、`agent_stream`、`owner_cuda_context`，`array_map_kernel_gpu_impl` 的 `map_fd`、`mmap_ptr` 都是 shm 里对象的字段，多个 agent 或 server/agent 两侧会互相覆盖。
- `nv_gpu_per_thread_array_map_impl::elem_update` 在 agent 里用的是 server 的设备指针（`runtime/src/bpf_map/gpu/nv_gpu_per_thread_array_map.cpp:135`）。
- `map_get_next_key` 把局部指针 `next_key = 0`，而不是写值（`nv_gpu_per_thread_array_map.cpp:165`；HOST 版 `nv_gpu_array_host_map.cpp:135`、`nv_gpu_shared_array_host_map.cpp:90` 同）。
- `array_map_kernel_gpu.cpp:239-241` 的 static `registered_host_memory` 只记最后一个 map，`atexit` 每次都注册一次。

**B.8.15 `BPFTIME_MAP_GPU_THREAD_COUNT` 与用户态看到的 value 大小不一致**（读码确认；影响为推断）
- 现象：1502 的实现按环境变量里的线程数分配并拷贝（`nv_gpu_per_thread_array_map.cpp:54,103-110`），handler 报给用户态的 value 大小却用 `attr.gpu_thread_count`（恒 1024，`runtime/src/handler/map_handler.cpp:76-78`）。
1512（HOST 版）完全不读这个环境变量（`map_handler.cpp:1098-1104`），与 README 说的 "Override for all GPU maps" 不符。
- 影响：设了环境变量后 update 时 `cuMemcpyHtoD` 可能读过用户缓冲区。

**B.8.16 其他可疑点**（推断）
- trampoline 拼进每个被修改的 PTX 文件，每个模块各有一把 `__bpftime_comm_lock`，多个模块的 kernel 并发调 CPU helper 时互不互斥，却共用一个 `CommSharedMem`。
- 只 hook 了 `cudaLaunchKernel(_ptsz)`、`cuLaunchKernel` 和 graph 节点 API；`cudaLaunchKernelExC`、`cuLaunchKernelEx`、`cudaLaunchCooperativeKernel` 不会被插桩；
  stream capture 中的 `cudaLaunchKernel` 直接走原函数（`attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp:109-111`）。
- 只镜像了 `cudaMemcpyTo/FromSymbol(Async)`，经 `cudaGetSymbolAddress` 拿到的原模块全局地址与补丁 kernel 看到的副本不一致。
- `start_late_bootstrap_async` 名字是 async，实际同步执行（注释也承认，`nv_attach_impl.cpp:1961-1963`）；LD_PRELOAD 场景下会抽 PTX 编一遍，`__cudaRegisterFatBinary` 时再处理一遍，靠 sha256 缓存避免重复编译。
- 识别 CUDA 程序靠名字前缀 `cuda__`（`runtime/include/bpftime_prog.hpp:59-62`），而内核的 prog 名只有 16 字节，`cuda__` 占了 6 个。

**B.8.17 注释、配置与死代码**（读码确认）
- `register_custom_helpers` 是空函数，头文件注释却说要注册 0502/0503（`nv_attach_impl.hpp:150-154` 对比 `nv_attach_impl.cpp:155-158`）；类注释 "Attach implementation of syscall trace" 是复制残留（`.hpp:142-144`）。
- `scan_fatbin_wrappers` 注释说 magic 是 'FbC1'，数值 `0x466243b1` 并不是这四个 ASCII（`nv_elf_introspect.cpp:251-252`），它其实是 CUDA 的 `FATBINC_MAGIC`。
- CMake：`BPFTIME_CUDA_ROOT` 被声明成 `option()`（BOOL，`cmake/StandardSettings.cmake:24`）；`BPFTIME_ENABLE_GDRCOPY` 没有 option 声明；
  生成的 `ptx_pass_config.h` 写死了**构建目录的绝对路径**（`attach/nv_attach_impl/CMakeLists.txt:71`），安装后删掉构建目录就找不到 pass 和 compiler（推断）。
- 死代码：`memory_utils.hpp`、`demo_ptx_prog.hpp`、`compile_ptx_to_elf`（`runtime/src/bpftime_prog.cpp:42-103`）、`patch_main_from_func_to_entry`、`configs/ptxpass/kprobe_entry.json`、`CommSharedMem::occupy_flag/time_sum` 等。

**B.8.18 GPU 相关文档过时**（读码确认）
- `attach/nv_attach_impl/README.md`：说"用 NVCC 重编译"（实际是 nvPTXCompiler 库）、"在 ld/st 之前注入"（实际是之后）、`BPFTIME_CUDA_ROOT` 影响重编译（只影响找 cuobjdump）、
  `BPFTIME_MAP_GPU_THREAD_COUNT` 覆盖所有 GPU map（1512 不读）、提到 `PTX_ATTACH_POINT`。
- `attach/nv_attach_impl/README-passes.md` 整篇描述的是旧形态（stdin/stdout 可执行文件、`BPFTIME_PTXPASS_DIR`、`/tmp/bpftime-recompile-nvcc/out.fatbin`），都已不存在。
- `example/gpu/README.md:224,240-260`：说 ringbuf 在 GPU 显存（实为 host shm）、实现在 `nv_gpu_array_map.cpp`（文件不存在）、行号引用 `nv_attach_impl.hpp:33-34`（实为 `:43-44`）；`:275` 把 506 列为可用；`:290-296` 的加锁顺序与实现相反。


**B.8.19 GPU 程序经 CPU 中转访问 map 时用的是 loader 视角**（读码确认；影响为推断；W5 回源码时发现）
- 现象：CUDA watcher 线程处理 GPU 发来的 map 请求时，调的是 `bpftime_map_lookup_elem`、`bpftime_map_update_elem`、`bpftime_map_delete_elem`，
这三个函数都传 `from_syscall = true`，走的是 loader（syscall）视角，不是 helper 视角。
- 位置：`runtime/src/attach/bpf_attach_ctx_cuda.cpp:140,202,217`；`runtime/src/bpftime_shm.cpp:115-119,128-133`。
- 影响：GPU 程序 lookup per-CPU map 拿到的是 loader 视角的结果（这个视角下是全部 CPU 的值，而不是当前 CPU 那一份）；对两种视角行为不同的 map（per-CPU 类、ARRAY_OF_MAPS 等），GPU 程序看到的语义和 CPU 上的 eBPF 程序不一致（推断，未评估实际影响）。

---

<a id="b-periphery"></a>

## B.9 外围工具：daemon、verifier、bpftimetool、bpftime-aot

### daemon

**B.9.1 `-u` 选项永远失败**（读码确认）
- 现象：`if (errno || uid < 0 || uid >= -1)`，任何非负 uid 都满足 `uid >= -1`，总是打印用法退出。`env.uid` 也从没被使用，内核侧没有 uid 过滤。
- 位置：`daemon/user/main.cpp:58`。README 却写着 `sudo bpftime_daemon -u 1000`（`daemon/README.md:132-133`）。

**B.9.2 不加 `-p` 时接管全系统的 uprobe**（推断，逻辑直接）
- 现象：`target_pid = 0` 时过滤只排除 daemon 自己（`daemon/kernel/bpf_kernel_config.h:28-40`）。
- 影响：这期间机器上任何进程（bpftrace、perf、其他 libbpf 工具）用 `perf_event_open` 建的 uprobe 都会被改写到 `/a`，在内核里失效。

**B.9.3 只接管 uprobe，不接管 kprobe、tracepoint 和旧式 uprobe**（读码确认）
- 现象：内核侧只比较 `attr.type == uprobe_perf_type`（`daemon/kernel/bpf_tracer.bpf.c:407`）；`kprobe_perf_type` 写了没读；用户态其他类型一律 "Unsupported perf event type"（`daemon/user/handle_bpf_event.cpp:342-345`）。
经 tracefs `uprobe_events` 建的旧式 uprobe（`PERF_TYPE_TRACEPOINT`）也不会被接管。

**B.9.4 只支持 4 种 map**（读码确认；后果为推断）
- 现象：driver 把内核 map 类型统一加 1000（`daemon/user/bpftime_driver.cpp:236`），runtime 只实现了 HASH、ARRAY、PERCPU_ARRAY、PERF_EVENT_ARRAY 的 kernel-user 版本（`runtime/include/bpftime_shm.hpp:113-120`），其中 PERCPU_ARRAY 版本还不可用（见 [B.7.20](#b-maps)）。
- 影响：用 RINGBUF（27 → 1027）的现代 libbpf 工具，`map_init` 不认识这个类型，返回 -1 被忽略（见 [B.1.4](#b-shm)），留下一个没有实现的 map，之后访问会出错（推断）。

**B.9.5 prog id 和 map id 共用 shm 槽位，可能撞号**（推断）
- 现象：prog 用内核 prog id 当槽位（`bpftime_driver.cpp:197`），map 用内核 map id 当槽位（`:254`），而内核里两者是独立编号的；uprobe 又用最小空槽抢小号（`:320`）。
- 影响：撞号时会误判"已存在"或创建失败。内核 id 超过 `max_fd_count`（默认 6144）时也会失败。

**B.9.6 内核侧的竞争和资源**（推断）
- LINK_CREATE：内核侧要求本线程之前加载过程序才发事件（查"本线程最近一次加载的 prog id"，`daemon/kernel/bpf_tracer.bpf.c:328-336`）；用户态真正 attach 时用的是 attr 里的 `prog_fd`（`daemon/user/handle_bpf_event.cpp:251,263-265`）。
  所以做 link 的线程之前没加载过程序时，打出 "Unable to get prog id" 并不发事件，这个 link 会丢；但不会挂错程序。
- `insns_data`、`old_uprobe_path`、`new_attr_buffer` 都是全局 `.bss` 变量（`daemon/kernel/bpf_obj_id_fd_map.h:40`、`bpf_tracer.bpf.c:392,395`），两个 CPU 同时加载程序或建 perf event 会互相覆盖。
- 还原 uprobe 路径时固定写回 255 字节（`bpf_tracer.bpf.c:542-543`），被改写工具的路径缓冲区短于 255 字节时，会把紧挨着的内存清零。
- `bpf_prog_insns_map` 每项约 32 KB、10240 项、HASH 预分配，单这一张 map 约占 320 MB 内核内存。
- daemon 以 `SHM_REMOVE_AND_CREATE` 建 shm，会清掉已有会话；shm 里的 prog/map/link 只增不减。daemon 退出时只解除映射，不删段（`daemon/user/bpftime_driver.cpp:407-410`），`/dev/shm` 里的段要手动 `bpftimetool remove`。

**B.9.7 小问题**（读码确认）
- `process_exec_maps` 遇到一个短命进程就 `return 0`，整轮遍历中止（`daemon/user/bpf_tracer.cpp:143-146`）。默认不加载 exec 程序，平时不触发。
- `-w` 的帮助文字说 "function addresses"（`daemon/user/main.cpp:29-30`），内核比较的是 `probe_offset`，即**文件偏移**（`bpf_tracer.bpf.c:410-411`）。
- `/a` 占位文件只有 x86-64 版（`daemon/assets/placeholder`），aarch64 上也写这个文件；daemon 在根目录留下 `/a`，从不删除。
- 配置项 `enable_replace_prog`、`submit_bpf_events`、`enable_auto_attach`、`duration_ms`、`is_driving_bpftime` 没有命令行入口；`bpftime_cli_path`、`bpftime_tool_path` 没人读（`daemon/user/daemon_config.hpp:29-48`）。
- 只声明没定义：`bpftime_tracepoint_create_server`（`daemon/user/bpftime_driver.hpp:75-76`）。没人调用：`bpftime_btf_load_server`、`get_kernel_bpf_prog_insns`。
  `handle_bpf_event` 的 `BPF_PROG_LOAD` 分支是死代码，内核从不为 PROG_LOAD 发事件，而且它索引字符串表时不查边界（`handle_bpf_event.cpp:274-280`）。

**B.9.8 `daemon/README.md` 与代码不符**（读码确认）
`BPFTIME_DAEMON_CONFIG`（`:119`）不存在；"Adjust duration_ms in config"（`:185`）没有入口；"Can filter by PID/UID"（`:158`）中 UID 不可用；
"/dev/shm/bpftime_maps_shm"（`:140`）只是默认名，可以被 `BPFTIME_GLOBAL_SHM_NAME` 改。

### verifier

**B.9.9 verifier 的覆盖面**：uprobe 程序不进 verifier 见 [B.2.12](#b-entry)；ufunc helper 组加错见 [B.2.11](#b-entry)。
另外 verifier 的配置都是 thread_local，syscall-server 只在第一个触发启动的线程里设 helper 列表，map 描述符写在建 map 的线程，验证在加载程序的线程。
三者不在同一线程时会出现假的验证失败（WARNING 档下只是告警）。推断。

**B.9.10 verifier 的测试与文档**（读码确认）
- `bpftime-verifier/test/simple.cpp:30-31` 把 `sizeof(bad_ebpf_program)`（16 字节）当指令条数传入，实际读了 16 条，越界。测试本来期望失败，所以能过。
- `bpftime-verifier/README.md:62` 是 `std::optional<std::string> result = auto ret = ...`，语法错误；`:13` 的 `${BPFTIME_VERIFIER_INCLUDES}s` 名字不对（实际是 `BPFTIME_VERIFIER_INCLUDE`）。

### bpftimetool

**B.9.11 JSON 导出导入**：prog type 是垃圾值见 [B.1.5](#b-shm)；导入导出不对称见 [B.1.6](#b-shm)。导入失败时已导入的部分不回滚（已实跑：导入含 memfd 的 JSON 返回 255，只剩 fd 3）。`import`、`load` 以 `SHM_CREATE_OR_OPEN` 建的段，进程退出后也不会自动删除。

**B.9.12 `load` 的用法说明**（读码确认；原附录 A.6 第 8 条）
- 现象：用法说明写 "Load a JSON file"，代码却把 `argv[3]` 当 JSON **文本**直接解析。导入文件用的是 `import` 子命令。
- 位置：`tools/bpftimetool/main.cpp:180-202`；解析处 `runtime/src/bpftime_shm_json.cpp:223-228`。

**B.9.13 `run` 的参数解析与 INTERPRET 档**（已实跑）
- 只在 `argc > 5` 时解析可选参数（`tools/bpftimetool/main.cpp:276-297`）：`run 3 in.bin repeat` 少一个值时多出来的参数被**悄悄忽略**，按默认值执行；`repeat` 必须写在 `type` 前面。
- `type INTERPRET` 在默认 llvm VM 下其实是 JIT：`ebpf_exec` 发现没编译过就现场编译（`vm/llvm-jit/src/vm.cpp:145-160`）。实跑 INTERPRET 跑 1 次 3.8 ms（基本是编译时间），1000 次平均 3.9 µs。
  "INTERPRET 档"的数字不能当解释器性能。
- `run` 不读 `BPFTIME_VM_NAME`：它以 `SHM_OPEN_ONLY` 打开 shm，用的是建段时存进去的配置（`tools/bpftimetool/main.cpp:87`）。只在 `run` 上设 `ubpf` 时测的其实还是 llvm；在建段时就设 `ubpf`，build-jit 的 bpftimetool 会直接 terminate（"Unknown VM type requested: ubpf"，核查时实跑）。

**B.9.14 `tools/bpftimetool/README.md` 的 JSON 格式写错了**（读码确认）
README 写 `{"handlers":[{"fd":3,...}]}`（`:201-241`），实际是以 fd 为 key 的对象 `{"3":{...}}`；README 也没介绍 `load` 子命令。

### bpftime-aot

**B.9.15 `compile` 只编第一个程序**（读码确认）
`return 0` 写在循环里（`tools/aot/main.cpp:138`）。README 说的是编 shm 里的程序（复数）。
`compile` 和 `load` 直接写 shm 里的 `aot_insns`，不加锁（`:135-137,174-177`），同时有 agent 在读时有竞争（推断）。

**B.9.16 `build` 不做 map 重定位；自带的 AOT 示例产物是旧 ABI**（读码确认）
- `build` 只调了 `bpf_object__open`，没有 load。libbpf 只在 load 阶段才把 LDDW 改成 `BPF_PSEUDO_MAP_FD/IDX`（`third_party/libbpf/src/libbpf.c:6370-6373,8921`），只 open 时 LDDW 仍是 `src_reg=0, imm=0`，AOT 把它编成常量 0，map 引用无效。
  README 也说"从 shm compile 才会做重定位"。`-e` 不会生成 `.ll` 文件。
- 当前 AOT 的约定：helper 编成对 `_bpf_helper_ext_NNNN` 的调用；按 fd 引用 map（LDDW src=1）在编译期就换成常量 fd（`vm/llvm-jit/src/compiler.cpp:1098-1127`）；只有 map value（src=2）才调外部的 `__lddw_helper_map_val`（`:1172-1190`）。
  AOT 编译和加载两侧都只定义 `__lddw_helper_map_val`，`__lddw_helper_map_by_fd` 已被注释掉（`vm/llvm-jit/src/llvm_jit_context.cpp:644-654,836-842`）。
  仓库自带的 `tools/aot/example/do_count.o` 还引用 `__lddw_helper_map_by_fd`，是旧 ABI 的产物，用当前 runtime 加载时这个符号会解析不到（推断，未实跑）。

**B.9.17 aot 示例跑不起来**（已实跑）
`tools/aot/example/main.c` 缺 `#include <stdarg.h>` 和 `_GNU_SOURCE`，clang-19 报 3 个 error；`:53` 的 `uint64_t* value_ptr = (uint64_t*)value_ptr;` 用自己初始化自己，是未定义行为，结果随编译选项变：按示例 Makefile 的 `-O2 -flto` 编会打印垃圾值，`-O0` 或用 gcc 编会段错误（rc=139）。
改成 `(uint64_t*)value` 后能正常输出 `counter_map: 1`（改的是 scratchpad 里的副本）。`va_start` 的用法同 [B.6.7](#b-helpers)。

---

<a id="b-build-test"></a>

## B.10 构建、测试、示例与文档

模块自己的 README 问题放在各模块分节里（B.2.17、B.3.13、B.4.10、B.5.18、B.8.18、B.9.8、B.9.10、B.9.14），这里放构建系统、测试、示例和顶层文档。

### 构建系统

**B.10.1 Frida 和 trap 的开关按架构 FORCE 覆盖，命令行改不了**（读码确认；R5、R7、R8 独立发现）
- 现象：每次 configure 都按 `CMAKE_SYSTEM_PROCESSOR` 用 `FORCE` 写缓存；`vm/compat/CMakeLists.txt:6` 还会再 include 一次 StandardSettings。
- 位置：`cmake/StandardSettings.cmake:76-82`。
- 影响：x86 上传 `-DBPFTIME_ENABLE_FRIDA=OFF` 或 `-DBPFTIME_ENABLE_TRAP_UPROBE=ON` 都不起作用。
  （`benchmark/trap-uprobe/README.md` 里的 `-DBPFTIME_ENABLE_TRAP_UPROBE=ON` 用在 riscv64 交叉构建里，toolchain 文件已经让 FORCE 命中 riscv 分支，所以只是多余，不是无效。）

**B.10.2 死选项和不起作用的选项**（读码确认）
- `BPFTIME_ENABLE_CCACHE` 不起作用，找到 ccache 就用（`cmake/StandardSettings.cmake:63-68`）。
- `BPFTIME_BUILD_KERNEL_BPF`、`BPFTIME_BUILD_EXECUTABLE`、`BPFTIME_USE_CATCH2`、`BPFTIME_VERBOSE_OUTPUT` 没人用；`cmake/rocm.cmake` 没被 include。
- `BPFTIME_ENABLE_FS_HELPER`、`BPFTIME_USE_VAR_HASH_MAP` 这两个宏在代码里有，但没有 CMake 入口。
- 根 `CMakeLists.txt:92` 的 "Enabling ubsan for Debug builds" 只是一句日志，没有对应的编译参数。
- 根 `CMakeLists.txt:231` 用了不存在的变量 `${CMAKE_CURRENT_BUILD_DIR}`，展开成相对路径；因为那条命令的工作目录是构建目录，碰巧能用（推断）。
- 安装位置写死为 `~/.bpftime`，不受 prefix 控制。

**B.10.3 Makefile 的问题**（读码确认；R7、R8 独立发现）
- `unit-test-runtime` 目标里 `export BPFTIME_VM_NAME=llvm` 单独占一行（`Makefile:48`），每行 recipe 是独立的 shell，对下一行无效。
- 同一目标的 `make -C runtime/test/bpf` 和 `cd build/runtime/test && make && ctest`（`:47,50`）针对的是**已废弃且为空**的 `runtime/test`（`runtime/test/CMakeLists.txt:11-24` 全被注释）。
  单测真正用的 `.bpf.o` 由 CMake 从 `runtime/unit-test/assets/` 现编（`runtime/unit-test/CMakeLists.txt:143-158`）。用 Ninja 生成器时这一行会失败（核查时实跑：`make: *** No targets specified and no makefile found.`）。
- `benchmark` 目标和 `benchmark/mpk/build.sh` 写死 `-DLLVM_DIR=/usr/lib/llvm-15/cmake`（`Makefile:83,93`）；`release-with-llvm-jit` 与 `release` 效果一样；`build-wo-libbpf` 的注释抄成了 "iouring"（`:62`）。

**B.10.4 `example/Makefile` 会进入 `gpu/`**（进入 `gpu/` 已确认；没有 CUDA 时失败为推断）
`example/Makefile:2`、`example/tracing/Makefile:2` 用 `find . -maxdepth 2 -name Makefile` 枚举子目录，`benchmark/Makefile:6` 用 `ls` 列出所有直接子目录。
算出来的子目录都含 `gpu/`，`make -C example` 会进去，没有 CUDA 时失败（未实跑）。

**B.10.5 `installation.md` 的错误**（读码确认）
工具名写成 "bpftime-vm"（`:64`，实际叫 `bpftime-aot`）；"(With ubpf jit)" 两节其实也默认打开了 LLVM JIT（`:128-140`）；`ENABLE_PROBE_READ_CHECK` 的说明写成了 "bpf_probe_write"（`:120`）；
说 `BPFTIME_ENABLE_CCACHE` "Default to OFF"，实际会自动启用。

### 单元测试

**B.10.6 `Test attach replace with ebpf` 在默认顺序下必然段错误**（已实跑，连续 3 次复现）
- 现象：用例先注册 ufunc（内部调 `gum_find_function` 查符号），后构造 `frida_attach_impl`，而 `gum_init_embedded()` 在后者的构造函数里。Frida 没初始化就被调用。
- 位置：`runtime/unit-test/attach_with_ebpf/test_attach_replace.cpp:82-84,95`；`attach/frida_uprobe_attach_impl/src/frida_attach_utils.cpp:137-145`；`frida_uprobe_attach_impl.cpp:40`。
- 影响：它在 Catch2 默认顺序里排第 2，单独跑或整套跑都崩（rc=139）；先跑过任何会构造 `frida_attach_impl` 的用例时能过。
绕开的命令：`bpftime_runtime_tests "~Test attach replace with ebpf" "~Test tail calling from userspace to kernel"`（后者需要 root；已实跑全过）。CI 的 Debug 构建上是否也崩未验证。

**B.10.7 单测会动默认共享内存段，还会留下垃圾**（已实跑）
- `bpftime::shm_remove` 的问题见 [B.1.1](#b-shm)。
- tailcall 两个测试都以 `bpftime_initialize_global_shm(SHM_REMOVE_AND_CREATE)` 按全局名删了再建，而 `bpftime_destroy_global_shm` 只析构不 unlink。两者结局不同：
  - user→user（`runtime/unit-test/tailcall/test_user_to_user_tailcall.cpp:10,16-18`）用的是单测自己那份正确的 `shm_remove`，删的是一个从没创建过的名字，所以重建出来的全局段（50 MB）会留下。核查时实跑确认。
  - user→kernel（`test_user_to_kernel_tailcall.cpp:17,69-71`，需要 root）用的是 `bpftime::shm_remove`，结束时会删掉全局段，就是 B.1.1 的效果。
  所以不设 `BPFTIME_GLOBAL_SHM_NAME` 跑单测，默认段 `bpftime_maps_shm` 会被重建、留下或删掉。
- `test_bpftime_shm_json.cpp:61` 的 50 MB 段从不删除，还写 `/tmp/bpftime_test_shm_json.json`；`test_bpftime_hash_map.cpp:27-28` 的段也不删。
固定姿势：`export BPFTIME_GLOBAL_SHM_NAME=notes_$RANDOM`，跑完对比 `ls /dev/shm` 清理。

**B.10.8 测试没测到它声称测的东西**（读码确认）
- `Test shm progs attach`：父进程调用被替换的函数后只打日志、不检查返回值（`test_shm_progs_attach.cpp:242-251`）；子进程里用 `assert` 检查（`:156,186`），在 `NDEBUG` 构建下被编译掉。通过只说明装配流程不崩。
- queue/stack 的并发测试注释说"依赖 queue_map_impl 内部的互斥"，但实现没有内部锁（`queue.hpp:58-59` 写着 "Map handler should lock externally"），测试自己在外面包了 `pthread_spin_lock`（`runtime/unit-test/maps/test_queue_map.cpp:278-360`，stack 同构）。
  map 自身的并发安全没被测到（R2、R8 独立发现，X#8）。
- `Queue Map Constructor Validation` 注释说测 `max_entries = 0`，实际 value_size 也是 0（`test_queue_map.cpp:53-58`）。
- `test_hashmap_sizes (kernel)` 没有断言（`kernel_unit_tests.cpp:148-163`，实跑输出 `assertions: - none -`）。
- `Test cuda compile` 没有断言，也不检查 `bpftime_prog_load(true)` 的返回值；它的程序名 `test_cuda_prog__cuda` 不以 `cuda__` 开头，不被当成 CUDA 程序，实际走的是 CPU JIT（`runtime/unit-test/cuda/test_cuda_compile.cpp:73-82`；R6、R8 独立发现）。

**B.10.9 永不触发的 `#error Only supports x86_64`**（读码确认；R5、R7 发现，原附录 A.6 第 9 条）
条件是 `!defined(__x86_64__) && defined(_M_X64)`，`_M_X64` 是 MSVC 的宏，GCC/Clang 下不会定义，这条守卫不起作用。
- 位置：`daemon/test/test_daemon.cpp:8-10`、`daemon/test/test_daemon_driver.cpp:14-16`、`attach/frida_uprobe_attach_impl/test/test_uprobe_uretprobe.cpp:10-12` 等。

**B.10.10 测试代码里的其他小问题**（读码确认）
- 106 个用例里有 36 个没有 tag（attach、probe、get_stack、tailcall、config、json 等），只能按名字过滤。
- `runtime_options_test_helper.c` 被 `add_dependencies` 拉进构建（`runtime/unit-test/CMakeLists.txt:90-94`），没有用例用它。
- `daemon/test/CMakeLists.txt:49-60` 遍历一个空列表，循环体还引用了 `bpftime_runtime_tests`；`daemon/test/common_def.hpp` 没被用到；`test_daemon.cpp` 是空用例。

**B.10.11 CI 覆盖的空白**（读码确认）
- 没有任何 job 在 `ENABLE_EBPF_VERIFIER=ON` 下编 syscall-server（`test-verifier.yml:47-51` 只编 `bpftime_verifier_tests`），verifier 与 syscall-server 的集成代码没被 CI 编译过。
- daemon 的单测只在 `build-gcc13.yml:100-117`（`LLVM_JIT=OFF`）跑；`benchmarks.yml`（`make benchmark`）和 `docker.yml` 只编译 daemon。没有任何 job 端到端跑 daemon 的内核劫持链。

### 示例与 benchmark 的文档

**B.10.12 示例 README 与代码不一致**（读码确认，R8 逐条核对）

| 文档位置 | 文档说法 | 实际 |
|---|---|---|
| `example/README.md:42` | 指向 `documents/avaliable-features.md` | 仓库里没有 `documents/` 目录 |
| `example/README.md:69-70` | opensnoop 演示 ring buffer 输出 | 用的是 PERF_EVENT_ARRAY + `bpf_perf_event_output`；ringbuf 版是 `opensnoop_ring_buf` |
| `example/README.md:162` | VM CLI 在 `vm/cli` | 实际在 `vm/llvm-jit/cli` |
| `example/malloc/README.md:5-55` | 用 cgroup id 作 key、u64 key | `malloc.bpf.c:9,32-36` 用 pid、u32 key，并且 `bpf_printk` |
| `example/minimal/README.md:43-89` | 讲的是 cgroup 计数程序 | `uprobe.bpf.c:6-11` 只是一个 uprobe + `bpf_printk` |
| `example/bpf_features/get_stack_id_example/README.md` | 正文复制自 malloc 的 README，运行命令指向 `example/malloc/malloc` | 实际程序用 STACK_TRACE + `bpf_get_stackid` + ringbuf，挂 malloc/free |
| `example/bpf_features/*/README.md` 与 `*.sh`（bloom、queue、stack、lpm） | `LD_PRELOAD=../../build/runtime/…` | 目录深了一层，应为 `../../../build`；Makefile 已改，脚本和 README 没跟上 |
| `bloom_filter_demo/README.md:27-33,156-158` 对比 `test_correct_bloom_filter.sh:8-12` | README 说必须 `BPFTIME_VM_NAME=ubpf` | 脚本说默认 llvm 推荐、ubpf 可能有问题，两者矛盾 |
| `lpm_trie_demo/README.md:17` | 脚本名 `run_lmp_trie_demo.sh` | 实际 `run_lpm_trie_demo.sh` |
| `queue_demo/run_*_demo.sh:189,192` | 路径 `example/queue_demo/` | 实际 `example/bpf_features/queue_demo/` |
| `example/tracing/statsnoop/README.md:3` | 链接 bcc 的 `bashreadline.bpf.c` | 应为 statsnoop |
| `example/tracing/opensnoop_ring_buf/README.md:10,24` | 路径 `example/opensnoop/…`、日志 "helper groups ffi" | 路径已移到 `example/tracing/…`，日志文本是旧版 |
| `example/tracing/goroutine/README.md:14,21` | `../../build/…` | 应为 `../../../build` |
| `example/tracing/sslsniff/README.md:6-7` | `libbpftime-syscal` / `l-server.so` 被断行；路径 `example/sslsniff/sslsniff` | 命令不可复制；路径应为 `example/tracing/sslsniff/sslsniff` |
| `example/hotpatch/redis/README.md:3,69` | 有预编译 tar.gz，用 `bpftime-cli workloads/…/poc.json` 注入 | 没有 tar.gz、没有 `workloads/`；当前 CLI 是 `bpftime`，没有 JSON 注入；`redis-patch.c:55-64` 的 attach 代码整段注释掉了 |
| `example/hotpatch/vim/README.md:6,46,57` | `workloads/…/poc5-vim`、`make`、`poc5.json` | 目录里只有 `poc.bpf.c` 和 README |
| `example/libbpftime_example/README.md` | 需要 `~/.bpftime/libbpftime.a` | 需要先用 `-DBPFTIME_BUILD_STATIC_LIB=ON` 构建并安装 |
| `example/bpf_features/tailcall_minimal/README.md` | 没提 root | loader 用真实 `syscall(__NR_bpf)` 把程序载入内核（`tailcall_minimal.c:113-178`），需要 root 或 CAP_BPF |
| `benchmark/uprobe/README.md:24,37,55` | baseline 函数 `__benchmark_test_function3`；`make -C benchmark/uretprobe` | `benchmark/test.c:7-29` 是 17 个 `__bench_*`；没有 `benchmark/uretprobe/` |
| `benchmark/uprobe/benchmark.py:14-20`、`benchmark/mpk/benchmark.py:17-22` | README 说在仓库根跑 | 脚本判断当前目录名是不是 `bpftime`，不是就拼 `cwd/bpftime/benchmark`；仓库目录不叫 `bpftime` 时路径错（推断） |
| `benchmark/ssl-nginx/README.md` | "one click" 用 `draw_figture.py`；`make -C example/sslsniff` | 跑测的是 `benchmark.py`，`draw_figture.py` 只画图；sslsniff 在 `example/tracing/sslsniff` |
| `benchmark/redis-durability-tuning/README.md` | 有第三种实现 `bpf-sync-kernel` | 目录里没有；`batch_process/redis.bpf.c:26,65` 把 redis 路径写死为某台机器上的绝对路径 |

---

## B.11 本机无法确认、需要 root / GPU / Jetson 的项

下面这些条目在本机（x86-64，非 root，无 GPU）上无法验证，正文和上面的条目里都按"推断"处理。

| 条目 | 需要什么 | 要验证的内容 |
|---|---|---|
| [B.4.5](#b-syscall-trace) | root（`vm.mmap_min_addr=0`） | 系统调用跳板的寄存器保存、red zone、rt_sigreturn 偏移这三个疑点 |
| [B.4.7](#b-syscall-trace) | Jetson（aarch64） | transformer 在 aarch64 上能否构建、加载 |
| [B.8](#b-gpu) 全节 | GPU（可在 Jetson 上） | 所有标"推断"的 GPU 运行时行为：RPC 参数覆盖、per-thread map 越界、GPU ringbuf 竞争、GPU hash 的 `BPF_NOEXIST` 等 |
| [B.9.2、B.9.4–B.9.6](#b-periphery) | root | daemon 整条链路：接管全系统 uprobe、map 类型限制的后果、id 撞号、LINK_CREATE 丢失 |
| [B.10.6](#b-build-test) | CI 同款 Debug 构建 | 单测默认顺序的段错误在 CI 上是否同样复现。线索：`test-runtime.yml:126-128,274` 不加过滤直接跑整套单测，若 Debug 下也必崩，这个 job 会一直是红的 |
| [B.7.18](#b-maps) | root | KERNEL_USER_ARRAY 的元素步长错位 |
| [B.2.13](#b-entry) | 动态 PMU 号不是 9/8 的内核；root | kernel 模式下 `/sys` mock 是否让 perf_event_open 建错类型；kernel 模式下 perf buffer 是否可用 |
| [B.1.11](#b-shm) | 打开 `BPFTIME_ENABLE_MPK` 的构建 | MPK 模式是否一启动就崩 |
| [B.10.6](#b-build-test) 中的 `Test tail calling from userspace to kernel` | root | 用户态到内核的 tail call |

---
[← 返回目录](README.md)
