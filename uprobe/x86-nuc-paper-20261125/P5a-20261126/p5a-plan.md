# P5a Dview spike 最终方案（综合 3 份设计和 2 份评审）

全程只读：没有编译，没有用 sudo，没有跑 benchmark。本轮另外核对了 Dnext 插入点、本地 linux 7.2 树的几处时序，以及 harness 的几行（见 §0）。
**取舍**：工程底座用设计 1（tid&mask 直接索引，最小 loader，miss 走精确 syscall）。嫁接设计 3 的有效位、exec 钩子、claim 和测量框架。嫁接设计 2 的停机 POISON 协议、后备链、kuid 语义和干扰对照。设计 2 的哈希窗口、iter 回填、sweep 一律放到 go 之后。

---

## 0. 事实标注（V = 已核实，A = 假设或凭记忆）

### 内核（本机 6.12.113 x86-64；源码行号来自本地 7.2 树 /home/y/src/linux）

| # | 事实 | 状态 | 来源 |
|---|---|---|---|
| K1 | `/sys/kernel/btf/vmlinux` 0444 可读，普通用户能 dump | V | 本轮 `ls -l` 确认；设计 2、3 都 dump 过 |
| K2 | `unprivileged_bpf_disabled=2` | V | 本轮读 /proc |
| K3 | 6.12 BTF 原型：`sched_process_exit(task)` 只有 1 个实参（7.2 多一个 group_dead）；`task_newtask(task, clone_flags)`；`task_rename(task, const char *comm)`；`sched_process_fork(parent, child)`；`sched_process_exec(p, pid_t old_pid, bprm)`；`commit_creds(struct cred *new)` 返回 int；`__set_task_comm` 是 static FUNC | V | 评审 2 读 BTF dump `scratchpad/vmlinux.btf.raw`；设计 1、2、3 一致 |
| K4 | copy_process 中的顺序：`p->pid` 赋值（:2364）→ `attach_pid`（:2533）→ `trace_task_newtask`（:2554）→ `trace_sched_process_fork`（:2758）→ `wake_up_new_task`（:2779）。创建类钩子都在子任务运行**之前**触发 | V（7.2）；6.12 相同为 A | 本轮 grep fork.c。设计 2 的 R4 说"fork tp 在唤醒之后"，**是错的** |
| K5 | `__set_task_comm` 里 `trace_task_rename(tsk, buf)` 在复制名字**之前**触发，所以必须用参数里的新名字 | V（7.2 fs/exec.c:1098）；6.12 是 task_lock + strscpy_pad 且 tp 在复制之前，为 A | 本轮核对 |
| K6 | exec 顺序：`de_thread`（:1134，内部 `exchange_tids` :1006、`release_task(leader)` :1032）→ `__set_task_comm`（:1253/1257）→ `commit_creds(bprm->cred)`（:1274）→ `exec_binprm` 里 `old_pid=current->pid`（:1715）→ load_binary → `trace_sched_process_exec`（:1748） | V（7.2）；6.12 相同为 A | 本轮 grep fs/exec.c |
| K7 | 非 leader 线程 exec 时，旧 leader 在 do_exit 中以 tgid 触发 exit tp。exec 线程的旧 tid T 换给旧 leader 后，旧 leader 被 release，**T 不再经过 exit tp** | V（由 K6 和 exit.c:980 推出） | 评审 1、2 |
| K8 | `bpf_map_mmap` 在 `!IS_ERR_OR_NULL(map->record)` 时拒绝映射，所以 value 里不能放 bpf_spin_lock，要自己实现 seqlock；只读映射会清掉 VM_MAYWRITE | V（7.2 syscall.c:1091、:1132）；6.12 为 A | 评审 1、2，本轮看到 :1091 |
| K9 | `unprivileged_bpf_disabled` 只在 map_create 和 prog_load 里检查（:1470/:3030），所以 agent 能否拿到 fd 只取决于 bpffs 上 pin 文件的权限 | V（7.2）；6.12 为 A | 设计 3、评审 1/2 |
| K10 | setresuid 在凭据不变时直接返回、不调 commit_creds，所以钩子微基准必须在 0 和 4242 之间交替 | V（7.2 kernel/sys.c:733-737） | 本轮核对 |
| K11 | `rseq_fork` 在带 CLONE_VM 时 reset 子任务的 rseq | V（7.2 include/linux/rseq.h:155-161） | 设计 3、评审 1/2 |
| K12 | BPF LSM 已启用；lockdown 是 none；PREEMPT_DYNAMIC 默认 voluntary | V | 设计 2、3 读 /sys/kernel/security、cmdline |
| K13 | `commit_creds` 在 available_filter_functions 里，fexit 能挂上 | **A**（需要 root；BTF 有它只是弱证据） | — |
| K14 | verifier 接受：mmapable array value 上的 32 位 cmpxchg 和 fetch_add（`-mcpu=v3`）、`bpf_probe_read_kernel_str` 写进 map value、64 次有界循环 | **A** | — |
| K15 | 只读 fd 加 PROT_READ + MAP_SHARED 的 mmap 能成功 | A（7.2 源码支持） | — |
| K16 | 容器里经 `-v /sys:/sys`（默认 rbind）能看到宿主 /sys/fs/bpf | **A**。`--privileged --pid=host -v /sys:/sys` 是 V（harness/session.sh:94，本轮核对） | — |
| K17 | 宿主 /sys/fs/bpf 是 bpffs，mode 700，属主 root | V | 设计 1 |
| K18 | mmapable array 的内存在创建时整块 vmalloc（2^18 槽 = 16 MiB 常驻） | A | — |
| K19 | copy_process 在 task_newtask 之后若失败，task 不走 do_exit。按 §2 的"创建钩子无条件覆盖"不变式，这种情况无害 | A | — |

### Tern 和 harness（本轮核对，均为 V）

