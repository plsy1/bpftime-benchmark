
/home/jetson/src/results/uprobe/array-jit-default-20261021/artifacts/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

000000000052f1a0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0>:
  52f1a0:	sub	sp, sp, #0x1c0
  52f1a4:	adrp	x2, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  52f1a8:	ldr	x2, [x2, #1160]
  52f1ac:	stp	x29, x30, [sp, #352]
  52f1b0:	add	x29, sp, #0x160
  52f1b4:	stp	x19, x20, [sp, #368]
  52f1b8:	mov	x19, x0
  52f1bc:	ldr	w0, [x0, #72]
  52f1c0:	stp	x23, x24, [sp, #400]
  52f1c4:	mov	x20, x1
  52f1c8:	stp	x25, x26, [sp, #416]
  52f1cc:	stp	x27, x28, [sp, #432]
  52f1d0:	ldr	x3, [x2]
  52f1d4:	str	x3, [sp, #344]
  52f1d8:	mov	x3, #0x0                   	// #0
  52f1dc:	cmp	w0, #0x1e
  52f1e0:	b.gt	52f278 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xd8>
  52f1e4:	cmp	w0, #0x0
  52f1e8:	b.le	52f1f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x58>
  52f1ec:	sub	w1, w0, #0x1
  52f1f0:	cmp	w1, #0x1d
  52f1f4:	b.ls	52f25c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xbc>  // b.plast
  52f1f8:	sbfiz	x2, x0, #1, #32
  52f1fc:	adrp	x1, 1d09000 <_ZTSN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0xd0>
  52f200:	add	x0, x2, w0, sxtw
  52f204:	add	x1, x1, #0xd70
  52f208:	add	x0, x1, x0, lsl #4
  52f20c:	ldr	x3, [x0, #32]
  52f210:	cbz	x3, 5314d4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2334>
  52f214:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  52f218:	ldr	x0, [x0, #1160]
  52f21c:	ldr	x2, [sp, #344]
  52f220:	ldr	x1, [x0]
  52f224:	subs	x2, x2, x1
  52f228:	mov	x1, #0x0                   	// #0
  52f22c:	b.ne	531410 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2270>  // b.any
  52f230:	ldp	x29, x30, [sp, #352]
  52f234:	mov	x1, x20
  52f238:	ldp	x23, x24, [sp, #400]
  52f23c:	mov	x16, x3
  52f240:	ldp	x25, x26, [sp, #416]
  52f244:	mov	w2, #0x0                   	// #0
  52f248:	ldp	x27, x28, [sp, #432]
  52f24c:	ldr	w0, [x19, #112]
  52f250:	ldp	x19, x20, [sp, #368]
  52f254:	add	sp, sp, #0x1c0
  52f258:	br	x16
  52f25c:	adrp	x2, 1d24000 <_ZTSZN7bpftime20llvm_bpf_jit_context14generateModuleERKSt6vectorINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESaIS7_EESB_bbRKS7_bEUlPN4llvm5ValueESG_E_+0x78>
  52f260:	add	x2, x2, #0x28
  52f264:	stp	x21, x22, [sp, #384]
  52f268:	ldrh	w2, [x2, w1, uxtw #1]
  52f26c:	adr	x1, 52f278 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xd8>
  52f270:	add	x2, x1, w2, sxth #2
  52f274:	br	x2
  52f278:	cmp	w0, #0x3ec
  52f27c:	b.eq	52f344 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1a4>  // b.none
  52f280:	b.gt	52fd3c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb9c>
  52f284:	cmp	w0, #0x3e9
  52f288:	b.eq	52fd6c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xbcc>  // b.none
  52f28c:	cmp	w0, #0x3ea
  52f290:	b.ne	52f1f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x58>  // b.any
  52f294:	ldr	x0, [x19, #120]
  52f298:	add	x19, x19, #0x78
  52f29c:	cmp	x0, #0x1
  52f2a0:	add	x0, x19, x0
  52f2a4:	csetm	x19, ne	// ne = any
  52f2a8:	and	x19, x19, x0
  52f2ac:	ldrb	w0, [x19, #48]
  52f2b0:	tbnz	w0, #0, 5302f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1158>
  52f2b4:	ldr	w0, [x19, #36]
  52f2b8:	ldr	w3, [x1]
  52f2bc:	cmp	w3, w0
  52f2c0:	b.cs	5312c8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2128>  // b.hs, b.nlast
  52f2c4:	ldr	w5, [x19, #32]
  52f2c8:	add	x4, x19, #0x8
  52f2cc:	ldr	x0, [x19, #8]
  52f2d0:	mov	w2, w5
  52f2d4:	mov	w1, #0x0                   	// #0
  52f2d8:	mul	w3, w3, w5
  52f2dc:	add	x4, x4, x0
  52f2e0:	cmp	x0, #0x1
  52f2e4:	csetm	x0, ne	// ne = any
  52f2e8:	and	x0, x0, x4
  52f2ec:	add	x0, x0, x3
  52f2f0:	bl	48bc60 <memset@plt>
  52f2f4:	nop
  52f2f8:	mov	x20, #0x0                   	// #0
  52f2fc:	nop
  52f300:	adrp	x1, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  52f304:	ldr	x1, [x1, #1160]
  52f308:	ldr	x0, [sp, #344]
  52f30c:	ldr	x2, [x1]
  52f310:	subs	x0, x0, x2
  52f314:	mov	x2, #0x0                   	// #0
  52f318:	b.ne	531410 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2270>  // b.any
  52f31c:	ldp	x29, x30, [sp, #352]
  52f320:	mov	x0, x20
  52f324:	ldp	x19, x20, [sp, #368]
  52f328:	ldp	x23, x24, [sp, #400]
  52f32c:	ldp	x25, x26, [sp, #416]
  52f330:	ldp	x27, x28, [sp, #432]
  52f334:	add	sp, sp, #0x1c0
  52f338:	ret
  52f33c:	ldp	x21, x22, [sp, #384]
  52f340:	b	52f1f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x58>
  52f344:	ldr	x0, [x19, #120]
  52f348:	add	x19, x19, #0x78
  52f34c:	ldr	w1, [x1]
  52f350:	cmp	x0, #0x1
  52f354:	add	x19, x19, x0
  52f358:	csetm	x0, ne	// ne = any
  52f35c:	and	x0, x0, x19
  52f360:	ldr	w0, [x0, #12]
  52f364:	str	w1, [sp, #92]
  52f368:	tbnz	w1, #31, 5313c0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2220>
  52f36c:	mov	w0, w0
  52f370:	cmp	x0, w1, sxtw
  52f374:	b.ls	5313c0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2220>  // b.plast
  52f378:	bl	5044d0 <_ZN6spdlog18default_logger_rawEv>
  52f37c:	mov	w3, #0xca                  	// #202
  52f380:	adrp	x2, 1d33000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe788>
  52f384:	add	x2, x2, #0xf98
  52f388:	str	x2, [sp, #96]
  52f38c:	str	w3, [sp, #104]
  52f390:	add	x1, sp, #0x30
  52f394:	adrp	x6, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  52f398:	add	x6, x6, #0xc20
  52f39c:	ldp	x8, x9, [sp, #96]
  52f3a0:	stp	x8, x9, [sp, #48]
  52f3a4:	add	x5, sp, #0x5c
  52f3a8:	str	x6, [sp, #112]
  52f3ac:	mov	x4, #0x43                  	// #67
  52f3b0:	str	x6, [x1, #16]
  52f3b4:	adrp	x3, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  52f3b8:	mov	w2, #0x2                   	// #2
  52f3bc:	add	x3, x3, #0xc78
  52f3c0:	bl	4c88f0 <_ZN6spdlog6logger4log_IJRKiEEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_>
  52f3c4:	b	52f2f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x158>
  52f3c8:	bl	48c090 <__errno_location@plt>
  52f3cc:	ldp	x21, x22, [sp, #384]
  52f3d0:	mov	w1, #0x16                  	// #22
  52f3d4:	str	w1, [x0]
  52f3d8:	mov	x20, #0xffffffffffffffff    	// #-1
  52f3dc:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52f3e0:	ldr	x0, [x19, #120]
  52f3e4:	add	x1, x19, #0x78
  52f3e8:	add	x19, x19, #0x74
  52f3ec:	cmp	x0, #0x1
  52f3f0:	add	x1, x1, x0
  52f3f4:	csetm	x28, ne	// ne = any
  52f3f8:	mov	x0, x19
  52f3fc:	and	x28, x28, x1
  52f400:	bl	48cea0 <pthread_spin_lock@plt>
  52f404:	ldr	x27, [x28]
  52f408:	cmp	x27, #0x4
  52f40c:	b.eq	530790 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x15f0>  // b.none
  52f410:	mov	x1, x20
  52f414:	add	x2, x27, x20
  52f418:	mov	x0, #0x0                   	// #0
  52f41c:	cbz	x27, 53116c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fcc>
  52f420:	ldrb	w3, [x1], #1
  52f424:	lsl	x4, x0, #5
  52f428:	sub	x0, x4, x0
  52f42c:	add	x0, x3, x0
  52f430:	cmp	x2, x1
  52f434:	b.ne	52f420 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x280>  // b.any
  52f438:	mov	x2, x0
  52f43c:	ldr	x23, [x28, #24]
  52f440:	add	x1, x28, #0x38
  52f444:	ldp	x25, x26, [x28, #8]
  52f448:	umulh	x23, x23, x2
  52f44c:	ldr	x2, [x28, #56]
  52f450:	msub	x23, x26, x23, x0
  52f454:	mov	x0, x1
  52f458:	add	x25, x27, x25
  52f45c:	add	x0, x0, x2
  52f460:	cmp	x26, x23
  52f464:	sub	x1, x23, x26
  52f468:	csel	x23, x1, x23, ls	// ls = plast
  52f46c:	cmp	x2, #0x1
  52f470:	csetm	x24, ne	// ne = any
  52f474:	mov	x21, x23
  52f478:	add	x25, x25, #0x4
  52f47c:	and	x24, x24, x0
  52f480:	b	52f498 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2f8>
  52f484:	add	x21, x21, #0x1
  52f488:	cmp	x26, x21
  52f48c:	csel	x21, x21, xzr, ne	// ne = any
  52f490:	cmp	x21, x23
  52f494:	b.eq	52f4e0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x340>  // b.none
  52f498:	mul	x22, x21, x25
  52f49c:	ldr	w0, [x24, x22]
  52f4a0:	cbz	w0, 52f4e0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x340>
  52f4a4:	cmp	w0, #0x1
  52f4a8:	b.ne	52f484 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2e4>  // b.any
  52f4ac:	add	x1, x22, #0x4
  52f4b0:	add	x0, x24, x1
  52f4b4:	cmp	x27, #0x4
  52f4b8:	b.eq	5307c8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1628>  // b.none
  52f4bc:	mov	x2, x27
  52f4c0:	mov	x1, x20
  52f4c4:	bl	48d570 <memcmp@plt>
  52f4c8:	cbnz	w0, 52f484 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2e4>
  52f4cc:	ldr	x0, [x28, #40]
  52f4d0:	mov	w1, #0x2                   	// #2
  52f4d4:	str	w1, [x24, x22]
  52f4d8:	sub	x0, x0, #0x1
  52f4dc:	str	x0, [x28, #40]
  52f4e0:	mov	x0, x19
  52f4e4:	bl	48c720 <pthread_spin_unlock@plt>
  52f4e8:	ldp	x21, x22, [sp, #384]
  52f4ec:	b	52f2f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x158>
  52f4f0:	ldr	x0, [x19, #120]
  52f4f4:	add	x19, x19, #0x78
  52f4f8:	ldr	w1, [x20]
  52f4fc:	cmp	x0, #0x1
  52f500:	add	x19, x19, x0
  52f504:	csetm	x0, ne	// ne = any
  52f508:	and	x0, x0, x19
  52f50c:	tbnz	w1, #31, 52f3c8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x228>
  52f510:	ldr	x2, [x0, #16]
  52f514:	sxtw	x1, w1
  52f518:	cmp	x1, x2
  52f51c:	b.cs	52f3c8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x228>  // b.hs, b.nlast
  52f520:	ldr	x2, [x0, #8]
  52f524:	add	x0, x0, #0x8
  52f528:	ldp	x21, x22, [sp, #384]
  52f52c:	cmp	x2, #0x1
  52f530:	add	x0, x0, x2
  52f534:	csetm	x2, ne	// ne = any
  52f538:	and	x0, x2, x0
  52f53c:	mov	w3, #0xffffffff            	// #-1
  52f540:	str	w3, [x0, x1, lsl #2]
  52f544:	b	52f2f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x158>
  52f548:	ldr	x0, [x19, #120]
  52f54c:	add	x19, x19, #0x78
  52f550:	cmp	x0, #0x1
  52f554:	add	x0, x19, x0
  52f558:	csetm	x19, ne	// ne = any
  52f55c:	and	x19, x19, x0
  52f560:	bl	48bb80 <sched_getcpu@plt>
  52f564:	ldr	w2, [x19, #144]
  52f568:	cmp	w2, #0x4
  52f56c:	b.eq	52fd9c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xbfc>  // b.none
  52f570:	ldr	x1, [x19, #200]
  52f574:	add	x3, x19, #0xc8
  52f578:	sbfiz	x0, x0, #5, #32
  52f57c:	mov	w2, w2
  52f580:	cmp	x1, #0x1
  52f584:	add	x3, x3, x1
  52f588:	csetm	x21, ne	// ne = any
  52f58c:	mov	x1, x20
  52f590:	and	x21, x21, x3
  52f594:	add	x20, sp, #0xe0
  52f598:	add	x21, x21, x0
  52f59c:	add	x0, x21, #0x8
  52f5a0:	ldr	x3, [x21, #8]
  52f5a4:	cmp	x3, #0x1
  52f5a8:	add	x0, x0, x3
  52f5ac:	csetm	x3, ne	// ne = any
  52f5b0:	and	x0, x3, x0
  52f5b4:	bl	48c120 <memcpy@plt>
  52f5b8:	mov	x8, x20
  52f5bc:	mov	x0, x19
  52f5c0:	mov	x1, x21
  52f5c4:	bl	4fae54 <_ZNK5boost9unordered6detail5tableINS1_3mapINS_12interprocess9allocatorISt4pairIKNS_9container6vectorIhNS5_IhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEESI_ESG_EESI_SI_N7bpftime24per_cpu_bytes_vec_hasherENSM_23per_cpu_bytes_vec_equalEEEE4findISI_EENS1_15iterator_detail8iteratorINS1_4nodeISK_SD_EENS1_6bucketISV_SD_EEEERKT_>
  52f5c8:	ldr	x0, [sp, #224]
  52f5cc:	cmp	x0, #0x1
  52f5d0:	add	x20, x20, x0
  52f5d4:	csetm	x0, ne	// ne = any
  52f5d8:	tst	x0, x20
  52f5dc:	b.eq	52fdac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc0c>  // b.none
  52f5e0:	ldp	x3, x2, [sp, #232]
  52f5e4:	add	x1, sp, #0xf8
  52f5e8:	sub	x20, x20, x1
  52f5ec:	mov	x0, #0xffffffffffffffe8    	// #-24
  52f5f0:	add	x21, sp, #0x128
  52f5f4:	add	x6, sp, #0x140
  52f5f8:	add	x23, sp, #0x130
  52f5fc:	cmp	x3, #0x1
  52f600:	csetm	x8, ne	// ne = any
  52f604:	cmp	x2, #0x1
  52f608:	csetm	x9, ne	// ne = any
  52f60c:	and	x8, x8, x0
  52f610:	cmp	x20, #0x1
  52f614:	add	x0, sp, #0x110
  52f618:	add	x8, x8, x3
  52f61c:	sub	x1, x1, x0
  52f620:	mov	x3, #0xffffffffffffffe8    	// #-24
  52f624:	csetm	x7, ne	// ne = any
  52f628:	and	x9, x9, x3
  52f62c:	cmp	x8, #0x1
  52f630:	add	x9, x9, x2
  52f634:	and	x7, x7, x1
  52f638:	add	x7, x7, x20
  52f63c:	mov	x1, x3
  52f640:	csetm	x5, ne	// ne = any
  52f644:	cmp	x9, #0x1
  52f648:	csetm	x3, ne	// ne = any
  52f64c:	and	x5, x5, x1
  52f650:	cmp	x7, #0x1
  52f654:	add	x5, x5, x8
  52f658:	csetm	x2, ne	// ne = any
  52f65c:	and	x3, x3, x1
  52f660:	sub	x0, x0, x21
  52f664:	add	x3, x3, x9
  52f668:	and	x2, x2, x0
  52f66c:	cmp	x5, #0x1
  52f670:	add	x22, x2, x7
  52f674:	csetm	x26, ne	// ne = any
  52f678:	cmp	x3, #0x1
  52f67c:	and	x26, x26, x1
  52f680:	csetm	x25, ne	// ne = any
  52f684:	cmp	x22, #0x1
  52f688:	add	x26, x26, x5
  52f68c:	csetm	x24, ne	// ne = any
  52f690:	and	x25, x25, x1
  52f694:	sub	x1, x21, x6
  52f698:	add	x25, x25, x3
  52f69c:	cmp	x26, #0x1
  52f6a0:	and	x1, x24, x1
  52f6a4:	csetm	x28, ne	// ne = any
  52f6a8:	add	x1, x1, x22
  52f6ac:	cmp	x25, #0x1
  52f6b0:	csetm	x27, ne	// ne = any
  52f6b4:	add	x0, sp, #0x148
  52f6b8:	cmp	x1, #0x1
  52f6bc:	add	x2, x6, x1
  52f6c0:	csetm	x10, ne	// ne = any
  52f6c4:	sub	x4, x23, x0
  52f6c8:	and	x10, x10, x2
  52f6cc:	and	x4, x28, x4
  52f6d0:	mov	x2, #0xffffffffffffffe8    	// #-24
  52f6d4:	and	x2, x27, x2
  52f6d8:	add	x2, x2, x25
  52f6dc:	add	x4, x4, x26
  52f6e0:	stp	x20, x8, [sp, #248]
  52f6e4:	sub	x11, x10, x6
  52f6e8:	add	x20, sp, #0x138
  52f6ec:	stp	x9, x7, [sp, #264]
  52f6f0:	stp	x5, x3, [sp, #280]
  52f6f4:	stp	x22, x26, [sp, #296]
  52f6f8:	stp	x25, x1, [sp, #312]
  52f6fc:	stp	x4, x2, [sp, #328]
  52f700:	ldr	x2, [x10]
  52f704:	cmp	x2, #0x1
  52f708:	csetm	x1, ne	// ne = any
  52f70c:	and	x1, x1, x11
  52f710:	add	x1, x1, x2
  52f714:	str	x1, [sp, #320]
  52f718:	cmp	x1, #0x1
  52f71c:	b.eq	530b80 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x19e0>  // b.none
  52f720:	add	x8, sp, #0xa0
  52f724:	add	x9, sp, #0xa8
  52f728:	sub	x4, x23, x8
  52f72c:	sub	x3, x20, x9
  52f730:	and	x4, x4, x28
  52f734:	and	x3, x3, x27
  52f738:	add	x4, x4, x26
  52f73c:	add	x3, x3, x25
  52f740:	cmp	x4, #0x1
  52f744:	add	x6, x8, x4
  52f748:	csetm	x5, ne	// ne = any
  52f74c:	stp	x4, x3, [sp, #160]
  52f750:	and	x6, x5, x6
  52f754:	add	x2, x21, x22
  52f758:	mov	x7, x6
  52f75c:	and	x2, x2, x24
  52f760:	ldr	x0, [x6]
  52f764:	cmp	x0, #0x1
  52f768:	add	x1, x6, x0
  52f76c:	csetm	x0, ne	// ne = any
  52f770:	and	x0, x0, x1
  52f774:	cmp	x0, x2
  52f778:	b.eq	52f7a4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x604>  // b.none
  52f77c:	nop
  52f780:	ldr	x1, [x0]
  52f784:	mov	x6, x0
  52f788:	mov	x7, x0
  52f78c:	cmp	x1, #0x1
  52f790:	add	x1, x0, x1
  52f794:	csetm	x0, ne	// ne = any
  52f798:	and	x0, x0, x1
  52f79c:	cmp	x0, x2
  52f7a0:	b.ne	52f780 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x5e0>  // b.any
  52f7a4:	cmp	x3, #0x1
  52f7a8:	add	x1, sp, #0xb8
  52f7ac:	add	x26, sp, #0xb0
  52f7b0:	csetm	x0, ne	// ne = any
  52f7b4:	sub	x9, x9, x1
  52f7b8:	sub	x1, x8, x26
  52f7bc:	and	x5, x5, x1
  52f7c0:	and	x0, x0, x9
  52f7c4:	add	x0, x0, x3
  52f7c8:	add	x5, x5, x4
  52f7cc:	stp	x5, x0, [sp, #176]
  52f7d0:	add	x20, sp, #0xc0
  52f7d4:	add	x28, sp, #0x98
  52f7d8:	ldr	x0, [x6]
  52f7dc:	cmp	x0, #0x1
  52f7e0:	add	x0, x0, x7
  52f7e4:	csetm	x1, ne	// ne = any
  52f7e8:	and	x1, x1, x0
  52f7ec:	sub	x0, x1, x7
  52f7f0:	ldr	x2, [x1]
  52f7f4:	cmp	x2, #0x1
  52f7f8:	csetm	x1, ne	// ne = any
  52f7fc:	and	x0, x1, x0
  52f800:	add	x0, x0, x2
  52f804:	str	x0, [x6]
  52f808:	ldr	x0, [sp, #176]
  52f80c:	cmp	x0, #0x1
  52f810:	add	x0, x26, x0
  52f814:	csetm	x1, ne	// ne = any
  52f818:	and	x0, x1, x0
  52f81c:	ldr	x22, [x0]
  52f820:	cmp	x22, #0x1
  52f824:	b.eq	530bc0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1a20>  // b.none
  52f828:	ldr	x3, [sp, #296]
  52f82c:	sub	x0, x21, x28
  52f830:	add	x2, x19, #0x20
  52f834:	cmp	x3, #0x1
  52f838:	csetm	x1, ne	// ne = any
  52f83c:	and	x1, x1, x0
  52f840:	add	x1, x1, x3
  52f844:	str	x1, [sp, #152]
  52f848:	add	x3, x28, x1
  52f84c:	sub	x28, x28, x20
  52f850:	ldr	x0, [x19, #32]
  52f854:	cmp	x0, #0x1
  52f858:	add	x2, x2, x0
  52f85c:	csetm	x0, ne	// ne = any
  52f860:	and	x0, x0, x2
  52f864:	cmp	x0, #0x0
  52f868:	sub	x0, x0, x26
  52f86c:	csetm	x2, ne	// ne = any
  52f870:	cmp	x1, #0x1
  52f874:	csetm	x1, ne	// ne = any
  52f878:	sub	x0, x0, #0x1
  52f87c:	and	x1, x1, x3
  52f880:	and	x2, x2, x0
  52f884:	add	x0, x1, #0x8
  52f888:	add	x1, x2, #0x1
  52f88c:	sub	x2, x0, x20
  52f890:	str	x1, [sp, #176]
  52f894:	cmp	x2, #0x1
  52f898:	csetm	x21, ne	// ne = any
  52f89c:	and	x21, x21, x0
  52f8a0:	add	x0, x21, #0x20
  52f8a4:	bl	4dcc50 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvED1Ev>
  52f8a8:	mov	x0, x21
  52f8ac:	bl	4dcc50 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvED1Ev>
  52f8b0:	ldr	x3, [sp, #152]
  52f8b4:	mov	x0, x26
  52f8b8:	mov	x1, x20
  52f8bc:	cmp	x3, #0x1
  52f8c0:	csetm	x2, ne	// ne = any
  52f8c4:	and	x2, x2, x28
  52f8c8:	add	x2, x2, x3
  52f8cc:	str	x2, [sp, #192]
  52f8d0:	bl	54fc90 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  52f8d4:	ldr	x0, [x19, #8]
  52f8d8:	ldp	x21, x22, [sp, #384]
  52f8dc:	sub	x0, x0, #0x1
  52f8e0:	str	x0, [x19, #8]
  52f8e4:	b	52f2f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x158>
  52f8e8:	ldr	x0, [x19, #120]
  52f8ec:	add	x1, x19, #0x78
  52f8f0:	add	x19, x19, #0x74
  52f8f4:	cmp	x0, #0x1
  52f8f8:	add	x1, x1, x0
  52f8fc:	csetm	x21, ne	// ne = any
  52f900:	mov	x0, x19
  52f904:	and	x21, x21, x1
  52f908:	bl	48cea0 <pthread_spin_lock@plt>
  52f90c:	ldr	w0, [x20]
  52f910:	tbnz	w0, #31, 5313d4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2234>
  52f914:	ldr	x1, [x21, #16]
  52f918:	sxtw	x0, w0
  52f91c:	cmp	x0, x1
  52f920:	b.cs	5313d4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2234>  // b.hs, b.nlast
  52f924:	ldr	x1, [x21, #8]
  52f928:	add	x21, x21, #0x8
  52f92c:	mov	w2, #0xffffffff            	// #-1
  52f930:	mov	x20, #0x0                   	// #0
  52f934:	cmp	x1, #0x1
  52f938:	add	x21, x21, x1
  52f93c:	csetm	x1, ne	// ne = any
  52f940:	and	x1, x1, x21
  52f944:	str	w2, [x1, x0, lsl #2]
  52f948:	mov	x0, x19
  52f94c:	bl	48c720 <pthread_spin_unlock@plt>
  52f950:	ldp	x21, x22, [sp, #384]
  52f954:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52f958:	bl	5044d0 <_ZN6spdlog18default_logger_rawEv>
  52f95c:	mov	x20, #0xffffffffffffffff    	// #-1
  52f960:	mov	w3, #0x7e                  	// #126
  52f964:	adrp	x2, 1d33000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe788>
  52f968:	add	x2, x2, #0xbf0
  52f96c:	str	x2, [sp, #96]
  52f970:	str	w3, [sp, #104]
  52f974:	add	x1, sp, #0x30
  52f978:	adrp	x5, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  52f97c:	add	x5, x5, #0xc20
  52f980:	ldp	x6, x7, [sp, #96]
  52f984:	stp	x6, x7, [sp, #48]
  52f988:	adrp	x3, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  52f98c:	str	x5, [sp, #112]
  52f990:	add	x3, x3, #0xc30
  52f994:	str	x5, [x1, #16]
  52f998:	mov	x4, #0x41                  	// #65
  52f99c:	mov	w2, #0x4                   	// #4
  52f9a0:	bl	4b30e0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  52f9a4:	bl	48c090 <__errno_location@plt>
  52f9a8:	ldp	x21, x22, [sp, #384]
  52f9ac:	mov	w1, #0x5f                  	// #95
  52f9b0:	str	w1, [x0]
  52f9b4:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52f9b8:	ldr	x0, [x19, #120]
  52f9bc:	add	x1, x19, #0x78
  52f9c0:	add	x19, x19, #0x74
  52f9c4:	cmp	x0, #0x1
  52f9c8:	add	x1, x1, x0
  52f9cc:	csetm	x21, ne	// ne = any
  52f9d0:	mov	x0, x19
  52f9d4:	and	x21, x21, x1
  52f9d8:	add	x20, x21, #0x8
  52f9dc:	bl	48cea0 <pthread_spin_lock@plt>
  52f9e0:	ldr	x9, [x20, #16]
  52f9e4:	cbz	x9, 52fcac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb0c>
  52f9e8:	ldr	x10, [x21, #16]
  52f9ec:	add	x5, sp, #0x140
  52f9f0:	add	x11, x20, #0x8
  52f9f4:	add	x2, sp, #0x110
  52f9f8:	cmp	x10, #0x1
  52f9fc:	sub	x0, x11, x5
  52fa00:	csetm	x8, ne	// ne = any
  52fa04:	ldr	w3, [x21]
  52fa08:	and	x0, x8, x0
  52fa0c:	sub	x6, x5, x2
  52fa10:	add	x0, x0, x10
  52fa14:	sub	x3, x9, x3
  52fa18:	cmp	x0, #0x1
  52fa1c:	add	x7, sp, #0xf8
  52fa20:	csetm	x1, ne	// ne = any
  52fa24:	sub	x12, x2, x7
  52fa28:	and	x6, x1, x6
  52fa2c:	add	x4, sp, #0x128
  52fa30:	add	x6, x6, x0
  52fa34:	sub	x2, x5, x4
  52fa38:	add	x6, x6, x3
  52fa3c:	and	x1, x1, x2
  52fa40:	cmp	x6, #0x1
  52fa44:	add	x0, x0, x9
  52fa48:	csetm	x3, ne	// ne = any
  52fa4c:	add	x0, x1, x0
  52fa50:	and	x3, x3, x12
  52fa54:	add	x2, sp, #0xe0
  52fa58:	add	x3, x3, x6
  52fa5c:	mov	x1, x2
  52fa60:	cmp	x3, #0x1
  52fa64:	sub	x2, x7, x2
  52fa68:	sub	x7, x4, x5
  52fa6c:	csetm	x4, ne	// ne = any
  52fa70:	cmp	x0, #0x1
  52fa74:	and	x4, x4, x2
  52fa78:	csetm	x2, ne	// ne = any
  52fa7c:	add	x4, x4, x3
  52fa80:	and	x2, x2, x7
  52fa84:	cmp	x4, #0x1
  52fa88:	add	x2, x2, x0
  52fa8c:	csetm	x7, ne	// ne = any
  52fa90:	cmp	x2, #0x1
  52fa94:	add	x1, x1, x4
  52fa98:	add	x5, x5, x2
  52fa9c:	csetm	x12, ne	// ne = any
  52faa0:	and	x7, x7, x1
  52faa4:	and	x1, x12, x5
  52faa8:	str	x4, [sp, #224]
  52faac:	str	x3, [sp, #248]
  52fab0:	str	x6, [sp, #272]
  52fab4:	str	x0, [sp, #296]
  52fab8:	str	x2, [sp, #320]
  52fabc:	cmp	x7, x1
  52fac0:	b.eq	52fb04 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x964>  // b.none
  52fac4:	add	x10, x10, x11
  52fac8:	cmp	x1, #0x0
  52facc:	and	x8, x8, x10
  52fad0:	add	x21, x8, x9
  52fad4:	ccmp	x1, x21, #0x4, ne	// ne = any
  52fad8:	ccmp	x7, #0x0, #0x4, ne	// ne = any
  52fadc:	b.eq	52faf8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x958>  // b.none
  52fae0:	sub	x22, x21, x1
  52fae4:	mov	x0, x7
  52fae8:	mov	x2, x22
  52faec:	bl	48b570 <memmove@plt>
  52faf0:	ldr	x9, [x20, #16]
  52faf4:	add	x7, x0, x22
  52faf8:	sub	x21, x21, x7
  52fafc:	sub	x9, x9, x21
  52fb00:	str	x9, [x20, #16]
  52fb04:	mov	x20, #0x0                   	// #0
  52fb08:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  52fb0c:	ldr	x0, [x19, #120]
  52fb10:	add	x1, x19, #0x78
  52fb14:	add	x19, x19, #0x74
  52fb18:	cmp	x0, #0x1
  52fb1c:	add	x1, x1, x0
  52fb20:	csetm	x21, ne	// ne = any
  52fb24:	mov	x0, x19
  52fb28:	and	x21, x21, x1
  52fb2c:	bl	48cea0 <pthread_spin_lock@plt>
  52fb30:	ldr	w0, [x21, #48]
  52fb34:	cbz	w0, 52fcac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb0c>
  52fb38:	ldr	w1, [x21, #40]
  52fb3c:	sub	w0, w0, #0x1
  52fb40:	ldr	w2, [x21, #4]
  52fb44:	mov	x20, #0x0                   	// #0
  52fb48:	add	w1, w1, #0x1
  52fb4c:	str	w0, [x21, #48]
  52fb50:	udiv	w0, w1, w2
  52fb54:	msub	w0, w0, w2, w1
  52fb58:	str	w0, [x21, #40]
  52fb5c:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  52fb60:	ldr	x0, [x19, #120]
  52fb64:	add	x1, x19, #0x78
  52fb68:	add	x19, x19, #0x74
  52fb6c:	cmp	x0, #0x1
  52fb70:	add	x1, x1, x0
  52fb74:	csetm	x22, ne	// ne = any
  52fb78:	mov	x0, x19
  52fb7c:	and	x22, x22, x1
  52fb80:	add	x24, x22, #0x38
  52fb84:	bl	48cea0 <pthread_spin_lock@plt>
  52fb88:	mov	x0, x24
  52fb8c:	bl	48cea0 <pthread_spin_lock@plt>
  52fb90:	cbz	x20, 531530 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2390>
  52fb94:	ldr	w23, [x20]
  52fb98:	ldr	w0, [x22, #16]
  52fb9c:	cmp	w23, w0
  52fba0:	b.hi	531418 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2278>  // b.pmore
  52fba4:	ldr	x27, [x22, #32]
  52fba8:	add	x25, x22, #0x20
  52fbac:	add	x21, sp, #0x140
  52fbb0:	cmp	x27, #0x1
  52fbb4:	sub	x3, x25, x21
  52fbb8:	csetm	x28, ne	// ne = any
  52fbbc:	and	x0, x28, x3
  52fbc0:	add	x0, x0, x27
  52fbc4:	cmp	x0, #0x1
  52fbc8:	b.ne	52fc14 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xa74>  // b.any
  52fbcc:	b	5307dc <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x163c>
  52fbd0:	add	x2, x20, x2, lsr #3
  52fbd4:	mvn	w1, w1
  52fbd8:	and	w1, w1, #0x7
  52fbdc:	ldrb	w0, [x2, #4]
  52fbe0:	asr	w0, w0, w1
  52fbe4:	ubfiz	x0, x0, #4, #1
  52fbe8:	add	x25, x0, #0x8
  52fbec:	add	x0, x26, x0
  52fbf0:	ldr	x27, [x0, #8]
  52fbf4:	add	x25, x26, x25
  52fbf8:	sub	x3, x25, x21
  52fbfc:	cmp	x27, #0x1
  52fc00:	csetm	x28, ne	// ne = any
  52fc04:	and	x0, x28, x3
  52fc08:	add	x0, x0, x27
  52fc0c:	cmp	x0, #0x1
  52fc10:	b.eq	531174 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fd4>  // b.none
  52fc14:	and	x3, x3, x28
  52fc18:	mov	x2, x20
  52fc1c:	add	x3, x3, x27
  52fc20:	mov	x0, x22
  52fc24:	cmp	x3, #0x1
  52fc28:	add	x3, x21, x3
  52fc2c:	csetm	x26, ne	// ne = any
  52fc30:	and	x26, x26, x3
  52fc34:	mov	x1, x26
  52fc38:	bl	4fd670 <_ZNK7bpftime17lpm_trie_map_impl20longest_prefix_matchEPKNS_13lpm_trie_nodeEPKNS_16bpf_lpm_trie_keyE>
  52fc3c:	ldr	w1, [x26, #32]
  52fc40:	cmp	w23, w1
  52fc44:	mov	w2, w1
  52fc48:	ccmp	x0, x2, #0x0, ne	// ne = any
  52fc4c:	b.ne	5307e0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1640>  // b.any
  52fc50:	ldr	w0, [x22, #12]
  52fc54:	cmp	x2, x0, lsl #3
  52fc58:	b.cc	52fbd0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xa30>  // b.lo, b.ul, b.last
  52fc5c:	mov	x0, x26
  52fc60:	mov	x25, #0x8                   	// #8
  52fc64:	b	52fbf0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xa50>
  52fc68:	ldr	x0, [x19, #120]
  52fc6c:	add	x1, x19, #0x78
  52fc70:	add	x19, x19, #0x74
  52fc74:	cmp	x0, #0x1
  52fc78:	add	x1, x1, x0
  52fc7c:	csetm	x22, ne	// ne = any
  52fc80:	mov	x0, x19
  52fc84:	and	x22, x22, x1
  52fc88:	bl	48cea0 <pthread_spin_lock@plt>
  52fc8c:	add	x21, x22, #0x80
  52fc90:	mov	x1, x20
  52fc94:	mov	x0, x21
  52fc98:	ldr	x2, [x22, #104]
  52fc9c:	add	x2, x20, x2
  52fca0:	bl	536e04 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvE6assignIPhEEvT_SG_PNS_11move_detail13disable_if_orIvNSH_7is_sameINSH_17integral_constantIjLj2EEENSK_IjLj0EEEEENSH_14is_convertibleISG_mEENS0_3dtl17is_input_iteratorISG_XsrNSQ_21has_iterator_categoryISG_EE5valueEEENSH_5bool_ILb0EEEE4typeE.constprop.0>
  52fca4:	ldr	x0, [x22, #8]
  52fca8:	cbnz	x0, 52fdc4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc24>
  52fcac:	bl	48c090 <__errno_location@plt>
  52fcb0:	mov	x20, #0xffffffffffffffff    	// #-1
  52fcb4:	mov	w1, #0x2                   	// #2
  52fcb8:	str	w1, [x0]
  52fcbc:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  52fcc0:	ldr	x0, [x19, #120]
  52fcc4:	add	x1, x19, #0x78
  52fcc8:	add	x19, x19, #0x74
  52fccc:	cmp	x0, #0x1
  52fcd0:	add	x1, x1, x0
  52fcd4:	csetm	x21, ne	// ne = any
  52fcd8:	mov	x0, x19
  52fcdc:	and	x21, x21, x1
  52fce0:	bl	48cea0 <pthread_spin_lock@plt>
  52fce4:	cbz	x20, 531478 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x22d8>
  52fce8:	ldr	x0, [x21, #24]
  52fcec:	add	x22, x21, #0x10
  52fcf0:	ldr	w20, [x20]
  52fcf4:	cbnz	x0, 530338 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1198>
  52fcf8:	bl	48c090 <__errno_location@plt>
  52fcfc:	mov	x20, #0xffffffffffffffff    	// #-1
  52fd00:	mov	w1, #0x2                   	// #2
  52fd04:	str	w1, [x0]
  52fd08:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  52fd0c:	add	x19, x19, #0x74
  52fd10:	mov	x20, #0xffffffffffffffff    	// #-1
  52fd14:	mov	x0, x19
  52fd18:	bl	48cea0 <pthread_spin_lock@plt>
  52fd1c:	bl	48c090 <__errno_location@plt>
  52fd20:	mov	x1, x0
  52fd24:	mov	w2, #0x5f                  	// #95
  52fd28:	mov	x0, x19
  52fd2c:	str	w2, [x1]
  52fd30:	bl	48c720 <pthread_spin_unlock@plt>
  52fd34:	ldp	x21, x22, [sp, #384]
  52fd38:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52fd3c:	cmp	w0, #0x3ee
  52fd40:	b.ne	52f1f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x58>  // b.any
  52fd44:	ldr	x0, [x19, #120]
  52fd48:	add	x19, x19, #0x78
  52fd4c:	cmp	x0, #0x1
  52fd50:	add	x19, x19, x0
  52fd54:	csetm	x0, ne	// ne = any
  52fd58:	and	x0, x0, x19
  52fd5c:	ldr	w0, [x0, #8]
  52fd60:	bl	1b37e80 <bpf_map_delete_elem>
  52fd64:	sxtw	x20, w0
  52fd68:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52fd6c:	ldr	x0, [x19, #120]
  52fd70:	add	x19, x19, #0x78
  52fd74:	cmp	x0, #0x1
  52fd78:	add	x0, x19, x0
  52fd7c:	csetm	x19, ne	// ne = any
  52fd80:	and	x19, x19, x0
  52fd84:	ldr	w0, [x19, #76]
  52fd88:	tbnz	w0, #31, 530780 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x15e0>
  52fd8c:	mov	x1, x20
  52fd90:	bl	1b37e80 <bpf_map_delete_elem>
  52fd94:	sxtw	x20, w0
  52fd98:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52fd9c:	ldr	x0, [x19, #80]
  52fda0:	add	x28, x19, #0x48
  52fda4:	ldr	w20, [x20]
  52fda8:	cbnz	x0, 530ce4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1b44>
  52fdac:	bl	48c090 <__errno_location@plt>
  52fdb0:	mov	x20, #0xffffffffffffffff    	// #-1
  52fdb4:	ldp	x21, x22, [sp, #384]
  52fdb8:	mov	w1, #0x2                   	// #2
  52fdbc:	str	w1, [x0]
  52fdc0:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  52fdc4:	add	x20, x22, #0x20
  52fdc8:	mov	x0, x21
  52fdcc:	bl	545910 <_ZNK7bpftime16bytes_vec_hasherclERKN5boost9container6vectorIhNS1_12interprocess9allocatorIhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEE.isra.0>
  52fdd0:	ldr	x1, [x20, #8]
  52fdd4:	bl	4fae00 <_ZN5boost9unordered6detail15prime_fmod_sizeIvE8positionEmm>
  52fdd8:	add	x23, sp, #0x128
  52fddc:	mov	x1, x0
  52fde0:	mov	x8, x23
  52fde4:	mov	x0, x20
  52fde8:	bl	4e1e10 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EENS9_ISN_SH_EENS1_15prime_fmod_sizeIvEEE2atEm>
  52fdec:	add	x20, sp, #0xc0
  52fdf0:	ldr	x0, [sp, #296]
  52fdf4:	cmp	x0, #0x1
  52fdf8:	add	x1, x23, x0
  52fdfc:	csetm	x0, ne	// ne = any
  52fe00:	and	x0, x0, x1
  52fe04:	sub	x1, x0, x20
  52fe08:	ldr	x0, [x0]
  52fe0c:	cmp	x0, #0x1
  52fe10:	csetm	x27, ne	// ne = any
  52fe14:	and	x27, x27, x1
  52fe18:	add	x27, x27, x0
  52fe1c:	str	x27, [sp, #192]
  52fe20:	cmp	x27, #0x1
  52fe24:	b.eq	52fcac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb0c>  // b.none
  52fe28:	ldr	x4, [x21, #16]
  52fe2c:	b	52fe54 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xcb4>
  52fe30:	ldr	x0, [x28]
  52fe34:	sub	x28, x28, x20
  52fe38:	cmp	x0, #0x1
  52fe3c:	csetm	x27, ne	// ne = any
  52fe40:	and	x27, x27, x28
  52fe44:	add	x27, x27, x0
  52fe48:	str	x27, [sp, #192]
  52fe4c:	cmp	x27, #0x1
  52fe50:	b.eq	52fcac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb0c>  // b.none
  52fe54:	add	x28, x20, x27
  52fe58:	add	x0, x28, #0x8
  52fe5c:	ldr	x1, [x0, #16]
  52fe60:	cmp	x4, x1
  52fe64:	b.ne	52fe30 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc90>  // b.any
  52fe68:	ldr	x5, [x22, #136]
  52fe6c:	add	x24, sp, #0x110
  52fe70:	add	x3, x22, #0x88
  52fe74:	add	x25, sp, #0xf8
  52fe78:	cmp	x5, #0x1
  52fe7c:	sub	x2, x3, x24
  52fe80:	csetm	x1, ne	// ne = any
  52fe84:	sub	x3, x3, x25
  52fe88:	and	x2, x1, x2
  52fe8c:	and	x1, x1, x3
  52fe90:	add	x2, x2, x5
  52fe94:	add	x1, x1, x5
  52fe98:	cmp	x2, #0x1
  52fe9c:	add	x21, sp, #0x140
  52fea0:	csetm	x3, ne	// ne = any
  52fea4:	cmp	x1, #0x1
  52fea8:	sub	x7, x24, x21
  52feac:	sub	x6, x25, x24
  52feb0:	csetm	x5, ne	// ne = any
  52feb4:	add	x1, x4, x1
  52feb8:	and	x3, x3, x7
  52febc:	and	x5, x5, x6
  52fec0:	add	x2, x3, x2
  52fec4:	add	x1, x5, x1
  52fec8:	str	x1, [sp, #272]
  52fecc:	add	x0, x0, #0x8
  52fed0:	str	x2, [sp, #320]
  52fed4:	add	x26, sp, #0xe0
  52fed8:	sub	x3, x0, x26
  52fedc:	sub	x7, x26, x25
  52fee0:	ldr	x6, [x28, #16]
  52fee4:	mov	x0, x21
  52fee8:	mov	x2, x25
  52feec:	mov	x1, x24
  52fef0:	cmp	x6, #0x1
  52fef4:	str	x4, [sp, #8]
  52fef8:	csetm	x5, ne	// ne = any
  52fefc:	and	x3, x5, x3
  52ff00:	add	x3, x3, x6
  52ff04:	cmp	x3, #0x1
  52ff08:	csetm	x5, ne	// ne = any
  52ff0c:	and	x5, x5, x7
  52ff10:	add	x3, x5, x3
  52ff14:	str	x3, [sp, #248]
  52ff18:	bl	4fcf54 <_ZN5boost9intrusive10algo_equalINS_9container12vec_iteratorINS_12interprocess10offset_ptrIhlmLm0EEELb1EEES7_EEbT_S8_T0_>
  52ff1c:	ldr	x4, [sp, #8]
  52ff20:	tbz	w0, #0, 52fe30 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc90>
  52ff24:	sub	x2, x20, x24
  52ff28:	mov	x1, x23
  52ff2c:	add	x2, x2, x27
  52ff30:	mov	x0, x21
  52ff34:	str	x2, [sp, #272]
  52ff38:	add	x28, sp, #0xe8
  52ff3c:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  52ff40:	ldr	x4, [sp, #272]
  52ff44:	sub	x3, x24, x26
  52ff48:	mov	x1, x21
  52ff4c:	mov	x0, x28
  52ff50:	cmp	x4, #0x1
  52ff54:	csetm	x2, ne	// ne = any
  52ff58:	and	x2, x2, x3
  52ff5c:	add	x2, x2, x4
  52ff60:	str	x2, [sp, #224]
  52ff64:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  52ff68:	ldr	x1, [sp, #224]
  52ff6c:	cmp	x1, #0x1
  52ff70:	add	x1, x26, x1
  52ff74:	csetm	x0, ne	// ne = any
  52ff78:	ands	x0, x0, x1
  52ff7c:	b.eq	52fcac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb0c>  // b.none
  52ff80:	ldr	x4, [x0, #72]
  52ff84:	add	x3, x0, #0x48
  52ff88:	sub	x3, x3, x21
  52ff8c:	add	x2, x0, #0x50
  52ff90:	cmp	x4, #0x1
  52ff94:	add	x27, sp, #0x148
  52ff98:	csetm	x1, ne	// ne = any
  52ff9c:	sub	x2, x2, x27
  52ffa0:	and	x1, x1, x3
  52ffa4:	add	x1, x1, x4
  52ffa8:	str	x1, [sp, #320]
  52ffac:	ldr	x1, [x0, #80]
  52ffb0:	cmp	x1, #0x1
  52ffb4:	csetm	x0, ne	// ne = any
  52ffb8:	and	x0, x0, x2
  52ffbc:	add	x0, x0, x1
  52ffc0:	str	x0, [sp, #328]
  52ffc4:	cmp	x0, #0x1
  52ffc8:	add	x0, x27, x0
  52ffcc:	csetm	x1, ne	// ne = any
  52ffd0:	ands	x1, x1, x0
  52ffd4:	b.ne	530854 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x16b4>  // b.any
  52ffd8:	mov	x1, x21
  52ffdc:	mov	x0, x22
  52ffe0:	bl	4fc710 <_ZN7bpftime21lru_var_hash_map_impl11evict_entryEN5boost12interprocess10shared_ptrINS_18lru_linklist_entryENS2_9allocatorIvNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEENS2_7deleterIS4_SD_EEEE>
  52ffe4:	mov	x0, x21
  52ffe8:	bl	4df2e0 <_ZN5boost12interprocess9ipcdetail12shared_countIN7bpftime18lru_linklist_entryENS0_9allocatorIvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIS4_SD_EEED1Ev>
  52ffec:	ldp	x3, x2, [sp, #224]
  52fff0:	sub	x26, x26, x25
  52fff4:	ldr	x0, [sp, #240]
  52fff8:	add	x1, sp, #0x100
  52fffc:	sub	x28, x28, x1
  530000:	sub	x25, x25, x23
  530004:	sub	x23, x23, x21
  530008:	cmp	x3, #0x1
  53000c:	csetm	x7, ne	// ne = any
  530010:	cmp	x2, #0x1
  530014:	and	x7, x7, x26
  530018:	csetm	x6, ne	// ne = any
  53001c:	cmp	x0, #0x1
  530020:	add	x7, x7, x3
  530024:	and	x6, x6, x28
  530028:	csetm	x5, ne	// ne = any
  53002c:	add	x6, x6, x2
  530030:	cmp	x7, #0x1
  530034:	mov	x2, #0xffffffffffffffe8    	// #-24
  530038:	and	x5, x5, x2
  53003c:	add	x5, x5, x0
  530040:	csetm	x4, ne	// ne = any
  530044:	add	x0, sp, #0x130
  530048:	cmp	x6, #0x1
  53004c:	sub	x1, x1, x0
  530050:	and	x4, x4, x25
  530054:	csetm	x11, ne	// ne = any
  530058:	cmp	x5, #0x1
  53005c:	add	x4, x4, x7
  530060:	and	x11, x11, x1
  530064:	csetm	x8, ne	// ne = any
  530068:	add	x11, x11, x6
  53006c:	cmp	x4, #0x1
  530070:	mov	x1, #0xffffffffffffffd0    	// #-48
  530074:	and	x8, x8, x1
  530078:	csetm	x26, ne	// ne = any
  53007c:	add	x8, x8, x5
  530080:	cmp	x11, #0x1
  530084:	and	x26, x26, x23
  530088:	sub	x0, x0, x27
  53008c:	csetm	x3, ne	// ne = any
  530090:	cmp	x8, #0x1
  530094:	add	x26, x26, x4
  530098:	and	x3, x3, x0
  53009c:	csetm	x2, ne	// ne = any
  5300a0:	add	x3, x3, x11
  5300a4:	cmp	x26, #0x1
  5300a8:	mov	x0, #0xffffffffffffffe8    	// #-24
  5300ac:	and	x2, x2, x0
  5300b0:	csetm	x1, ne	// ne = any
  5300b4:	add	x2, x2, x8
  5300b8:	cmp	x3, #0x1
  5300bc:	sub	x0, x21, x20
  5300c0:	add	x23, sp, #0xc8
  5300c4:	csetm	x10, ne	// ne = any
  5300c8:	and	x0, x1, x0
  5300cc:	cmp	x2, #0x1
  5300d0:	add	x28, sp, #0x150
  5300d4:	add	x25, sp, #0xd0
  5300d8:	stp	x7, x6, [sp, #248]
  5300dc:	sub	x6, x27, x23
  5300e0:	add	x0, x0, x26
  5300e4:	csetm	x9, ne	// ne = any
  5300e8:	and	x6, x10, x6
  5300ec:	str	x5, [sp, #264]
  5300f0:	sub	x5, x28, x25
  5300f4:	cmp	x0, #0x1
  5300f8:	add	x6, x6, x3
  5300fc:	and	x5, x9, x5
  530100:	add	x5, x5, x2
  530104:	str	x0, [sp, #192]
  530108:	add	x0, x20, x0
  53010c:	stp	x6, x5, [sp, #200]
  530110:	csetm	x6, ne	// ne = any
  530114:	and	x0, x6, x0
  530118:	stp	x4, x11, [sp, #296]
  53011c:	sub	x5, x0, x20
  530120:	stp	x8, x26, [sp, #312]
  530124:	stp	x3, x2, [sp, #328]
  530128:	ldr	x4, [x0]
  53012c:	cmp	x4, #0x1
  530130:	csetm	x0, ne	// ne = any
  530134:	and	x0, x0, x5
  530138:	add	x0, x0, x4
  53013c:	str	x0, [sp, #192]
  530140:	cmp	x0, #0x1
  530144:	b.eq	5309c4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1824>  // b.none
  530148:	add	x7, sp, #0xa0
  53014c:	add	x8, sp, #0xa8
  530150:	sub	x4, x27, x7
  530154:	sub	x6, x28, x8
  530158:	and	x4, x4, x10
  53015c:	and	x6, x6, x9
  530160:	add	x4, x4, x3
  530164:	add	x6, x6, x2
  530168:	cmp	x4, #0x1
  53016c:	add	x2, x7, x4
  530170:	csetm	x9, ne	// ne = any
  530174:	stp	x4, x6, [sp, #160]
  530178:	and	x2, x9, x2
  53017c:	add	x3, x21, x26
  530180:	mov	x5, x2
  530184:	and	x3, x3, x1
  530188:	ldr	x0, [x2]
  53018c:	cmp	x0, #0x1
  530190:	add	x1, x0, x2
  530194:	csetm	x0, ne	// ne = any
  530198:	and	x0, x0, x1
  53019c:	cmp	x3, x0
  5301a0:	b.eq	5301cc <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x102c>  // b.none
  5301a4:	nop
  5301a8:	ldr	x1, [x0]
  5301ac:	mov	x2, x0
  5301b0:	mov	x5, x0
  5301b4:	cmp	x1, #0x1
  5301b8:	add	x1, x0, x1
  5301bc:	csetm	x0, ne	// ne = any
  5301c0:	and	x0, x0, x1
  5301c4:	cmp	x3, x0
  5301c8:	b.ne	5301a8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1008>  // b.any
  5301cc:	cmp	x6, #0x1
  5301d0:	add	x26, sp, #0xb0
  5301d4:	add	x27, sp, #0xb8
  5301d8:	csetm	x1, ne	// ne = any
  5301dc:	sub	x8, x8, x27
  5301e0:	sub	x7, x7, x26
  5301e4:	and	x1, x1, x8
  5301e8:	and	x0, x9, x7
  5301ec:	add	x1, x1, x6
  5301f0:	add	x0, x0, x4
  5301f4:	stp	x0, x1, [sp, #176]
  5301f8:	ldr	x0, [x2]
  5301fc:	cmp	x0, #0x1
  530200:	add	x0, x0, x5
  530204:	csetm	x1, ne	// ne = any
  530208:	and	x1, x1, x0
  53020c:	sub	x0, x1, x5
  530210:	ldr	x3, [x1]
  530214:	cmp	x3, #0x1
  530218:	csetm	x1, ne	// ne = any
  53021c:	and	x0, x1, x0
  530220:	add	x0, x0, x3
  530224:	str	x0, [x2]
  530228:	ldr	x0, [sp, #176]
  53022c:	cmp	x0, #0x1
  530230:	add	x0, x26, x0
  530234:	csetm	x1, ne	// ne = any
  530238:	and	x0, x1, x0
  53023c:	ldr	x28, [x0]
  530240:	cmp	x28, #0x1
  530244:	b.eq	530a64 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x18c4>  // b.none
  530248:	ldr	x3, [sp, #320]
  53024c:	sub	x21, x21, x24
  530250:	mov	x1, x24
  530254:	mov	x0, x22
  530258:	cmp	x3, #0x1
  53025c:	csetm	x2, ne	// ne = any
  530260:	and	x2, x2, x21
  530264:	add	x2, x2, x3
  530268:	str	x2, [sp, #272]
  53026c:	bl	4e1b90 <_ZN5boost9unordered6detail5tableINS1_3mapINS_12interprocess9allocatorISt4pairIKNS_9container6vectorIhNS5_IhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESG_EESI_SL_NSK_16bytes_vec_hasherESt8equal_toISI_EEEE11delete_nodeENSC_INS1_4nodeISM_SD_EElmLm0EEE>
  530270:	ldr	x1, [x22, #8]
  530274:	adrp	x2, 1d3d000 <_ZZL25UpgradeIntrinsicFunction1PN4llvm8FunctionERS1_E9StoreInts+0x330>
  530278:	fmov	d2, x23
  53027c:	fmov	d0, x26
  530280:	sub	x1, x1, #0x1
  530284:	str	x1, [x22, #8]
  530288:	ldr	q1, [x2, #96]
  53028c:	mov	v2.d[1], x25
  530290:	ldur	q3, [sp, #200]
  530294:	mov	v0.d[1], x27
  530298:	ldr	x4, [sp, #192]
  53029c:	add	x0, sp, #0x98
  5302a0:	cmeq	v1.2d, v3.2d, v1.2d
  5302a4:	sub	x20, x20, x0
  5302a8:	sub	v0.2d, v2.2d, v0.2d
  5302ac:	cmp	x4, #0x1
  5302b0:	csetm	x2, ne	// ne = any
  5302b4:	sub	x3, x0, x24
  5302b8:	and	x2, x2, x20
  5302bc:	add	x0, sp, #0x118
  5302c0:	add	x2, x2, x4
  5302c4:	str	x2, [sp, #152]
  5302c8:	bic	v0.16b, v0.16b, v1.16b
  5302cc:	cmp	x2, #0x1
  5302d0:	csetm	x4, ne	// ne = any
  5302d4:	mov	x1, x26
  5302d8:	and	x3, x4, x3
  5302dc:	mov	x20, #0x0                   	// #0
  5302e0:	add	x2, x3, x2
  5302e4:	str	x2, [sp, #272]
  5302e8:	add	v0.2d, v0.2d, v3.2d
  5302ec:	str	q0, [sp, #176]
  5302f0:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  5302f4:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  5302f8:	mov	x0, x19
  5302fc:	bl	503620 <_ZN7bpftime26array_map_kernel_user_impl11init_map_fdEv>
  530300:	tbz	w0, #0, 52f3d8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x238>
  530304:	ldr	x0, [x19, #56]
  530308:	cbz	x0, 5312b4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2114>
  53030c:	ldr	w1, [x19, #36]
  530310:	ldr	w3, [x20]
  530314:	cmp	w3, w1
  530318:	b.cs	5312c8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2128>  // b.hs, b.nlast
  53031c:	ldr	w4, [x19, #32]
  530320:	mov	w1, #0x0                   	// #0
  530324:	mov	w2, w4
  530328:	mul	w3, w3, w4
  53032c:	add	x0, x0, x3
  530330:	bl	48bc60 <memset@plt>
  530334:	b	52f2f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x158>
  530338:	add	x0, x21, #0x30
  53033c:	mov	x1, x0
  530340:	str	x1, [sp, #40]
  530344:	mov	w0, w20
  530348:	ldr	x1, [x1, #8]
  53034c:	cmp	x1, #0x1c
  530350:	b.hi	5313a4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2204>  // b.pmore
  530354:	adrp	x3, 1d09000 <_ZTSN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0xd0>
  530358:	add	x3, x3, #0xa38
  53035c:	adrp	x2, 1d09000 <_ZTSN5boost12interprocess9ipcdetail17placement_destroyINS_9container3setIiSt4lessIiENS0_9allocatorIiNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEEvEEEE+0xd0>
  530360:	add	x2, x2, #0xb20
  530364:	ldr	x3, [x3, x1, lsl #3]
  530368:	ldr	x1, [x2, x1, lsl #3]
  53036c:	mul	x0, x0, x3
  530370:	mov	w1, w1
  530374:	umulh	x1, x0, x1
  530378:	ldr	x0, [sp, #40]
  53037c:	add	x23, sp, #0x128
  530380:	mov	x8, x23
  530384:	add	x25, sp, #0xf8
  530388:	bl	4e1fb0 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKjNS_9container6vectorImNS_12interprocess9allocatorImNS9_15segment_managerIcNS9_15rbtree_best_fitINS9_12mutex_familyENS9_10offset_ptrIvlmLm0EEELm0EEENS9_10iset_indexEEEEEvEEESF_EESF_EENSA_ISL_SI_EENS1_15prime_fmod_sizeIvEEE2atEm>
  53038c:	ldr	x8, [sp, #296]
  530390:	cmp	x8, #0x1
  530394:	add	x0, x23, x8
  530398:	csetm	x7, ne	// ne = any
  53039c:	and	x0, x7, x0
  5303a0:	sub	x1, x0, x25
  5303a4:	ldr	x0, [x0]
  5303a8:	cmp	x0, #0x1
  5303ac:	csetm	x3, ne	// ne = any
  5303b0:	and	x3, x3, x1
  5303b4:	add	x3, x3, x0
  5303b8:	str	x3, [sp, #248]
  5303bc:	cmp	x3, #0x1
  5303c0:	b.eq	52fcf8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb58>  // b.none
  5303c4:	add	x0, x25, x3
  5303c8:	ldr	w1, [x0, #8]
  5303cc:	cmp	w20, w1
  5303d0:	b.ne	53137c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x21dc>  // b.any
  5303d4:	ldr	x9, [sp, #304]
  5303d8:	add	x27, sp, #0x110
  5303dc:	sub	x28, x25, x27
  5303e0:	add	x2, sp, #0xe0
  5303e4:	add	x3, x28, x3
  5303e8:	cmp	x9, #0x1
  5303ec:	add	x24, sp, #0x140
  5303f0:	csetm	x4, ne	// ne = any
  5303f4:	add	x20, sp, #0x130
  5303f8:	cmp	x3, #0x1
  5303fc:	add	x26, sp, #0x148
  530400:	sub	x0, x27, x2
  530404:	sub	x10, x20, x26
  530408:	sub	x5, x23, x24
  53040c:	csetm	x6, ne	// ne = any
  530410:	and	x7, x7, x5
  530414:	and	x4, x4, x10
  530418:	and	x6, x6, x0
  53041c:	add	x6, x6, x3
  530420:	add	x7, x7, x8
  530424:	add	x4, x4, x9
  530428:	add	x0, sp, #0xe8
  53042c:	mov	x1, x24
  530430:	stp	x0, x5, [sp, #8]
  530434:	stp	x10, x2, [sp, #24]
  530438:	str	x6, [sp, #224]
  53043c:	str	x3, [sp, #272]
  530440:	stp	x7, x4, [sp, #320]
  530444:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  530448:	ldp	x10, x2, [sp, #24]
  53044c:	ldr	x3, [sp, #224]
  530450:	ldp	x0, x5, [sp, #8]
  530454:	cmp	x3, #0x1
  530458:	add	x2, x2, x3
  53045c:	csetm	x3, ne	// ne = any
  530460:	tst	x3, x2
  530464:	b.eq	52fcf8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb58>  // b.none
  530468:	ldp	x6, x4, [sp, #232]
  53046c:	add	x3, sp, #0x100
  530470:	sub	x2, x2, x25
  530474:	sub	x0, x0, x3
  530478:	cmp	x6, #0x1
  53047c:	csetm	x12, ne	// ne = any
  530480:	cmp	x4, #0x1
  530484:	and	x12, x12, x0
  530488:	csetm	x11, ne	// ne = any
  53048c:	cmp	x2, #0x1
  530490:	add	x12, x12, x6
  530494:	csetm	x8, ne	// ne = any
  530498:	mov	x0, #0xffffffffffffffe8    	// #-24
  53049c:	and	x11, x11, x0
  5304a0:	cmp	x12, #0x1
  5304a4:	add	x11, x11, x4
  5304a8:	and	x8, x8, x28
  5304ac:	add	x0, sp, #0x118
  5304b0:	add	x8, x8, x2
  5304b4:	sub	x3, x3, x0
  5304b8:	csetm	x7, ne	// ne = any
  5304bc:	cmp	x11, #0x1
  5304c0:	and	x7, x7, x3
  5304c4:	csetm	x6, ne	// ne = any
  5304c8:	cmp	x8, #0x1
  5304cc:	sub	x3, x27, x23
  5304d0:	add	x7, x7, x12
  5304d4:	csetm	x27, ne	// ne = any
  5304d8:	mov	x4, #0xffffffffffffffe8    	// #-24
  5304dc:	and	x6, x6, x4
  5304e0:	and	x27, x27, x3
  5304e4:	add	x6, x6, x11
  5304e8:	cmp	x7, #0x1
  5304ec:	add	x27, x27, x8
  5304f0:	sub	x0, x0, x20
  5304f4:	csetm	x4, ne	// ne = any
  5304f8:	cmp	x6, #0x1
  5304fc:	and	x4, x4, x0
  530500:	csetm	x28, ne	// ne = any
  530504:	cmp	x27, #0x1
  530508:	add	x4, x4, x7
  53050c:	csetm	x25, ne	// ne = any
  530510:	mov	x0, #0xffffffffffffffe8    	// #-24
  530514:	and	x28, x28, x0
  530518:	cmp	x4, #0x1
  53051c:	add	x28, x28, x6
  530520:	and	x0, x25, x5
  530524:	add	x0, x0, x27
  530528:	csetm	x9, ne	// ne = any
  53052c:	cmp	x28, #0x1
  530530:	and	x3, x9, x10
  530534:	csetm	x5, ne	// ne = any
  530538:	cmp	x0, #0x1
  53053c:	csetm	x10, ne	// ne = any
  530540:	stp	x2, x12, [sp, #248]
  530544:	add	x2, x24, x0
  530548:	and	x10, x10, x2
  53054c:	mov	x2, #0xffffffffffffffe8    	// #-24
  530550:	and	x2, x5, x2
  530554:	add	x3, x3, x4
  530558:	add	x2, x2, x28
  53055c:	stp	x11, x8, [sp, #264]
  530560:	add	x8, sp, #0x138
  530564:	stp	x7, x6, [sp, #280]
  530568:	sub	x6, x10, x24
  53056c:	stp	x27, x4, [sp, #296]
  530570:	stp	x28, x0, [sp, #312]
  530574:	str	x3, [sp, #328]
  530578:	str	x2, [sp, #336]
  53057c:	ldr	x2, [x10]
  530580:	cmp	x2, #0x1
  530584:	csetm	x0, ne	// ne = any
  530588:	and	x0, x0, x6
  53058c:	add	x0, x0, x2
  530590:	str	x0, [sp, #320]
  530594:	cmp	x0, #0x1
  530598:	b.eq	530a18 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1878>  // b.none
  53059c:	add	x6, sp, #0xa0
  5305a0:	add	x7, sp, #0xa8
  5305a4:	sub	x20, x20, x6
  5305a8:	sub	x0, x8, x7
  5305ac:	and	x20, x20, x9
  5305b0:	and	x0, x0, x5
  5305b4:	add	x20, x20, x4
  5305b8:	add	x5, x0, x28
  5305bc:	cmp	x20, #0x1
  5305c0:	add	x2, x6, x20
  5305c4:	csetm	x8, ne	// ne = any
  5305c8:	stp	x20, x5, [sp, #160]
  5305cc:	and	x2, x8, x2
  5305d0:	add	x27, x23, x27
  5305d4:	mov	x4, x2
  5305d8:	and	x3, x25, x27
  5305dc:	ldr	x1, [x2]
  5305e0:	b	5305f0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1450>
  5305e4:	ldr	x1, [x0]
  5305e8:	mov	x2, x0
  5305ec:	mov	x4, x0
  5305f0:	cmp	x1, #0x1
  5305f4:	add	x1, x1, x2
  5305f8:	csetm	x0, ne	// ne = any
  5305fc:	and	x0, x0, x1
  530600:	cmp	x0, x3
  530604:	b.ne	5305e4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1444>  // b.any
  530608:	cmp	x5, #0x1
  53060c:	add	x3, sp, #0xb8
  530610:	add	x26, sp, #0xb0
  530614:	csetm	x1, ne	// ne = any
  530618:	sub	x7, x7, x3
  53061c:	sub	x3, x6, x26
  530620:	and	x0, x8, x3
  530624:	and	x1, x1, x7
  530628:	add	x1, x1, x5
  53062c:	add	x0, x0, x20
  530630:	stp	x0, x1, [sp, #176]
  530634:	ldr	x0, [x2]
  530638:	cmp	x0, #0x1
  53063c:	add	x0, x0, x4
  530640:	csetm	x1, ne	// ne = any
  530644:	and	x1, x1, x0
  530648:	sub	x0, x1, x4
  53064c:	ldr	x3, [x1]
  530650:	cmp	x3, #0x1
  530654:	csetm	x1, ne	// ne = any
  530658:	and	x0, x1, x0
  53065c:	add	x0, x0, x3
  530660:	str	x0, [x2]
  530664:	ldr	x0, [sp, #176]
  530668:	cmp	x0, #0x1
  53066c:	add	x0, x26, x0
  530670:	csetm	x1, ne	// ne = any
  530674:	and	x0, x1, x0
  530678:	ldr	x4, [x0]
  53067c:	cmp	x4, #0x1
  530680:	b.eq	530898 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x16f8>  // b.none
  530684:	add	x20, sp, #0xc0
  530688:	add	x25, sp, #0x90
  53068c:	add	x28, sp, #0x98
  530690:	add	x27, sp, #0x88
  530694:	ldr	x0, [sp, #296]
  530698:	sub	x23, x23, x27
  53069c:	ldr	x1, [sp, #40]
  5306a0:	cmp	x0, #0x1
  5306a4:	csetm	x4, ne	// ne = any
  5306a8:	and	x4, x4, x23
  5306ac:	add	x4, x4, x0
  5306b0:	str	x4, [sp, #136]
  5306b4:	add	x3, x27, x4
  5306b8:	ldr	x0, [x21, #48]
  5306bc:	cmp	x0, #0x1
  5306c0:	add	x24, x1, x0
  5306c4:	csetm	x0, ne	// ne = any
  5306c8:	and	x0, x0, x24
  5306cc:	cmp	x0, #0x0
  5306d0:	sub	x0, x0, x25
  5306d4:	sub	x1, x0, #0x1
  5306d8:	csetm	x0, ne	// ne = any
  5306dc:	and	x0, x0, x1
  5306e0:	add	x0, x0, #0x1
  5306e4:	str	x0, [sp, #144]
  5306e8:	cmp	x0, #0x1
  5306ec:	add	x0, x25, x0
  5306f0:	csetm	x1, ne	// ne = any
  5306f4:	and	x0, x1, x0
  5306f8:	cmp	x0, #0x0
  5306fc:	sub	x0, x0, x28
  530700:	csetm	x2, ne	// ne = any
  530704:	cmp	x4, #0x1
  530708:	csetm	x5, ne	// ne = any
  53070c:	sub	x0, x0, #0x1
  530710:	and	x1, x5, x3
  530714:	and	x2, x2, x0
  530718:	add	x0, x1, #0x8
  53071c:	add	x1, x2, #0x1
  530720:	sub	x26, x0, x26
  530724:	str	x1, [sp, #152]
  530728:	cmp	x26, #0x1
  53072c:	str	x26, [sp, #176]
  530730:	csetm	x1, ne	// ne = any
  530734:	and	x1, x1, x0
  530738:	add	x0, x1, #0x8
  53073c:	ldr	x2, [x0, #24]
  530740:	cbnz	x2, 530860 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x16c0>
  530744:	sub	x2, x27, x20
  530748:	mov	x1, x20
  53074c:	and	x2, x2, x5
  530750:	mov	x0, x25
  530754:	add	x2, x2, x4
  530758:	str	x2, [sp, #192]
  53075c:	bl	54fc90 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  530760:	ldr	x0, [x22, #8]
  530764:	mov	x20, #0x0                   	// #0
  530768:	sub	x0, x0, #0x1
  53076c:	str	x0, [x22, #8]
  530770:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  530774:	ldp	x21, x22, [sp, #384]
  530778:	mov	x20, #0xffffffffffffffea    	// #-22
  53077c:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  530780:	mov	x0, x19
  530784:	bl	4fd934 <_ZN7bpftime25hash_map_kernel_user_impl11init_map_fdEv>
  530788:	ldr	w0, [x19, #76]
  53078c:	b	52fd8c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xbec>
  530790:	ldrb	w3, [x20, #1]
  530794:	mov	x4, #0x745f                	// #29791
  530798:	ldrb	w0, [x20]
  53079c:	mov	w2, #0x1f                  	// #31
  5307a0:	ldrb	w6, [x20, #2]
  5307a4:	lsl	x1, x3, #4
  5307a8:	ldrb	w5, [x20, #3]
  5307ac:	sub	x1, x1, x3
  5307b0:	add	x1, x3, x1, lsl #6
  5307b4:	umaddl	x2, w2, w6, x5
  5307b8:	madd	x0, x0, x4, x1
  5307bc:	add	x0, x0, x2
  5307c0:	mov	x2, x0
  5307c4:	b	52f43c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x29c>
  5307c8:	ldr	w1, [x24, x1]
  5307cc:	ldr	w0, [x20]
  5307d0:	cmp	w1, w0
  5307d4:	b.ne	52f484 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2e4>  // b.any
  5307d8:	b	52f4cc <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x32c>
  5307dc:	mov	x26, #0x0                   	// #0
  5307e0:	sub	x0, x25, x21
  5307e4:	and	x0, x0, x28
  5307e8:	add	x0, x0, x27
  5307ec:	cmp	x0, #0x1
  5307f0:	b.eq	531174 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fd4>  // b.none
  5307f4:	ldr	w0, [x26, #32]
  5307f8:	cmp	w23, w0
  5307fc:	b.ne	531174 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fd4>  // b.any
  530800:	mov	x2, x20
  530804:	mov	x1, x26
  530808:	mov	x0, x22
  53080c:	bl	4fd670 <_ZNK7bpftime17lpm_trie_map_impl20longest_prefix_matchEPKNS_13lpm_trie_nodeEPKNS_16bpf_lpm_trie_keyE>
  530810:	cmp	x0, w23, uxtw
  530814:	b.ne	531174 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fd4>  // b.any
  530818:	ldrb	w0, [x26, #40]
  53081c:	tbnz	w0, #0, 531174 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fd4>
  530820:	ldr	w1, [x22, #12]
  530824:	mov	w0, #0x1                   	// #1
  530828:	strb	w0, [x26, #40]
  53082c:	add	x0, x26, #0x30
  530830:	bl	547c70 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvE11priv_resizeINS0_12value_init_tENS_11move_detail17integral_constantIjLj2EEEEEvmRKT_T0_.isra.0>
  530834:	ldr	x0, [x22, #40]
  530838:	cbz	x0, 530844 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x16a4>
  53083c:	sub	x0, x0, #0x1
  530840:	str	x0, [x22, #40]
  530844:	mov	x20, #0x0                   	// #0
  530848:	mov	x0, x24
  53084c:	bl	48c720 <pthread_spin_unlock@plt>
  530850:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  530854:	mov	w0, #0x1                   	// #1
  530858:	bl	1d063a0 <__aarch64_ldadd4_sync>
  53085c:	b	52ffd8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xe38>
  530860:	ldr	x4, [x1, #16]
  530864:	add	x3, x0, #0x8
  530868:	sub	x3, x3, x20
  53086c:	mov	x1, x20
  530870:	cmp	x4, #0x1
  530874:	csetm	x2, ne	// ne = any
  530878:	and	x2, x2, x3
  53087c:	add	x2, x2, x4
  530880:	str	x2, [sp, #192]
  530884:	bl	54fc90 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  530888:	ldr	x4, [sp, #136]
  53088c:	cmp	x4, #0x1
  530890:	csetm	x5, ne	// ne = any
  530894:	b	530744 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x15a4>
  530898:	add	x20, sp, #0xc0
  53089c:	mov	x1, x26
  5308a0:	mov	x0, x20
  5308a4:	str	x4, [sp, #8]
  5308a8:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  5308ac:	add	x25, sp, #0x90
  5308b0:	ldp	x3, x0, [sp, #192]
  5308b4:	sub	x5, x20, x25
  5308b8:	add	x28, sp, #0x98
  5308bc:	add	x27, sp, #0x88
  5308c0:	ldr	x4, [sp, #8]
  5308c4:	cmp	x3, #0x1
  5308c8:	csetm	x1, ne	// ne = any
  5308cc:	cmp	x0, #0x1
  5308d0:	csetm	x2, ne	// ne = any
  5308d4:	and	x1, x1, x5
  5308d8:	and	x2, x2, #0x30
  5308dc:	add	x1, x1, x3
  5308e0:	add	x2, x2, x0
  5308e4:	stp	x1, x2, [sp, #144]
  5308e8:	cmp	x2, #0x1
  5308ec:	csetm	x6, ne	// ne = any
  5308f0:	add	x3, x28, x2
  5308f4:	and	x3, x6, x3
  5308f8:	cmp	x1, #0x1
  5308fc:	cset	x0, eq	// eq = none
  530900:	add	x1, x25, x1
  530904:	sub	x0, x0, #0x1
  530908:	ldp	x5, x7, [x3]
  53090c:	and	x0, x0, x1
  530910:	cmp	x5, #0x1
  530914:	add	x5, x3, x5
  530918:	csetm	x1, ne	// ne = any
  53091c:	and	x1, x1, x5
  530920:	sub	x0, x0, x1
  530924:	asr	x0, x0, #3
  530928:	lsl	x0, x4, x0
  53092c:	bic	x0, x7, x0
  530930:	str	x0, [x3, #8]
  530934:	cbnz	x0, 530694 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x14f4>
  530938:	sub	x0, x28, x27
  53093c:	dup	v0.2d, x4
  530940:	and	x0, x6, x0
  530944:	add	x0, x0, x2
  530948:	cmp	x0, #0x1
  53094c:	add	x0, x27, x0
  530950:	csetm	x1, ne	// ne = any
  530954:	and	x0, x1, x0
  530958:	add	x3, x0, #0x10
  53095c:	add	x5, x0, #0x18
  530960:	ldp	x1, x4, [x0, #16]
  530964:	cmp	x1, #0x1
  530968:	add	x1, x1, x3
  53096c:	csetm	x2, ne	// ne = any
  530970:	cmp	x4, #0x1
  530974:	and	x2, x2, x1
  530978:	csetm	x1, ne	// ne = any
  53097c:	sub	x6, x0, x2
  530980:	and	x1, x1, x6
  530984:	add	x1, x1, x4
  530988:	str	x1, [x2, #24]
  53098c:	ldp	x4, x1, [x0, #16]
  530990:	cmp	x1, #0x1
  530994:	add	x1, x5, x1
  530998:	csetm	x2, ne	// ne = any
  53099c:	cmp	x4, #0x1
  5309a0:	and	x2, x2, x1
  5309a4:	csetm	x1, ne	// ne = any
  5309a8:	add	x5, x2, #0x10
  5309ac:	sub	x3, x3, x5
  5309b0:	and	x1, x1, x3
  5309b4:	add	x1, x1, x4
  5309b8:	str	x1, [x2, #16]
  5309bc:	str	q0, [x0, #16]
  5309c0:	b	530694 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x14f4>
  5309c4:	mov	x0, x23
  5309c8:	stp	x3, x2, [sp, #8]
  5309cc:	stp	x1, x10, [sp, #24]
  5309d0:	str	x9, [sp, #40]
  5309d4:	bl	4e2490 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEE9incrementEv>
  5309d8:	ldr	x0, [sp, #200]
  5309dc:	ldp	x3, x2, [sp, #8]
  5309e0:	cmp	x0, #0x1
  5309e4:	add	x4, x0, x23
  5309e8:	csetm	x0, ne	// ne = any
  5309ec:	and	x0, x0, x4
  5309f0:	sub	x5, x0, x20
  5309f4:	ldp	x1, x10, [sp, #24]
  5309f8:	ldr	x4, [x0]
  5309fc:	ldr	x9, [sp, #40]
  530a00:	cmp	x4, #0x1
  530a04:	csetm	x0, ne	// ne = any
  530a08:	and	x0, x0, x5
  530a0c:	add	x0, x0, x4
  530a10:	str	x0, [sp, #192]
  530a14:	b	530148 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xfa8>
  530a18:	mov	x0, x26
  530a1c:	stp	x4, x8, [sp, #8]
  530a20:	stp	x9, x5, [sp, #24]
  530a24:	bl	4e2600 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKjNS_9container6vectorImNS_12interprocess9allocatorImNS9_15segment_managerIcNS9_15rbtree_best_fitINS9_12mutex_familyENS9_10offset_ptrIvlmLm0EEELm0EEENS9_10iset_indexEEEEEvEEESF_EESF_EEE9incrementEv>
  530a28:	ldr	x0, [sp, #328]
  530a2c:	ldp	x4, x8, [sp, #8]
  530a30:	cmp	x0, #0x1
  530a34:	add	x2, x0, x26
  530a38:	csetm	x0, ne	// ne = any
  530a3c:	and	x0, x0, x2
  530a40:	sub	x1, x0, x24
  530a44:	ldp	x9, x5, [sp, #24]
  530a48:	ldr	x2, [x0]
  530a4c:	cmp	x2, #0x1
  530a50:	csetm	x0, ne	// ne = any
  530a54:	and	x0, x0, x1
  530a58:	add	x0, x0, x2
  530a5c:	str	x0, [sp, #320]
  530a60:	b	53059c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x13fc>
  530a64:	mov	x1, x26
  530a68:	mov	x0, x24
  530a6c:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  530a70:	ldp	x2, x3, [sp, #272]
  530a74:	add	x1, sp, #0x90
  530a78:	sub	x5, x24, x1
  530a7c:	add	x4, sp, #0x98
  530a80:	cmp	x2, #0x1
  530a84:	csetm	x0, ne	// ne = any
  530a88:	cmp	x3, #0x1
  530a8c:	and	x0, x0, x5
  530a90:	cset	x5, ne	// ne = any
  530a94:	add	x0, x0, x2
  530a98:	add	x5, x3, x5, lsl #7
  530a9c:	add	x2, x1, x0
  530aa0:	cmp	x5, #0x1
  530aa4:	add	x1, x4, x5
  530aa8:	csetm	x6, ne	// ne = any
  530aac:	cmp	x0, #0x1
  530ab0:	and	x1, x6, x1
  530ab4:	stp	x0, x5, [sp, #144]
  530ab8:	cset	x0, eq	// eq = none
  530abc:	sub	x0, x0, #0x1
  530ac0:	and	x0, x0, x2
  530ac4:	ldp	x2, x7, [x1]
  530ac8:	cmp	x2, #0x1
  530acc:	add	x2, x1, x2
  530ad0:	csetm	x3, ne	// ne = any
  530ad4:	and	x2, x3, x2
  530ad8:	sub	x0, x0, x2
  530adc:	asr	x0, x0, #3
  530ae0:	lsl	x0, x28, x0
  530ae4:	bic	x0, x7, x0
  530ae8:	str	x0, [x1, #8]
  530aec:	cbnz	x0, 530248 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x10a8>
  530af0:	add	x1, sp, #0x88
  530af4:	dup	v0.2d, x28
  530af8:	sub	x4, x4, x1
  530afc:	and	x0, x6, x4
  530b00:	add	x0, x0, x5
  530b04:	cmp	x0, #0x1
  530b08:	add	x1, x1, x0
  530b0c:	csetm	x0, ne	// ne = any
  530b10:	and	x0, x0, x1
  530b14:	add	x3, x0, #0x10
  530b18:	add	x4, x0, #0x18
  530b1c:	ldp	x1, x5, [x0, #16]
  530b20:	cmp	x1, #0x1
  530b24:	add	x1, x1, x3
  530b28:	csetm	x2, ne	// ne = any
  530b2c:	cmp	x5, #0x1
  530b30:	and	x2, x2, x1
  530b34:	csetm	x1, ne	// ne = any
  530b38:	sub	x6, x0, x2
  530b3c:	and	x1, x1, x6
  530b40:	add	x1, x1, x5
  530b44:	str	x1, [x2, #24]
  530b48:	ldp	x5, x1, [x0, #16]
  530b4c:	cmp	x1, #0x1
  530b50:	add	x1, x4, x1
  530b54:	csetm	x2, ne	// ne = any
  530b58:	cmp	x5, #0x1
  530b5c:	and	x2, x2, x1
  530b60:	csetm	x1, ne	// ne = any
  530b64:	add	x4, x2, #0x10
  530b68:	sub	x3, x3, x4
  530b6c:	and	x1, x1, x3
  530b70:	add	x1, x1, x5
  530b74:	str	x1, [x2, #16]
  530b78:	str	q0, [x0, #16]
  530b7c:	b	530248 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x10a8>
  530b80:	stp	x0, x6, [sp, #8]
  530b84:	bl	4e2770 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEESJ_ESE_EESE_EEE9incrementEv>
  530b88:	ldp	x0, x6, [sp, #8]
  530b8c:	ldr	x1, [sp, #328]
  530b90:	cmp	x1, #0x1
  530b94:	add	x1, x1, x0
  530b98:	csetm	x0, ne	// ne = any
  530b9c:	and	x0, x0, x1
  530ba0:	sub	x6, x0, x6
  530ba4:	ldr	x1, [x0]
  530ba8:	cmp	x1, #0x1
  530bac:	csetm	x0, ne	// ne = any
  530bb0:	and	x0, x0, x6
  530bb4:	add	x0, x0, x1
  530bb8:	str	x0, [sp, #320]
  530bbc:	b	52f720 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x580>
  530bc0:	mov	x1, x26
  530bc4:	mov	x0, x20
  530bc8:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  530bcc:	ldp	x5, x3, [sp, #192]
  530bd0:	add	x0, sp, #0xc8
  530bd4:	sub	x0, x0, x28
  530bd8:	add	x4, sp, #0x90
  530bdc:	sub	x6, x20, x4
  530be0:	cmp	x5, #0x1
  530be4:	csetm	x2, ne	// ne = any
  530be8:	cmp	x3, #0x1
  530bec:	csetm	x1, ne	// ne = any
  530bf0:	and	x2, x2, x6
  530bf4:	and	x1, x1, x0
  530bf8:	add	x2, x2, x5
  530bfc:	add	x1, x1, x3
  530c00:	stp	x2, x1, [sp, #144]
  530c04:	cmp	x1, #0x1
  530c08:	csetm	x5, ne	// ne = any
  530c0c:	add	x3, x28, x1
  530c10:	and	x3, x5, x3
  530c14:	cmp	x2, #0x1
  530c18:	add	x4, x4, x2
  530c1c:	cset	x0, eq	// eq = none
  530c20:	sub	x0, x0, #0x1
  530c24:	ldp	x2, x6, [x3]
  530c28:	and	x0, x0, x4
  530c2c:	cmp	x2, #0x1
  530c30:	add	x2, x3, x2
  530c34:	csetm	x4, ne	// ne = any
  530c38:	and	x2, x4, x2
  530c3c:	sub	x0, x0, x2
  530c40:	asr	x0, x0, #3
  530c44:	lsl	x0, x22, x0
  530c48:	bic	x0, x6, x0
  530c4c:	str	x0, [x3, #8]
  530c50:	cbnz	x0, 52f828 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x688>
  530c54:	add	x2, sp, #0x88
  530c58:	dup	v0.2d, x22
  530c5c:	sub	x0, x28, x2
  530c60:	and	x0, x5, x0
  530c64:	add	x0, x0, x1
  530c68:	cmp	x0, #0x1
  530c6c:	add	x2, x2, x0
  530c70:	csetm	x0, ne	// ne = any
  530c74:	and	x0, x0, x2
  530c78:	add	x3, x0, #0x10
  530c7c:	add	x4, x0, #0x18
  530c80:	ldp	x1, x5, [x0, #16]
  530c84:	cmp	x1, #0x1
  530c88:	add	x1, x1, x3
  530c8c:	csetm	x2, ne	// ne = any
  530c90:	cmp	x5, #0x1
  530c94:	and	x2, x2, x1
  530c98:	csetm	x1, ne	// ne = any
  530c9c:	sub	x6, x0, x2
  530ca0:	and	x1, x1, x6
  530ca4:	add	x1, x1, x5
  530ca8:	str	x1, [x2, #24]
  530cac:	ldp	x5, x1, [x0, #16]
  530cb0:	cmp	x1, #0x1
  530cb4:	add	x1, x4, x1
  530cb8:	csetm	x2, ne	// ne = any
  530cbc:	cmp	x5, #0x1
  530cc0:	and	x2, x2, x1
  530cc4:	csetm	x1, ne	// ne = any
  530cc8:	add	x4, x2, #0x10
  530ccc:	sub	x3, x3, x4
  530cd0:	and	x1, x1, x3
  530cd4:	add	x1, x1, x5
  530cd8:	str	x1, [x2, #16]
  530cdc:	str	q0, [x0, #16]
  530ce0:	b	52f828 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x688>
  530ce4:	ldr	x1, [x19, #112]
  530ce8:	add	x22, x19, #0x68
  530cec:	add	x23, sp, #0x128
  530cf0:	mov	w0, w20
  530cf4:	add	x25, sp, #0xf8
  530cf8:	bl	4fae00 <_ZN5boost9unordered6detail15prime_fmod_sizeIvE8positionEmm>
  530cfc:	mov	x1, x0
  530d00:	mov	x8, x23
  530d04:	mov	x0, x22
  530d08:	bl	4e22f0 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEESD_EENSB_9allocatorISA_NSB_15segment_managerIcNSB_15rbtree_best_fitINSB_12mutex_familyESD_Lm0EEENSB_10iset_indexEEEEENS1_15prime_fmod_sizeIvEEE2atEm>
  530d0c:	ldr	x0, [sp, #296]
  530d10:	cmp	x0, #0x1
  530d14:	add	x1, x23, x0
  530d18:	csetm	x0, ne	// ne = any
  530d1c:	and	x0, x0, x1
  530d20:	sub	x2, x0, x25
  530d24:	ldr	x0, [x0]
  530d28:	cmp	x0, #0x1
  530d2c:	csetm	x1, ne	// ne = any
  530d30:	and	x1, x1, x2
  530d34:	add	x1, x1, x0
  530d38:	str	x1, [sp, #248]
  530d3c:	cmp	x1, #0x1
  530d40:	b.eq	52fdac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc0c>  // b.none
  530d44:	add	x0, x25, x1
  530d48:	ldr	w2, [x0, #8]
  530d4c:	cmp	w20, w2
  530d50:	b.ne	5313e8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2248>  // b.any
  530d54:	add	x20, sp, #0x110
  530d58:	add	x21, sp, #0x140
  530d5c:	sub	x24, x25, x20
  530d60:	mov	x0, x21
  530d64:	add	x2, x24, x1
  530d68:	mov	x1, x23
  530d6c:	str	x2, [sp, #272]
  530d70:	add	x26, sp, #0xc0
  530d74:	add	x27, sp, #0xc8
  530d78:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  530d7c:	ldr	x3, [sp, #272]
  530d80:	sub	x4, x20, x26
  530d84:	mov	x0, x27
  530d88:	mov	x1, x21
  530d8c:	cmp	x3, #0x1
  530d90:	csetm	x2, ne	// ne = any
  530d94:	and	x2, x2, x4
  530d98:	add	x2, x2, x3
  530d9c:	str	x2, [sp, #192]
  530da0:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  530da4:	ldr	x0, [sp, #192]
  530da8:	cmp	x0, #0x1
  530dac:	add	x3, x26, x0
  530db0:	csetm	x5, ne	// ne = any
  530db4:	ands	x5, x5, x3
  530db8:	b.eq	52fdac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc0c>  // b.none
  530dbc:	ldp	x6, x4, [sp, #200]
  530dc0:	add	x1, sp, #0xe8
  530dc4:	sub	x27, x27, x1
  530dc8:	add	x2, sp, #0xe0
  530dcc:	sub	x20, x20, x21
  530dd0:	add	x26, sp, #0xb0
  530dd4:	cmp	x6, #0x1
  530dd8:	csetm	x0, ne	// ne = any
  530ddc:	cmp	x4, #0x1
  530de0:	and	x14, x0, x27
  530de4:	csetm	x13, ne	// ne = any
  530de8:	sub	x0, x3, x2
  530dec:	add	x14, x14, x6
  530df0:	cmp	x0, #0x1
  530df4:	and	x13, x13, #0xffffffffffffffe0
  530df8:	add	x13, x13, x4
  530dfc:	sub	x2, x2, x25
  530e00:	csetm	x12, ne	// ne = any
  530e04:	add	x3, sp, #0x100
  530e08:	cmp	x14, #0x1
  530e0c:	sub	x1, x1, x3
  530e10:	and	x12, x12, x2
  530e14:	csetm	x11, ne	// ne = any
  530e18:	cmp	x13, #0x1
  530e1c:	add	x12, x12, x0
  530e20:	and	x11, x11, x1
  530e24:	csetm	x2, ne	// ne = any
  530e28:	add	x11, x11, x14
  530e2c:	cmp	x12, #0x1
  530e30:	mov	x1, #0xffffffffffffffe8    	// #-24
  530e34:	and	x2, x2, x1
  530e38:	add	x2, x2, x13
  530e3c:	csetm	x4, ne	// ne = any
  530e40:	add	x1, sp, #0x118
  530e44:	cmp	x11, #0x1
  530e48:	sub	x3, x3, x1
  530e4c:	and	x4, x4, x24
  530e50:	csetm	x9, ne	// ne = any
  530e54:	cmp	x2, #0x1
  530e58:	add	x4, x4, x12
  530e5c:	and	x9, x9, x3
  530e60:	csetm	x8, ne	// ne = any
  530e64:	add	x9, x9, x11
  530e68:	cmp	x4, #0x1
  530e6c:	mov	x3, #0xffffffffffffffe8    	// #-24
  530e70:	and	x8, x8, x3
  530e74:	csetm	x7, ne	// ne = any
  530e78:	add	x8, x8, x2
  530e7c:	cmp	x9, #0x1
  530e80:	add	x27, sp, #0x148
  530e84:	csetm	x6, ne	// ne = any
  530e88:	sub	x1, x1, x27
  530e8c:	cmp	x8, #0x1
  530e90:	and	x7, x7, x20
  530e94:	and	x6, x6, x1
  530e98:	add	x7, x7, x4
  530e9c:	csetm	x3, ne	// ne = any
  530ea0:	add	x6, x6, x9
  530ea4:	cmp	x7, #0x1
  530ea8:	mov	x1, #0xffffffffffffffd0    	// #-48
  530eac:	and	x3, x3, x1
  530eb0:	add	x3, x3, x8
  530eb4:	csetm	x10, ne	// ne = any
  530eb8:	add	x24, sp, #0xa0
  530ebc:	cmp	x6, #0x1
  530ec0:	csetm	x15, ne	// ne = any
  530ec4:	sub	x1, x27, x24
  530ec8:	cmp	x3, #0x1
  530ecc:	and	x15, x15, x1
  530ed0:	csetm	x1, ne	// ne = any
  530ed4:	ldr	w25, [x5, #24]
  530ed8:	add	x15, x15, x6
  530edc:	stp	x0, x14, [sp, #224]
  530ee0:	mov	x0, #0xa8                  	// #168
  530ee4:	and	x1, x1, x0
  530ee8:	cmp	x15, #0x1
  530eec:	add	x1, x1, x3
  530ef0:	add	x5, sp, #0x78
  530ef4:	sub	x16, x21, x5
  530ef8:	csetm	x0, ne	// ne = any
  530efc:	cmp	x1, #0x1
  530f00:	and	x10, x10, x16
  530f04:	sub	x16, x24, x26
  530f08:	stp	x13, x12, [sp, #240]
  530f0c:	csetm	x13, ne	// ne = any
  530f10:	add	x10, x10, x7
  530f14:	and	x0, x0, x16
  530f18:	and	x13, x13, #0xfffffffffffffff0
  530f1c:	add	x0, x0, x15
  530f20:	cmp	x10, #0x1
  530f24:	add	x12, sp, #0x80
  530f28:	stp	x1, x0, [sp, #168]
  530f2c:	add	x1, x13, x1
  530f30:	str	x1, [sp, #184]
  530f34:	csetm	x1, ne	// ne = any
  530f38:	cmp	x0, #0x1
  530f3c:	stp	x11, x2, [sp, #256]
  530f40:	sub	x2, x5, x12
  530f44:	add	x0, x26, x0
  530f48:	and	x1, x1, x2
  530f4c:	csetm	x2, ne	// ne = any
  530f50:	add	x1, x1, x10
  530f54:	and	x2, x2, x0
  530f58:	str	x10, [sp, #120]
  530f5c:	str	x1, [sp, #128]
  530f60:	add	x11, x12, x1
  530f64:	str	x15, [sp, #160]
  530f68:	stp	x4, x9, [sp, #272]
  530f6c:	mov	x4, x2
  530f70:	str	x8, [sp, #288]
  530f74:	stp	x7, x6, [sp, #320]
  530f78:	str	x3, [sp, #336]
  530f7c:	ldr	x0, [x2]
  530f80:	cmp	x0, #0x1
  530f84:	add	x6, x2, x0
  530f88:	csetm	x0, ne	// ne = any
  530f8c:	cmp	x1, #0x1
  530f90:	csetm	x3, ne	// ne = any
  530f94:	and	x0, x0, x6
  530f98:	and	x3, x3, x11
  530f9c:	cmp	x0, x3
  530fa0:	b.eq	530fcc <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1e2c>  // b.none
  530fa4:	nop
  530fa8:	ldr	x1, [x0]
  530fac:	mov	x2, x0
  530fb0:	mov	x4, x0
  530fb4:	cmp	x1, #0x1
  530fb8:	add	x1, x0, x1
  530fbc:	csetm	x0, ne	// ne = any
  530fc0:	and	x0, x0, x1
  530fc4:	cmp	x0, x3
  530fc8:	b.ne	530fa8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1e08>  // b.any
  530fcc:	ldr	x1, [x3]
  530fd0:	sub	x3, x3, x4
  530fd4:	add	x20, sp, #0x130
  530fd8:	cmp	x1, #0x1
  530fdc:	csetm	x0, ne	// ne = any
  530fe0:	and	x0, x0, x3
  530fe4:	add	x0, x0, x1
  530fe8:	str	x0, [x2]
  530fec:	ldr	x0, [sp, #176]
  530ff0:	cmp	x0, #0x1
  530ff4:	add	x0, x26, x0
  530ff8:	csetm	x1, ne	// ne = any
  530ffc:	and	x0, x1, x0
  531000:	ldr	x2, [x0]
  531004:	cmp	x2, #0x1
  531008:	b.eq	531188 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fe8>  // b.none
  53100c:	ldr	x0, [x28, #8]
  531010:	sub	x5, x5, x23
  531014:	ldr	x6, [sp, #120]
  531018:	sub	x0, x0, #0x1
  53101c:	str	x0, [x28, #8]
  531020:	add	x2, x19, #0x140
  531024:	mov	w28, #0x1                   	// #1
  531028:	add	x4, x19, #0x148
  53102c:	ldr	x0, [x19, #104]
  531030:	strb	w28, [sp, #312]
  531034:	cmp	x0, #0x1
  531038:	add	x22, x22, x0
  53103c:	csetm	x0, ne	// ne = any
  531040:	and	x0, x0, x22
  531044:	cmp	x0, #0x0
  531048:	sub	x0, x0, x26
  53104c:	sub	x0, x0, #0x1
  531050:	csetm	x1, ne	// ne = any
  531054:	and	x1, x1, x0
  531058:	cmp	x6, #0x1
  53105c:	csetm	x3, ne	// ne = any
  531060:	add	x0, x1, #0x1
  531064:	cmp	x1, #0x0
  531068:	add	x1, x26, x0
  53106c:	csetm	x0, ne	// ne = any
  531070:	and	x3, x3, x5
  531074:	and	x0, x0, x1
  531078:	add	x3, x3, x6
  53107c:	cmp	x0, #0x0
  531080:	sub	x0, x0, x20
  531084:	sub	x0, x0, #0x1
  531088:	csetm	x1, ne	// ne = any
  53108c:	and	x1, x1, x0
  531090:	add	x1, x1, #0x1
  531094:	stp	x3, x1, [sp, #296]
  531098:	ldp	x5, x6, [x2, #16]
  53109c:	ldr	x1, [x19, #328]
  5310a0:	cmp	x1, #0x1
  5310a4:	add	x0, x4, x1
  5310a8:	csetm	x4, ne	// ne = any
  5310ac:	add	x1, x5, x5, lsl #1
  5310b0:	and	x0, x4, x0
  5310b4:	add	x1, x0, x1, lsl #3
  5310b8:	cmp	x5, x6
  5310bc:	b.eq	5312dc <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x213c>  // b.none
  5310c0:	cmp	x1, #0x0
  5310c4:	sub	x1, x1, x21
  5310c8:	sub	x1, x1, #0x1
  5310cc:	csetm	x0, ne	// ne = any
  5310d0:	and	x0, x0, x1
  5310d4:	add	x0, x0, #0x1
  5310d8:	str	x0, [sp, #320]
  5310dc:	cmp	x0, #0x1
  5310e0:	add	x0, x21, x0
  5310e4:	csetm	x1, ne	// ne = any
  5310e8:	cmp	x3, #0x1
  5310ec:	and	x1, x1, x0
  5310f0:	csetm	x4, ne	// ne = any
  5310f4:	mov	x21, x1
  5310f8:	sub	x23, x23, x1
  5310fc:	and	x4, x4, x23
  531100:	add	x3, x4, x3
  531104:	str	x3, [x1]
  531108:	str	xzr, [x21, #8]!
  53110c:	strb	wzr, [x21, #8]
  531110:	ldrb	w0, [sp, #312]
  531114:	tbnz	w0, #0, 531150 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1fb0>
  531118:	ldr	x0, [x2, #16]
  53111c:	add	x0, x0, #0x1
  531120:	str	x0, [x2, #16]
  531124:	ldr	x0, [x19, #296]
  531128:	add	x1, x19, #0x128
  53112c:	ldr	w2, [x19, #352]
  531130:	cmp	x0, #0x1
  531134:	add	x1, x1, x0
  531138:	csetm	x0, ne	// ne = any
  53113c:	and	x0, x0, x1
  531140:	ldp	x21, x22, [sp, #384]
  531144:	str	w2, [x0, w25, uxtw #2]
  531148:	str	w25, [x19, #352]
  53114c:	b	52f2f8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x158>
  531150:	mov	x1, x20
  531154:	mov	x0, x21
  531158:	str	x2, [sp, #8]
  53115c:	bl	4dc9e4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_EC1ERKSI_>
  531160:	ldr	x2, [sp, #8]
  531164:	strb	w28, [x21, #8]
  531168:	b	531118 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1f78>
  53116c:	mov	x2, #0x0                   	// #0
  531170:	b	52f43c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x29c>
  531174:	bl	48c090 <__errno_location@plt>
  531178:	mov	x20, #0xffffffffffffffff    	// #-1
  53117c:	mov	w1, #0x2                   	// #2
  531180:	str	w1, [x0]
  531184:	b	530848 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x16a8>
  531188:	mov	x1, x26
  53118c:	mov	x0, x23
  531190:	stp	x5, x2, [sp, #8]
  531194:	bl	4df200 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  531198:	add	x7, sp, #0x98
  53119c:	ldp	x4, x0, [sp, #296]
  5311a0:	sub	x8, x20, x7
  5311a4:	add	x6, sp, #0x90
  5311a8:	sub	x9, x23, x6
  5311ac:	ldp	x5, x2, [sp, #8]
  5311b0:	cmp	x4, #0x1
  5311b4:	csetm	x3, ne	// ne = any
  5311b8:	cmp	x0, #0x1
  5311bc:	csetm	x1, ne	// ne = any
  5311c0:	and	x3, x3, x9
  5311c4:	and	x1, x1, x8
  5311c8:	add	x3, x3, x4
  5311cc:	add	x1, x1, x0
  5311d0:	stp	x3, x1, [sp, #144]
  5311d4:	cmp	x1, #0x1
  5311d8:	csetm	x8, ne	// ne = any
  5311dc:	add	x4, x7, x1
  5311e0:	and	x4, x8, x4
  5311e4:	cmp	x3, #0x1
  5311e8:	add	x6, x6, x3
  5311ec:	cset	x0, eq	// eq = none
  5311f0:	sub	x0, x0, #0x1
  5311f4:	ldp	x3, x9, [x4]
  5311f8:	and	x0, x0, x6
  5311fc:	cmp	x3, #0x1
  531200:	add	x3, x4, x3
  531204:	csetm	x6, ne	// ne = any
  531208:	and	x3, x6, x3
  53120c:	sub	x0, x0, x3
  531210:	asr	x0, x0, #3
  531214:	lsl	x0, x2, x0
  531218:	bic	x0, x9, x0
  53121c:	str	x0, [x4, #8]
  531220:	cbnz	x0, 53100c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1e6c>
  531224:	add	x3, sp, #0x88
  531228:	dup	v0.2d, x2
  53122c:	sub	x7, x7, x3
  531230:	and	x0, x8, x7
  531234:	add	x0, x0, x1
  531238:	cmp	x0, #0x1
  53123c:	add	x3, x3, x0
  531240:	csetm	x0, ne	// ne = any
  531244:	and	x0, x0, x3
  531248:	add	x3, x0, #0x10
  53124c:	add	x4, x0, #0x18
  531250:	ldp	x1, x6, [x0, #16]
  531254:	cmp	x1, #0x1
  531258:	add	x1, x1, x3
  53125c:	csetm	x2, ne	// ne = any
  531260:	cmp	x6, #0x1
  531264:	and	x2, x2, x1
  531268:	csetm	x1, ne	// ne = any
  53126c:	sub	x7, x0, x2
  531270:	and	x1, x1, x7
  531274:	add	x1, x1, x6
  531278:	str	x1, [x2, #24]
  53127c:	ldp	x6, x1, [x0, #16]
  531280:	cmp	x1, #0x1
  531284:	add	x1, x4, x1
  531288:	csetm	x2, ne	// ne = any
  53128c:	cmp	x6, #0x1
  531290:	and	x2, x2, x1
  531294:	csetm	x1, ne	// ne = any
  531298:	add	x4, x2, #0x10
  53129c:	sub	x3, x3, x4
  5312a0:	and	x1, x1, x3
  5312a4:	add	x1, x1, x6
  5312a8:	str	x1, [x2, #16]
  5312ac:	str	q0, [x0, #16]
  5312b0:	b	53100c <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1e6c>
  5312b4:	ldr	w0, [x19, #40]
  5312b8:	mov	x1, x20
  5312bc:	bl	1b37e80 <bpf_map_delete_elem>
  5312c0:	sxtw	x20, w0
  5312c4:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  5312c8:	bl	48c090 <__errno_location@plt>
  5312cc:	mov	x20, #0xffffffffffffffff    	// #-1
  5312d0:	mov	w1, #0x2                   	// #2
  5312d4:	str	w1, [x0]
  5312d8:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  5312dc:	mov	x0, x2
  5312e0:	mov	x8, x21
  5312e4:	mov	x2, x23
  5312e8:	bl	5358e0 <_ZN5boost9container6vectorINS_9unordered15node_handle_mapINS2_6detail4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEEjSA_NSC_9allocatorISB_NSC_15segment_managerIcNSC_15rbtree_best_fitINSC_12mutex_familyESE_Lm0EEENSC_10iset_indexEEEEEEENSG_ISO_SM_EEvE37priv_insert_forward_range_no_capacityINS0_3dtl20insert_emplace_proxyISP_JSO_EEEEENS0_12vec_iteratorINSD_ISO_lmLm0EEELb0EEEPSO_mT_NS_11move_detail17integral_constantIjLj2EEE.constprop.0>
  5312ec:	ldr	x0, [sp, #296]
  5312f0:	cmp	x0, #0x1
  5312f4:	b.eq	531124 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1f84>  // b.none
  5312f8:	ldr	x2, [sp, #304]
  5312fc:	add	x0, x23, x0
  531300:	sub	x1, x0, x26
  531304:	add	x21, sp, #0x98
  531308:	cmp	x2, #0x1
  53130c:	add	x2, x20, x2
  531310:	csetm	x0, ne	// ne = any
  531314:	sub	x3, x26, x27
  531318:	and	x0, x0, x2
  53131c:	cmp	x0, #0x0
  531320:	sub	x0, x0, x21
  531324:	csetm	x2, ne	// ne = any
  531328:	cmp	x1, #0x1
  53132c:	sub	x0, x0, #0x1
  531330:	csetm	x20, ne	// ne = any
  531334:	and	x0, x2, x0
  531338:	and	x20, x20, x3
  53133c:	add	x0, x0, #0x1
  531340:	add	x20, x20, x1
  531344:	str	x0, [sp, #152]
  531348:	stp	x21, x20, [sp, #320]
  53134c:	cmp	x20, #0x1
  531350:	b.eq	531124 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1f84>  // b.none
  531354:	mov	x0, x24
  531358:	mov	x1, x21
  53135c:	bl	4dc9e4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_EC1ERKSI_>
  531360:	sub	x2, x27, x26
  531364:	mov	x1, x26
  531368:	add	x2, x2, x20
  53136c:	mov	x0, x21
  531370:	str	x2, [sp, #176]
  531374:	bl	54fc90 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  531378:	b	531124 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1f84>
  53137c:	ldr	x1, [x25, x3]
  531380:	sub	x0, x0, x25
  531384:	cmp	x1, #0x1
  531388:	csetm	x3, ne	// ne = any
  53138c:	and	x3, x3, x0
  531390:	add	x3, x3, x1
  531394:	str	x3, [sp, #248]
  531398:	cmp	x3, #0x1
  53139c:	b.ne	5303c4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1224>  // b.any
  5313a0:	b	52fcf8 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xb58>
  5313a4:	sub	x1, x1, #0x1d
  5313a8:	adrp	x2, 2b0c000 <_ZTVN5boost12interprocess9ipcdetail17placement_destroyIN7bpftime15handler_managerEEE+0x28>
  5313ac:	add	x2, x2, #0x3c8
  5313b0:	ldr	x1, [x2, x1, lsl #3]
  5313b4:	blr	x1
  5313b8:	mov	x1, x0
  5313bc:	b	530378 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x11d8>
  5313c0:	bl	48c090 <__errno_location@plt>
  5313c4:	mov	x20, #0xffffffffffffffff    	// #-1
  5313c8:	mov	w1, #0x16                  	// #22
  5313cc:	str	w1, [x0]
  5313d0:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  5313d4:	bl	48c090 <__errno_location@plt>
  5313d8:	mov	x20, #0xffffffffffffffff    	// #-1
  5313dc:	mov	w1, #0x16                  	// #22
  5313e0:	str	w1, [x0]
  5313e4:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  5313e8:	ldr	x2, [x25, x1]
  5313ec:	sub	x0, x0, x25
  5313f0:	cmp	x2, #0x1
  5313f4:	csetm	x1, ne	// ne = any
  5313f8:	and	x1, x1, x0
  5313fc:	add	x1, x1, x2
  531400:	str	x1, [sp, #248]
  531404:	cmp	x1, #0x1
  531408:	b.ne	530d44 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x1ba4>  // b.any
  53140c:	b	52fdac <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0xc0c>
  531410:	stp	x21, x22, [sp, #384]
  531414:	bl	48d490 <__stack_chk_fail@plt>
  531418:	bl	5044d0 <_ZN6spdlog18default_logger_rawEv>
  53141c:	mov	w3, #0x20c                 	// #524
  531420:	adrp	x2, 1d33000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe788>
  531424:	add	x2, x2, #0xe08
  531428:	str	x2, [sp, #96]
  53142c:	str	w3, [sp, #104]
  531430:	add	x1, sp, #0x30
  531434:	adrp	x6, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  531438:	add	x6, x6, #0xc20
  53143c:	ldp	x8, x9, [sp, #96]
  531440:	stp	x8, x9, [sp, #48]
  531444:	adrp	x2, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  531448:	str	x6, [sp, #112]
  53144c:	add	x5, x22, #0x10
  531450:	str	x6, [x1, #16]
  531454:	mov	x4, x20
  531458:	add	x2, x2, #0xcf8
  53145c:	mov	x3, #0x40                  	// #64
  531460:	bl	5397f0 <_ZN6spdlog6logger4log_IJRjS2_EEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_.constprop.0>
  531464:	bl	48c090 <__errno_location@plt>
  531468:	mov	x20, #0xffffffffffffffff    	// #-1
  53146c:	mov	w1, #0x16                  	// #22
  531470:	str	w1, [x0]
  531474:	b	530848 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x16a8>
  531478:	bl	5044d0 <_ZN6spdlog18default_logger_rawEv>
  53147c:	mov	w3, #0x49                  	// #73
  531480:	adrp	x2, 1d33000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe788>
  531484:	add	x2, x2, #0xcb0
  531488:	str	x2, [sp, #96]
  53148c:	str	w3, [sp, #104]
  531490:	add	x1, sp, #0x30
  531494:	adrp	x5, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  531498:	add	x5, x5, #0xc20
  53149c:	ldp	x6, x7, [sp, #96]
  5314a0:	stp	x6, x7, [sp, #48]
  5314a4:	adrp	x3, 1d33000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe788>
  5314a8:	str	x5, [sp, #112]
  5314ac:	add	x3, x3, #0xd18
  5314b0:	str	x5, [x1, #16]
  5314b4:	mov	x4, #0x30                  	// #48
  5314b8:	mov	w2, #0x4                   	// #4
  5314bc:	bl	4b30e0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  5314c0:	bl	48c090 <__errno_location@plt>
  5314c4:	mov	x20, #0xffffffffffffffff    	// #-1
  5314c8:	mov	w1, #0x16                  	// #22
  5314cc:	str	w1, [x0]
  5314d0:	b	52f948 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x7a8>
  5314d4:	bl	5044d0 <_ZN6spdlog18default_logger_rawEv>
  5314d8:	mov	x20, #0xffffffffffffffff    	// #-1
  5314dc:	mov	w3, #0x327                 	// #807
  5314e0:	adrp	x2, 1d34000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf788>
  5314e4:	add	x2, x2, #0xd8
  5314e8:	str	x2, [sp, #96]
  5314ec:	str	w3, [sp, #104]
  5314f0:	add	x1, sp, #0x30
  5314f4:	ldr	w7, [x19, #72]
  5314f8:	adrp	x6, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  5314fc:	ldp	x8, x9, [sp, #96]
  531500:	add	x6, x6, #0xd40
  531504:	stp	x8, x9, [sp, #48]
  531508:	add	x5, sp, #0x5c
  53150c:	mov	x4, #0x2e                  	// #46
  531510:	str	w7, [sp, #92]
  531514:	adrp	x3, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  531518:	str	x6, [sp, #112]
  53151c:	add	x3, x3, #0xd50
  531520:	str	x6, [x1, #16]
  531524:	mov	w2, #0x4                   	// #4
  531528:	bl	4c88f0 <_ZN6spdlog6logger4log_IJRKiEEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_>
  53152c:	b	52f300 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x160>
  531530:	bl	5044d0 <_ZN6spdlog18default_logger_rawEv>
  531534:	mov	w3, #0x202                 	// #514
  531538:	adrp	x2, 1d33000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe788>
  53153c:	add	x2, x2, #0xe08
  531540:	str	x2, [sp, #96]
  531544:	str	w3, [sp, #104]
  531548:	add	x1, sp, #0x30
  53154c:	adrp	x5, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  531550:	add	x5, x5, #0xc20
  531554:	ldp	x6, x7, [sp, #96]
  531558:	stp	x6, x7, [sp, #48]
  53155c:	adrp	x3, 1d38000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x13788>
  531560:	str	x5, [sp, #112]
  531564:	add	x3, x3, #0xcc0
  531568:	str	x5, [x1, #16]
  53156c:	mov	x4, #0x33                  	// #51
  531570:	mov	w2, #0x4                   	// #4
  531574:	bl	4b30e0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  531578:	b	531464 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x22c4>
  53157c:	mov	x20, x0
  531580:	b	5315c4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2424>
  531584:	mov	x1, x0
  531588:	mov	x0, x19
  53158c:	mov	x19, x1
  531590:	bl	48c720 <pthread_spin_unlock@plt>
  531594:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  531598:	ldr	x0, [x0, #1160]
  53159c:	ldr	x2, [sp, #344]
  5315a0:	ldr	x1, [x0]
  5315a4:	subs	x2, x2, x1
  5315a8:	mov	x1, #0x0                   	// #0
  5315ac:	b.ne	531414 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2274>  // b.any
  5315b0:	mov	x0, x19
  5315b4:	bl	48c590 <_Unwind_Resume@plt>
  5315b8:	mov	x20, x0
  5315bc:	mov	x0, x21
  5315c0:	bl	4df2e0 <_ZN5boost12interprocess9ipcdetail12shared_countIN7bpftime18lru_linklist_entryENS0_9allocatorIvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIS4_SD_EEED1Ev>
  5315c4:	mov	x0, x19
  5315c8:	bl	48c720 <pthread_spin_unlock@plt>
  5315cc:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  5315d0:	ldr	x0, [x0, #1160]
  5315d4:	ldr	x2, [sp, #344]
  5315d8:	ldr	x1, [x0]
  5315dc:	subs	x2, x2, x1
  5315e0:	mov	x1, #0x0                   	// #0
  5315e4:	b.ne	531414 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2274>  // b.any
  5315e8:	mov	x0, x20
  5315ec:	bl	48c590 <_Unwind_Resume@plt>
  5315f0:	mov	x20, x0
  5315f4:	mov	x0, x24
  5315f8:	bl	48c720 <pthread_spin_unlock@plt>
  5315fc:	b	5315c4 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2424>
  531600:	mov	x19, x0
  531604:	mov	x0, x23
  531608:	bl	4e1ab0 <_ZN5boost9unordered15node_handle_mapINS0_6detail4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEEjS8_NSA_9allocatorIS9_NSA_15segment_managerIcNSA_15rbtree_best_fitINSA_12mutex_familyESC_Lm0EEENSA_10iset_indexEEEEEED1Ev>
  53160c:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  531610:	ldr	x0, [x0, #1160]
  531614:	ldr	x2, [sp, #344]
  531618:	ldr	x1, [x0]
  53161c:	subs	x2, x2, x1
  531620:	mov	x1, #0x0                   	// #0
  531624:	b.eq	5315b0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2410>  // b.none
  531628:	b	531414 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0+0x2274>

Disassembly of section .fini:
