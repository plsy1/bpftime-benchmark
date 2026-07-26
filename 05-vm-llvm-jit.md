# 4. vm/：执行引擎（LLVM JIT）

三层：vm-core C API（薄壳）→ compat 抽象基类+工厂（`"llvm"`/`"ubpf"` 按名注册）→
llvm-jit（独立库 llvmbpf 内嵌版）。

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `vm/compat/include/bpftime_vm_compat.hpp` | 265 | **枢纽，先读**：虚接口+工厂表+`struct ebpf_vm` 真实定义 |
| `vm/vm-core/{ebpf-vm.h,ebpf-vm.cpp}` | 340 | 确认 C API 只是转发；`ebpf_jit_fn(mem, len)` 即 ctx 传递方式 |
| `vm/llvm-jit/include/llvmbpf.hpp` + `src/vm.cpp` | 276 | `ext_funcs` 表、`jitted_function` 缓存、"没编译先编译"语义 |
| `vm/llvm-jit/src/compiler_utils.cpp` | 482 | `emitExtFuncCall`(L309)：helper 编号→符号 `_bpf_helper_ext_%04d` |
| `vm/llvm-jit/src/llvm_jit_context.cpp` | 814 | L505-610 符号绑定段：absoluteSymbols 绑 helper 函数指针 |
| `vm/llvm-jit/src/compiler.cpp` | 1376 | 按需：block 划分、寄存器 alloca、512B 栈、lddw 伪指令 |

坑：
- **helper 必须在 compile 前全部 register**：编号在编译期固化为符号，之后改
  `ext_funcs` 不生效。
- llvm 后端**没有真解释器**：`exec()` 是惰性 JIT + 缓存；真解释器只有 ubpf。
- helper 统一按 5×i64 原型发射（依赖调用约定容忍多传参）；`bpf_tail_call`
  (imm==12) 是硬编码特例，call 后直接 br 到 exit。
- bpf-to-bpf 子函数调用不走本机栈：自建 callStack alloca，整个程序 JIT 成单个
  `bpf_main`。

验证问题：`call 25` 到 `bpf_perf_event_output` 中间有几层间接？（答：零层）
compile 后再 register 新 helper 有效吗？JIT `bpf_main` 的两个参数在 uprobe 场景
对应什么？

---
[← 返回目录](README.md)