**bpf_helper.cpp**
- 凭据缓存在 :711-758。`cached_credentials` 是**进程级**全局 atomic，`query_credentials()`（:715）每次调 getuid/getgid。
  - W7r 的根因因此有两层：raw syscall 绕过 interposer；缓存是进程级的，而 raw setresuid 只改调用线程。
- comm 相关：`careful_copy_comm` 在 :878；`careful_get_current_comm` 在 :890，它的 miss 分支用 `prctl(PR_GET_NAME)`；`bpftime_get_current_comm` 在 :943，careful 分支在 :947-948。

**其他 Tern 文件**
- agent.cpp:915：`static const bool careful_bound = bpftime_agent_setup_careful();`
- producer_identity.hpp：`identity_cache` 在 :53-57；`current_producer_identity(int&, int64_t&)` 在 :68，inline。
- producer_identity.cpp:24：careful 用加载时 lambda 读环境变量。
- `/usr/include/linux/bpf.h` 存在，含 BPF_F_MMAPABLE。agent 可以直接用 raw `syscall(__NR_bpf)`，不依赖 libbpf。

**E5 和 harness**
- e5target.c :61-62 是 SCEN[] 表（15 个场景）。:432 的 W12 会让 tid 绕回、发生复用。
- run_e5.py:76 的 `leg_parts` 按 '+' 切分 leg 名。
- session.sh:104 支持 `!` 开头的宿主命令行。

---

## 1. spike 检验的 claim 和 go/no-go

### 1.1 claim
**C-VIEW**：在 6.12 x86 上，由一个特权 loader 用内核 eBPF 维护每线程的 {tid, tgid, kuid, kgid, comm}，放在 BPF_F_MMAPABLE array 里，Tern 只读 mmap 后在用户态读取。叠加在 CAREFUL 之上时：
- (a) E5 里剩下的 W7r（uid/gid）和 W8c/W8d（comm）全部变成 0/500，其余格子不退化。新增的 W7t（多线程进程里只有一个线程 raw setresuid）也是 0/500。
- (b) nginx 上，相对同一二进制的 CAREFUL，用户态读路径的增量 ≤ 0.084 µs/event。

**范围**
- pid/tid **不**经过视图：视图以 careful 缓存的 tid 为 key，所以只能写 "uid/gid/comm exact given a correct thread key"。W5r（raw `syscall(SYS_clone)`）预注册为失败用例。
- 只覆盖 init pidns；需要特权 loader。
- 不回填 loader 启动前已存在的线程：缺条目只会回退。
- 不做 SCM_RIGHTS，不做 cgroup 过滤。

### 1.2 GO（预登记，以下全部满足）
- **G1 数据通路**
  - 5 个钩子都挂上：task_newtask、commit_creds、task_rename、sched_process_exec、sched_process_exit。commit_creds 按 fexit → fentry → kprobe 的顺序，任一成功即可，记录 `hook_mode`；LSM 不算。
  - E5 每个 VIEW 目标，以及 nginx 的 master 和全部 worker，日志里都有 `View: mapped`。
- **G2 正确性**
  - selftest S1–S10 全过（§3.1），lock_fail=0。
  - I9 中 `Dviewpc+CAREFUL+VIEW` 除预注册的 W5r 格子外全部 0/500。
  - nslots=16 冲突压力 leg 也是 0/500，且 miss > 0。
  - K leg 与真值一致。
- **G3 用户态成本**
  - M1：单次完整快照读（uid/gid/comm）的 warm 中位数 ≤ 10 ns。参考点：getuid+getgid 实测 164.9 ns（x86-facts.md:501）。
  - M4 nginx：`CAREFUL+VIEW − CAREFUL` 的 µs/event 配对差，90% CI 上界 ≤ +0.084。
  - 每个 worker 的视图 miss ≤ 0.1% 的读次数。
- **G4 内核成本**：只报告，但设上限。fork+exit 和 pthread_create+join 每次增量 ≤ 1 µs 且 ≤ 10%；setresuid 和 prctl 的增量照实报。

### 1.3 NO-GO（任一即判）
- commit_creds 的 fexit、fentry、kprobe 都挂不上。
- 读侧在容器里经 pin 和 map-id 两条路都映射不了，且半天修不好。
- G2 出现无法解释的错值，查 2 h 仍无结论。
- M1 > 25 ns，或 G3 的 CI 上界 > 0.084。

### 1.4 部分 GO 和日期
- **部分 GO**：G1、G2、M1 过了，nginx 没赶上。正文 discussion 写实测的读单价和钩子代价，吞吐仍写推算，12-15 再定。
- **no-go**：按 eval-plan :415-416 把工程日转给 N5b。
- **替换原判据**：eval-plan 原判据"seqlock 读 ≤ 一次 getuid"必过，没有区分力，改成上面四条。
- **日期**：:50 写 12-08，:403 写 12-11，以 :403 的 12-11 为准，并修正 :50。

---

## 2. 实现步骤

**分支和对照**
- 从 6b6b247e 拉 `ladder/dview-6b6b247e`，开独立 worktree `/home/y/src/bpftime-paper/dview-6b6b247e`。
- 构建 `dview`/`dviewpc`，cmake 选项照 dnext/dnextpc。config-x86.json 先备份，再登记 `Dview`/`Dviewpc`。
- spike 的所有 leg 用**同一个** Dviewpc 二进制，只靠环境变量切换。

