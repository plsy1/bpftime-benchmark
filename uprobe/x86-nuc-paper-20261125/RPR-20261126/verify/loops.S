
.text
.globl L_dir_0
.p2align 6
L_dir_0:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  mov %rdi,%rbp
  jmp 1f
.p2align 6
.skip 0,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call helper
  dec %rbp
  test %r13d,%r13d
  mov %rbp,%r13
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_ind_0
.p2align 6
L_ind_0:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  movabs $helper,%rbp
  jmp 1f
.p2align 6
.skip 0,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call *%rbp
  dec %r13
  test %r12d,%r12d
  mov %r13,%r12
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_dir_16
.p2align 6
L_dir_16:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  mov %rdi,%rbp
  jmp 1f
.p2align 6
.skip 16,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call helper
  dec %rbp
  test %r13d,%r13d
  mov %rbp,%r13
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_ind_16
.p2align 6
L_ind_16:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  movabs $helper,%rbp
  jmp 1f
.p2align 6
.skip 16,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call *%rbp
  dec %r13
  test %r12d,%r12d
  mov %r13,%r12
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_dir_32
.p2align 6
L_dir_32:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  mov %rdi,%rbp
  jmp 1f
.p2align 6
.skip 32,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call helper
  dec %rbp
  test %r13d,%r13d
  mov %rbp,%r13
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_ind_32
.p2align 6
L_ind_32:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  movabs $helper,%rbp
  jmp 1f
.p2align 6
.skip 32,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call *%rbp
  dec %r13
  test %r12d,%r12d
  mov %r13,%r12
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_dir_48
.p2align 6
L_dir_48:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  mov %rdi,%rbp
  jmp 1f
.p2align 6
.skip 48,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call helper
  dec %rbp
  test %r13d,%r13d
  mov %rbp,%r13
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret


.text
.globl L_ind_48
.p2align 6
L_ind_48:
  endbr64
  push %rbp
  push %r15
  push %r14
  push %r13
  push %r12
  push %rbx
  sub $0x4058,%rsp
  mov %rsi,%r14
  mov %rdx,%rbx
  mov %rdi,%r12
  mov %rdi,%r13
  movabs $helper,%rbp
  jmp 1f
.p2align 6
.skip 48,0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  call *%rbp
  dec %r13
  test %r12d,%r12d
  mov %r13,%r12
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret
