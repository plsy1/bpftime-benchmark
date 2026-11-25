.text
.p2align 6
tpop_d: add $8,%rsp
  jmp back_d
.p2align 6
tpop_i: add $8,%rsp
  jmp back_i
.p2align 6
tret: ret
.globl CP_dir
.p2align 6
CP_dir: push %rbp
  mov %rdi,%rbp
.p2align 6
1: call tpop_d
back_d: dec %rbp
  jne 1b
  pop %rbp
  ret
.globl CP_ind
.p2align 6
CP_ind: push %rbp
  push %rbx
  movabs $tpop_i,%rbx
  mov %rdi,%rbp
.p2align 6
1: call *%rbx
back_i: dec %rbp
  jne 1b
  pop %rbx
  pop %rbp
  ret
.globl CM_ind
.p2align 6
CM_ind: push %rbp
  push %rbx
  movabs $tret,%rbx
  push %rbx
  mov %rdi,%rbp
.p2align 6
1: call *(%rsp)
  dec %rbp
  jne 1b
  pop %rbx
  pop %rbx
  pop %rbp
  ret
