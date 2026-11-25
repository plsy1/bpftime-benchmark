import sys
# generate asm loops mirroring the bpftime JIT (call rel32) and AOT (movabs; call *%rbp) code
out=[]
def loop(name, indirect, off):
    out.append(f"""
.text
.globl {name}
.p2align 6
{name}:
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
  {"movabs $helper,%rbp" if indirect else "mov %rdi,%rbp"}
  jmp 1f
.p2align 6
.skip {off},0x90
1:
  mov $5,%esi
  mov %r14,%rdi
  mov %rbx,%rdx
  {"call *%rbp" if indirect else "call helper"}
  {"dec %r13" if indirect else "dec %rbp"}
  {"test %r12d,%r12d" if indirect else "test %r13d,%r13d"}
  {"mov %r13,%r12" if indirect else "mov %rbp,%r13"}
  jne 1b
  add $0x4058,%rsp
  pop %rbx
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  pop %rbp
  ret
""")
offs=[0,16,32,48]
for o in offs:
    loop(f"L_dir_{o}",False,o); loop(f"L_ind_{o}",True,o)
open(sys.argv[1],"w").write("\n".join(out))
h=open(sys.argv[2],"w")
h.write("#define OFFS {%s}\n"%",".join(map(str,offs)))
for o in offs: h.write(f"void L_dir_{o}(long,void*,void*); void L_ind_{o}(long,void*,void*);\n")
h.write("typedef void (*lf)(long,void*,void*);\nstatic lf DIR[]={%s}; static lf IND[]={%s};\n"%(",".join(f"L_dir_{o}" for o in offs),",".join(f"L_ind_{o}" for o in offs)))
