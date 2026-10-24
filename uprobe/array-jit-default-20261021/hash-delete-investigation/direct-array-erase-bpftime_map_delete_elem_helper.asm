
/home/jetson/src/results/uprobe/array-jit-default-20261021/hash-delete-investigation/artifacts/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004f2004 <bpftime_map_delete_elem_helper>:
  4f2004:	sub	sp, sp, #0x1c0
  4f2008:	cmp	w0, #0x0
  4f200c:	adrp	x2, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f2010:	ldr	x2, [x2, #1160]
  4f2014:	stp	x29, x30, [sp, #352]
  4f2018:	add	x29, sp, #0x160
  4f201c:	stp	x21, x22, [sp, #384]
  4f2020:	adrp	x21, 2e58000 <_frida__dwarf_failsafe_error+0x8>
  4f2024:	add	x21, x21, #0x870
  4f2028:	stp	x19, x20, [sp, #368]
  4f202c:	stp	x23, x24, [sp, #400]
  4f2030:	stp	x25, x26, [sp, #416]
  4f2034:	stp	x27, x28, [sp, #432]
  4f2038:	ldr	x3, [x21, #72]
  4f203c:	ldr	x4, [x2]
  4f2040:	str	x4, [sp, #344]
  4f2044:	mov	x4, #0x0                   	// #0
  4f2048:	ccmp	x3, #0x0, #0x4, ge	// ge = tcont
  4f204c:	b.ne	4f20a0 <bpftime_map_delete_elem_helper+0x9c>  // b.any
  4f2050:	bl	48c090 <__errno_location@plt>
  4f2054:	mov	x20, #0xffffffffffffffff    	// #-1
  4f2058:	mov	w1, #0x2                   	// #2
  4f205c:	str	w1, [x0]
  4f2060:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f2064:	ldr	x0, [x0, #1160]
  4f2068:	ldr	x2, [sp, #344]
  4f206c:	ldr	x1, [x0]
  4f2070:	subs	x2, x2, x1
  4f2074:	mov	x1, #0x0                   	// #0
  4f2078:	b.ne	4f4344 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f207c:	ldp	x29, x30, [sp, #352]
  4f2080:	mov	x0, x20
  4f2084:	ldp	x19, x20, [sp, #368]
  4f2088:	ldp	x21, x22, [sp, #384]
  4f208c:	ldp	x23, x24, [sp, #400]
  4f2090:	ldp	x25, x26, [sp, #416]
  4f2094:	ldp	x27, x28, [sp, #432]
  4f2098:	add	sp, sp, #0x1c0
  4f209c:	ret
  4f20a0:	mov	x19, x0
  4f20a4:	ldr	x0, [x3, #16]
  4f20a8:	cmp	x0, w19, sxtw
  4f20ac:	b.ls	4f2050 <bpftime_map_delete_elem_helper+0x4c>  // b.plast
  4f20b0:	mov	x20, x1
  4f20b4:	mov	x22, x1
  4f20b8:	mov	x0, x3
  4f20bc:	mov	w1, w19
  4f20c0:	bl	4dca20 <_ZNK7bpftime15handler_manager11get_handlerEi>
  4f20c4:	ldrb	w0, [x0, #160]
  4f20c8:	cmp	w0, #0x1
  4f20cc:	b.ne	4f2050 <bpftime_map_delete_elem_helper+0x4c>  // b.any
  4f20d0:	ldr	x0, [x21, #72]
  4f20d4:	mov	w1, w19
  4f20d8:	bl	4dca20 <_ZNK7bpftime15handler_manager11get_handlerEi>
  4f20dc:	mov	x19, x0
  4f20e0:	ldrb	w0, [x0, #160]
  4f20e4:	cmp	w0, #0x1
  4f20e8:	b.ne	4f4480 <bpftime_map_delete_elem_helper+0x247c>  // b.any
  4f20ec:	ldr	w0, [x19, #72]
  4f20f0:	cmp	w0, #0x1e
  4f20f4:	b.gt	4f218c <bpftime_map_delete_elem_helper+0x188>
  4f20f8:	cmp	w0, #0x0
  4f20fc:	b.le	4f210c <bpftime_map_delete_elem_helper+0x108>
  4f2100:	sub	w1, w0, #0x1
  4f2104:	cmp	w1, #0x1d
  4f2108:	b.ls	4f2174 <bpftime_map_delete_elem_helper+0x170>  // b.plast
  4f210c:	sbfiz	x2, x0, #1, #32
  4f2110:	adrp	x1, 1d13000 <_ZTSN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0xd0>
  4f2114:	add	x0, x2, w0, sxtw
  4f2118:	add	x1, x1, #0xd70
  4f211c:	add	x0, x1, x0, lsl #4
  4f2120:	ldr	x3, [x0, #32]
  4f2124:	cbz	x3, 4f36f8 <bpftime_map_delete_elem_helper+0x16f4>
  4f2128:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f212c:	ldr	x0, [x0, #1160]
  4f2130:	ldr	x2, [sp, #344]
  4f2134:	ldr	x1, [x0]
  4f2138:	subs	x2, x2, x1
  4f213c:	mov	x1, #0x0                   	// #0
  4f2140:	b.ne	4f4344 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f2144:	ldp	x29, x30, [sp, #352]
  4f2148:	mov	x1, x20
  4f214c:	ldp	x21, x22, [sp, #384]
  4f2150:	mov	x16, x3
  4f2154:	ldp	x23, x24, [sp, #400]
  4f2158:	mov	w2, #0x0                   	// #0
  4f215c:	ldp	x25, x26, [sp, #416]
  4f2160:	ldp	x27, x28, [sp, #432]
  4f2164:	ldr	w0, [x19, #112]
  4f2168:	ldp	x19, x20, [sp, #368]
  4f216c:	add	sp, sp, #0x1c0
  4f2170:	br	x16
  4f2174:	adrp	x2, 1d2c000 <_ZTSN5boost12interprocess28interprocess_recursive_mutexE+0x290>
  4f2178:	add	x2, x2, #0x370
  4f217c:	ldrh	w2, [x2, w1, uxtw #1]
  4f2180:	adr	x1, 4f218c <bpftime_map_delete_elem_helper+0x188>
  4f2184:	add	x2, x1, w2, sxth #2
  4f2188:	br	x2
  4f218c:	cmp	w0, #0x3ec
  4f2190:	b.eq	4f2210 <bpftime_map_delete_elem_helper+0x20c>  // b.none
  4f2194:	b.gt	4f2c74 <bpftime_map_delete_elem_helper+0xc70>
  4f2198:	cmp	w0, #0x3e9
  4f219c:	b.eq	4f2ca8 <bpftime_map_delete_elem_helper+0xca4>  // b.none
  4f21a0:	cmp	w0, #0x3ea
  4f21a4:	b.ne	4f210c <bpftime_map_delete_elem_helper+0x108>  // b.any
  4f21a8:	ldr	x0, [x19, #120]
  4f21ac:	add	x19, x19, #0x78
  4f21b0:	cmp	x0, #0x1
  4f21b4:	add	x0, x19, x0
  4f21b8:	csetm	x19, ne	// ne = any
  4f21bc:	and	x19, x19, x0
  4f21c0:	ldrb	w0, [x19, #48]
  4f21c4:	tbnz	w0, #0, 4f2cfc <bpftime_map_delete_elem_helper+0xcf8>
  4f21c8:	ldr	w0, [x19, #36]
  4f21cc:	ldr	w3, [x20]
  4f21d0:	cmp	w3, w0
  4f21d4:	b.cs	4f2ce8 <bpftime_map_delete_elem_helper+0xce4>  // b.hs, b.nlast
  4f21d8:	ldr	w5, [x19, #32]
  4f21dc:	add	x4, x19, #0x8
  4f21e0:	ldr	x0, [x19, #8]
  4f21e4:	mov	w2, w5
  4f21e8:	mov	w1, #0x0                   	// #0
  4f21ec:	mov	x20, #0x0                   	// #0
  4f21f0:	mul	w3, w3, w5
  4f21f4:	add	x4, x4, x0
  4f21f8:	cmp	x0, #0x1
  4f21fc:	csetm	x0, ne	// ne = any
  4f2200:	and	x0, x0, x4
  4f2204:	add	x0, x0, x3
  4f2208:	bl	48bc60 <memset@plt>
  4f220c:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2210:	ldr	x0, [x19, #120]
  4f2214:	add	x19, x19, #0x78
  4f2218:	ldr	w1, [x20]
  4f221c:	cmp	x0, #0x1
  4f2220:	add	x19, x19, x0
  4f2224:	csetm	x0, ne	// ne = any
  4f2228:	and	x0, x0, x19
  4f222c:	ldr	w0, [x0, #12]
  4f2230:	str	w1, [sp, #92]
  4f2234:	tbnz	w1, #31, 4f42c4 <bpftime_map_delete_elem_helper+0x22c0>
  4f2238:	mov	w0, w0
  4f223c:	cmp	x0, w1, sxtw
  4f2240:	b.ls	4f42c4 <bpftime_map_delete_elem_helper+0x22c0>  // b.plast
  4f2244:	bl	506114 <_ZN6spdlog18default_logger_rawEv>
  4f2248:	mov	x20, #0x0                   	// #0
  4f224c:	mov	w3, #0xca                  	// #202
  4f2250:	adrp	x2, 1d3d000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe778>
  4f2254:	add	x2, x2, #0xfa8
  4f2258:	str	x2, [sp, #96]
  4f225c:	str	w3, [sp, #104]
  4f2260:	add	x1, sp, #0x30
  4f2264:	adrp	x6, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f2268:	add	x6, x6, #0xed0
  4f226c:	ldp	x8, x9, [sp, #96]
  4f2270:	stp	x8, x9, [sp, #48]
  4f2274:	add	x5, sp, #0x5c
  4f2278:	str	x6, [sp, #112]
  4f227c:	mov	x4, #0x43                  	// #67
  4f2280:	str	x6, [x1, #16]
  4f2284:	adrp	x3, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f2288:	mov	w2, #0x2                   	// #2
  4f228c:	add	x3, x3, #0xf28
  4f2290:	bl	4c88f0 <_ZN6spdlog6logger4log_IJRKiEEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_>
  4f2294:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2298:	bl	48c090 <__errno_location@plt>
  4f229c:	mov	w1, #0x16                  	// #22
  4f22a0:	str	w1, [x0]
  4f22a4:	mov	x20, #0xffffffffffffffff    	// #-1
  4f22a8:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f22ac:	ldr	x0, [x19, #120]
  4f22b0:	add	x19, x19, #0x78
  4f22b4:	ldr	w1, [x20]
  4f22b8:	cmp	x0, #0x1
  4f22bc:	add	x19, x19, x0
  4f22c0:	csetm	x0, ne	// ne = any
  4f22c4:	and	x0, x0, x19
  4f22c8:	tbnz	w1, #31, 4f2298 <bpftime_map_delete_elem_helper+0x294>
  4f22cc:	ldr	x2, [x0, #16]
  4f22d0:	sxtw	x1, w1
  4f22d4:	cmp	x1, x2
  4f22d8:	b.cs	4f2298 <bpftime_map_delete_elem_helper+0x294>  // b.hs, b.nlast
  4f22dc:	ldr	x2, [x0, #8]
  4f22e0:	add	x0, x0, #0x8
  4f22e4:	mov	w3, #0xffffffff            	// #-1
  4f22e8:	mov	x20, #0x0                   	// #0
  4f22ec:	cmp	x2, #0x1
  4f22f0:	add	x0, x0, x2
  4f22f4:	csetm	x2, ne	// ne = any
  4f22f8:	and	x0, x2, x0
  4f22fc:	str	w3, [x0, x1, lsl #2]
  4f2300:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2304:	bl	506114 <_ZN6spdlog18default_logger_rawEv>
  4f2308:	mov	x20, #0xffffffffffffffff    	// #-1
  4f230c:	mov	w3, #0x7e                  	// #126
  4f2310:	adrp	x2, 1d3d000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe778>
  4f2314:	add	x2, x2, #0xc00
  4f2318:	str	x2, [sp, #96]
  4f231c:	str	w3, [sp, #104]
  4f2320:	add	x1, sp, #0x30
  4f2324:	adrp	x5, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f2328:	add	x5, x5, #0xed0
  4f232c:	ldp	x6, x7, [sp, #96]
  4f2330:	stp	x6, x7, [sp, #48]
  4f2334:	adrp	x3, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f2338:	str	x5, [sp, #112]
  4f233c:	add	x3, x3, #0xee0
  4f2340:	str	x5, [x1, #16]
  4f2344:	mov	x4, #0x41                  	// #65
  4f2348:	mov	w2, #0x4                   	// #4
  4f234c:	bl	4b30e0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  4f2350:	bl	48c090 <__errno_location@plt>
  4f2354:	mov	w1, #0x5f                  	// #95
  4f2358:	str	w1, [x0]
  4f235c:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2360:	ldr	x0, [x19, #120]
  4f2364:	add	x1, x19, #0x78
  4f2368:	add	x19, x19, #0x74
  4f236c:	cmp	x0, #0x1
  4f2370:	add	x1, x1, x0
  4f2374:	csetm	x21, ne	// ne = any
  4f2378:	mov	x0, x19
  4f237c:	and	x21, x21, x1
  4f2380:	add	x20, x21, #0x8
  4f2384:	bl	48cea0 <pthread_spin_lock@plt>
  4f2388:	ldr	x9, [x20, #16]
  4f238c:	cbz	x9, 4f27e0 <bpftime_map_delete_elem_helper+0x7dc>
  4f2390:	ldr	x10, [x21, #16]
  4f2394:	add	x5, sp, #0x140
  4f2398:	add	x11, x20, #0x8
  4f239c:	add	x2, sp, #0x110
  4f23a0:	cmp	x10, #0x1
  4f23a4:	sub	x0, x11, x5
  4f23a8:	csetm	x8, ne	// ne = any
  4f23ac:	ldr	w3, [x21]
  4f23b0:	and	x0, x8, x0
  4f23b4:	sub	x6, x5, x2
  4f23b8:	add	x0, x0, x10
  4f23bc:	sub	x3, x9, x3
  4f23c0:	cmp	x0, #0x1
  4f23c4:	add	x7, sp, #0xf8
  4f23c8:	csetm	x1, ne	// ne = any
  4f23cc:	sub	x12, x2, x7
  4f23d0:	and	x6, x1, x6
  4f23d4:	add	x4, sp, #0x128
  4f23d8:	add	x6, x6, x0
  4f23dc:	sub	x2, x5, x4
  4f23e0:	add	x6, x6, x3
  4f23e4:	and	x1, x1, x2
  4f23e8:	cmp	x6, #0x1
  4f23ec:	add	x0, x0, x9
  4f23f0:	csetm	x3, ne	// ne = any
  4f23f4:	add	x0, x1, x0
  4f23f8:	and	x3, x3, x12
  4f23fc:	add	x2, sp, #0xe0
  4f2400:	add	x3, x3, x6
  4f2404:	mov	x1, x2
  4f2408:	cmp	x3, #0x1
  4f240c:	sub	x2, x7, x2
  4f2410:	sub	x7, x4, x5
  4f2414:	csetm	x4, ne	// ne = any
  4f2418:	cmp	x0, #0x1
  4f241c:	and	x4, x4, x2
  4f2420:	csetm	x2, ne	// ne = any
  4f2424:	add	x4, x4, x3
  4f2428:	and	x2, x2, x7
  4f242c:	cmp	x4, #0x1
  4f2430:	add	x2, x2, x0
  4f2434:	csetm	x7, ne	// ne = any
  4f2438:	cmp	x2, #0x1
  4f243c:	add	x1, x1, x4
  4f2440:	add	x5, x5, x2
  4f2444:	csetm	x12, ne	// ne = any
  4f2448:	and	x7, x7, x1
  4f244c:	and	x1, x12, x5
  4f2450:	str	x4, [sp, #224]
  4f2454:	str	x3, [sp, #248]
  4f2458:	str	x6, [sp, #272]
  4f245c:	str	x0, [sp, #296]
  4f2460:	str	x2, [sp, #320]
  4f2464:	cmp	x7, x1
  4f2468:	b.eq	4f24ac <bpftime_map_delete_elem_helper+0x4a8>  // b.none
  4f246c:	add	x10, x10, x11
  4f2470:	cmp	x1, #0x0
  4f2474:	and	x8, x8, x10
  4f2478:	add	x21, x8, x9
  4f247c:	ccmp	x1, x21, #0x4, ne	// ne = any
  4f2480:	ccmp	x7, #0x0, #0x4, ne	// ne = any
  4f2484:	b.eq	4f24a0 <bpftime_map_delete_elem_helper+0x49c>  // b.none
  4f2488:	sub	x22, x21, x1
  4f248c:	mov	x0, x7
  4f2490:	mov	x2, x22
  4f2494:	bl	48b570 <memmove@plt>
  4f2498:	ldr	x9, [x20, #16]
  4f249c:	add	x7, x0, x22
  4f24a0:	sub	x21, x21, x7
  4f24a4:	sub	x9, x9, x21
  4f24a8:	str	x9, [x20, #16]
  4f24ac:	mov	x20, #0x0                   	// #0
  4f24b0:	mov	x0, x19
  4f24b4:	bl	48c720 <pthread_spin_unlock@plt>
  4f24b8:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f24bc:	ldr	x0, [x19, #120]
  4f24c0:	add	x1, x19, #0x78
  4f24c4:	add	x19, x19, #0x74
  4f24c8:	cmp	x0, #0x1
  4f24cc:	add	x1, x1, x0
  4f24d0:	csetm	x28, ne	// ne = any
  4f24d4:	mov	x0, x19
  4f24d8:	and	x28, x28, x1
  4f24dc:	bl	48cea0 <pthread_spin_lock@plt>
  4f24e0:	ldr	x27, [x28]
  4f24e4:	cmp	x27, #0x4
  4f24e8:	b.eq	4f36c0 <bpftime_map_delete_elem_helper+0x16bc>  // b.none
  4f24ec:	add	x2, x20, x27
  4f24f0:	mov	x0, #0x0                   	// #0
  4f24f4:	cbz	x27, 4f4084 <bpftime_map_delete_elem_helper+0x2080>
  4f24f8:	ldrb	w1, [x20], #1
  4f24fc:	lsl	x3, x0, #5
  4f2500:	sub	x0, x3, x0
  4f2504:	add	x0, x1, x0
  4f2508:	cmp	x2, x20
  4f250c:	b.ne	4f24f8 <bpftime_map_delete_elem_helper+0x4f4>  // b.any
  4f2510:	mov	x2, x0
  4f2514:	ldr	x23, [x28, #24]
  4f2518:	add	x1, x28, #0x38
  4f251c:	ldp	x25, x26, [x28, #8]
  4f2520:	umulh	x23, x23, x2
  4f2524:	ldr	x2, [x28, #56]
  4f2528:	msub	x23, x26, x23, x0
  4f252c:	mov	x0, x1
  4f2530:	add	x25, x27, x25
  4f2534:	add	x0, x0, x2
  4f2538:	cmp	x26, x23
  4f253c:	sub	x1, x23, x26
  4f2540:	csel	x23, x1, x23, ls	// ls = plast
  4f2544:	cmp	x2, #0x1
  4f2548:	csetm	x24, ne	// ne = any
  4f254c:	mov	x20, x23
  4f2550:	add	x25, x25, #0x4
  4f2554:	and	x24, x24, x0
  4f2558:	b	4f2570 <bpftime_map_delete_elem_helper+0x56c>
  4f255c:	add	x20, x20, #0x1
  4f2560:	cmp	x26, x20
  4f2564:	csel	x20, x20, xzr, ne	// ne = any
  4f2568:	cmp	x20, x23
  4f256c:	b.eq	4f25b8 <bpftime_map_delete_elem_helper+0x5b4>  // b.none
  4f2570:	mul	x21, x20, x25
  4f2574:	ldr	w0, [x24, x21]
  4f2578:	cbz	w0, 4f25b8 <bpftime_map_delete_elem_helper+0x5b4>
  4f257c:	cmp	w0, #0x1
  4f2580:	b.ne	4f255c <bpftime_map_delete_elem_helper+0x558>  // b.any
  4f2584:	add	x1, x21, #0x4
  4f2588:	add	x0, x24, x1
  4f258c:	cmp	x27, #0x4
  4f2590:	b.eq	4f3754 <bpftime_map_delete_elem_helper+0x1750>  // b.none
  4f2594:	mov	x2, x27
  4f2598:	mov	x1, x22
  4f259c:	bl	48d570 <memcmp@plt>
  4f25a0:	cbnz	w0, 4f255c <bpftime_map_delete_elem_helper+0x558>
  4f25a4:	ldr	x0, [x28, #40]
  4f25a8:	mov	w1, #0x2                   	// #2
  4f25ac:	str	w1, [x24, x21]
  4f25b0:	sub	x0, x0, #0x1
  4f25b4:	str	x0, [x28, #40]
  4f25b8:	mov	x0, x19
  4f25bc:	mov	x20, #0x0                   	// #0
  4f25c0:	bl	48c720 <pthread_spin_unlock@plt>
  4f25c4:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f25c8:	ldr	x0, [x19, #120]
  4f25cc:	add	x1, x19, #0x78
  4f25d0:	add	x19, x19, #0x74
  4f25d4:	cmp	x0, #0x1
  4f25d8:	add	x1, x1, x0
  4f25dc:	csetm	x21, ne	// ne = any
  4f25e0:	mov	x0, x19
  4f25e4:	and	x21, x21, x1
  4f25e8:	bl	48cea0 <pthread_spin_lock@plt>
  4f25ec:	ldr	w0, [x21, #48]
  4f25f0:	cbz	w0, 4f27e0 <bpftime_map_delete_elem_helper+0x7dc>
  4f25f4:	ldr	w1, [x21, #40]
  4f25f8:	sub	w0, w0, #0x1
  4f25fc:	ldr	w2, [x21, #4]
  4f2600:	mov	x20, #0x0                   	// #0
  4f2604:	add	w1, w1, #0x1
  4f2608:	str	w0, [x21, #48]
  4f260c:	udiv	w0, w1, w2
  4f2610:	msub	w0, w0, w2, w1
  4f2614:	str	w0, [x21, #40]
  4f2618:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f261c:	ldr	x0, [x19, #120]
  4f2620:	add	x1, x19, #0x78
  4f2624:	add	x19, x19, #0x74
  4f2628:	cmp	x0, #0x1
  4f262c:	add	x1, x1, x0
  4f2630:	csetm	x22, ne	// ne = any
  4f2634:	mov	x0, x19
  4f2638:	and	x22, x22, x1
  4f263c:	add	x24, x22, #0x38
  4f2640:	bl	48cea0 <pthread_spin_lock@plt>
  4f2644:	mov	x0, x24
  4f2648:	bl	48cea0 <pthread_spin_lock@plt>
  4f264c:	cbz	x20, 4f4404 <bpftime_map_delete_elem_helper+0x2400>
  4f2650:	ldr	w23, [x20]
  4f2654:	ldr	w0, [x22, #16]
  4f2658:	cmp	w23, w0
  4f265c:	b.hi	4f4348 <bpftime_map_delete_elem_helper+0x2344>  // b.pmore
  4f2660:	ldr	x28, [x22, #32]
  4f2664:	add	x25, x22, #0x20
  4f2668:	add	x21, sp, #0x140
  4f266c:	cmp	x28, #0x1
  4f2670:	sub	x1, x25, x21
  4f2674:	csetm	x26, ne	// ne = any
  4f2678:	and	x0, x26, x1
  4f267c:	add	x0, x0, x28
  4f2680:	cmp	x0, #0x1
  4f2684:	b.ne	4f2748 <bpftime_map_delete_elem_helper+0x744>  // b.any
  4f2688:	mov	x27, #0x0                   	// #0
  4f268c:	nop
  4f2690:	sub	x0, x25, x21
  4f2694:	and	x0, x0, x26
  4f2698:	add	x0, x0, x28
  4f269c:	cmp	x0, #0x1
  4f26a0:	b.eq	4f408c <bpftime_map_delete_elem_helper+0x2088>  // b.none
  4f26a4:	ldr	w0, [x27, #32]
  4f26a8:	cmp	w23, w0
  4f26ac:	b.ne	4f408c <bpftime_map_delete_elem_helper+0x2088>  // b.any
  4f26b0:	mov	x2, x20
  4f26b4:	mov	x1, x27
  4f26b8:	mov	x0, x22
  4f26bc:	bl	505d80 <_ZNK7bpftime17lpm_trie_map_impl20longest_prefix_matchEPKNS_13lpm_trie_nodeEPKNS_16bpf_lpm_trie_keyE>
  4f26c0:	cmp	x0, w23, uxtw
  4f26c4:	b.ne	4f408c <bpftime_map_delete_elem_helper+0x2088>  // b.any
  4f26c8:	ldrb	w0, [x27, #40]
  4f26cc:	tbnz	w0, #0, 4f408c <bpftime_map_delete_elem_helper+0x2088>
  4f26d0:	ldr	w1, [x22, #12]
  4f26d4:	mov	w0, #0x1                   	// #1
  4f26d8:	strb	w0, [x27, #40]
  4f26dc:	add	x0, x27, #0x30
  4f26e0:	bl	54f930 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvE11priv_resizeINS0_12value_init_tENS_11move_detail17integral_constantIjLj2EEEEEvmRKT_T0_.isra.0>
  4f26e4:	ldr	x0, [x22, #40]
  4f26e8:	cbz	x0, 4f26f4 <bpftime_map_delete_elem_helper+0x6f0>
  4f26ec:	sub	x0, x0, #0x1
  4f26f0:	str	x0, [x22, #40]
  4f26f4:	mov	x20, #0x0                   	// #0
  4f26f8:	mov	x0, x24
  4f26fc:	bl	48c720 <pthread_spin_unlock@plt>
  4f2700:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f2704:	add	x2, x20, x2, lsr #3
  4f2708:	mvn	w1, w1
  4f270c:	and	w1, w1, #0x7
  4f2710:	ldrb	w0, [x2, #4]
  4f2714:	asr	w0, w0, w1
  4f2718:	ubfiz	x0, x0, #4, #1
  4f271c:	add	x25, x0, #0x8
  4f2720:	add	x0, x27, x0
  4f2724:	ldr	x28, [x0, #8]
  4f2728:	add	x25, x27, x25
  4f272c:	sub	x1, x25, x21
  4f2730:	cmp	x28, #0x1
  4f2734:	csetm	x26, ne	// ne = any
  4f2738:	and	x0, x26, x1
  4f273c:	add	x0, x0, x28
  4f2740:	cmp	x0, #0x1
  4f2744:	b.eq	4f408c <bpftime_map_delete_elem_helper+0x2088>  // b.none
  4f2748:	and	x1, x1, x26
  4f274c:	mov	x2, x20
  4f2750:	add	x1, x1, x28
  4f2754:	mov	x0, x22
  4f2758:	cmp	x1, #0x1
  4f275c:	add	x1, x21, x1
  4f2760:	csetm	x27, ne	// ne = any
  4f2764:	and	x27, x27, x1
  4f2768:	mov	x1, x27
  4f276c:	bl	505d80 <_ZNK7bpftime17lpm_trie_map_impl20longest_prefix_matchEPKNS_13lpm_trie_nodeEPKNS_16bpf_lpm_trie_keyE>
  4f2770:	ldr	w1, [x27, #32]
  4f2774:	mov	w2, w1
  4f2778:	cmp	x0, x2
  4f277c:	ccmp	w23, w1, #0x4, eq	// eq = none
  4f2780:	b.eq	4f2690 <bpftime_map_delete_elem_helper+0x68c>  // b.none
  4f2784:	ldr	w0, [x22, #12]
  4f2788:	cmp	x2, x0, lsl #3
  4f278c:	b.cc	4f2704 <bpftime_map_delete_elem_helper+0x700>  // b.lo, b.ul, b.last
  4f2790:	mov	x0, x27
  4f2794:	mov	x25, #0x8                   	// #8
  4f2798:	b	4f2724 <bpftime_map_delete_elem_helper+0x720>
  4f279c:	ldr	x0, [x19, #120]
  4f27a0:	add	x1, x19, #0x78
  4f27a4:	add	x19, x19, #0x74
  4f27a8:	cmp	x0, #0x1
  4f27ac:	add	x1, x1, x0
  4f27b0:	csetm	x22, ne	// ne = any
  4f27b4:	mov	x0, x19
  4f27b8:	and	x22, x22, x1
  4f27bc:	bl	48cea0 <pthread_spin_lock@plt>
  4f27c0:	add	x21, x22, #0x80
  4f27c4:	mov	x1, x20
  4f27c8:	mov	x0, x21
  4f27cc:	ldr	x2, [x22, #104]
  4f27d0:	add	x2, x20, x2
  4f27d4:	bl	53a174 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvE6assignIPhEEvT_SG_PNS_11move_detail13disable_if_orIvNSH_7is_sameINSH_17integral_constantIjLj2EEENSK_IjLj0EEEEENSH_14is_convertibleISG_mEENS0_3dtl17is_input_iteratorISG_XsrNSQ_21has_iterator_categoryISG_EE5valueEEENSH_5bool_ILb0EEEE4typeE.constprop.0>
  4f27d8:	ldr	x0, [x22, #8]
  4f27dc:	cbnz	x0, 4f2d40 <bpftime_map_delete_elem_helper+0xd3c>
  4f27e0:	bl	48c090 <__errno_location@plt>
  4f27e4:	mov	x20, #0xffffffffffffffff    	// #-1
  4f27e8:	mov	w1, #0x2                   	// #2
  4f27ec:	str	w1, [x0]
  4f27f0:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f27f4:	ldr	x0, [x19, #120]
  4f27f8:	add	x1, x19, #0x78
  4f27fc:	add	x19, x19, #0x74
  4f2800:	cmp	x0, #0x1
  4f2804:	add	x1, x1, x0
  4f2808:	csetm	x21, ne	// ne = any
  4f280c:	mov	x0, x19
  4f2810:	and	x21, x21, x1
  4f2814:	bl	48cea0 <pthread_spin_lock@plt>
  4f2818:	cbz	x20, 4f43a8 <bpftime_map_delete_elem_helper+0x23a4>
  4f281c:	ldr	x0, [x21, #24]
  4f2820:	add	x22, x21, #0x10
  4f2824:	ldr	w20, [x20]
  4f2828:	cbnz	x0, 4f326c <bpftime_map_delete_elem_helper+0x1268>
  4f282c:	bl	48c090 <__errno_location@plt>
  4f2830:	mov	x20, #0xffffffffffffffff    	// #-1
  4f2834:	mov	w1, #0x2                   	// #2
  4f2838:	str	w1, [x0]
  4f283c:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f2840:	add	x19, x19, #0x74
  4f2844:	mov	x20, #0xffffffffffffffff    	// #-1
  4f2848:	mov	x0, x19
  4f284c:	bl	48cea0 <pthread_spin_lock@plt>
  4f2850:	bl	48c090 <__errno_location@plt>
  4f2854:	mov	x1, x0
  4f2858:	mov	w2, #0x5f                  	// #95
  4f285c:	mov	x0, x19
  4f2860:	str	w2, [x1]
  4f2864:	bl	48c720 <pthread_spin_unlock@plt>
  4f2868:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f286c:	ldr	x0, [x19, #120]
  4f2870:	add	x19, x19, #0x78
  4f2874:	cmp	x0, #0x1
  4f2878:	add	x0, x19, x0
  4f287c:	csetm	x19, ne	// ne = any
  4f2880:	and	x19, x19, x0
  4f2884:	bl	48bb80 <sched_getcpu@plt>
  4f2888:	ldr	w2, [x19, #144]
  4f288c:	cmp	w2, #0x4
  4f2890:	b.eq	4f2cd8 <bpftime_map_delete_elem_helper+0xcd4>  // b.none
  4f2894:	ldr	x1, [x19, #200]
  4f2898:	add	x4, x19, #0xc8
  4f289c:	sbfiz	x3, x0, #5, #32
  4f28a0:	mov	w2, w2
  4f28a4:	cmp	x1, #0x1
  4f28a8:	add	x4, x4, x1
  4f28ac:	csetm	x0, ne	// ne = any
  4f28b0:	mov	x1, x20
  4f28b4:	and	x20, x0, x4
  4f28b8:	add	x20, x20, x3
  4f28bc:	add	x0, x20, #0x8
  4f28c0:	ldr	x3, [x20, #8]
  4f28c4:	cmp	x3, #0x1
  4f28c8:	add	x0, x0, x3
  4f28cc:	csetm	x3, ne	// ne = any
  4f28d0:	and	x0, x3, x0
  4f28d4:	bl	48c120 <memcpy@plt>
  4f28d8:	add	x8, sp, #0xe0
  4f28dc:	mov	x1, x20
  4f28e0:	mov	x0, x19
  4f28e4:	mov	x20, x8
  4f28e8:	bl	4fb144 <_ZNK5boost9unordered6detail5tableINS1_3mapINS_12interprocess9allocatorISt4pairIKNS_9container6vectorIhNS5_IhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEESI_ESG_EESI_SI_N7bpftime24per_cpu_bytes_vec_hasherENSM_23per_cpu_bytes_vec_equalEEEE4findISI_EENS1_15iterator_detail8iteratorINS1_4nodeISK_SD_EENS1_6bucketISV_SD_EEEERKT_>
  4f28ec:	ldr	x0, [sp, #224]
  4f28f0:	cmp	x0, #0x1
  4f28f4:	add	x20, x20, x0
  4f28f8:	csetm	x0, ne	// ne = any
  4f28fc:	tst	x0, x20
  4f2900:	b.eq	4f2ce8 <bpftime_map_delete_elem_helper+0xce4>  // b.none
  4f2904:	ldp	x3, x2, [sp, #232]
  4f2908:	add	x1, sp, #0xf8
  4f290c:	sub	x20, x20, x1
  4f2910:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f2914:	add	x21, sp, #0x128
  4f2918:	add	x6, sp, #0x140
  4f291c:	add	x23, sp, #0x130
  4f2920:	cmp	x3, #0x1
  4f2924:	csetm	x8, ne	// ne = any
  4f2928:	cmp	x2, #0x1
  4f292c:	csetm	x9, ne	// ne = any
  4f2930:	and	x8, x8, x0
  4f2934:	cmp	x20, #0x1
  4f2938:	add	x0, sp, #0x110
  4f293c:	add	x8, x8, x3
  4f2940:	sub	x1, x1, x0
  4f2944:	mov	x3, #0xffffffffffffffe8    	// #-24
  4f2948:	csetm	x7, ne	// ne = any
  4f294c:	and	x9, x9, x3
  4f2950:	cmp	x8, #0x1
  4f2954:	add	x9, x9, x2
  4f2958:	and	x7, x7, x1
  4f295c:	add	x7, x7, x20
  4f2960:	mov	x1, x3
  4f2964:	csetm	x5, ne	// ne = any
  4f2968:	cmp	x9, #0x1
  4f296c:	mov	x2, x1
  4f2970:	csetm	x3, ne	// ne = any
  4f2974:	and	x5, x5, x1
  4f2978:	cmp	x7, #0x1
  4f297c:	add	x5, x5, x8
  4f2980:	csetm	x1, ne	// ne = any
  4f2984:	and	x3, x3, x2
  4f2988:	sub	x0, x0, x21
  4f298c:	add	x3, x3, x9
  4f2990:	cmp	x5, #0x1
  4f2994:	and	x1, x1, x0
  4f2998:	csetm	x26, ne	// ne = any
  4f299c:	add	x24, x1, x7
  4f29a0:	cmp	x3, #0x1
  4f29a4:	csetm	x25, ne	// ne = any
  4f29a8:	and	x26, x26, x2
  4f29ac:	cmp	x24, #0x1
  4f29b0:	add	x26, x26, x5
  4f29b4:	csetm	x22, ne	// ne = any
  4f29b8:	and	x25, x25, x2
  4f29bc:	sub	x1, x21, x6
  4f29c0:	add	x25, x25, x3
  4f29c4:	cmp	x26, #0x1
  4f29c8:	and	x1, x22, x1
  4f29cc:	add	x1, x1, x24
  4f29d0:	csetm	x28, ne	// ne = any
  4f29d4:	cmp	x25, #0x1
  4f29d8:	add	x0, sp, #0x148
  4f29dc:	csetm	x27, ne	// ne = any
  4f29e0:	cmp	x1, #0x1
  4f29e4:	add	x2, x6, x1
  4f29e8:	csetm	x10, ne	// ne = any
  4f29ec:	sub	x4, x23, x0
  4f29f0:	and	x10, x10, x2
  4f29f4:	and	x4, x28, x4
  4f29f8:	mov	x2, #0xffffffffffffffe8    	// #-24
  4f29fc:	and	x2, x27, x2
  4f2a00:	add	x4, x4, x26
  4f2a04:	add	x2, x2, x25
  4f2a08:	stp	x20, x8, [sp, #248]
  4f2a0c:	sub	x11, x10, x6
  4f2a10:	stp	x9, x7, [sp, #264]
  4f2a14:	add	x20, sp, #0x138
  4f2a18:	stp	x5, x3, [sp, #280]
  4f2a1c:	stp	x24, x26, [sp, #296]
  4f2a20:	stp	x25, x1, [sp, #312]
  4f2a24:	stp	x4, x2, [sp, #328]
  4f2a28:	ldr	x1, [x10]
  4f2a2c:	cmp	x1, #0x1
  4f2a30:	csetm	x5, ne	// ne = any
  4f2a34:	and	x5, x5, x11
  4f2a38:	add	x5, x5, x1
  4f2a3c:	str	x5, [sp, #320]
  4f2a40:	cmp	x5, #0x1
  4f2a44:	b.eq	4f3a9c <bpftime_map_delete_elem_helper+0x1a98>  // b.none
  4f2a48:	add	x8, sp, #0xa0
  4f2a4c:	add	x9, sp, #0xa8
  4f2a50:	sub	x4, x23, x8
  4f2a54:	sub	x3, x20, x9
  4f2a58:	and	x4, x4, x28
  4f2a5c:	and	x3, x3, x27
  4f2a60:	add	x4, x4, x26
  4f2a64:	add	x3, x3, x25
  4f2a68:	cmp	x4, #0x1
  4f2a6c:	add	x6, x8, x4
  4f2a70:	csetm	x5, ne	// ne = any
  4f2a74:	stp	x4, x3, [sp, #160]
  4f2a78:	and	x6, x5, x6
  4f2a7c:	add	x1, x21, x24
  4f2a80:	mov	x7, x6
  4f2a84:	and	x2, x22, x1
  4f2a88:	ldr	x0, [x6]
  4f2a8c:	cmp	x0, #0x1
  4f2a90:	add	x1, x6, x0
  4f2a94:	csetm	x0, ne	// ne = any
  4f2a98:	and	x0, x0, x1
  4f2a9c:	cmp	x2, x0
  4f2aa0:	b.eq	4f2acc <bpftime_map_delete_elem_helper+0xac8>  // b.none
  4f2aa4:	nop
  4f2aa8:	ldr	x1, [x0]
  4f2aac:	mov	x6, x0
  4f2ab0:	mov	x7, x0
  4f2ab4:	cmp	x1, #0x1
  4f2ab8:	add	x1, x0, x1
  4f2abc:	csetm	x0, ne	// ne = any
  4f2ac0:	and	x0, x0, x1
  4f2ac4:	cmp	x2, x0
  4f2ac8:	b.ne	4f2aa8 <bpftime_map_delete_elem_helper+0xaa4>  // b.any
  4f2acc:	cmp	x3, #0x1
  4f2ad0:	add	x1, sp, #0xb8
  4f2ad4:	add	x26, sp, #0xb0
  4f2ad8:	csetm	x0, ne	// ne = any
  4f2adc:	sub	x9, x9, x1
  4f2ae0:	sub	x1, x8, x26
  4f2ae4:	and	x5, x5, x1
  4f2ae8:	and	x0, x0, x9
  4f2aec:	add	x0, x0, x3
  4f2af0:	add	x5, x5, x4
  4f2af4:	stp	x5, x0, [sp, #176]
  4f2af8:	add	x20, sp, #0xc0
  4f2afc:	add	x28, sp, #0x98
  4f2b00:	ldr	x0, [x6]
  4f2b04:	cmp	x0, #0x1
  4f2b08:	add	x0, x0, x7
  4f2b0c:	csetm	x1, ne	// ne = any
  4f2b10:	and	x1, x1, x0
  4f2b14:	sub	x0, x1, x7
  4f2b18:	ldr	x2, [x1]
  4f2b1c:	cmp	x2, #0x1
  4f2b20:	csetm	x1, ne	// ne = any
  4f2b24:	and	x0, x1, x0
  4f2b28:	add	x0, x0, x2
  4f2b2c:	str	x0, [x6]
  4f2b30:	ldr	x0, [sp, #176]
  4f2b34:	cmp	x0, #0x1
  4f2b38:	add	x0, x26, x0
  4f2b3c:	csetm	x1, ne	// ne = any
  4f2b40:	and	x0, x1, x0
  4f2b44:	ldr	x22, [x0]
  4f2b48:	cmp	x22, #0x1
  4f2b4c:	b.eq	4f3adc <bpftime_map_delete_elem_helper+0x1ad8>  // b.none
  4f2b50:	ldr	x3, [sp, #296]
  4f2b54:	sub	x0, x21, x28
  4f2b58:	add	x2, x19, #0x20
  4f2b5c:	cmp	x3, #0x1
  4f2b60:	csetm	x1, ne	// ne = any
  4f2b64:	and	x1, x1, x0
  4f2b68:	add	x1, x1, x3
  4f2b6c:	str	x1, [sp, #152]
  4f2b70:	add	x3, x28, x1
  4f2b74:	sub	x28, x28, x20
  4f2b78:	ldr	x0, [x19, #32]
  4f2b7c:	cmp	x0, #0x1
  4f2b80:	add	x2, x2, x0
  4f2b84:	csetm	x0, ne	// ne = any
  4f2b88:	and	x0, x0, x2
  4f2b8c:	cmp	x0, #0x0
  4f2b90:	sub	x0, x0, x26
  4f2b94:	csetm	x2, ne	// ne = any
  4f2b98:	cmp	x1, #0x1
  4f2b9c:	csetm	x1, ne	// ne = any
  4f2ba0:	sub	x0, x0, #0x1
  4f2ba4:	and	x1, x1, x3
  4f2ba8:	and	x2, x2, x0
  4f2bac:	add	x0, x1, #0x8
  4f2bb0:	add	x1, x2, #0x1
  4f2bb4:	sub	x2, x0, x20
  4f2bb8:	str	x1, [sp, #176]
  4f2bbc:	cmp	x2, #0x1
  4f2bc0:	csetm	x21, ne	// ne = any
  4f2bc4:	and	x21, x21, x0
  4f2bc8:	add	x0, x21, #0x20
  4f2bcc:	bl	4dcc50 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvED1Ev>
  4f2bd0:	mov	x0, x21
  4f2bd4:	bl	4dcc50 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvED1Ev>
  4f2bd8:	ldr	x3, [sp, #152]
  4f2bdc:	mov	x1, x20
  4f2be0:	mov	x0, x26
  4f2be4:	mov	x20, #0x0                   	// #0
  4f2be8:	cmp	x3, #0x1
  4f2bec:	csetm	x2, ne	// ne = any
  4f2bf0:	and	x2, x2, x28
  4f2bf4:	add	x2, x2, x3
  4f2bf8:	str	x2, [sp, #192]
  4f2bfc:	bl	54fca4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f2c00:	ldr	x0, [x19, #8]
  4f2c04:	sub	x0, x0, #0x1
  4f2c08:	str	x0, [x19, #8]
  4f2c0c:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2c10:	ldr	x0, [x19, #120]
  4f2c14:	add	x1, x19, #0x78
  4f2c18:	add	x19, x19, #0x74
  4f2c1c:	cmp	x0, #0x1
  4f2c20:	add	x1, x1, x0
  4f2c24:	csetm	x21, ne	// ne = any
  4f2c28:	mov	x0, x19
  4f2c2c:	and	x21, x21, x1
  4f2c30:	bl	48cea0 <pthread_spin_lock@plt>
  4f2c34:	ldr	w0, [x20]
  4f2c38:	tbnz	w0, #31, 4f42d8 <bpftime_map_delete_elem_helper+0x22d4>
  4f2c3c:	ldr	x1, [x21, #16]
  4f2c40:	sxtw	x0, w0
  4f2c44:	cmp	x0, x1
  4f2c48:	b.cs	4f42d8 <bpftime_map_delete_elem_helper+0x22d4>  // b.hs, b.nlast
  4f2c4c:	ldr	x1, [x21, #8]
  4f2c50:	add	x21, x21, #0x8
  4f2c54:	mov	w2, #0xffffffff            	// #-1
  4f2c58:	mov	x20, #0x0                   	// #0
  4f2c5c:	cmp	x1, #0x1
  4f2c60:	add	x21, x21, x1
  4f2c64:	csetm	x1, ne	// ne = any
  4f2c68:	and	x1, x1, x21
  4f2c6c:	str	w2, [x1, x0, lsl #2]
  4f2c70:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f2c74:	cmp	w0, #0x3ee
  4f2c78:	b.ne	4f210c <bpftime_map_delete_elem_helper+0x108>  // b.any
  4f2c7c:	ldr	x0, [x19, #120]
  4f2c80:	add	x19, x19, #0x78
  4f2c84:	mov	x1, x20
  4f2c88:	cmp	x0, #0x1
  4f2c8c:	add	x19, x19, x0
  4f2c90:	csetm	x0, ne	// ne = any
  4f2c94:	and	x0, x0, x19
  4f2c98:	ldr	w0, [x0, #8]
  4f2c9c:	bl	1b42ea0 <bpf_map_delete_elem>
  4f2ca0:	sxtw	x20, w0
  4f2ca4:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2ca8:	ldr	x0, [x19, #120]
  4f2cac:	add	x19, x19, #0x78
  4f2cb0:	cmp	x0, #0x1
  4f2cb4:	add	x0, x19, x0
  4f2cb8:	csetm	x19, ne	// ne = any
  4f2cbc:	and	x19, x19, x0
  4f2cc0:	ldr	w0, [x19, #76]
  4f2cc4:	tbnz	w0, #31, 4f36b0 <bpftime_map_delete_elem_helper+0x16ac>
  4f2cc8:	mov	x1, x20
  4f2ccc:	bl	1b42ea0 <bpf_map_delete_elem>
  4f2cd0:	sxtw	x20, w0
  4f2cd4:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2cd8:	ldr	x0, [x19, #80]
  4f2cdc:	add	x28, x19, #0x48
  4f2ce0:	ldr	w20, [x20]
  4f2ce4:	cbnz	x0, 4f3c00 <bpftime_map_delete_elem_helper+0x1bfc>
  4f2ce8:	bl	48c090 <__errno_location@plt>
  4f2cec:	mov	x20, #0xffffffffffffffff    	// #-1
  4f2cf0:	mov	w1, #0x2                   	// #2
  4f2cf4:	str	w1, [x0]
  4f2cf8:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2cfc:	mov	x0, x19
  4f2d00:	bl	509eb0 <_ZN7bpftime26array_map_kernel_user_impl11init_map_fdEv>
  4f2d04:	tbz	w0, #0, 4f22a4 <bpftime_map_delete_elem_helper+0x2a0>
  4f2d08:	ldr	x0, [x19, #56]
  4f2d0c:	cbz	x0, 4f41cc <bpftime_map_delete_elem_helper+0x21c8>
  4f2d10:	ldr	w1, [x19, #36]
  4f2d14:	ldr	w3, [x20]
  4f2d18:	cmp	w3, w1
  4f2d1c:	b.cs	4f2ce8 <bpftime_map_delete_elem_helper+0xce4>  // b.hs, b.nlast
  4f2d20:	ldr	w4, [x19, #32]
  4f2d24:	mov	w1, #0x0                   	// #0
  4f2d28:	mov	x20, #0x0                   	// #0
  4f2d2c:	mov	w2, w4
  4f2d30:	mul	w3, w3, w4
  4f2d34:	add	x0, x0, x3
  4f2d38:	bl	48bc60 <memset@plt>
  4f2d3c:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f2d40:	add	x20, x22, #0x20
  4f2d44:	mov	x0, x21
  4f2d48:	bl	549090 <_ZNK7bpftime16bytes_vec_hasherclERKN5boost9container6vectorIhNS1_12interprocess9allocatorIhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEE.isra.0>
  4f2d4c:	ldr	x1, [x20, #8]
  4f2d50:	bl	4fb0f0 <_ZN5boost9unordered6detail15prime_fmod_sizeIvE8positionEmm>
  4f2d54:	add	x23, sp, #0x128
  4f2d58:	mov	x1, x0
  4f2d5c:	mov	x8, x23
  4f2d60:	mov	x0, x20
  4f2d64:	bl	4e1e10 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EENS9_ISN_SH_EENS1_15prime_fmod_sizeIvEEE2atEm>
  4f2d68:	add	x20, sp, #0xc0
  4f2d6c:	ldr	x0, [sp, #296]
  4f2d70:	cmp	x0, #0x1
  4f2d74:	add	x1, x23, x0
  4f2d78:	csetm	x0, ne	// ne = any
  4f2d7c:	and	x0, x0, x1
  4f2d80:	sub	x1, x0, x20
  4f2d84:	ldr	x0, [x0]
  4f2d88:	cmp	x0, #0x1
  4f2d8c:	csetm	x27, ne	// ne = any
  4f2d90:	and	x27, x27, x1
  4f2d94:	add	x27, x27, x0
  4f2d98:	str	x27, [sp, #192]
  4f2d9c:	cmp	x27, #0x1
  4f2da0:	b.eq	4f27e0 <bpftime_map_delete_elem_helper+0x7dc>  // b.none
  4f2da4:	ldr	x4, [x21, #16]
  4f2da8:	b	4f2dd0 <bpftime_map_delete_elem_helper+0xdcc>
  4f2dac:	ldr	x0, [x28]
  4f2db0:	sub	x28, x28, x20
  4f2db4:	cmp	x0, #0x1
  4f2db8:	csetm	x27, ne	// ne = any
  4f2dbc:	and	x27, x27, x28
  4f2dc0:	add	x27, x27, x0
  4f2dc4:	str	x27, [sp, #192]
  4f2dc8:	cmp	x27, #0x1
  4f2dcc:	b.eq	4f27e0 <bpftime_map_delete_elem_helper+0x7dc>  // b.none
  4f2dd0:	add	x28, x20, x27
  4f2dd4:	add	x0, x28, #0x8
  4f2dd8:	ldr	x1, [x0, #16]
  4f2ddc:	cmp	x4, x1
  4f2de0:	b.ne	4f2dac <bpftime_map_delete_elem_helper+0xda8>  // b.any
  4f2de4:	ldr	x5, [x22, #136]
  4f2de8:	add	x24, sp, #0x110
  4f2dec:	add	x3, x22, #0x88
  4f2df0:	add	x25, sp, #0xf8
  4f2df4:	cmp	x5, #0x1
  4f2df8:	sub	x2, x3, x24
  4f2dfc:	csetm	x1, ne	// ne = any
  4f2e00:	sub	x3, x3, x25
  4f2e04:	and	x2, x1, x2
  4f2e08:	and	x1, x1, x3
  4f2e0c:	add	x2, x2, x5
  4f2e10:	add	x1, x1, x5
  4f2e14:	cmp	x2, #0x1
  4f2e18:	add	x21, sp, #0x140
  4f2e1c:	csetm	x3, ne	// ne = any
  4f2e20:	cmp	x1, #0x1
  4f2e24:	sub	x7, x24, x21
  4f2e28:	sub	x6, x25, x24
  4f2e2c:	csetm	x5, ne	// ne = any
  4f2e30:	add	x1, x4, x1
  4f2e34:	and	x3, x3, x7
  4f2e38:	and	x5, x5, x6
  4f2e3c:	add	x2, x3, x2
  4f2e40:	add	x1, x5, x1
  4f2e44:	str	x1, [sp, #272]
  4f2e48:	add	x0, x0, #0x8
  4f2e4c:	str	x2, [sp, #320]
  4f2e50:	add	x26, sp, #0xe0
  4f2e54:	sub	x3, x0, x26
  4f2e58:	sub	x7, x26, x25
  4f2e5c:	ldr	x6, [x28, #16]
  4f2e60:	mov	x0, x21
  4f2e64:	mov	x2, x25
  4f2e68:	mov	x1, x24
  4f2e6c:	cmp	x6, #0x1
  4f2e70:	str	x4, [sp]
  4f2e74:	csetm	x5, ne	// ne = any
  4f2e78:	and	x3, x5, x3
  4f2e7c:	add	x3, x3, x6
  4f2e80:	cmp	x3, #0x1
  4f2e84:	csetm	x5, ne	// ne = any
  4f2e88:	and	x5, x5, x7
  4f2e8c:	add	x3, x5, x3
  4f2e90:	str	x3, [sp, #248]
  4f2e94:	bl	4fd224 <_ZN5boost9intrusive10algo_equalINS_9container12vec_iteratorINS_12interprocess10offset_ptrIhlmLm0EEELb1EEES7_EEbT_S8_T0_>
  4f2e98:	ldr	x4, [sp]
  4f2e9c:	tbz	w0, #0, 4f2dac <bpftime_map_delete_elem_helper+0xda8>
  4f2ea0:	sub	x2, x20, x24
  4f2ea4:	mov	x1, x23
  4f2ea8:	add	x2, x2, x27
  4f2eac:	mov	x0, x21
  4f2eb0:	str	x2, [sp, #272]
  4f2eb4:	add	x28, sp, #0xe8
  4f2eb8:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f2ebc:	ldr	x4, [sp, #272]
  4f2ec0:	sub	x3, x24, x26
  4f2ec4:	mov	x1, x21
  4f2ec8:	mov	x0, x28
  4f2ecc:	cmp	x4, #0x1
  4f2ed0:	csetm	x2, ne	// ne = any
  4f2ed4:	and	x2, x2, x3
  4f2ed8:	add	x2, x2, x4
  4f2edc:	str	x2, [sp, #224]
  4f2ee0:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f2ee4:	ldr	x1, [sp, #224]
  4f2ee8:	cmp	x1, #0x1
  4f2eec:	add	x1, x26, x1
  4f2ef0:	csetm	x0, ne	// ne = any
  4f2ef4:	ands	x0, x0, x1
  4f2ef8:	b.eq	4f27e0 <bpftime_map_delete_elem_helper+0x7dc>  // b.none
  4f2efc:	ldr	x4, [x0, #72]
  4f2f00:	add	x3, x0, #0x48
  4f2f04:	sub	x3, x3, x21
  4f2f08:	add	x2, x0, #0x50
  4f2f0c:	cmp	x4, #0x1
  4f2f10:	add	x27, sp, #0x148
  4f2f14:	csetm	x1, ne	// ne = any
  4f2f18:	sub	x2, x2, x27
  4f2f1c:	and	x1, x1, x3
  4f2f20:	add	x1, x1, x4
  4f2f24:	str	x1, [sp, #320]
  4f2f28:	ldr	x1, [x0, #80]
  4f2f2c:	cmp	x1, #0x1
  4f2f30:	csetm	x0, ne	// ne = any
  4f2f34:	and	x0, x0, x2
  4f2f38:	add	x0, x0, x1
  4f2f3c:	str	x0, [sp, #328]
  4f2f40:	cmp	x0, #0x1
  4f2f44:	add	x0, x27, x0
  4f2f48:	csetm	x1, ne	// ne = any
  4f2f4c:	ands	x1, x1, x0
  4f2f50:	b.ne	4f3768 <bpftime_map_delete_elem_helper+0x1764>  // b.any
  4f2f54:	mov	x1, x21
  4f2f58:	mov	x0, x22
  4f2f5c:	bl	4fc9e0 <_ZN7bpftime21lru_var_hash_map_impl11evict_entryEN5boost12interprocess10shared_ptrINS_18lru_linklist_entryENS2_9allocatorIvNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEENS2_7deleterIS4_SD_EEEE>
  4f2f60:	mov	x0, x21
  4f2f64:	bl	4df2e0 <_ZN5boost12interprocess9ipcdetail12shared_countIN7bpftime18lru_linklist_entryENS0_9allocatorIvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIS4_SD_EEED1Ev>
  4f2f68:	ldp	x3, x2, [sp, #224]
  4f2f6c:	sub	x26, x26, x25
  4f2f70:	ldr	x0, [sp, #240]
  4f2f74:	add	x1, sp, #0x100
  4f2f78:	sub	x28, x28, x1
  4f2f7c:	sub	x25, x25, x23
  4f2f80:	sub	x23, x23, x21
  4f2f84:	cmp	x3, #0x1
  4f2f88:	csetm	x7, ne	// ne = any
  4f2f8c:	cmp	x2, #0x1
  4f2f90:	and	x7, x7, x26
  4f2f94:	csetm	x6, ne	// ne = any
  4f2f98:	cmp	x0, #0x1
  4f2f9c:	add	x7, x7, x3
  4f2fa0:	and	x6, x6, x28
  4f2fa4:	csetm	x5, ne	// ne = any
  4f2fa8:	add	x6, x6, x2
  4f2fac:	cmp	x7, #0x1
  4f2fb0:	mov	x2, #0xffffffffffffffe8    	// #-24
  4f2fb4:	and	x5, x5, x2
  4f2fb8:	add	x5, x5, x0
  4f2fbc:	csetm	x4, ne	// ne = any
  4f2fc0:	add	x0, sp, #0x130
  4f2fc4:	cmp	x6, #0x1
  4f2fc8:	sub	x1, x1, x0
  4f2fcc:	and	x4, x4, x25
  4f2fd0:	csetm	x11, ne	// ne = any
  4f2fd4:	cmp	x5, #0x1
  4f2fd8:	add	x4, x4, x7
  4f2fdc:	and	x11, x11, x1
  4f2fe0:	csetm	x8, ne	// ne = any
  4f2fe4:	add	x11, x11, x6
  4f2fe8:	cmp	x4, #0x1
  4f2fec:	mov	x1, #0xffffffffffffffd0    	// #-48
  4f2ff0:	and	x8, x8, x1
  4f2ff4:	csetm	x26, ne	// ne = any
  4f2ff8:	add	x8, x8, x5
  4f2ffc:	cmp	x11, #0x1
  4f3000:	and	x26, x26, x23
  4f3004:	sub	x0, x0, x27
  4f3008:	csetm	x3, ne	// ne = any
  4f300c:	cmp	x8, #0x1
  4f3010:	add	x26, x26, x4
  4f3014:	and	x3, x3, x0
  4f3018:	csetm	x2, ne	// ne = any
  4f301c:	add	x3, x3, x11
  4f3020:	cmp	x26, #0x1
  4f3024:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f3028:	and	x2, x2, x0
  4f302c:	csetm	x1, ne	// ne = any
  4f3030:	add	x2, x2, x8
  4f3034:	cmp	x3, #0x1
  4f3038:	sub	x0, x21, x20
  4f303c:	add	x23, sp, #0xc8
  4f3040:	csetm	x10, ne	// ne = any
  4f3044:	and	x0, x1, x0
  4f3048:	cmp	x2, #0x1
  4f304c:	add	x28, sp, #0x150
  4f3050:	add	x25, sp, #0xd0
  4f3054:	stp	x7, x6, [sp, #248]
  4f3058:	sub	x6, x27, x23
  4f305c:	add	x0, x0, x26
  4f3060:	csetm	x9, ne	// ne = any
  4f3064:	and	x6, x10, x6
  4f3068:	str	x5, [sp, #264]
  4f306c:	sub	x5, x28, x25
  4f3070:	cmp	x0, #0x1
  4f3074:	add	x6, x6, x3
  4f3078:	and	x5, x9, x5
  4f307c:	add	x5, x5, x2
  4f3080:	str	x0, [sp, #192]
  4f3084:	add	x0, x20, x0
  4f3088:	stp	x6, x5, [sp, #200]
  4f308c:	csetm	x6, ne	// ne = any
  4f3090:	and	x0, x6, x0
  4f3094:	stp	x4, x11, [sp, #296]
  4f3098:	sub	x5, x0, x20
  4f309c:	stp	x8, x26, [sp, #312]
  4f30a0:	stp	x3, x2, [sp, #328]
  4f30a4:	ldr	x4, [x0]
  4f30a8:	cmp	x4, #0x1
  4f30ac:	csetm	x0, ne	// ne = any
  4f30b0:	and	x0, x0, x5
  4f30b4:	add	x0, x0, x4
  4f30b8:	str	x0, [sp, #192]
  4f30bc:	cmp	x0, #0x1
  4f30c0:	b.eq	4f39f4 <bpftime_map_delete_elem_helper+0x19f0>  // b.none
  4f30c4:	add	x7, sp, #0xa0
  4f30c8:	add	x8, sp, #0xa8
  4f30cc:	sub	x4, x27, x7
  4f30d0:	sub	x6, x28, x8
  4f30d4:	and	x4, x4, x10
  4f30d8:	and	x6, x6, x9
  4f30dc:	add	x4, x4, x3
  4f30e0:	add	x6, x6, x2
  4f30e4:	cmp	x4, #0x1
  4f30e8:	add	x2, x7, x4
  4f30ec:	csetm	x9, ne	// ne = any
  4f30f0:	stp	x4, x6, [sp, #160]
  4f30f4:	and	x2, x9, x2
  4f30f8:	add	x3, x21, x26
  4f30fc:	mov	x5, x2
  4f3100:	and	x3, x3, x1
  4f3104:	ldr	x0, [x2]
  4f3108:	cmp	x0, #0x1
  4f310c:	add	x1, x0, x2
  4f3110:	csetm	x0, ne	// ne = any
  4f3114:	and	x0, x0, x1
  4f3118:	cmp	x3, x0
  4f311c:	b.eq	4f3144 <bpftime_map_delete_elem_helper+0x1140>  // b.none
  4f3120:	ldr	x1, [x0]
  4f3124:	mov	x2, x0
  4f3128:	mov	x5, x0
  4f312c:	cmp	x1, #0x1
  4f3130:	add	x1, x0, x1
  4f3134:	csetm	x0, ne	// ne = any
  4f3138:	and	x0, x0, x1
  4f313c:	cmp	x3, x0
  4f3140:	b.ne	4f3120 <bpftime_map_delete_elem_helper+0x111c>  // b.any
  4f3144:	cmp	x6, #0x1
  4f3148:	add	x26, sp, #0xb0
  4f314c:	add	x27, sp, #0xb8
  4f3150:	csetm	x1, ne	// ne = any
  4f3154:	sub	x8, x8, x27
  4f3158:	sub	x7, x7, x26
  4f315c:	and	x1, x1, x8
  4f3160:	and	x0, x9, x7
  4f3164:	add	x1, x1, x6
  4f3168:	add	x0, x0, x4
  4f316c:	stp	x0, x1, [sp, #176]
  4f3170:	ldr	x0, [x2]
  4f3174:	cmp	x0, #0x1
  4f3178:	add	x0, x0, x5
  4f317c:	csetm	x1, ne	// ne = any
  4f3180:	and	x1, x1, x0
  4f3184:	sub	x0, x1, x5
  4f3188:	ldr	x3, [x1]
  4f318c:	cmp	x3, #0x1
  4f3190:	csetm	x1, ne	// ne = any
  4f3194:	and	x0, x1, x0
  4f3198:	add	x0, x0, x3
  4f319c:	str	x0, [x2]
  4f31a0:	ldr	x0, [sp, #176]
  4f31a4:	cmp	x0, #0x1
  4f31a8:	add	x0, x26, x0
  4f31ac:	csetm	x1, ne	// ne = any
  4f31b0:	and	x0, x1, x0
  4f31b4:	ldr	x28, [x0]
  4f31b8:	cmp	x28, #0x1
  4f31bc:	b.eq	4f38d8 <bpftime_map_delete_elem_helper+0x18d4>  // b.none
  4f31c0:	ldr	x3, [sp, #320]
  4f31c4:	sub	x21, x21, x24
  4f31c8:	mov	x1, x24
  4f31cc:	mov	x0, x22
  4f31d0:	cmp	x3, #0x1
  4f31d4:	csetm	x2, ne	// ne = any
  4f31d8:	and	x2, x2, x21
  4f31dc:	add	x2, x2, x3
  4f31e0:	str	x2, [sp, #272]
  4f31e4:	bl	4e1b90 <_ZN5boost9unordered6detail5tableINS1_3mapINS_12interprocess9allocatorISt4pairIKNS_9container6vectorIhNS5_IhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESG_EESI_SL_NSK_16bytes_vec_hasherESt8equal_toISI_EEEE11delete_nodeENSC_INS1_4nodeISM_SD_EElmLm0EEE>
  4f31e8:	ldr	x1, [x22, #8]
  4f31ec:	adrp	x2, 1d47000 <_ZZL25UpgradeIntrinsicFunction1PN4llvm8FunctionERS1_E9StoreInts+0x320>
  4f31f0:	fmov	d2, x23
  4f31f4:	fmov	d0, x26
  4f31f8:	sub	x1, x1, #0x1
  4f31fc:	str	x1, [x22, #8]
  4f3200:	ldr	q1, [x2, #112]
  4f3204:	mov	v2.d[1], x25
  4f3208:	ldur	q3, [sp, #200]
  4f320c:	mov	v0.d[1], x27
  4f3210:	ldr	x4, [sp, #192]
  4f3214:	add	x0, sp, #0x98
  4f3218:	cmeq	v1.2d, v3.2d, v1.2d
  4f321c:	sub	x20, x20, x0
  4f3220:	sub	v0.2d, v2.2d, v0.2d
  4f3224:	cmp	x4, #0x1
  4f3228:	csetm	x2, ne	// ne = any
  4f322c:	sub	x3, x0, x24
  4f3230:	and	x2, x2, x20
  4f3234:	add	x0, sp, #0x118
  4f3238:	add	x2, x2, x4
  4f323c:	str	x2, [sp, #152]
  4f3240:	bic	v0.16b, v0.16b, v1.16b
  4f3244:	cmp	x2, #0x1
  4f3248:	csetm	x4, ne	// ne = any
  4f324c:	mov	x1, x26
  4f3250:	and	x3, x4, x3
  4f3254:	add	x2, x3, x2
  4f3258:	str	x2, [sp, #272]
  4f325c:	add	v0.2d, v0.2d, v3.2d
  4f3260:	str	q0, [sp, #176]
  4f3264:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f3268:	b	4f24ac <bpftime_map_delete_elem_helper+0x4a8>
  4f326c:	add	x0, x21, #0x30
  4f3270:	mov	x1, x0
  4f3274:	str	x1, [sp, #32]
  4f3278:	mov	w0, w20
  4f327c:	ldr	x1, [x1, #8]
  4f3280:	cmp	x1, #0x1c
  4f3284:	b.hi	4f42a8 <bpftime_map_delete_elem_helper+0x22a4>  // b.pmore
  4f3288:	adrp	x3, 1d13000 <_ZTSN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0xd0>
  4f328c:	add	x3, x3, #0xa38
  4f3290:	adrp	x2, 1d13000 <_ZTSN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0xd0>
  4f3294:	add	x2, x2, #0xb20
  4f3298:	ldr	x3, [x3, x1, lsl #3]
  4f329c:	ldr	x1, [x2, x1, lsl #3]
  4f32a0:	mul	x0, x0, x3
  4f32a4:	mov	w1, w1
  4f32a8:	umulh	x1, x0, x1
  4f32ac:	ldr	x0, [sp, #32]
  4f32b0:	add	x23, sp, #0x128
  4f32b4:	mov	x8, x23
  4f32b8:	add	x25, sp, #0xf8
  4f32bc:	bl	4e1fb0 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKjNS_9container6vectorImNS_12interprocess9allocatorImNS9_15segment_managerIcNS9_15rbtree_best_fitINS9_12mutex_familyENS9_10offset_ptrIvlmLm0EEELm0EEENS9_10iset_indexEEEEEvEEESF_EESF_EENSA_ISL_SI_EENS1_15prime_fmod_sizeIvEEE2atEm>
  4f32c0:	ldr	x1, [sp, #296]
  4f32c4:	cmp	x1, #0x1
  4f32c8:	add	x0, x23, x1
  4f32cc:	csetm	x8, ne	// ne = any
  4f32d0:	and	x0, x8, x0
  4f32d4:	sub	x2, x0, x25
  4f32d8:	ldr	x0, [x0]
  4f32dc:	cmp	x0, #0x1
  4f32e0:	csetm	x3, ne	// ne = any
  4f32e4:	and	x3, x3, x2
  4f32e8:	add	x3, x3, x0
  4f32ec:	str	x3, [sp, #248]
  4f32f0:	cmp	x3, #0x1
  4f32f4:	b.eq	4f282c <bpftime_map_delete_elem_helper+0x828>  // b.none
  4f32f8:	add	x0, x25, x3
  4f32fc:	ldr	w2, [x0, #8]
  4f3300:	cmp	w20, w2
  4f3304:	b.ne	4f41e0 <bpftime_map_delete_elem_helper+0x21dc>  // b.any
  4f3308:	ldr	x10, [sp, #304]
  4f330c:	add	x27, sp, #0x110
  4f3310:	sub	x9, x25, x27
  4f3314:	add	x2, sp, #0xe0
  4f3318:	add	x3, x9, x3
  4f331c:	cmp	x10, #0x1
  4f3320:	add	x28, sp, #0x140
  4f3324:	csetm	x4, ne	// ne = any
  4f3328:	add	x20, sp, #0x130
  4f332c:	cmp	x3, #0x1
  4f3330:	add	x26, sp, #0x148
  4f3334:	sub	x0, x27, x2
  4f3338:	sub	x5, x23, x28
  4f333c:	csetm	x6, ne	// ne = any
  4f3340:	sub	x24, x20, x26
  4f3344:	and	x8, x8, x5
  4f3348:	and	x6, x6, x0
  4f334c:	and	x4, x4, x24
  4f3350:	add	x8, x8, x1
  4f3354:	add	x4, x4, x10
  4f3358:	add	x6, x6, x3
  4f335c:	add	x0, sp, #0xe8
  4f3360:	mov	x1, x28
  4f3364:	stp	x0, x5, [sp]
  4f3368:	stp	x2, x9, [sp, #16]
  4f336c:	str	x6, [sp, #224]
  4f3370:	str	x3, [sp, #272]
  4f3374:	stp	x8, x4, [sp, #320]
  4f3378:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f337c:	ldp	x2, x9, [sp, #16]
  4f3380:	ldr	x1, [sp, #224]
  4f3384:	ldp	x0, x5, [sp]
  4f3388:	cmp	x1, #0x1
  4f338c:	add	x2, x2, x1
  4f3390:	csetm	x1, ne	// ne = any
  4f3394:	tst	x1, x2
  4f3398:	b.eq	4f282c <bpftime_map_delete_elem_helper+0x828>  // b.none
  4f339c:	ldp	x4, x3, [sp, #232]
  4f33a0:	add	x1, sp, #0x100
  4f33a4:	sub	x2, x2, x25
  4f33a8:	sub	x0, x0, x1
  4f33ac:	cmp	x4, #0x1
  4f33b0:	csetm	x11, ne	// ne = any
  4f33b4:	cmp	x3, #0x1
  4f33b8:	and	x11, x11, x0
  4f33bc:	csetm	x12, ne	// ne = any
  4f33c0:	cmp	x2, #0x1
  4f33c4:	add	x11, x11, x4
  4f33c8:	csetm	x10, ne	// ne = any
  4f33cc:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f33d0:	and	x12, x12, x0
  4f33d4:	cmp	x11, #0x1
  4f33d8:	add	x12, x12, x3
  4f33dc:	and	x10, x10, x9
  4f33e0:	add	x0, sp, #0x118
  4f33e4:	add	x10, x10, x2
  4f33e8:	sub	x1, x1, x0
  4f33ec:	csetm	x9, ne	// ne = any
  4f33f0:	cmp	x12, #0x1
  4f33f4:	and	x9, x9, x1
  4f33f8:	csetm	x8, ne	// ne = any
  4f33fc:	cmp	x10, #0x1
  4f3400:	add	x9, x9, x11
  4f3404:	sub	x4, x27, x23
  4f3408:	mov	x1, #0xffffffffffffffe8    	// #-24
  4f340c:	csetm	x27, ne	// ne = any
  4f3410:	and	x8, x8, x1
  4f3414:	cmp	x9, #0x1
  4f3418:	add	x8, x8, x12
  4f341c:	and	x27, x27, x4
  4f3420:	add	x27, x27, x10
  4f3424:	sub	x0, x0, x20
  4f3428:	csetm	x4, ne	// ne = any
  4f342c:	cmp	x8, #0x1
  4f3430:	and	x4, x4, x0
  4f3434:	csetm	x1, ne	// ne = any
  4f3438:	cmp	x27, #0x1
  4f343c:	add	x4, x4, x9
  4f3440:	csetm	x25, ne	// ne = any
  4f3444:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f3448:	and	x1, x1, x0
  4f344c:	cmp	x4, #0x1
  4f3450:	add	x1, x1, x8
  4f3454:	and	x0, x25, x5
  4f3458:	add	x0, x0, x27
  4f345c:	csetm	x6, ne	// ne = any
  4f3460:	cmp	x1, #0x1
  4f3464:	stp	x2, x11, [sp, #248]
  4f3468:	csetm	x5, ne	// ne = any
  4f346c:	cmp	x0, #0x1
  4f3470:	add	x2, x28, x0
  4f3474:	csetm	x7, ne	// ne = any
  4f3478:	and	x3, x6, x24
  4f347c:	and	x7, x7, x2
  4f3480:	mov	x2, #0xffffffffffffffe8    	// #-24
  4f3484:	and	x2, x5, x2
  4f3488:	add	x3, x3, x4
  4f348c:	add	x2, x2, x1
  4f3490:	stp	x12, x10, [sp, #264]
  4f3494:	stp	x9, x8, [sp, #280]
  4f3498:	sub	x8, x7, x28
  4f349c:	add	x9, sp, #0x138
  4f34a0:	stp	x27, x4, [sp, #296]
  4f34a4:	stp	x1, x0, [sp, #312]
  4f34a8:	str	x3, [sp, #328]
  4f34ac:	str	x2, [sp, #336]
  4f34b0:	ldr	x2, [x7]
  4f34b4:	cmp	x2, #0x1
  4f34b8:	csetm	x0, ne	// ne = any
  4f34bc:	and	x0, x0, x8
  4f34c0:	add	x0, x0, x2
  4f34c4:	str	x0, [sp, #320]
  4f34c8:	cmp	x0, #0x1
  4f34cc:	b.eq	4f3a48 <bpftime_map_delete_elem_helper+0x1a44>  // b.none
  4f34d0:	add	x7, sp, #0xa0
  4f34d4:	add	x8, sp, #0xa8
  4f34d8:	sub	x20, x20, x7
  4f34dc:	sub	x0, x9, x8
  4f34e0:	and	x20, x20, x6
  4f34e4:	and	x0, x0, x5
  4f34e8:	add	x20, x20, x4
  4f34ec:	add	x5, x0, x1
  4f34f0:	cmp	x20, #0x1
  4f34f4:	add	x2, x7, x20
  4f34f8:	csetm	x6, ne	// ne = any
  4f34fc:	stp	x20, x5, [sp, #160]
  4f3500:	and	x2, x6, x2
  4f3504:	add	x27, x23, x27
  4f3508:	mov	x4, x2
  4f350c:	and	x3, x25, x27
  4f3510:	ldr	x1, [x2]
  4f3514:	b	4f3524 <bpftime_map_delete_elem_helper+0x1520>
  4f3518:	ldr	x1, [x0]
  4f351c:	mov	x2, x0
  4f3520:	mov	x4, x0
  4f3524:	cmp	x1, #0x1
  4f3528:	add	x1, x1, x2
  4f352c:	csetm	x0, ne	// ne = any
  4f3530:	and	x0, x0, x1
  4f3534:	cmp	x3, x0
  4f3538:	b.ne	4f3518 <bpftime_map_delete_elem_helper+0x1514>  // b.any
  4f353c:	cmp	x5, #0x1
  4f3540:	add	x3, sp, #0xb8
  4f3544:	add	x26, sp, #0xb0
  4f3548:	csetm	x1, ne	// ne = any
  4f354c:	sub	x8, x8, x3
  4f3550:	sub	x3, x7, x26
  4f3554:	and	x0, x6, x3
  4f3558:	and	x1, x1, x8
  4f355c:	add	x1, x1, x5
  4f3560:	add	x0, x0, x20
  4f3564:	stp	x0, x1, [sp, #176]
  4f3568:	ldr	x0, [x2]
  4f356c:	cmp	x0, #0x1
  4f3570:	add	x0, x0, x4
  4f3574:	csetm	x1, ne	// ne = any
  4f3578:	and	x1, x1, x0
  4f357c:	sub	x0, x1, x4
  4f3580:	ldr	x3, [x1]
  4f3584:	cmp	x3, #0x1
  4f3588:	csetm	x1, ne	// ne = any
  4f358c:	and	x0, x1, x0
  4f3590:	add	x0, x0, x3
  4f3594:	str	x0, [x2]
  4f3598:	ldr	x0, [sp, #176]
  4f359c:	cmp	x0, #0x1
  4f35a0:	add	x0, x26, x0
  4f35a4:	csetm	x1, ne	// ne = any
  4f35a8:	and	x0, x1, x0
  4f35ac:	ldr	x4, [x0]
  4f35b0:	cmp	x4, #0x1
  4f35b4:	b.eq	4f37ac <bpftime_map_delete_elem_helper+0x17a8>  // b.none
  4f35b8:	add	x20, sp, #0xc0
  4f35bc:	add	x25, sp, #0x90
  4f35c0:	add	x28, sp, #0x98
  4f35c4:	add	x27, sp, #0x88
  4f35c8:	ldr	x0, [sp, #296]
  4f35cc:	sub	x23, x23, x27
  4f35d0:	ldr	x1, [sp, #32]
  4f35d4:	cmp	x0, #0x1
  4f35d8:	csetm	x4, ne	// ne = any
  4f35dc:	and	x4, x4, x23
  4f35e0:	add	x4, x4, x0
  4f35e4:	str	x4, [sp, #136]
  4f35e8:	add	x3, x27, x4
  4f35ec:	ldr	x0, [x21, #48]
  4f35f0:	cmp	x0, #0x1
  4f35f4:	add	x24, x1, x0
  4f35f8:	csetm	x0, ne	// ne = any
  4f35fc:	and	x0, x0, x24
  4f3600:	cmp	x0, #0x0
  4f3604:	sub	x0, x0, x25
  4f3608:	sub	x1, x0, #0x1
  4f360c:	csetm	x0, ne	// ne = any
  4f3610:	and	x0, x0, x1
  4f3614:	add	x0, x0, #0x1
  4f3618:	str	x0, [sp, #144]
  4f361c:	cmp	x0, #0x1
  4f3620:	add	x0, x25, x0
  4f3624:	csetm	x1, ne	// ne = any
  4f3628:	and	x0, x1, x0
  4f362c:	cmp	x0, #0x0
  4f3630:	sub	x0, x0, x28
  4f3634:	csetm	x2, ne	// ne = any
  4f3638:	cmp	x4, #0x1
  4f363c:	csetm	x5, ne	// ne = any
  4f3640:	sub	x0, x0, #0x1
  4f3644:	and	x1, x5, x3
  4f3648:	and	x2, x2, x0
  4f364c:	add	x0, x1, #0x8
  4f3650:	add	x1, x2, #0x1
  4f3654:	sub	x26, x0, x26
  4f3658:	str	x1, [sp, #152]
  4f365c:	cmp	x26, #0x1
  4f3660:	str	x26, [sp, #176]
  4f3664:	csetm	x1, ne	// ne = any
  4f3668:	and	x1, x1, x0
  4f366c:	add	x0, x1, #0x8
  4f3670:	ldr	x2, [x0, #24]
  4f3674:	cbnz	x2, 4f3774 <bpftime_map_delete_elem_helper+0x1770>
  4f3678:	sub	x2, x27, x20
  4f367c:	mov	x1, x20
  4f3680:	and	x2, x2, x5
  4f3684:	mov	x0, x25
  4f3688:	add	x2, x2, x4
  4f368c:	str	x2, [sp, #192]
  4f3690:	bl	54fca4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f3694:	ldr	x0, [x22, #8]
  4f3698:	mov	x20, #0x0                   	// #0
  4f369c:	sub	x0, x0, #0x1
  4f36a0:	str	x0, [x22, #8]
  4f36a4:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f36a8:	mov	x20, #0xffffffffffffffea    	// #-22
  4f36ac:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f36b0:	mov	x0, x19
  4f36b4:	bl	508090 <_ZN7bpftime25hash_map_kernel_user_impl11init_map_fdEv>
  4f36b8:	ldr	w0, [x19, #76]
  4f36bc:	b	4f2cc8 <bpftime_map_delete_elem_helper+0xcc4>
  4f36c0:	ldrb	w3, [x20, #1]
  4f36c4:	mov	x4, #0x745f                	// #29791
  4f36c8:	ldrb	w0, [x20]
  4f36cc:	mov	w2, #0x1f                  	// #31
  4f36d0:	ldrb	w6, [x20, #2]
  4f36d4:	lsl	x1, x3, #4
  4f36d8:	ldrb	w5, [x20, #3]
  4f36dc:	sub	x1, x1, x3
  4f36e0:	add	x1, x3, x1, lsl #6
  4f36e4:	umaddl	x2, w2, w6, x5
  4f36e8:	madd	x0, x0, x4, x1
  4f36ec:	add	x0, x0, x2
  4f36f0:	mov	x2, x0
  4f36f4:	b	4f2514 <bpftime_map_delete_elem_helper+0x510>
  4f36f8:	bl	506114 <_ZN6spdlog18default_logger_rawEv>
  4f36fc:	mov	x20, #0xffffffffffffffff    	// #-1
  4f3700:	mov	w3, #0x327                 	// #807
  4f3704:	adrp	x2, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f3708:	add	x2, x2, #0xe8
  4f370c:	str	x2, [sp, #96]
  4f3710:	str	w3, [sp, #104]
  4f3714:	add	x1, sp, #0x30
  4f3718:	ldr	w7, [x19, #72]
  4f371c:	adrp	x6, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f3720:	ldp	x8, x9, [sp, #96]
  4f3724:	add	x6, x6, #0xff0
  4f3728:	stp	x8, x9, [sp, #48]
  4f372c:	add	x5, sp, #0x5c
  4f3730:	mov	x4, #0x2e                  	// #46
  4f3734:	str	w7, [sp, #92]
  4f3738:	adrp	x3, 1d3f000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x10778>
  4f373c:	str	x6, [sp, #112]
  4f3740:	add	x3, x3, #0x0
  4f3744:	str	x6, [x1, #16]
  4f3748:	mov	w2, #0x4                   	// #4
  4f374c:	bl	4c88f0 <_ZN6spdlog6logger4log_IJRKiEEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_>
  4f3750:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f3754:	ldr	w1, [x24, x1]
  4f3758:	ldr	w0, [x22]
  4f375c:	cmp	w1, w0
  4f3760:	b.ne	4f255c <bpftime_map_delete_elem_helper+0x558>  // b.any
  4f3764:	b	4f25a4 <bpftime_map_delete_elem_helper+0x5a0>
  4f3768:	mov	w0, #0x1                   	// #1
  4f376c:	bl	1d103a0 <__aarch64_ldadd4_sync>
  4f3770:	b	4f2f54 <bpftime_map_delete_elem_helper+0xf50>
  4f3774:	ldr	x4, [x1, #16]
  4f3778:	add	x3, x0, #0x8
  4f377c:	sub	x3, x3, x20
  4f3780:	mov	x1, x20
  4f3784:	cmp	x4, #0x1
  4f3788:	csetm	x2, ne	// ne = any
  4f378c:	and	x2, x2, x3
  4f3790:	add	x2, x2, x4
  4f3794:	str	x2, [sp, #192]
  4f3798:	bl	54fca4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f379c:	ldr	x4, [sp, #136]
  4f37a0:	cmp	x4, #0x1
  4f37a4:	csetm	x5, ne	// ne = any
  4f37a8:	b	4f3678 <bpftime_map_delete_elem_helper+0x1674>
  4f37ac:	add	x20, sp, #0xc0
  4f37b0:	mov	x1, x26
  4f37b4:	mov	x0, x20
  4f37b8:	str	x4, [sp]
  4f37bc:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f37c0:	add	x25, sp, #0x90
  4f37c4:	ldp	x3, x0, [sp, #192]
  4f37c8:	sub	x5, x20, x25
  4f37cc:	add	x28, sp, #0x98
  4f37d0:	add	x27, sp, #0x88
  4f37d4:	ldr	x4, [sp]
  4f37d8:	cmp	x3, #0x1
  4f37dc:	csetm	x1, ne	// ne = any
  4f37e0:	cmp	x0, #0x1
  4f37e4:	csetm	x2, ne	// ne = any
  4f37e8:	and	x1, x1, x5
  4f37ec:	and	x2, x2, #0x30
  4f37f0:	add	x1, x1, x3
  4f37f4:	add	x2, x2, x0
  4f37f8:	stp	x1, x2, [sp, #144]
  4f37fc:	cmp	x2, #0x1
  4f3800:	csetm	x6, ne	// ne = any
  4f3804:	add	x3, x28, x2
  4f3808:	and	x3, x6, x3
  4f380c:	cmp	x1, #0x1
  4f3810:	cset	x0, eq	// eq = none
  4f3814:	add	x1, x25, x1
  4f3818:	sub	x0, x0, #0x1
  4f381c:	ldp	x5, x7, [x3]
  4f3820:	and	x0, x0, x1
  4f3824:	cmp	x5, #0x1
  4f3828:	add	x5, x3, x5
  4f382c:	csetm	x1, ne	// ne = any
  4f3830:	and	x1, x1, x5
  4f3834:	sub	x0, x0, x1
  4f3838:	asr	x0, x0, #3
  4f383c:	lsl	x0, x4, x0
  4f3840:	bic	x0, x7, x0
  4f3844:	str	x0, [x3, #8]
  4f3848:	cbnz	x0, 4f35c8 <bpftime_map_delete_elem_helper+0x15c4>
  4f384c:	sub	x0, x28, x27
  4f3850:	dup	v0.2d, x4
  4f3854:	and	x0, x6, x0
  4f3858:	add	x0, x0, x2
  4f385c:	cmp	x0, #0x1
  4f3860:	add	x0, x27, x0
  4f3864:	csetm	x1, ne	// ne = any
  4f3868:	and	x0, x1, x0
  4f386c:	add	x3, x0, #0x10
  4f3870:	add	x5, x0, #0x18
  4f3874:	ldp	x1, x4, [x0, #16]
  4f3878:	cmp	x1, #0x1
  4f387c:	add	x1, x1, x3
  4f3880:	csetm	x2, ne	// ne = any
  4f3884:	cmp	x4, #0x1
  4f3888:	and	x2, x2, x1
  4f388c:	csetm	x1, ne	// ne = any
  4f3890:	sub	x6, x0, x2
  4f3894:	and	x1, x1, x6
  4f3898:	add	x1, x1, x4
  4f389c:	str	x1, [x2, #24]
  4f38a0:	ldp	x4, x1, [x0, #16]
  4f38a4:	cmp	x1, #0x1
  4f38a8:	add	x1, x5, x1
  4f38ac:	csetm	x2, ne	// ne = any
  4f38b0:	cmp	x4, #0x1
  4f38b4:	and	x2, x2, x1
  4f38b8:	csetm	x1, ne	// ne = any
  4f38bc:	add	x5, x2, #0x10
  4f38c0:	sub	x3, x3, x5
  4f38c4:	and	x1, x1, x3
  4f38c8:	add	x1, x1, x4
  4f38cc:	str	x1, [x2, #16]
  4f38d0:	str	q0, [x0, #16]
  4f38d4:	b	4f35c8 <bpftime_map_delete_elem_helper+0x15c4>
  4f38d8:	mov	x1, x26
  4f38dc:	mov	x0, x24
  4f38e0:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f38e4:	ldp	x2, x3, [sp, #272]
  4f38e8:	add	x1, sp, #0x90
  4f38ec:	sub	x5, x24, x1
  4f38f0:	add	x4, sp, #0x98
  4f38f4:	cmp	x2, #0x1
  4f38f8:	csetm	x0, ne	// ne = any
  4f38fc:	cmp	x3, #0x1
  4f3900:	and	x0, x0, x5
  4f3904:	cset	x5, ne	// ne = any
  4f3908:	add	x0, x0, x2
  4f390c:	add	x5, x3, x5, lsl #7
  4f3910:	add	x2, x1, x0
  4f3914:	cmp	x5, #0x1
  4f3918:	add	x1, x4, x5
  4f391c:	csetm	x6, ne	// ne = any
  4f3920:	cmp	x0, #0x1
  4f3924:	and	x1, x6, x1
  4f3928:	stp	x0, x5, [sp, #144]
  4f392c:	cset	x0, eq	// eq = none
  4f3930:	sub	x0, x0, #0x1
  4f3934:	and	x0, x0, x2
  4f3938:	ldp	x2, x7, [x1]
  4f393c:	cmp	x2, #0x1
  4f3940:	add	x2, x1, x2
  4f3944:	csetm	x3, ne	// ne = any
  4f3948:	and	x2, x3, x2
  4f394c:	sub	x0, x0, x2
  4f3950:	asr	x0, x0, #3
  4f3954:	lsl	x0, x28, x0
  4f3958:	bic	x0, x7, x0
  4f395c:	str	x0, [x1, #8]
  4f3960:	cbnz	x0, 4f31c0 <bpftime_map_delete_elem_helper+0x11bc>
  4f3964:	add	x1, sp, #0x88
  4f3968:	dup	v0.2d, x28
  4f396c:	sub	x4, x4, x1
  4f3970:	and	x0, x6, x4
  4f3974:	add	x0, x0, x5
  4f3978:	cmp	x0, #0x1
  4f397c:	add	x1, x1, x0
  4f3980:	csetm	x0, ne	// ne = any
  4f3984:	and	x0, x0, x1
  4f3988:	add	x3, x0, #0x10
  4f398c:	add	x4, x0, #0x18
  4f3990:	ldp	x1, x5, [x0, #16]
  4f3994:	cmp	x1, #0x1
  4f3998:	add	x1, x1, x3
  4f399c:	csetm	x2, ne	// ne = any
  4f39a0:	cmp	x5, #0x1
  4f39a4:	and	x2, x2, x1
  4f39a8:	csetm	x1, ne	// ne = any
  4f39ac:	sub	x6, x0, x2
  4f39b0:	and	x1, x1, x6
  4f39b4:	add	x1, x1, x5
  4f39b8:	str	x1, [x2, #24]
  4f39bc:	ldp	x5, x1, [x0, #16]
  4f39c0:	cmp	x1, #0x1
  4f39c4:	add	x1, x4, x1
  4f39c8:	csetm	x2, ne	// ne = any
  4f39cc:	cmp	x5, #0x1
  4f39d0:	and	x2, x2, x1
  4f39d4:	csetm	x1, ne	// ne = any
  4f39d8:	add	x4, x2, #0x10
  4f39dc:	sub	x3, x3, x4
  4f39e0:	and	x1, x1, x3
  4f39e4:	add	x1, x1, x5
  4f39e8:	str	x1, [x2, #16]
  4f39ec:	str	q0, [x0, #16]
  4f39f0:	b	4f31c0 <bpftime_map_delete_elem_helper+0x11bc>
  4f39f4:	mov	x0, x23
  4f39f8:	stp	x3, x2, [sp]
  4f39fc:	stp	x1, x10, [sp, #16]
  4f3a00:	str	x9, [sp, #32]
  4f3a04:	bl	4e2490 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEE9incrementEv>
  4f3a08:	ldr	x0, [sp, #200]
  4f3a0c:	ldp	x3, x2, [sp]
  4f3a10:	cmp	x0, #0x1
  4f3a14:	add	x4, x0, x23
  4f3a18:	csetm	x0, ne	// ne = any
  4f3a1c:	and	x0, x0, x4
  4f3a20:	sub	x5, x0, x20
  4f3a24:	ldp	x1, x10, [sp, #16]
  4f3a28:	ldr	x4, [x0]
  4f3a2c:	ldr	x9, [sp, #32]
  4f3a30:	cmp	x4, #0x1
  4f3a34:	csetm	x0, ne	// ne = any
  4f3a38:	and	x0, x0, x5
  4f3a3c:	add	x0, x0, x4
  4f3a40:	str	x0, [sp, #192]
  4f3a44:	b	4f30c4 <bpftime_map_delete_elem_helper+0x10c0>
  4f3a48:	mov	x0, x26
  4f3a4c:	stp	x4, x9, [sp]
  4f3a50:	stp	x1, x6, [sp, #16]
  4f3a54:	str	x5, [sp, #40]
  4f3a58:	bl	4e2600 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKjNS_9container6vectorImNS_12interprocess9allocatorImNS9_15segment_managerIcNS9_15rbtree_best_fitINS9_12mutex_familyENS9_10offset_ptrIvlmLm0EEELm0EEENS9_10iset_indexEEEEEvEEESF_EESF_EEE9incrementEv>
  4f3a5c:	ldr	x0, [sp, #328]
  4f3a60:	ldp	x4, x9, [sp]
  4f3a64:	cmp	x0, #0x1
  4f3a68:	add	x2, x0, x26
  4f3a6c:	csetm	x0, ne	// ne = any
  4f3a70:	and	x0, x0, x2
  4f3a74:	sub	x28, x0, x28
  4f3a78:	ldp	x1, x6, [sp, #16]
  4f3a7c:	ldr	x2, [x0]
  4f3a80:	ldr	x5, [sp, #40]
  4f3a84:	cmp	x2, #0x1
  4f3a88:	csetm	x0, ne	// ne = any
  4f3a8c:	and	x0, x0, x28
  4f3a90:	add	x0, x0, x2
  4f3a94:	str	x0, [sp, #320]
  4f3a98:	b	4f34d0 <bpftime_map_delete_elem_helper+0x14cc>
  4f3a9c:	stp	x0, x6, [sp]
  4f3aa0:	bl	4e2770 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEESJ_ESE_EESE_EEE9incrementEv>
  4f3aa4:	ldp	x0, x6, [sp]
  4f3aa8:	ldr	x1, [sp, #328]
  4f3aac:	cmp	x1, #0x1
  4f3ab0:	add	x1, x1, x0
  4f3ab4:	csetm	x0, ne	// ne = any
  4f3ab8:	and	x0, x0, x1
  4f3abc:	sub	x6, x0, x6
  4f3ac0:	ldr	x1, [x0]
  4f3ac4:	cmp	x1, #0x1
  4f3ac8:	csetm	x0, ne	// ne = any
  4f3acc:	and	x0, x0, x6
  4f3ad0:	add	x0, x0, x1
  4f3ad4:	str	x0, [sp, #320]
  4f3ad8:	b	4f2a48 <bpftime_map_delete_elem_helper+0xa44>
  4f3adc:	mov	x1, x26
  4f3ae0:	mov	x0, x20
  4f3ae4:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f3ae8:	ldp	x5, x3, [sp, #192]
  4f3aec:	add	x0, sp, #0xc8
  4f3af0:	sub	x0, x0, x28
  4f3af4:	add	x4, sp, #0x90
  4f3af8:	sub	x6, x20, x4
  4f3afc:	cmp	x5, #0x1
  4f3b00:	csetm	x2, ne	// ne = any
  4f3b04:	cmp	x3, #0x1
  4f3b08:	csetm	x1, ne	// ne = any
  4f3b0c:	and	x2, x2, x6
  4f3b10:	and	x1, x1, x0
  4f3b14:	add	x2, x2, x5
  4f3b18:	add	x1, x1, x3
  4f3b1c:	stp	x2, x1, [sp, #144]
  4f3b20:	cmp	x1, #0x1
  4f3b24:	csetm	x5, ne	// ne = any
  4f3b28:	add	x3, x28, x1
  4f3b2c:	and	x3, x5, x3
  4f3b30:	cmp	x2, #0x1
  4f3b34:	add	x4, x4, x2
  4f3b38:	cset	x0, eq	// eq = none
  4f3b3c:	sub	x0, x0, #0x1
  4f3b40:	ldp	x2, x6, [x3]
  4f3b44:	and	x0, x0, x4
  4f3b48:	cmp	x2, #0x1
  4f3b4c:	add	x2, x3, x2
  4f3b50:	csetm	x4, ne	// ne = any
  4f3b54:	and	x2, x4, x2
  4f3b58:	sub	x0, x0, x2
  4f3b5c:	asr	x0, x0, #3
  4f3b60:	lsl	x0, x22, x0
  4f3b64:	bic	x0, x6, x0
  4f3b68:	str	x0, [x3, #8]
  4f3b6c:	cbnz	x0, 4f2b50 <bpftime_map_delete_elem_helper+0xb4c>
  4f3b70:	add	x2, sp, #0x88
  4f3b74:	dup	v0.2d, x22
  4f3b78:	sub	x0, x28, x2
  4f3b7c:	and	x0, x5, x0
  4f3b80:	add	x0, x0, x1
  4f3b84:	cmp	x0, #0x1
  4f3b88:	add	x2, x2, x0
  4f3b8c:	csetm	x0, ne	// ne = any
  4f3b90:	and	x0, x0, x2
  4f3b94:	add	x3, x0, #0x10
  4f3b98:	add	x4, x0, #0x18
  4f3b9c:	ldp	x1, x5, [x0, #16]
  4f3ba0:	cmp	x1, #0x1
  4f3ba4:	add	x1, x1, x3
  4f3ba8:	csetm	x2, ne	// ne = any
  4f3bac:	cmp	x5, #0x1
  4f3bb0:	and	x2, x2, x1
  4f3bb4:	csetm	x1, ne	// ne = any
  4f3bb8:	sub	x6, x0, x2
  4f3bbc:	and	x1, x1, x6
  4f3bc0:	add	x1, x1, x5
  4f3bc4:	str	x1, [x2, #24]
  4f3bc8:	ldp	x5, x1, [x0, #16]
  4f3bcc:	cmp	x1, #0x1
  4f3bd0:	add	x1, x4, x1
  4f3bd4:	csetm	x2, ne	// ne = any
  4f3bd8:	cmp	x5, #0x1
  4f3bdc:	and	x2, x2, x1
  4f3be0:	csetm	x1, ne	// ne = any
  4f3be4:	add	x4, x2, #0x10
  4f3be8:	sub	x3, x3, x4
  4f3bec:	and	x1, x1, x3
  4f3bf0:	add	x1, x1, x5
  4f3bf4:	str	x1, [x2, #16]
  4f3bf8:	str	q0, [x0, #16]
  4f3bfc:	b	4f2b50 <bpftime_map_delete_elem_helper+0xb4c>
  4f3c00:	ldr	x1, [x19, #112]
  4f3c04:	add	x22, x19, #0x68
  4f3c08:	add	x23, sp, #0x128
  4f3c0c:	mov	w0, w20
  4f3c10:	add	x25, sp, #0xf8
  4f3c14:	bl	4fb0f0 <_ZN5boost9unordered6detail15prime_fmod_sizeIvE8positionEmm>
  4f3c18:	mov	x1, x0
  4f3c1c:	mov	x8, x23
  4f3c20:	mov	x0, x22
  4f3c24:	bl	4e22f0 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEESD_EENSB_9allocatorISA_NSB_15segment_managerIcNSB_15rbtree_best_fitINSB_12mutex_familyESD_Lm0EEENSB_10iset_indexEEEEENS1_15prime_fmod_sizeIvEEE2atEm>
  4f3c28:	ldr	x0, [sp, #296]
  4f3c2c:	cmp	x0, #0x1
  4f3c30:	add	x1, x23, x0
  4f3c34:	csetm	x0, ne	// ne = any
  4f3c38:	and	x0, x0, x1
  4f3c3c:	sub	x2, x0, x25
  4f3c40:	ldr	x0, [x0]
  4f3c44:	cmp	x0, #0x1
  4f3c48:	csetm	x1, ne	// ne = any
  4f3c4c:	and	x1, x1, x2
  4f3c50:	add	x1, x1, x0
  4f3c54:	str	x1, [sp, #248]
  4f3c58:	cmp	x1, #0x1
  4f3c5c:	b.eq	4f2ce8 <bpftime_map_delete_elem_helper+0xce4>  // b.none
  4f3c60:	add	x0, x25, x1
  4f3c64:	ldr	w2, [x0, #8]
  4f3c68:	cmp	w20, w2
  4f3c6c:	b.ne	4f42ec <bpftime_map_delete_elem_helper+0x22e8>  // b.any
  4f3c70:	add	x20, sp, #0x110
  4f3c74:	add	x21, sp, #0x140
  4f3c78:	sub	x24, x25, x20
  4f3c7c:	mov	x0, x21
  4f3c80:	add	x2, x24, x1
  4f3c84:	mov	x1, x23
  4f3c88:	str	x2, [sp, #272]
  4f3c8c:	add	x26, sp, #0xc0
  4f3c90:	add	x27, sp, #0xc8
  4f3c94:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f3c98:	ldr	x3, [sp, #272]
  4f3c9c:	sub	x4, x20, x26
  4f3ca0:	mov	x0, x27
  4f3ca4:	mov	x1, x21
  4f3ca8:	cmp	x3, #0x1
  4f3cac:	csetm	x2, ne	// ne = any
  4f3cb0:	and	x2, x2, x4
  4f3cb4:	add	x2, x2, x3
  4f3cb8:	str	x2, [sp, #192]
  4f3cbc:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f3cc0:	ldr	x0, [sp, #192]
  4f3cc4:	cmp	x0, #0x1
  4f3cc8:	add	x3, x26, x0
  4f3ccc:	csetm	x5, ne	// ne = any
  4f3cd0:	ands	x5, x5, x3
  4f3cd4:	b.eq	4f2ce8 <bpftime_map_delete_elem_helper+0xce4>  // b.none
  4f3cd8:	ldp	x6, x4, [sp, #200]
  4f3cdc:	add	x1, sp, #0xe8
  4f3ce0:	sub	x27, x27, x1
  4f3ce4:	add	x2, sp, #0xe0
  4f3ce8:	sub	x20, x20, x21
  4f3cec:	add	x26, sp, #0xb0
  4f3cf0:	cmp	x6, #0x1
  4f3cf4:	csetm	x0, ne	// ne = any
  4f3cf8:	cmp	x4, #0x1
  4f3cfc:	and	x14, x0, x27
  4f3d00:	csetm	x13, ne	// ne = any
  4f3d04:	sub	x0, x3, x2
  4f3d08:	add	x14, x14, x6
  4f3d0c:	cmp	x0, #0x1
  4f3d10:	and	x13, x13, #0xffffffffffffffe0
  4f3d14:	add	x13, x13, x4
  4f3d18:	sub	x2, x2, x25
  4f3d1c:	csetm	x12, ne	// ne = any
  4f3d20:	add	x3, sp, #0x100
  4f3d24:	cmp	x14, #0x1
  4f3d28:	sub	x1, x1, x3
  4f3d2c:	and	x12, x12, x2
  4f3d30:	csetm	x11, ne	// ne = any
  4f3d34:	cmp	x13, #0x1
  4f3d38:	add	x12, x12, x0
  4f3d3c:	and	x11, x11, x1
  4f3d40:	csetm	x2, ne	// ne = any
  4f3d44:	add	x11, x11, x14
  4f3d48:	cmp	x12, #0x1
  4f3d4c:	mov	x1, #0xffffffffffffffe8    	// #-24
  4f3d50:	and	x2, x2, x1
  4f3d54:	add	x2, x2, x13
  4f3d58:	csetm	x4, ne	// ne = any
  4f3d5c:	add	x1, sp, #0x118
  4f3d60:	cmp	x11, #0x1
  4f3d64:	sub	x3, x3, x1
  4f3d68:	and	x4, x4, x24
  4f3d6c:	csetm	x9, ne	// ne = any
  4f3d70:	cmp	x2, #0x1
  4f3d74:	add	x4, x4, x12
  4f3d78:	and	x9, x9, x3
  4f3d7c:	csetm	x8, ne	// ne = any
  4f3d80:	add	x9, x9, x11
  4f3d84:	cmp	x4, #0x1
  4f3d88:	mov	x3, #0xffffffffffffffe8    	// #-24
  4f3d8c:	and	x8, x8, x3
  4f3d90:	csetm	x7, ne	// ne = any
  4f3d94:	add	x8, x8, x2
  4f3d98:	cmp	x9, #0x1
  4f3d9c:	add	x27, sp, #0x148
  4f3da0:	csetm	x6, ne	// ne = any
  4f3da4:	sub	x1, x1, x27
  4f3da8:	cmp	x8, #0x1
  4f3dac:	and	x7, x7, x20
  4f3db0:	and	x6, x6, x1
  4f3db4:	add	x7, x7, x4
  4f3db8:	csetm	x3, ne	// ne = any
  4f3dbc:	add	x6, x6, x9
  4f3dc0:	cmp	x7, #0x1
  4f3dc4:	mov	x1, #0xffffffffffffffd0    	// #-48
  4f3dc8:	and	x3, x3, x1
  4f3dcc:	add	x3, x3, x8
  4f3dd0:	csetm	x10, ne	// ne = any
  4f3dd4:	add	x24, sp, #0xa0
  4f3dd8:	cmp	x6, #0x1
  4f3ddc:	csetm	x15, ne	// ne = any
  4f3de0:	sub	x1, x27, x24
  4f3de4:	cmp	x3, #0x1
  4f3de8:	and	x15, x15, x1
  4f3dec:	csetm	x1, ne	// ne = any
  4f3df0:	ldr	w25, [x5, #24]
  4f3df4:	add	x15, x15, x6
  4f3df8:	stp	x0, x14, [sp, #224]
  4f3dfc:	mov	x0, #0xa8                  	// #168
  4f3e00:	and	x1, x1, x0
  4f3e04:	cmp	x15, #0x1
  4f3e08:	add	x1, x1, x3
  4f3e0c:	add	x5, sp, #0x78
  4f3e10:	sub	x16, x21, x5
  4f3e14:	csetm	x0, ne	// ne = any
  4f3e18:	cmp	x1, #0x1
  4f3e1c:	and	x10, x10, x16
  4f3e20:	sub	x16, x24, x26
  4f3e24:	stp	x13, x12, [sp, #240]
  4f3e28:	csetm	x13, ne	// ne = any
  4f3e2c:	add	x10, x10, x7
  4f3e30:	and	x0, x0, x16
  4f3e34:	and	x13, x13, #0xfffffffffffffff0
  4f3e38:	add	x0, x0, x15
  4f3e3c:	cmp	x10, #0x1
  4f3e40:	add	x12, sp, #0x80
  4f3e44:	stp	x1, x0, [sp, #168]
  4f3e48:	add	x1, x13, x1
  4f3e4c:	str	x1, [sp, #184]
  4f3e50:	csetm	x1, ne	// ne = any
  4f3e54:	cmp	x0, #0x1
  4f3e58:	stp	x11, x2, [sp, #256]
  4f3e5c:	sub	x2, x5, x12
  4f3e60:	add	x0, x26, x0
  4f3e64:	and	x1, x1, x2
  4f3e68:	csetm	x2, ne	// ne = any
  4f3e6c:	add	x1, x1, x10
  4f3e70:	and	x2, x2, x0
  4f3e74:	str	x10, [sp, #120]
  4f3e78:	str	x1, [sp, #128]
  4f3e7c:	add	x11, x12, x1
  4f3e80:	str	x15, [sp, #160]
  4f3e84:	stp	x4, x9, [sp, #272]
  4f3e88:	mov	x4, x2
  4f3e8c:	str	x8, [sp, #288]
  4f3e90:	stp	x7, x6, [sp, #320]
  4f3e94:	str	x3, [sp, #336]
  4f3e98:	ldr	x0, [x2]
  4f3e9c:	cmp	x0, #0x1
  4f3ea0:	add	x6, x2, x0
  4f3ea4:	csetm	x0, ne	// ne = any
  4f3ea8:	cmp	x1, #0x1
  4f3eac:	csetm	x3, ne	// ne = any
  4f3eb0:	and	x0, x0, x6
  4f3eb4:	and	x3, x3, x11
  4f3eb8:	cmp	x3, x0
  4f3ebc:	b.eq	4f3ee4 <bpftime_map_delete_elem_helper+0x1ee0>  // b.none
  4f3ec0:	ldr	x1, [x0]
  4f3ec4:	mov	x2, x0
  4f3ec8:	mov	x4, x0
  4f3ecc:	cmp	x1, #0x1
  4f3ed0:	add	x1, x0, x1
  4f3ed4:	csetm	x0, ne	// ne = any
  4f3ed8:	and	x0, x0, x1
  4f3edc:	cmp	x0, x3
  4f3ee0:	b.ne	4f3ec0 <bpftime_map_delete_elem_helper+0x1ebc>  // b.any
  4f3ee4:	ldr	x1, [x3]
  4f3ee8:	sub	x3, x3, x4
  4f3eec:	add	x20, sp, #0x130
  4f3ef0:	cmp	x1, #0x1
  4f3ef4:	csetm	x0, ne	// ne = any
  4f3ef8:	and	x0, x0, x3
  4f3efc:	add	x0, x0, x1
  4f3f00:	str	x0, [x2]
  4f3f04:	ldr	x0, [sp, #176]
  4f3f08:	cmp	x0, #0x1
  4f3f0c:	add	x0, x26, x0
  4f3f10:	csetm	x1, ne	// ne = any
  4f3f14:	and	x0, x1, x0
  4f3f18:	ldr	x2, [x0]
  4f3f1c:	cmp	x2, #0x1
  4f3f20:	b.eq	4f40a0 <bpftime_map_delete_elem_helper+0x209c>  // b.none
  4f3f24:	ldr	x0, [x28, #8]
  4f3f28:	sub	x5, x5, x23
  4f3f2c:	ldr	x6, [sp, #120]
  4f3f30:	sub	x0, x0, #0x1
  4f3f34:	str	x0, [x28, #8]
  4f3f38:	add	x2, x19, #0x140
  4f3f3c:	mov	w28, #0x1                   	// #1
  4f3f40:	add	x4, x19, #0x148
  4f3f44:	ldr	x0, [x19, #104]
  4f3f48:	strb	w28, [sp, #312]
  4f3f4c:	cmp	x0, #0x1
  4f3f50:	add	x22, x22, x0
  4f3f54:	csetm	x0, ne	// ne = any
  4f3f58:	and	x0, x0, x22
  4f3f5c:	cmp	x0, #0x0
  4f3f60:	sub	x0, x0, x26
  4f3f64:	sub	x0, x0, #0x1
  4f3f68:	csetm	x1, ne	// ne = any
  4f3f6c:	and	x1, x1, x0
  4f3f70:	cmp	x6, #0x1
  4f3f74:	csetm	x3, ne	// ne = any
  4f3f78:	add	x0, x1, #0x1
  4f3f7c:	cmp	x1, #0x0
  4f3f80:	add	x1, x26, x0
  4f3f84:	csetm	x0, ne	// ne = any
  4f3f88:	and	x3, x3, x5
  4f3f8c:	and	x0, x0, x1
  4f3f90:	add	x3, x3, x6
  4f3f94:	cmp	x0, #0x0
  4f3f98:	sub	x0, x0, x20
  4f3f9c:	sub	x0, x0, #0x1
  4f3fa0:	csetm	x1, ne	// ne = any
  4f3fa4:	and	x1, x1, x0
  4f3fa8:	add	x1, x1, #0x1
  4f3fac:	stp	x3, x1, [sp, #296]
  4f3fb0:	ldp	x5, x6, [x2, #16]
  4f3fb4:	ldr	x1, [x19, #328]
  4f3fb8:	cmp	x1, #0x1
  4f3fbc:	add	x0, x4, x1
  4f3fc0:	csetm	x4, ne	// ne = any
  4f3fc4:	add	x1, x5, x5, lsl #1
  4f3fc8:	and	x0, x4, x0
  4f3fcc:	add	x1, x0, x1, lsl #3
  4f3fd0:	cmp	x5, x6
  4f3fd4:	b.eq	4f4208 <bpftime_map_delete_elem_helper+0x2204>  // b.none
  4f3fd8:	cmp	x1, #0x0
  4f3fdc:	sub	x1, x1, x21
  4f3fe0:	sub	x1, x1, #0x1
  4f3fe4:	csetm	x0, ne	// ne = any
  4f3fe8:	and	x0, x0, x1
  4f3fec:	add	x0, x0, #0x1
  4f3ff0:	str	x0, [sp, #320]
  4f3ff4:	cmp	x0, #0x1
  4f3ff8:	add	x0, x21, x0
  4f3ffc:	csetm	x1, ne	// ne = any
  4f4000:	cmp	x3, #0x1
  4f4004:	and	x1, x1, x0
  4f4008:	csetm	x4, ne	// ne = any
  4f400c:	mov	x21, x1
  4f4010:	sub	x23, x23, x1
  4f4014:	and	x4, x4, x23
  4f4018:	add	x3, x4, x3
  4f401c:	str	x3, [x1]
  4f4020:	str	xzr, [x21, #8]!
  4f4024:	strb	wzr, [x21, #8]
  4f4028:	ldrb	w0, [sp, #312]
  4f402c:	tbnz	w0, #0, 4f4068 <bpftime_map_delete_elem_helper+0x2064>
  4f4030:	ldr	x0, [x2, #16]
  4f4034:	add	x0, x0, #0x1
  4f4038:	str	x0, [x2, #16]
  4f403c:	ldr	x0, [x19, #296]
  4f4040:	add	x1, x19, #0x128
  4f4044:	ldr	w2, [x19, #352]
  4f4048:	mov	x20, #0x0                   	// #0
  4f404c:	cmp	x0, #0x1
  4f4050:	add	x1, x1, x0
  4f4054:	csetm	x0, ne	// ne = any
  4f4058:	and	x0, x0, x1
  4f405c:	str	w2, [x0, w25, uxtw #2]
  4f4060:	str	w25, [x19, #352]
  4f4064:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f4068:	mov	x1, x20
  4f406c:	mov	x0, x21
  4f4070:	str	x2, [sp]
  4f4074:	bl	4dc9e4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_EC1ERKSI_>
  4f4078:	ldr	x2, [sp]
  4f407c:	strb	w28, [x21, #8]
  4f4080:	b	4f4030 <bpftime_map_delete_elem_helper+0x202c>
  4f4084:	mov	x2, #0x0                   	// #0
  4f4088:	b	4f2514 <bpftime_map_delete_elem_helper+0x510>
  4f408c:	bl	48c090 <__errno_location@plt>
  4f4090:	mov	x20, #0xffffffffffffffff    	// #-1
  4f4094:	mov	w1, #0x2                   	// #2
  4f4098:	str	w1, [x0]
  4f409c:	b	4f26f8 <bpftime_map_delete_elem_helper+0x6f4>
  4f40a0:	mov	x1, x26
  4f40a4:	mov	x0, x23
  4f40a8:	stp	x5, x2, [sp]
  4f40ac:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f40b0:	add	x7, sp, #0x98
  4f40b4:	ldp	x4, x0, [sp, #296]
  4f40b8:	sub	x8, x20, x7
  4f40bc:	add	x6, sp, #0x90
  4f40c0:	sub	x9, x23, x6
  4f40c4:	ldp	x5, x2, [sp]
  4f40c8:	cmp	x4, #0x1
  4f40cc:	csetm	x3, ne	// ne = any
  4f40d0:	cmp	x0, #0x1
  4f40d4:	csetm	x1, ne	// ne = any
  4f40d8:	and	x3, x3, x9
  4f40dc:	and	x1, x1, x8
  4f40e0:	add	x3, x3, x4
  4f40e4:	add	x1, x1, x0
  4f40e8:	stp	x3, x1, [sp, #144]
  4f40ec:	cmp	x1, #0x1
  4f40f0:	csetm	x8, ne	// ne = any
  4f40f4:	add	x4, x7, x1
  4f40f8:	and	x4, x8, x4
  4f40fc:	cmp	x3, #0x1
  4f4100:	add	x6, x6, x3
  4f4104:	cset	x0, eq	// eq = none
  4f4108:	sub	x0, x0, #0x1
  4f410c:	ldp	x3, x9, [x4]
  4f4110:	and	x0, x0, x6
  4f4114:	cmp	x3, #0x1
  4f4118:	add	x3, x4, x3
  4f411c:	csetm	x6, ne	// ne = any
  4f4120:	and	x3, x6, x3
  4f4124:	sub	x0, x0, x3
  4f4128:	asr	x0, x0, #3
  4f412c:	lsl	x0, x2, x0
  4f4130:	bic	x0, x9, x0
  4f4134:	str	x0, [x4, #8]
  4f4138:	cbnz	x0, 4f3f24 <bpftime_map_delete_elem_helper+0x1f20>
  4f413c:	add	x3, sp, #0x88
  4f4140:	dup	v0.2d, x2
  4f4144:	sub	x7, x7, x3
  4f4148:	and	x0, x8, x7
  4f414c:	add	x0, x0, x1
  4f4150:	cmp	x0, #0x1
  4f4154:	add	x3, x3, x0
  4f4158:	csetm	x0, ne	// ne = any
  4f415c:	and	x0, x0, x3
  4f4160:	add	x3, x0, #0x10
  4f4164:	add	x4, x0, #0x18
  4f4168:	ldp	x1, x6, [x0, #16]
  4f416c:	cmp	x1, #0x1
  4f4170:	add	x1, x1, x3
  4f4174:	csetm	x2, ne	// ne = any
  4f4178:	cmp	x6, #0x1
  4f417c:	and	x2, x2, x1
  4f4180:	csetm	x1, ne	// ne = any
  4f4184:	sub	x7, x0, x2
  4f4188:	and	x1, x1, x7
  4f418c:	add	x1, x1, x6
  4f4190:	str	x1, [x2, #24]
  4f4194:	ldp	x6, x1, [x0, #16]
  4f4198:	cmp	x1, #0x1
  4f419c:	add	x1, x4, x1
  4f41a0:	csetm	x2, ne	// ne = any
  4f41a4:	cmp	x6, #0x1
  4f41a8:	and	x2, x2, x1
  4f41ac:	csetm	x1, ne	// ne = any
  4f41b0:	add	x4, x2, #0x10
  4f41b4:	sub	x3, x3, x4
  4f41b8:	and	x1, x1, x3
  4f41bc:	add	x1, x1, x6
  4f41c0:	str	x1, [x2, #16]
  4f41c4:	str	q0, [x0, #16]
  4f41c8:	b	4f3f24 <bpftime_map_delete_elem_helper+0x1f20>
  4f41cc:	ldr	w0, [x19, #40]
  4f41d0:	mov	x1, x20
  4f41d4:	bl	1b42ea0 <bpf_map_delete_elem>
  4f41d8:	sxtw	x20, w0
  4f41dc:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f41e0:	ldr	x2, [x25, x3]
  4f41e4:	sub	x0, x0, x25
  4f41e8:	cmp	x2, #0x1
  4f41ec:	csetm	x3, ne	// ne = any
  4f41f0:	and	x3, x3, x0
  4f41f4:	add	x3, x3, x2
  4f41f8:	str	x3, [sp, #248]
  4f41fc:	cmp	x3, #0x1
  4f4200:	b.ne	4f32f8 <bpftime_map_delete_elem_helper+0x12f4>  // b.any
  4f4204:	b	4f282c <bpftime_map_delete_elem_helper+0x828>
  4f4208:	mov	x0, x2
  4f420c:	mov	x8, x21
  4f4210:	mov	x2, x23
  4f4214:	bl	538c50 <_ZN5boost9container6vectorINS_9unordered15node_handle_mapINS2_6detail4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEEjSA_NSC_9allocatorISB_NSC_15segment_managerIcNSC_15rbtree_best_fitINSC_12mutex_familyESE_Lm0EEENSC_10iset_indexEEEEEEENSG_ISO_SM_EEvE37priv_insert_forward_range_no_capacityINS0_3dtl20insert_emplace_proxyISP_JSO_EEEEENS0_12vec_iteratorINSD_ISO_lmLm0EEELb0EEEPSO_mT_NS_11move_detail17integral_constantIjLj2EEE.constprop.0>
  4f4218:	ldr	x0, [sp, #296]
  4f421c:	cmp	x0, #0x1
  4f4220:	b.eq	4f403c <bpftime_map_delete_elem_helper+0x2038>  // b.none
  4f4224:	ldr	x2, [sp, #304]
  4f4228:	add	x0, x23, x0
  4f422c:	sub	x1, x0, x26
  4f4230:	add	x21, sp, #0x98
  4f4234:	cmp	x2, #0x1
  4f4238:	add	x2, x20, x2
  4f423c:	csetm	x0, ne	// ne = any
  4f4240:	sub	x3, x26, x27
  4f4244:	and	x0, x0, x2
  4f4248:	cmp	x0, #0x0
  4f424c:	sub	x0, x0, x21
  4f4250:	csetm	x2, ne	// ne = any
  4f4254:	cmp	x1, #0x1
  4f4258:	sub	x0, x0, #0x1
  4f425c:	csetm	x20, ne	// ne = any
  4f4260:	and	x0, x2, x0
  4f4264:	and	x20, x20, x3
  4f4268:	add	x0, x0, #0x1
  4f426c:	add	x20, x20, x1
  4f4270:	str	x0, [sp, #152]
  4f4274:	stp	x21, x20, [sp, #320]
  4f4278:	cmp	x20, #0x1
  4f427c:	b.eq	4f403c <bpftime_map_delete_elem_helper+0x2038>  // b.none
  4f4280:	mov	x0, x24
  4f4284:	mov	x1, x21
  4f4288:	bl	4dc9e4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_EC1ERKSI_>
  4f428c:	sub	x2, x27, x26
  4f4290:	mov	x1, x26
  4f4294:	add	x2, x2, x20
  4f4298:	mov	x0, x21
  4f429c:	str	x2, [sp, #176]
  4f42a0:	bl	54fca4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f42a4:	b	4f403c <bpftime_map_delete_elem_helper+0x2038>
  4f42a8:	sub	x1, x1, #0x1d
  4f42ac:	adrp	x2, 2b0c000 <_ZTVN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0x8>
  4f42b0:	add	x2, x2, #0x3b8
  4f42b4:	ldr	x1, [x2, x1, lsl #3]
  4f42b8:	blr	x1
  4f42bc:	mov	x1, x0
  4f42c0:	b	4f32ac <bpftime_map_delete_elem_helper+0x12a8>
  4f42c4:	bl	48c090 <__errno_location@plt>
  4f42c8:	mov	x20, #0xffffffffffffffff    	// #-1
  4f42cc:	mov	w1, #0x16                  	// #22
  4f42d0:	str	w1, [x0]
  4f42d4:	b	4f2060 <bpftime_map_delete_elem_helper+0x5c>
  4f42d8:	bl	48c090 <__errno_location@plt>
  4f42dc:	mov	x20, #0xffffffffffffffff    	// #-1
  4f42e0:	mov	w1, #0x16                  	// #22
  4f42e4:	str	w1, [x0]
  4f42e8:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f42ec:	ldr	x2, [x25, x1]
  4f42f0:	sub	x0, x0, x25
  4f42f4:	cmp	x2, #0x1
  4f42f8:	csetm	x1, ne	// ne = any
  4f42fc:	and	x1, x1, x0
  4f4300:	add	x1, x1, x2
  4f4304:	str	x1, [sp, #248]
  4f4308:	cmp	x1, #0x1
  4f430c:	b.ne	4f3c60 <bpftime_map_delete_elem_helper+0x1c5c>  // b.any
  4f4310:	b	4f2ce8 <bpftime_map_delete_elem_helper+0xce4>
  4f4314:	mov	x20, x0
  4f4318:	mov	x0, x24
  4f431c:	bl	48c720 <pthread_spin_unlock@plt>
  4f4320:	mov	x0, x19
  4f4324:	bl	48c720 <pthread_spin_unlock@plt>
  4f4328:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f432c:	ldr	x0, [x0, #1160]
  4f4330:	ldr	x2, [sp, #344]
  4f4334:	ldr	x1, [x0]
  4f4338:	subs	x2, x2, x1
  4f433c:	mov	x1, #0x0                   	// #0
  4f4340:	b.eq	4f44f0 <bpftime_map_delete_elem_helper+0x24ec>  // b.none
  4f4344:	bl	48d490 <__stack_chk_fail@plt>
  4f4348:	bl	506114 <_ZN6spdlog18default_logger_rawEv>
  4f434c:	mov	w3, #0x20c                 	// #524
  4f4350:	adrp	x2, 1d3d000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe778>
  4f4354:	add	x2, x2, #0xe18
  4f4358:	str	x2, [sp, #96]
  4f435c:	str	w3, [sp, #104]
  4f4360:	add	x1, sp, #0x30
  4f4364:	adrp	x6, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f4368:	add	x6, x6, #0xed0
  4f436c:	ldp	x8, x9, [sp, #96]
  4f4370:	stp	x8, x9, [sp, #48]
  4f4374:	adrp	x2, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f4378:	str	x6, [sp, #112]
  4f437c:	add	x5, x22, #0x10
  4f4380:	str	x6, [x1, #16]
  4f4384:	mov	x4, x20
  4f4388:	add	x2, x2, #0xfa8
  4f438c:	mov	x3, #0x40                  	// #64
  4f4390:	bl	53b1e0 <_ZN6spdlog6logger4log_IJRjS2_EEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_.constprop.0>
  4f4394:	bl	48c090 <__errno_location@plt>
  4f4398:	mov	x20, #0xffffffffffffffff    	// #-1
  4f439c:	mov	w1, #0x16                  	// #22
  4f43a0:	str	w1, [x0]
  4f43a4:	b	4f26f8 <bpftime_map_delete_elem_helper+0x6f4>
  4f43a8:	bl	506114 <_ZN6spdlog18default_logger_rawEv>
  4f43ac:	mov	w3, #0x49                  	// #73
  4f43b0:	adrp	x2, 1d3d000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe778>
  4f43b4:	add	x2, x2, #0xcc0
  4f43b8:	str	x2, [sp, #96]
  4f43bc:	str	w3, [sp, #104]
  4f43c0:	add	x1, sp, #0x30
  4f43c4:	adrp	x5, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f43c8:	add	x5, x5, #0xed0
  4f43cc:	ldp	x6, x7, [sp, #96]
  4f43d0:	stp	x6, x7, [sp, #48]
  4f43d4:	adrp	x3, 1d3d000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe778>
  4f43d8:	str	x5, [sp, #112]
  4f43dc:	add	x3, x3, #0xd28
  4f43e0:	str	x5, [x1, #16]
  4f43e4:	mov	x4, #0x30                  	// #48
  4f43e8:	mov	w2, #0x4                   	// #4
  4f43ec:	bl	4b30e0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  4f43f0:	bl	48c090 <__errno_location@plt>
  4f43f4:	mov	x20, #0xffffffffffffffff    	// #-1
  4f43f8:	mov	w1, #0x16                  	// #22
  4f43fc:	str	w1, [x0]
  4f4400:	b	4f24b0 <bpftime_map_delete_elem_helper+0x4ac>
  4f4404:	bl	506114 <_ZN6spdlog18default_logger_rawEv>
  4f4408:	mov	w3, #0x202                 	// #514
  4f440c:	adrp	x2, 1d3d000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe778>
  4f4410:	add	x2, x2, #0xe18
  4f4414:	str	x2, [sp, #96]
  4f4418:	str	w3, [sp, #104]
  4f441c:	add	x1, sp, #0x30
  4f4420:	adrp	x5, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f4424:	add	x5, x5, #0xed0
  4f4428:	ldp	x6, x7, [sp, #96]
  4f442c:	stp	x6, x7, [sp, #48]
  4f4430:	adrp	x3, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4f4434:	str	x5, [sp, #112]
  4f4438:	add	x3, x3, #0xf70
  4f443c:	str	x5, [x1, #16]
  4f4440:	mov	x4, #0x33                  	// #51
  4f4444:	mov	w2, #0x4                   	// #4
  4f4448:	bl	4b30e0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  4f444c:	b	4f4394 <bpftime_map_delete_elem_helper+0x2390>
  4f4450:	mov	x19, x0
  4f4454:	mov	x0, x23
  4f4458:	bl	4e1ab0 <_ZN5boost9unordered15node_handle_mapINS0_6detail4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEEjS8_NSA_9allocatorIS9_NSA_15segment_managerIcNSA_15rbtree_best_fitINSA_12mutex_familyESC_Lm0EEENSA_10iset_indexEEEEEED1Ev>
  4f445c:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f4460:	ldr	x0, [x0, #1160]
  4f4464:	ldr	x2, [sp, #344]
  4f4468:	ldr	x1, [x0]
  4f446c:	subs	x2, x2, x1
  4f4470:	mov	x1, #0x0                   	// #0
  4f4474:	b.ne	4f4344 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f4478:	mov	x0, x19
  4f447c:	bl	48c590 <_Unwind_Resume@plt>
  4f4480:	adrp	x1, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f4484:	ldr	x1, [x1, #1160]
  4f4488:	ldr	x3, [sp, #344]
  4f448c:	ldr	x2, [x1]
  4f4490:	subs	x3, x3, x2
  4f4494:	mov	x2, #0x0                   	// #0
  4f4498:	b.ne	4f4344 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f449c:	cmp	w0, #0xff
  4f44a0:	cset	w0, eq	// eq = none
  4f44a4:	bl	48da54 <_ZSt26__throw_bad_variant_accessb>
  4f44a8:	mov	x1, x0
  4f44ac:	mov	x0, x19
  4f44b0:	mov	x19, x1
  4f44b4:	bl	48c720 <pthread_spin_unlock@plt>
  4f44b8:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f44bc:	ldr	x0, [x0, #1160]
  4f44c0:	ldr	x2, [sp, #344]
  4f44c4:	ldr	x1, [x0]
  4f44c8:	subs	x2, x2, x1
  4f44cc:	mov	x1, #0x0                   	// #0
  4f44d0:	b.eq	4f4478 <bpftime_map_delete_elem_helper+0x2474>  // b.none
  4f44d4:	b	4f4344 <bpftime_map_delete_elem_helper+0x2340>
  4f44d8:	mov	x20, x0
  4f44dc:	b	4f4320 <bpftime_map_delete_elem_helper+0x231c>
  4f44e0:	mov	x20, x0
  4f44e4:	mov	x0, x21
  4f44e8:	bl	4df2e0 <_ZN5boost12interprocess9ipcdetail12shared_countIN7bpftime18lru_linklist_entryENS0_9allocatorIvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIS4_SD_EEED1Ev>
  4f44ec:	b	4f4320 <bpftime_map_delete_elem_helper+0x231c>
  4f44f0:	mov	x0, x20
  4f44f4:	bl	48c590 <_Unwind_Resume@plt>

Disassembly of section .fini:
