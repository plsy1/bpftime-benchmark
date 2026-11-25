.text
.p2align 6
tret: ret
.p2align 6
tback_d: jmp back_d
.p2align 6
tback_i: jmp back_i
/* call trivial ret: direct */
.globl C_dir
.p2align 6
C_dir: push %rbp
  mov %rdi,%rbp
.p2align 6
1: call tret
  dec %rbp
  jne 1b
  pop %rbp
  ret
.globl C_ind
.p2align 6
C_ind: push %rbp
  push %rbx
  movabs $tret,%rbx
  mov %rdi,%rbp
.p2align 6
1: call *%rbx
  dec %rbp
  jne 1b
  pop %rbx
  pop %rbp
  ret
/* jumps, no call/ret */
.globl J_dir
.p2align 6
J_dir: push %rbp
  mov %rdi,%rbp
.p2align 6
1: jmp tback_d
back_d: dec %rbp
  jne 1b
  pop %rbp
  ret
.globl J_ind
.p2align 6
J_ind: push %rbp
  push %rbx
  movabs $tback_i,%rbx
  mov %rdi,%rbp
.p2align 6
1: jmp *%rbx
back_i: dec %rbp
  jne 1b
  pop %rbx
  pop %rbp
  ret