### 步骤 1：共享 ABI 头 `runtime/include/bpftime_view_abi.h`（新增，C，BPF 和 C++ 共用）
```c
#define TERN_VIEW_MAGIC 0x5745495654524554ull   /* "TERNVIEW" */
#define TERN_VIEW_ABI 1
#define TV_F_IDS   1u      /* tid/tgid valid */
#define TV_F_CREDS 2u      /* uid/gid valid */
#define TV_F_COMM  4u      /* comm valid */
#define TV_F_STALE 0x80000000u
enum { TV_STATE_INIT = 0, TV_STATE_LIVE = 1, TV_STATE_DEAD = 2 };
struct tern_view_slot {          /* 64 B = 1 cache line；mmapable array 数据区页对齐 */
    __u32 seq;                   /* 偶=稳定，奇=写者持锁 */
    __u32 tid, tgid;             /* init-ns；tid==0 为空 */
    __u32 flags;
    __u32 uid, gid;              /* cred->uid.val/gid.val = kuid（init userns），与 bpf_get_current_uid_gid 同语义 */
    __u64 start_time;            /* 防御性：exit 清除时比对 */
    char  comm[16];              /* ≤15 字符 + NUL 填充（strscpy_pad 语义） */
    __u32 writer, _r;            /* 调试：1 newtask 2 creds 3 rename 4 exec 5 rekey */
    __u64 _pad;
};
struct tern_view_hdr {           /* 索引 0，64 B；loader 经 RW mmap 写 */
    __u64 magic; __u32 abi, nslots, state, hook_mode;
    __u64 pidns_ino, pidns_dev;  /* loader 的 /proc/self/ns/pid */
    __u32 loader_pid, _r; __u64 _pad[2];
};
/* 槽下标 = 1 + (tid & (nslots-1)) */
```

### 步骤 2：BPF 程序 `tools/view/view.bpf.c`（新增，约 180 行）

**maps**
- `tv_slots`：ARRAY，`BPF_F_MMAPABLE`，`max_entries = 1 + nslots`。默认 nslots = 2^18（16 MiB），loader 在 load 前用 `bpf_map__set_max_entries` 设置。
  - 为什么用 2^18 而不是设计 1 的 2^16：全机约 1100 个线程，2^16 时单个线程与其他线程撞槽的概率约 1.7%；nginx 有 5 个进程，至少一个长期回退的概率约 8%，会污染 M4。
- `tv_stats`：PERCPU_ARRAY u64，计数项为 newtask、creds、rename、exec、exit、rekey、collision、lock_fail、dead_write。

**rodata**：`const volatile __u32 nslots_mask`。

**写者协议**
- 每个 slot 一把 cmpxchg seqlock。拿锁：读到偶数后 `__sync_val_compare_and_swap(&s->seq, v, v+1)`。放锁：`__sync_fetch_and_add(&s->seq, 1)`。两步都用带 FETCH 的原子操作，x86 上是 lock 前缀指令；为 arm64 移植也保留这一点。
- 自旋上限 64 次。拿不到锁就 `__sync_fetch_and_or(&s->flags, TV_F_STALE)`，lock_fail 加 1，然后放弃。读者把 STALE 当 miss，所以丢更新只会变慢，不会读到错值。

**不变式（写进 spike 记录）**
- **I-a**：每个 task 在执行任何用户代码之前都会经过 task_newtask 钩子（K4），这个钩子**无条件**整条覆盖本 tid 的槽。
  - 所以漏掉 exit 留下的陈旧槽（K7、K19）只可能被"持有同一 tid 的活 task"读到，而这个 task 一定在陈旧条目之后经过了创建钩子，槽已被覆盖。
  - 这一条修掉了设计 1 的 exec 残留槽 bug。
- **I-b**：每个钩子只写自己负责的字段并置自己的有效位：creds 只写 uid/gid，rename 只写 comm。碰到 key 不符（撞槽，或 loader 启动前就存在的 task）时 **rekey**：写 tid/tgid/start_time，flags 只保留 F_IDS 加自己那一位。读者要求 `(flags & need) == need`，否则回退。
  - 这样从结构上消除了 rename 与 creds 之间的写回竞态和半有效槽（设计 3 的方案）。

```c
static __always_inline struct tern_view_slot *slot_of(__u32 tid)
{ __u32 k = 1 + (tid & nslots_mask); return bpf_map_lookup_elem(&tv_slots, &k); }
static __always_inline bool view_dead(void)
{ __u32 z = 0; struct tern_view_hdr *h = (void *)bpf_map_lookup_elem(&tv_slots, &z);
  return h && READ_ONCE(h->state) == TV_STATE_DEAD; }
static __always_inline int lock(struct tern_view_slot *s) { /* 见上，64 次 */ }
static __always_inline void unlock(struct tern_view_slot *s)
{ if (view_dead()) { s->flags |= TV_F_STALE; stat(ST_DEAD_WRITE); } __sync_fetch_and_add(&s->seq, 1); }
static __always_inline void rekey(struct tern_view_slot *s, struct task_struct *t)
{ if (s->tid && s->tid != t->pid) stat(ST_COLLISION);
  s->tid = t->pid; s->tgid = t->tgid; s->start_time = t->start_time; s->flags = TV_F_IDS; }
static __always_inline void fill_full(struct tern_view_slot *s, struct task_struct *t)
{ rekey(s, t);
  s->uid = BPF_CORE_READ(t, cred, uid.val); s->gid = BPF_CORE_READ(t, cred, gid.val);
  __builtin_memset(s->comm, 0, 16); bpf_probe_read_kernel_str(s->comm, 16, t->comm);
  s->flags = TV_F_IDS | TV_F_CREDS | TV_F_COMM; }

SEC("tp_btf/task_newtask")   /* 在 copy_process 中、wake_up 之前（K4）；覆盖所有 copy_process 调用者 */
int BPF_PROG(on_newtask, struct task_struct *p, unsigned long clone_flags)
{ if (p->flags & PF_KTHREAD) return 0;
  struct tern_view_slot *s = slot_of(p->pid); if (!s || !lock(s)) return 0;
  fill_full(s, p); s->writer = 1; unlock(s); return 0; }

SEC("fexit/commit_creds")    /* 只作用于 current；fexit 时 current->cred == new */
int BPF_PROG(on_creds, struct cred *new, int ret)
{ struct task_struct *t = bpf_get_current_task_btf(); if (t->flags & PF_KTHREAD) return 0;
  struct tern_view_slot *s = slot_of(t->pid); if (!s || !lock(s)) return 0;
  if (s->tid != t->pid || s->tgid != t->tgid) { rekey(s, t); s->writer = 5; }
  s->uid = new->uid.val; s->gid = new->gid.val; s->flags |= TV_F_CREDS; if (s->writer != 5) s->writer = 2;
  unlock(s); return 0; }
/* 后备（默认 autoload=false，loader 依次启用）：SEC("fentry/commit_creds") 用参数 new 写，
   commit_creds 恒返回 0（A）；SEC("kprobe/commit_creds") 用 PT_REGS_PARM1 读 new。 */

SEC("tp_btf/task_rename")    /* tp 在复制前触发（K5）：只用参数里的新名字；task 可能不是 current（/proc 写，W8d） */
int BPF_PROG(on_rename, struct task_struct *t, const char *comm)
{ struct tern_view_slot *s = slot_of(t->pid); if (!s || !lock(s)) return 0;
  if (s->tid != t->pid || s->tgid != t->tgid) rekey(s, t);
  __builtin_memset(s->comm, 0, 16); bpf_probe_read_kernel_str(s->comm, 16, comm);
  s->flags |= TV_F_COMM; s->writer = 3; unlock(s); return 0; }

SEC("tp_btf/sched_process_exec")   /* exec 中的改名和 commit_creds 都已完成（K6）*/
int BPF_PROG(on_exec, struct task_struct *p, pid_t old_pid, struct linux_binprm *b)
{ if (old_pid != p->pid) {        /* 非 leader 线程 exec：清掉旧 tid（K7），只清同 tgid 的条目 */
    struct tern_view_slot *o = slot_of(old_pid);
    if (o && o->tid == old_pid && o->tgid == p->tgid && lock(o)) {
      if (o->tid == old_pid && o->tgid == p->tgid) { o->tid = 0; o->flags = 0; } unlock(o); } }
  struct tern_view_slot *s = slot_of(p->pid); if (!s || !lock(s)) return 0;
  fill_full(s, p); s->writer = 4; unlock(s); return 0; }

SEC("tp_btf/sched_process_exit")   /* 6.12 只有 1 个参数（K3），每个线程在 do_exit 中各触发一次 */
int BPF_PROG(on_exit, struct task_struct *p)
{ struct tern_view_slot *s = slot_of(p->pid);
  if (!s || s->tid != p->pid || !lock(s)) return 0;
  if (s->tid == p->pid && s->start_time == p->start_time) { s->tid = 0; s->flags = 0; }
  unlock(s); return 0; }
```

**已知残余竞态（declared，用 S9 压力测试和计数器观察）**
1. 子线程在 attach_pid（:2533）之后、task_newtask（:2554）之前，兄弟线程经 /proc 改了它的名：rename 钩子先写入新名，newtask 钩子读 `p->comm` 时 memcpy 可能还没发生，于是写回旧名。窗口只有几条指令。
2. 两个并发改名在 7.2 上没有 task_lock 串行化（6.12 有 task_lock，为 A），最后写进槽的名字和最后完成 memcpy 的名字可能不同。这和内核 helper 自身的同类竞态同级。

**override_creds** 只在 syscall 内部临时生效，用户态探针不会在同一线程的 syscall 中途执行，所以不影响。

### 步骤 3：loader `tools/view/tern_viewd.c` 和 `tools/view/build.sh`（新增，约 200 行）

**构建**（照抄 e5/src/build.sh）
- 用 bootstrap bpftool 生成 vmlinux.h：`btf dump file /sys/kernel/btf/vmlinux format c`，也可以复用 scratchpad/vmlinux.h。
- `clang-19 -target bpf -mcpu=v3 -g -O2` 编 BPF 程序，`bpftool gen skeleton` 生成 skeleton，gcc 链接 third_party 里的静态 libbpf。
- **viewd 绝不能带 bpftime syscall-server preload 运行**，否则 bpf() 会被拦到用户态。

**子命令**：采用 pinned link，崩溃也不会静默变陈旧（评审 1 指出的致命问题）。
- **`viewd start --pin-dir /sys/fs/bpf/<name> [--nslots-log2 18] [--idfile F]`**
  1. 发现残留的 pin 目录就报错退出，不擅自覆盖。
  2. open skeleton，设置 `nslots_mask` 和 `max_entries`，load。
  3. 按 exit、exec、creds、rename、newtask 的顺序 attach。creds 依次尝试 fexit → fentry → kprobe：每次失败就 `bpf_program__set_autoload` 换下一个程序，重新 open 和 load；`hook_mode` 记 1、2、3。
  4. 对 map 做 RW mmap，写 hdr：magic、abi、nslots、`stat("/proc/self/ns/pid")` 得到的 ino/dev、loader_pid、hook_mode，最后写 state=LIVE。
  5. 每个 link 用 `bpf_link__pin` pin 到 `<dir>/link_*`；map pin 到 `<dir>/view`，模式 0600。
  6. idfile 写入 map id（由 `bpf_obj_get_info_by_fd` 取得）。
  7. stdout 打印 `ready mode=fexit|fentry|kprobe nslots=N`，然后**退出**。钩子靠 pinned link 存活，不需要常驻进程。
- **`viewd status --pin-dir D`**：输出 per-CPU 计数之和、已占用槽数、`/proc/*/task` 的线程数，格式为 JSON。harness 在测量窗口前后各取一次，算差分。
- **`viewd stop --pin-dir D`**，顺序如下：
  1. hdr.state=DEAD。之后钩子在 unlock 时给写过的槽置 STALE。
  2. 用户态按同一套 seq 协议（gcc `__atomic_compare_exchange`）给每个槽持锁置 STALE。
  3. unpin 并关闭所有 link，钩子 detach。
  4. unpin map，删除 idfile。
  - agent 已有的 mmap 仍然持有 map 引用，但所有槽都是 STALE，读端一律回退。

