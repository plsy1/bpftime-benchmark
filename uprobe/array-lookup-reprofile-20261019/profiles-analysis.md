# Accepted-baseline production sampling

Top-frame event counts, filtered to the victim PID. Each capture is a five-second delayed window. Counts are descriptive; they are not exact time per instruction, and profiled wall times are not benchmark results.

## array_lookup-1

4975 top-frame samples; CPU 5; sampling period 1728000 cycles.

| Instruction region | Samples | Share of all target samples |
| --- | ---: | ---: |
| key_bounds | 2142 | 43.06% |
| manager_fd_type_resolution | 1160 | 23.32% |
| anonymous_JIT_or_trampoline | 675 | 13.57% |
| data_pointer_and_address | 628 | 12.62% |
| map_impl_offset_pointer | 223 | 4.48% |
| other_runtime_or_system | 112 | 2.25% |
| entry_exit_registers | 35 | 0.70% |

Hottest helper PCs (ELF addresses; ARM sampling may skid):

| PC | Helper offset | Samples | Instruction |
| --- | --- | ---: | --- |
| `0x4e58cc` | `+0x88` | 1978 | `cmp	w2, w0` |
| `0x4e58d4` | `+0x90` | 363 | `ldr	x3, [x1, #8]` |
| `0x4e5890` | `+0x4c` | 273 | `cmp	w2, #0x1` |
| `0x4e5898` | `+0x54` | 171 | `ldr	w2, [x0, #72]` |
| `0x4e58ac` | `+0x68` | 119 | `ldr	w2, [x1]` |
| `0x4e588c` | `+0x48` | 105 | `ldrb	w2, [x0, #160]` |
| `0x4e58bc` | `+0x78` | 100 | `add	x0, x0, x1` |
| `0x4e58dc` | `+0x98` | 71 | `ldr	w4, [x1, #32]` |
| `0x4e58e0` | `+0x9c` | 55 | `cmp	x3, #0x1` |
| `0x4e589c` | `+0x58` | 54 | `cmp	w2, #0x2` |
| `0x4e5880` | `+0x3c` | 53 | `and	x0, x2, x4` |
| `0x4e58b4` | `+0x70` | 52 | `ldr	x1, [x0]` |

## array_lookup-2

5463 top-frame samples; CPU 5; sampling period 1572863 cycles.

| Instruction region | Samples | Share of all target samples |
| --- | ---: | ---: |
| key_bounds | 2351 | 43.03% |
| manager_fd_type_resolution | 1225 | 22.42% |
| anonymous_JIT_or_trampoline | 769 | 14.08% |
| data_pointer_and_address | 747 | 13.67% |
| map_impl_offset_pointer | 226 | 4.14% |
| other_runtime_or_system | 115 | 2.11% |
| entry_exit_registers | 30 | 0.55% |

Hottest helper PCs (ELF addresses; ARM sampling may skid):

| PC | Helper offset | Samples | Instruction |
| --- | --- | ---: | --- |
| `0x4e58cc` | `+0x88` | 2181 | `cmp	w2, w0` |
| `0x4e58d4` | `+0x90` | 473 | `ldr	x3, [x1, #8]` |
| `0x4e5890` | `+0x4c` | 286 | `cmp	w2, #0x1` |
| `0x4e5898` | `+0x54` | 175 | `ldr	w2, [x0, #72]` |
| `0x4e58ac` | `+0x68` | 128 | `ldr	w2, [x1]` |
| `0x4e588c` | `+0x48` | 103 | `ldrb	w2, [x0, #160]` |
| `0x4e58bc` | `+0x78` | 98 | `add	x0, x0, x1` |
| `0x4e58e0` | `+0x9c` | 74 | `cmp	x3, #0x1` |
| `0x4e589c` | `+0x58` | 73 | `cmp	w2, #0x2` |
| `0x4e58dc` | `+0x98` | 64 | `ldr	w4, [x1, #32]` |
| `0x4e5864` | `+0x20` | 57 | `ldr	x3, [x2, #8]` |
| `0x4e58b4` | `+0x70` | 54 | `ldr	x1, [x0]` |

## percpu_array_lookup-1

