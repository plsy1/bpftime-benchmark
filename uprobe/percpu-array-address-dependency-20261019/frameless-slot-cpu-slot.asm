
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/frameless-slot/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .text:

00000000004ef2c0 <bpftime::per_cpu_array_map_impl::lookup_cpu_slot(unsigned char*, unsigned long)>:
  4ef2c0:	stp	x29, x30, [sp, #-32]!
  4ef2c4:	mov	x29, sp
  4ef2c8:	stp	x19, x20, [sp, #16]
  4ef2cc:	mov	x20, x1
  4ef2d0:	mov	x19, x0
  4ef2d4:	bl	48a680 <sched_getcpu@plt>
  4ef2d8:	sxtw	x0, w0
  4ef2dc:	madd	x0, x0, x20, x19
  4ef2e0:	ldp	x19, x20, [sp, #16]
  4ef2e4:	ldp	x29, x30, [sp], #32
  4ef2e8:	ret