**崩溃语义**：start 中途崩溃时，pin 目录不完整，runner 的残留检查会报出来。start 之后不存在常驻进程，也就没有"进程死了钩子随之 detach"的问题。runner 在每个 leg 结束时检查 link pin 仍在、`status` 中各钩子计数单调增加；不满足就把这个 leg 作废。

### 步骤 4：agent 读侧 `runtime/src/kernel_view.{hpp,cpp}`（新增，约 150 行）

**加入构建**：加入 `runtime/CMakeLists.txt` 的主源文件列表，**不放进** `BPFTIME_BUILD_WITH_LIBBPF` 分支。只用 `<linux/bpf.h>` 加 raw `syscall(__NR_bpf, …)`，大约 20 行（评审 2 的建议），这样不改变 Dnextpc 的构建配置，B6 的"同一二进制"仍然成立。

**模式开关**：`extern "C" bool bpftime_view_mode`，写法照 producer_identity.cpp:24，用加载时 lambda 读 `BPFTIME_VIEW`。只读一次，因为 nginx 会清空 worker 的环境变量。

**`extern "C" bool bpftime_view_setup()`**
- 由 agent.cpp 调用，紧接在 :915 `careful_bound` 那行之后：
  ```cpp
  static const bool view_ok = bpftime_view_setup();
  if (bpftime_view_mode) SPDLOG_INFO("View: {}", ...);
  ```
- careful 未绑定时仍然启用视图（供消融 leg 使用），同时打 WARN："key from non-careful identity cache"。
- 拿 fd 的顺序：
  1. `BPF_OBJ_GET`，路径取 `BPFTIME_VIEW_PIN`（默认 `/sys/fs/bpf/tern_view/view`），`file_flags = BPF_F_RDONLY`；
  2. 失败时读 `BPFTIME_VIEW_ID_FILE` 里的 id，调 `BPF_MAP_GET_FD_BY_ID`，`open_flags = BPF_F_RDONLY`。这条需要 CAP_SYS_ADMIN；E5 目标和 nginx master 都是 root，特权容器里也有这个能力。
- 校验（任一失败：打 WARN 写明原因，视图在这个进程里关闭，helper 走原路径）：
  - `BPF_OBJ_GET_INFO_BY_FD`：type 为 ARRAY、带 MMAPABLE、value_size 为 64、max_entries−1 是 2 的幂。
  - `mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0)`。sz 的算法照 array_map_kernel_user.cpp:28-36，但**不要**它的 PROT_WRITE。映射完 close(fd)。
  - hdr 的 magic、abi 一致，state==LIVE，pidns ino/dev 等于本进程的 `/proc/self/ns/pid`。
  - 自检：读 `gettid()` 对应的槽，只记录 hit/miss，不会因此关闭视图。
- 成功时发布 `bpftime_view_slots`（slot 数组基址，含 +1 偏移）和 `bpftime_view_mask`，打 INFO `View: mapped N slots via pin|id, self hit|miss`。
- 继承关系：nginx master 以 root 映射后，worker fork 时继承这块 MAP_SHARED 映射，setuid 之后照样能读。exec 之后由新的 agent 重新映射。

**内联读函数**（异步信号安全，无锁，无 syscall）
```cpp
static inline bool bpftime_view_read(uint32_t tid, uint32_t tgid, uint32_t need,
                                     uint32_t *uid, uint32_t *gid, char comm[16]) {
  const tern_view_slot *s = &bpftime_view_slots[tid & bpftime_view_mask];
  for (int r = 0; r < 4; r++) {
    uint32_t q = __atomic_load_n(&s->seq, __ATOMIC_ACQUIRE);
    if (q & 1) { __builtin_ia32_pause(); continue; }
    uint32_t t = __atomic_load_n(&s->tid, __ATOMIC_RELAXED), g = __atomic_load_n(&s->tgid, __ATOMIC_RELAXED);
    uint32_t f = __atomic_load_n(&s->flags, __ATOMIC_RELAXED);
    uint32_t u = __atomic_load_n(&s->uid, __ATOMIC_RELAXED), gg = __atomic_load_n(&s->gid, __ATOMIC_RELAXED);
    uint64_t c0 = __atomic_load_n((const uint64_t *)s->comm, __ATOMIC_RELAXED);
    uint64_t c1 = __atomic_load_n((const uint64_t *)s->comm + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if (__atomic_load_n(&s->seq, __ATOMIC_RELAXED) != q) continue;
    if (t != tid || g != tgid || (f & TV_F_STALE) || (f & need) != need) return false;
    if (uid) { *uid = u; *gid = gg; }
    if (comm) { memcpy(comm, &c0, 8); memcpy(comm + 8, &c1, 8); }
    return true;
  }
  return false;
}
```
- miss 计数：冷路径上的 `std::atomic<uint64_t>`，relaxed 自增。`__attribute__((destructor))` 打 `View: reads <n> misses <m>`。reads 计数只在 `BPFTIME_VIEW_COUNT=1` 时开，避免热路径多一次 RMW。

### 步骤 5：helper 接入 `runtime/src/bpf_helper.cpp`

**`bpf_get_current_uid_gid`**（:734）函数第一行，放在 `credential_cache_enabled` 检查之前：
```cpp
if (__builtin_expect(bpftime_view_slots != nullptr, 0)) {
  int pid; int64_t tid; bpftime::current_producer_identity(pid, tid);
  uint32_t u, g;
  if (bpftime_view_read((uint32_t)tid, (uint32_t)pid, TV_F_IDS|TV_F_CREDS, &u, &g, nullptr))
    return ((uint64_t)g << 32) | u;
  bpftime_view_count_miss();
  return query_credentials();      // 精确 syscall；绝不走进程级缓存
}
```