4972 top-frame samples; CPU 5; sampling period 1728000 cycles.

| Instruction region | Samples | Share of all target samples |
| --- | ---: | ---: |
| manager_fd_type_resolution | 1690 | 33.99% |
| cpu_query_libc | 942 | 18.95% |
| key_bounds | 652 | 13.11% |
| data_pointer_and_address | 489 | 9.84% |
| entry_exit_registers | 473 | 9.51% |
| anonymous_JIT_or_trampoline | 343 | 6.90% |
| map_impl_offset_pointer | 164 | 3.30% |
| other_runtime_or_system | 99 | 1.99% |
| cpu_query_plt | 84 | 1.69% |
| cpu_query_callsite | 36 | 0.72% |

Hottest helper PCs (ELF addresses; ARM sampling may skid):

| PC | Helper offset | Samples | Instruction |
| --- | --- | ---: | --- |
| `0x4e579c` | `+0x6c` | 729 | `cmp	w2, #0x1` |
| `0x4e57ec` | `+0xbc` | 543 | `cmp	w1, w2` |
| `0x4e57a4` | `+0x74` | 234 | `ldr	w2, [x0, #72]` |
| `0x4e5798` | `+0x68` | 211 | `ldrb	w2, [x0, #160]` |
| `0x4e57f4` | `+0xc4` | 205 | `ldr	x3, [x20, #8]` |
| `0x4e575c` | `+0x2c` | 189 | `ldp	x29, x30, [sp], #32` |
| `0x4e57bc` | `+0x8c` | 114 | `stp	x19, x20, [sp, #16]` |
| `0x4e57d0` | `+0xa0` | 94 | `add	x0, x0, x1` |
| `0x4e57a8` | `+0x78` | 75 | `cmp	w2, #0x6` |
| `0x4e5734` | `+0x4` | 68 | `adrp	x2, 2e38000 <trace_init_lock+0x28>` |
| `0x4e5770` | `+0x40` | 68 | `ldr	x3, [x2, #8]` |
| `0x4e5814` | `+0xe4` | 67 | `csetm	x3, ne	// ne = any` |

## percpu_array_lookup-2

5461 top-frame samples; CPU 5; sampling period 1572863 cycles.

| Instruction region | Samples | Share of all target samples |
| --- | ---: | ---: |
| manager_fd_type_resolution | 1894 | 34.68% |
| cpu_query_libc | 1016 | 18.60% |
| key_bounds | 649 | 11.88% |
| data_pointer_and_address | 580 | 10.62% |
| entry_exit_registers | 534 | 9.78% |
| anonymous_JIT_or_trampoline | 366 | 6.70% |
| map_impl_offset_pointer | 182 | 3.33% |
| other_runtime_or_system | 102 | 1.87% |
| cpu_query_plt | 95 | 1.74% |
| cpu_query_callsite | 43 | 0.79% |

Hottest helper PCs (ELF addresses; ARM sampling may skid):

| PC | Helper offset | Samples | Instruction |
| --- | --- | ---: | --- |
| `0x4e579c` | `+0x6c` | 813 | `cmp	w2, #0x1` |
| `0x4e57ec` | `+0xbc` | 545 | `cmp	w1, w2` |
| `0x4e57a4` | `+0x74` | 275 | `ldr	w2, [x0, #72]` |
| `0x4e57f4` | `+0xc4` | 262 | `ldr	x3, [x20, #8]` |
| `0x4e5798` | `+0x68` | 257 | `ldrb	w2, [x0, #160]` |
| `0x4e575c` | `+0x2c` | 225 | `ldp	x29, x30, [sp], #32` |
| `0x4e57bc` | `+0x8c` | 107 | `stp	x19, x20, [sp, #16]` |
| `0x4e57d0` | `+0xa0` | 104 | `add	x0, x0, x1` |
| `0x4e57a8` | `+0x78` | 79 | `cmp	w2, #0x6` |
| `0x4e5760` | `+0x30` | 77 | `ret` |
| `0x4e5814` | `+0xe4` | 68 | `csetm	x3, ne	// ne = any` |
| `0x4e5770` | `+0x40` | 64 | `ldr	x3, [x2, #8]` |

