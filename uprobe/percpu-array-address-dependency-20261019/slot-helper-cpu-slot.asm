
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/slot-helper/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .text:

00000000004f02b0 <bpftime::per_cpu_array_map_impl::lookup_cpu_slot(unsigned char*, unsigned long)>:
  4f02b0:	stp	x29, x30, [sp, #-32]!
  4f02b4:	mov	x29, sp
  4f02b8:	stp	x19, x20, [sp, #16]
  4f02bc:	mov	x20, x1
  4f02c0:	mov	x19, x0
  4f02c4:	bl	48a680 <sched_getcpu@plt>
  4f02c8:	sxtw	x0, w0
  4f02cc:	madd	x0, x0, x20, x19
  4f02d0:	ldp	x19, x20, [sp, #16]
  4f02d4:	ldp	x29, x30, [sp], #32
  4f02d8:	ret