**`bpftime_get_current_comm`**（:943），在 :947 careful 分支之前插入同样结构：
- 命中：`careful_copy_comm(buf, size, comm)`，保持内核 helper 的语义。
- miss：调新抽出来的 `read_comm_exact(buf, size)`。它取自 :903-916 的 `prctl(PR_GET_NAME)` 那段，不写缓存。
- 注意 `careful_copy_comm` 目前在 `#if __linux__` 内的匿名 namespace 里，要保证新分支在同一作用域内。

**语义差异要写进论文**
- 视图返回 kuid/kgid 的 `.val`，即 init userns 的值，与内核 `bpf_get_current_uid_gid` 一致。
- miss 时的 `getuid()` 返回的是本 userns 的值。在 init userns 里两者相同；E5 和 nginx 都在 init userns。

**不改** `bpftime_get_current_pid_tgid`。视图关闭时的代价：每个 helper 多一次全局指针 load 和一个分支。

### 步骤 6：E5 改动（`/home/y/src/bpftime-x86-runs/paper-20261124/e5/`，先备份）

**e5target.c 新增两个场景**，同步 SCEN[]（:61-62）和 run_e5.py 的 SCEN_POINTS：
- **W7t**：2 线程进程，只在 worker 线程里 raw `syscall(SYS_setresuid/setresgid)`，两个线程分别审计。预测 CAREFUL 下错（进程级缓存），VIEW 下 0/500。
- **W5r**：raw `syscall(SYS_clone, SIGCHLD, 0, …)` 的子进程审计。预注册：VIEW 下 tgid/tid 仍错（key 来自继承的 TLS）；uid/gid/comm 读到父线程的槽，值碰巧与真值相同，判为对。如果另加 W5rn（子进程再改名）则 comm 错；W5rn 可选。

**run_e5.py**（约 40 行）
- leg 环境里有 `BPFTIME_VIEW=1` 时，在启动 bpftime 工具**之前**执行：
  - `viewd start --pin-dir /sys/fs/bpf/tern_view_e5_<pid> --idfile … [--nslots-log2 $VIEWD_NSLOTS_LOG2]`，等 `ready`，deadline 10 s；
  - 把 `BPFTIME_VIEW_PIN` 传给目标。`VIEWD_*` 开头的键只给 runner 自己用，不传下去。
- 目标结束后依次执行 `status`（计数写进 kind=leg 行）和 `stop`。
- 父进程的残留检查加两项：没有 `/sys/fs/bpf/tern_view*`；`bpftool link` 里没有 tern 程序。
- 先写 `PREDICTIONS-v4.md` 并取 sha，时间要早于 Dview 构建。

---

## 3. 测试和测量
所有构建、sudo 和测量都要先 `flock` 测量锁，只在空闲窗口进行。

### 3.1 独立 selftest `tools/view/view_selftest.c`（root，不链接 Tern）
自带读者，复制 §2 步骤 4 的读函数。真值取自 `/proc/thread-self/status` 的 Uid/Gid 第 1 列（real），以及 `/proc/thread-self/comm`；每个场景 10^4 次，每个场景结束后检查 `status` 计数。
- S1：主线程 raw setresuid/setresgid，在 0 和 4242 之间交替。
- S2：**非主线程** raw setresuid，两个线程都检查。
- S3：prctl 和 pthread_setname_np 改名，名字长度 15、16、40。
- S4：写自己的 /proc comm（对应 W8c）。
- S5：主线程写 worker 的 /proc comm（对应 W8d）。
- S6：fork 子进程和 pthread 新线程的首次读。
- S7：非 leader 线程 execve 自身（用 argv 做标记），exec 后读；同时检查旧 tid 的槽已被清掉。
- S8：线程退出后槽被清；用 W12 式的 tid 绕回 churn，确认没有陈旧命中。
- S9：并发折磨。两个线程分别循环改名和 raw setresuid，主线程跨线程改名，各跑 10^6 次。读到的值必须属于合法取值集合（不撕裂）。记录 lock_fail（预期为 0）和 collision。
- S10：`viewd start --nslots-log2 4` 后重跑 S1–S6，必须出现 miss，不能出现错值。
- `--bench` 模式（M1）：pin 在一个 P 核上，10^7 次读自己的完整快照，对照 `getuid()+getgid()` 和 `prctl(PR_GET_NAME)`，报 ns/次的中位数。

### 3.2 Tern 单元测试 `runtime/unit-test/test_kernel_view.cpp`（Catch2，不需要内核）
- 注入接口：`bpftime_view_attach_for_test(void *slots, uint32_t nslots)`，用匿名 mmap 伪造视图。
- 命中时两个 helper 返回视图值。
- 回退到 syscall 的情况：奇数 seq、tid 不符、tgid 不符、STALE、缺 F_COMM 位（uid 命中但 comm 回退）。
- 撕裂压力：写线程按 seq 协议写 uid==gid==i，读者 10^6 次不能读到不一致的值。
- 初始化失败路径：magic 不符、state==DEAD、pidns 不符时视图关闭，helper 返回 syscall 值。
- `size < 16` 时的 comm 复制语义；复用现有 test_get_current_comm.cpp 和 test_get_current_uid_gid.cpp 的 getuid/getgid wrap。

### 3.3 M0 冒烟（root，约 15 min，CP1）
- `grep -w commit_creds /sys/kernel/tracing/available_filter_functions`，用来核实 K13。
- `viewd start` 打出 ready，核实 K14。
- selftest 能做 RDONLY fd + PROT_READ 的 mmap；PROT_WRITE 和 mprotect 都被拒绝（K15）。
- 在 harness 容器里 `docker exec … ls -l /sys/fs/bpf/tern_view`，核实 K16。
- 非 root 能否 BPF_OBJ_GET 只记录，不作要求。

### 3.4 M2 内核钩子代价 `tools/view/hookbench.c`（约 10 min）
- 4 项：raw setresuid 0↔4242（K10）、prctl 两个名字交替、fork+_exit+waitpid、pthread_create+join。
- 每项 10^5–10^6 次。viewd 按 off/on/off/on 跑 4 块，报每次操作的增量 ns 和 CI。

### 3.5 M3 E5（I9，约 8 min 机时，15+2 场景 × 500 次）
legs 如下，同一个 Dviewpc 二进制：
1. `K`
2. `Dviewpc+BPFTIME_CAREFUL=1`：对照，预测与 I8 的 CAREFUL 列逐格相同，加上 W7t 错、W5r 的 p1 tgid/tid 错。
3. `Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1`：预测除 W5r 的 tgid/tid 外**全部 0/500**。
4. `Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1+VIEWD_NSLOTS_LOG2=4`：预测 0/500，miss > 0。
5. （可选，最小版砍掉）`Dviewpc+BPFTIME_VIEW=1`，careful 关：只预注册"W7r/W7t/W8c/W8d 的 uid/gid/comm 正确"，其他格子不做预测。

每个 VIEW 目标的日志都要有 `View: mapped`。另外记录 miss 数和 viewd 的 lock_fail（预测为 0）、collision。

### 3.6 M4 nginx（spike 版，约 20 min 机时，CP4）
- 单独一个 session plan：
  - 第一行 `!` 宿主行：`sudo -n …/tern_viewd start --pin-dir /sys/fs/bpf/tern_view --idfile $H/view.id`；
  - 接着 `!` 宿主行 `viewd status > $OUT/viewd-pre.json`；
  - legs：`Dviewpc+BPFTIME_CAREFUL=1`、`Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1`，2 round × 11 rep；
  - 最后 `!` 宿主行跑 `status` 和 `stop`。
- 两个 leg 期间钩子都开着，所以差值只反映用户态读路径。内核侧成本由 M2 给出。
- 窗口内的钩子写入计数差分应该约等于 0（nginx 稳态下没有 fork、setuid、改名），这一点证明钩子代价不进入稳态。
- 容器里看不到 pin 时，在 leg 上加 `+BPFTIME_VIEW_ID_FILE=/bench/view.id`（$H 挂载为 /bench）。
- 核对项：
  - master 和所有 worker 日志都有 `View: mapped`（worker 是继承的映射）；
  - 每个 worker 的 miss ≤ 0.1%；如果某个 worker 因撞槽 miss 很高，换 pid 重跑并记录；
  - worker 降权后的 uid 与 Kb 一致。
- 判据：配对差 µs/event 的 90% CI 上界 ≤ +0.084。
- 预测（推断，未测）：每请求 6 次 uid_gid 和 2 次 comm，约 +0.00–0.03 µs/event。

### 3.7 go 之后才做的测量（不属于 spike）
- C2 同场加 leg `Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1`，run_paper.py 逐 leg 起停 viewd。分析：
  - 对 CAREFUL 做 TOST（±0.084）；
  - 对 Kb 的比值（带 CI）；
  - 精确的代价 x(VIEW) − x(Dnextpc) µs/event，替代 1.53；
  - perf sc 3 rep，确认每请求的 getuid/getgid/prctl 不比 CAREFUL 多。
- 干扰对照：viewd 已加载时跑 Kb，对比未加载的 Kb。
- 惰性检验：`Dviewpc`（VIEW 关）对 `Dnextpc`，B6 式。
- I9 扩展场景：W10p/W10u，以及 E-V5（不带 `--pid=host` 的容器）。**没跑 E-V5 之前，不能写"VIEW 在容器里也精确"。**

---

## 4. 风险和后备

| 风险 | 后备 | 状态 |
|---|---|---|
| commit_creds 挂不上 fexit（K13） | 依次退到 fentry、kprobe；LSM（task_fix_setuid 等）只记录，不算 go，因为 set_cred_ucounts 在 LSM 钩子之后仍可能失败 | 在 CP1 验证 |
| verifier 拒绝 mmapable value 上的 cmpxchg（K14） | ① 加 `-mcpu=v3` 并确认 clang-19 产出 BPF_CMPXCHG；② 实在不行就退到单写者设计：只用 fetch_add 做 seq，每个钩子只写自己的字段，跨线程 rename 置 STALE。精度不变，只是 W8d 会回退成 miss | CP1 |
| 容器里看不到 bpffs（K16） | idfile 加 `BPF_MAP_GET_FD_BY_ID`（特权容器里的 root 有 CAP_SYS_ADMIN）；再不行就给 docker run 加 `-v /sys/fs/bpf:/sys/fs/bpf`（改 session.sh，先备份） | CP1 |
| agent 里的 bpf() 被某种 syscall 改写拦住 | agent 只用 raw syscall，由日志里的 `View: mapped` 确认；如果被拦，改到 constructor 最早阶段初始化 | CP3 |
| loader 崩溃导致静默陈旧 | 用 pinned link，不靠常驻进程；stop 时 DEAD→STALE→detach；runner 每个 leg 检查 link pin 和计数单调性，不满足就作废该 leg | 设计内已处理 |
| 撞槽让某个 nginx worker 长期回退，污染 M4 | 默认 2^18 槽；按 worker 检查 miss 率，超阈值就重启 nginx 换 pid；go 之后考虑设计 2 的 W=8 探测窗口 | M4 检查 |
| 残余竞态（newtask 与 /proc 改名、并发改名） | 写成 declared；靠 S9 和计数器观察；go 之后用 pending_comm 方案修 | 不影响 go |
| 全机钩子干扰别的测量 | 只在 VIEW leg 期间启用；非 VIEW leg 的 runner 残留检查要求没有 tern pin；go 之后加 Kb 干扰对照 | — |
| 信息泄露（全机线程的 uid/comm 可见） | pin 模式 0600、agent 只读映射；go 之后加 cgroup 过滤，threat model 段要写 | 论文 |
| 读取用户态缓存 key 的 W5r 缺口 | 预注册为失败用例；论文措辞带 "given a correct thread key" | 论文 |
| 进度超时 | 最小路径约 10 h（§5）；砍掉单元测试、消融 leg 5 和 M2，M4 放到夜里跑 | — |
| Jetson 移植 | L4T 内核如果早于 6.0，arm64 没有 fentry/fexit，commit_creds 只能用 kprobe；写端原子已经是 FETCH 版本。先查 Jetson 的 `uname -r` | go 之后 |

**论文写作项**（no-go 也要改）
1. **W-rseq 第 2 点论证是反的**（eval-plan:413，K11）。vfork 子进程共用父进程的线程指针，按 `__rseq_offset` 找到的是父进程的 rseq 区，读到的是父进程的值。要补一条 CLONE_VM "lent" 规则：不带 CLONE_SETTLS 的 CLONE_VM 子进程运行期间，内核在父进程的区里置标志，到 mm_release/vfork_done 时清掉，读者看到标志就回退。
2. eval-plan:278/408 的 leg 名 "Dnextpc+VIEW" 改成 "Dnextpc+CAREFUL+VIEW"。
3. 论文写明两点：kuid 语义；W7r 的根因是进程级凭据缓存。
4. go 时的措辞按设计 3 §1.3 的三套写法选，但 "answers every audited scenario as the kernel does" 必须紧跟 "given a correct thread key" 并点名 W5r。discussion.tex:6 带 "for an unprivileged runtime" 限定，no-go 时不必删。

---

## 5. 工时（小时；CP = 检查点，需要 sudo 或测量窗口）

| # | 步骤 | h | 最小路径 |
|---|---|---|---|
| 0 | PREDICTIONS-v4（含 W7t、W5r）和 go/no-go 预登记，取 sha；不需要锁 | 0.5 | 是 |
| 1 | ABI 头 | 0.5 | 是 |
| 2 | view.bpf.c：5 个钩子加 fentry/kprobe 后备 | 2.5 | 是 |
| 3 | tern_viewd.c：start/status/stop、pinned link、后备链；build.sh | 2 | 是 |
| CP1 | M0 冒烟：K13–K16，verifier，容器可见性 | 0.5 | 是 |
| 4 | view_selftest S1–S10 加 `--bench`（M1）；**CP2** selftest 全过 | 2 | 是（S9、S10 可缩短） |
| 5 | hookbench 和 M2 | 0.5 | 否 |
| 6 | Tern：kernel_view.{hpp,cpp}（raw bpf syscall）、两个 helper、`read_comm_exact`、agent.cpp、CMake | 2 | 是 |
| 7 | 单元测试 | 1 | 否 |
| 8 | 构建 Dview/Dviewpc 并登记 | 0.25 | 是 |
| 9 | e5target 加 W7t、W5r，run_e5.py 起停 viewd 和残留检查 | 1.25 | 是 |
| CP3 | M3 I9 跑完并逐格比对（机时约 8 min） | 0.5 | 是 |
| CP4 | M4 nginx（人工约 0.5 h，机时约 20 min） | 0.5 | 是（可夜跑） |
| 10 | go/no-go 记录，并改 eval-plan :50/:278/:408/:413 | 0.5 | 是 |
| | **合计** | **约 14.5 h** | **约 10.5 h** |

**日程**
- 12-09：步骤 0–4、CP1、CP2。
- 12-10：步骤 6–9、CP3，夜里跑 CP4。
- 12-11：决定。
- 所有编译、sudo、加载和测量都要排进测量锁空闲的窗口。

**go 之后**（12-22 前，约 16 h，另加机时约 30 min）：run_paper 逐 leg 起停（3 h）、C2 的 VIEW leg 加 TOST、Kb 干扰对照和惰性检验（2 h）、iter 回填（2 h）、cgroup 过滤（2 h）、W10p/W10u 和 E-V5（3 h）、钩子 microbench 定稿（1 h）、论文改写（3 h）。arm64 移植另计 8–16 h。

---

## 相关路径
- **Tern（Dnext）**：/home/y/src/bpftime-paper/dnext-c71a9987/runtime/src/bpf_helper.cpp（:711-758 凭据、:878-916 careful comm、:943 comm helper）、runtime/src/producer_identity.{hpp:53-68,cpp:24}、runtime/agent/agent.cpp:915、runtime/src/bpf_map/shared/array_map_kernel_user.cpp:28-100、runtime/CMakeLists.txt、runtime/unit-test/test_get_current_{comm,uid_gid}.cpp
- **E5 和 harness**：/home/y/src/bpftime-x86-runs/paper-20261124/e5/{run_e5.py:76,src/e5target.c:61,432,src/build.sh}、/home/y/src/bpftime-x86-runs/paper-20261124/harness/{session.sh:94,104,run_paper.py,config-x86.json}
- **计划**：/home/y/src/paper-drafts/atc-2027/notes/eval-plan.md:397-416（以及 :50）
- **内核源码（7.2）**：/home/y/src/linux/kernel/fork.c:2364,2533,2554,2758,2779；fs/exec.c:1006,1032,1098,1134,1253,1274,1715,1748；kernel/exit.c:980；kernel/bpf/syscall.c:1091,1132,1470,3030；kernel/sys.c:733-737；include/linux/rseq.h:155-161
- **BTF**：/tmp/claude-1000/-home-y-src/8ed65bbb-2405-45b6-a329-69008d14bf3d/scratchpad/vmlinux.h，以及同目录下的 vmlinux.btf.raw