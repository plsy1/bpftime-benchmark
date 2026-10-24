
/home/jetson/src/results/uprobe/percpu-array-jit-direct-cpu-20261020/artifacts/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004f36a4 <bpftime_map_delete_elem_helper>:
  4f36a4:	sub	sp, sp, #0x1c0
  4f36a8:	cmp	w0, #0x0
  4f36ac:	adrp	x2, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f36b0:	ldr	x2, [x2, #1160]
  4f36b4:	stp	x29, x30, [sp, #352]
  4f36b8:	add	x29, sp, #0x160
  4f36bc:	stp	x21, x22, [sp, #384]
  4f36c0:	adrp	x21, 2e58000 <_frida__dwarf_failsafe_error+0x8>
  4f36c4:	add	x21, x21, #0x870
  4f36c8:	stp	x19, x20, [sp, #368]
  4f36cc:	stp	x23, x24, [sp, #400]
  4f36d0:	stp	x25, x26, [sp, #416]
  4f36d4:	stp	x27, x28, [sp, #432]
  4f36d8:	ldr	x3, [x21, #72]
  4f36dc:	ldr	x4, [x2]
  4f36e0:	str	x4, [sp, #344]
  4f36e4:	mov	x4, #0x0                   	// #0
  4f36e8:	ccmp	x3, #0x0, #0x4, ge	// ge = tcont
  4f36ec:	b.ne	4f3740 <bpftime_map_delete_elem_helper+0x9c>  // b.any
  4f36f0:	bl	48c090 <__errno_location@plt>
  4f36f4:	mov	x20, #0xffffffffffffffff    	// #-1
  4f36f8:	mov	w1, #0x2                   	// #2
  4f36fc:	str	w1, [x0]
  4f3700:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f3704:	ldr	x0, [x0, #1160]
  4f3708:	ldr	x2, [sp, #344]
  4f370c:	ldr	x1, [x0]
  4f3710:	subs	x2, x2, x1
  4f3714:	mov	x1, #0x0                   	// #0
  4f3718:	b.ne	4f59e4 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f371c:	ldp	x29, x30, [sp, #352]
  4f3720:	mov	x0, x20
  4f3724:	ldp	x19, x20, [sp, #368]
  4f3728:	ldp	x21, x22, [sp, #384]
  4f372c:	ldp	x23, x24, [sp, #400]
  4f3730:	ldp	x25, x26, [sp, #416]
  4f3734:	ldp	x27, x28, [sp, #432]
  4f3738:	add	sp, sp, #0x1c0
  4f373c:	ret
  4f3740:	mov	x19, x0
  4f3744:	ldr	x0, [x3, #16]
  4f3748:	cmp	x0, w19, sxtw
  4f374c:	b.ls	4f36f0 <bpftime_map_delete_elem_helper+0x4c>  // b.plast
  4f3750:	mov	x20, x1
  4f3754:	mov	x22, x1
  4f3758:	mov	x0, x3
  4f375c:	mov	w1, w19
  4f3760:	bl	4dcd00 <_ZNK7bpftime15handler_manager11get_handlerEi>
  4f3764:	ldrb	w0, [x0, #160]
  4f3768:	cmp	w0, #0x1
  4f376c:	b.ne	4f36f0 <bpftime_map_delete_elem_helper+0x4c>  // b.any
  4f3770:	ldr	x0, [x21, #72]
  4f3774:	mov	w1, w19
  4f3778:	bl	4dcd00 <_ZNK7bpftime15handler_manager11get_handlerEi>
  4f377c:	mov	x19, x0
  4f3780:	ldrb	w0, [x0, #160]
  4f3784:	cmp	w0, #0x1
  4f3788:	b.ne	4f5b20 <bpftime_map_delete_elem_helper+0x247c>  // b.any
  4f378c:	ldr	w0, [x19, #72]
  4f3790:	cmp	w0, #0x1e
  4f3794:	b.gt	4f382c <bpftime_map_delete_elem_helper+0x188>
  4f3798:	cmp	w0, #0x0
  4f379c:	b.le	4f37ac <bpftime_map_delete_elem_helper+0x108>
  4f37a0:	sub	w1, w0, #0x1
  4f37a4:	cmp	w1, #0x1d
  4f37a8:	b.ls	4f3814 <bpftime_map_delete_elem_helper+0x170>  // b.plast
  4f37ac:	sbfiz	x2, x0, #1, #32
  4f37b0:	adrp	x1, 1d0c000 <_ZTSSt23_Sp_counted_ptr_inplaceIN5boost12interprocess27basic_managed_shared_memoryIcNS1_15rbtree_best_fitINS1_12mutex_familyENS1_10offset_ptrIvlmLm0EEELm0EEENS1_10iset_indexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x58>
  4f37b4:	add	x0, x2, w0, sxtw
  4f37b8:	add	x1, x1, #0xcb0
  4f37bc:	add	x0, x1, x0, lsl #4
  4f37c0:	ldr	x3, [x0, #32]
  4f37c4:	cbz	x3, 4f4d98 <bpftime_map_delete_elem_helper+0x16f4>
  4f37c8:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f37cc:	ldr	x0, [x0, #1160]
  4f37d0:	ldr	x2, [sp, #344]
  4f37d4:	ldr	x1, [x0]
  4f37d8:	subs	x2, x2, x1
  4f37dc:	mov	x1, #0x0                   	// #0
  4f37e0:	b.ne	4f59e4 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f37e4:	ldp	x29, x30, [sp, #352]
  4f37e8:	mov	x1, x20
  4f37ec:	ldp	x21, x22, [sp, #384]
  4f37f0:	mov	x16, x3
  4f37f4:	ldp	x23, x24, [sp, #400]
  4f37f8:	mov	w2, #0x0                   	// #0
  4f37fc:	ldp	x25, x26, [sp, #416]
  4f3800:	ldp	x27, x28, [sp, #432]
  4f3804:	ldr	w0, [x19, #112]
  4f3808:	ldp	x19, x20, [sp, #368]
  4f380c:	add	sp, sp, #0x1c0
  4f3810:	br	x16
  4f3814:	adrp	x2, 1d25000 <_ZN7bpftime20global_map_ops_tableE+0x18350>
  4f3818:	add	x2, x2, #0x2b0
  4f381c:	ldrh	w2, [x2, w1, uxtw #1]
  4f3820:	adr	x1, 4f382c <bpftime_map_delete_elem_helper+0x188>
  4f3824:	add	x2, x1, w2, sxth #2
  4f3828:	br	x2
  4f382c:	cmp	w0, #0x3ec
  4f3830:	b.eq	4f38b0 <bpftime_map_delete_elem_helper+0x20c>  // b.none
  4f3834:	b.gt	4f4314 <bpftime_map_delete_elem_helper+0xc70>
  4f3838:	cmp	w0, #0x3e9
  4f383c:	b.eq	4f4348 <bpftime_map_delete_elem_helper+0xca4>  // b.none
  4f3840:	cmp	w0, #0x3ea
  4f3844:	b.ne	4f37ac <bpftime_map_delete_elem_helper+0x108>  // b.any
  4f3848:	ldr	x0, [x19, #120]
  4f384c:	add	x19, x19, #0x78
  4f3850:	cmp	x0, #0x1
  4f3854:	add	x0, x19, x0
  4f3858:	csetm	x19, ne	// ne = any
  4f385c:	and	x19, x19, x0
  4f3860:	ldrb	w0, [x19, #48]
  4f3864:	tbnz	w0, #0, 4f439c <bpftime_map_delete_elem_helper+0xcf8>
  4f3868:	ldr	w0, [x19, #36]
  4f386c:	ldr	w3, [x20]
  4f3870:	cmp	w3, w0
  4f3874:	b.cs	4f4388 <bpftime_map_delete_elem_helper+0xce4>  // b.hs, b.nlast
  4f3878:	ldr	w5, [x19, #32]
  4f387c:	add	x4, x19, #0x8
  4f3880:	ldr	x0, [x19, #8]
  4f3884:	mov	w2, w5
  4f3888:	mov	w1, #0x0                   	// #0
  4f388c:	mov	x20, #0x0                   	// #0
  4f3890:	mul	w3, w3, w5
  4f3894:	add	x4, x4, x0
  4f3898:	cmp	x0, #0x1
  4f389c:	csetm	x0, ne	// ne = any
  4f38a0:	and	x0, x0, x4
  4f38a4:	add	x0, x0, x3
  4f38a8:	bl	48bc60 <memset@plt>
  4f38ac:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f38b0:	ldr	x0, [x19, #120]
  4f38b4:	add	x19, x19, #0x78
  4f38b8:	ldr	w1, [x20]
  4f38bc:	cmp	x0, #0x1
  4f38c0:	add	x19, x19, x0
  4f38c4:	csetm	x0, ne	// ne = any
  4f38c8:	and	x0, x0, x19
  4f38cc:	ldr	w0, [x0, #12]
  4f38d0:	str	w1, [sp, #92]
  4f38d4:	tbnz	w1, #31, 4f5964 <bpftime_map_delete_elem_helper+0x22c0>
  4f38d8:	mov	w0, w0
  4f38dc:	cmp	x0, w1, sxtw
  4f38e0:	b.ls	4f5964 <bpftime_map_delete_elem_helper+0x22c0>  // b.plast
  4f38e4:	bl	506894 <_ZN6spdlog18default_logger_rawEv>
  4f38e8:	mov	x20, #0x0                   	// #0
  4f38ec:	mov	w3, #0xca                  	// #202
  4f38f0:	adrp	x2, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f38f4:	add	x2, x2, #0xe18
  4f38f8:	str	x2, [sp, #96]
  4f38fc:	str	w3, [sp, #104]
  4f3900:	add	x1, sp, #0x30
  4f3904:	adrp	x6, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f3908:	add	x6, x6, #0xd78
  4f390c:	ldp	x8, x9, [sp, #96]
  4f3910:	stp	x8, x9, [sp, #48]
  4f3914:	add	x5, sp, #0x5c
  4f3918:	str	x6, [sp, #112]
  4f391c:	mov	x4, #0x43                  	// #67
  4f3920:	str	x6, [x1, #16]
  4f3924:	adrp	x3, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f3928:	mov	w2, #0x2                   	// #2
  4f392c:	add	x3, x3, #0xdd0
  4f3930:	bl	4c98f0 <_ZN6spdlog6logger4log_IJRKiEEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_>
  4f3934:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f3938:	bl	48c090 <__errno_location@plt>
  4f393c:	mov	w1, #0x16                  	// #22
  4f3940:	str	w1, [x0]
  4f3944:	mov	x20, #0xffffffffffffffff    	// #-1
  4f3948:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f394c:	ldr	x0, [x19, #120]
  4f3950:	add	x19, x19, #0x78
  4f3954:	ldr	w1, [x20]
  4f3958:	cmp	x0, #0x1
  4f395c:	add	x19, x19, x0
  4f3960:	csetm	x0, ne	// ne = any
  4f3964:	and	x0, x0, x19
  4f3968:	tbnz	w1, #31, 4f3938 <bpftime_map_delete_elem_helper+0x294>
  4f396c:	ldr	x2, [x0, #16]
  4f3970:	sxtw	x1, w1
  4f3974:	cmp	x1, x2
  4f3978:	b.cs	4f3938 <bpftime_map_delete_elem_helper+0x294>  // b.hs, b.nlast
  4f397c:	ldr	x2, [x0, #8]
  4f3980:	add	x0, x0, #0x8
  4f3984:	mov	w3, #0xffffffff            	// #-1
  4f3988:	mov	x20, #0x0                   	// #0
  4f398c:	cmp	x2, #0x1
  4f3990:	add	x0, x0, x2
  4f3994:	csetm	x2, ne	// ne = any
  4f3998:	and	x0, x2, x0
  4f399c:	str	w3, [x0, x1, lsl #2]
  4f39a0:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f39a4:	bl	506894 <_ZN6spdlog18default_logger_rawEv>
  4f39a8:	mov	x20, #0xffffffffffffffff    	// #-1
  4f39ac:	mov	w3, #0x7e                  	// #126
  4f39b0:	adrp	x2, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f39b4:	add	x2, x2, #0xa70
  4f39b8:	str	x2, [sp, #96]
  4f39bc:	str	w3, [sp, #104]
  4f39c0:	add	x1, sp, #0x30
  4f39c4:	adrp	x5, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f39c8:	add	x5, x5, #0xd78
  4f39cc:	ldp	x6, x7, [sp, #96]
  4f39d0:	stp	x6, x7, [sp, #48]
  4f39d4:	adrp	x3, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f39d8:	str	x5, [sp, #112]
  4f39dc:	add	x3, x3, #0xd88
  4f39e0:	str	x5, [x1, #16]
  4f39e4:	mov	x4, #0x41                  	// #65
  4f39e8:	mov	w2, #0x4                   	// #4
  4f39ec:	bl	4b40c0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  4f39f0:	bl	48c090 <__errno_location@plt>
  4f39f4:	mov	w1, #0x5f                  	// #95
  4f39f8:	str	w1, [x0]
  4f39fc:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f3a00:	ldr	x0, [x19, #120]
  4f3a04:	add	x1, x19, #0x78
  4f3a08:	add	x19, x19, #0x74
  4f3a0c:	cmp	x0, #0x1
  4f3a10:	add	x1, x1, x0
  4f3a14:	csetm	x21, ne	// ne = any
  4f3a18:	mov	x0, x19
  4f3a1c:	and	x21, x21, x1
  4f3a20:	add	x20, x21, #0x8
  4f3a24:	bl	48cea0 <pthread_spin_lock@plt>
  4f3a28:	ldr	x9, [x20, #16]
  4f3a2c:	cbz	x9, 4f3e80 <bpftime_map_delete_elem_helper+0x7dc>
  4f3a30:	ldr	x10, [x21, #16]
  4f3a34:	add	x5, sp, #0x140
  4f3a38:	add	x11, x20, #0x8
  4f3a3c:	add	x2, sp, #0x110
  4f3a40:	cmp	x10, #0x1
  4f3a44:	sub	x0, x11, x5
  4f3a48:	csetm	x8, ne	// ne = any
  4f3a4c:	ldr	w3, [x21]
  4f3a50:	and	x0, x8, x0
  4f3a54:	sub	x6, x5, x2
  4f3a58:	add	x0, x0, x10
  4f3a5c:	sub	x3, x9, x3
  4f3a60:	cmp	x0, #0x1
  4f3a64:	add	x7, sp, #0xf8
  4f3a68:	csetm	x1, ne	// ne = any
  4f3a6c:	sub	x12, x2, x7
  4f3a70:	and	x6, x1, x6
  4f3a74:	add	x4, sp, #0x128
  4f3a78:	add	x6, x6, x0
  4f3a7c:	sub	x2, x5, x4
  4f3a80:	add	x6, x6, x3
  4f3a84:	and	x1, x1, x2
  4f3a88:	cmp	x6, #0x1
  4f3a8c:	add	x0, x0, x9
  4f3a90:	csetm	x3, ne	// ne = any
  4f3a94:	add	x0, x1, x0
  4f3a98:	and	x3, x3, x12
  4f3a9c:	add	x2, sp, #0xe0
  4f3aa0:	add	x3, x3, x6
  4f3aa4:	mov	x1, x2
  4f3aa8:	cmp	x3, #0x1
  4f3aac:	sub	x2, x7, x2
  4f3ab0:	sub	x7, x4, x5
  4f3ab4:	csetm	x4, ne	// ne = any
  4f3ab8:	cmp	x0, #0x1
  4f3abc:	and	x4, x4, x2
  4f3ac0:	csetm	x2, ne	// ne = any
  4f3ac4:	add	x4, x4, x3
  4f3ac8:	and	x2, x2, x7
  4f3acc:	cmp	x4, #0x1
  4f3ad0:	add	x2, x2, x0
  4f3ad4:	csetm	x7, ne	// ne = any
  4f3ad8:	cmp	x2, #0x1
  4f3adc:	add	x1, x1, x4
  4f3ae0:	add	x5, x5, x2
  4f3ae4:	csetm	x12, ne	// ne = any
  4f3ae8:	and	x7, x7, x1
  4f3aec:	and	x1, x12, x5
  4f3af0:	str	x4, [sp, #224]
  4f3af4:	str	x3, [sp, #248]
  4f3af8:	str	x6, [sp, #272]
  4f3afc:	str	x0, [sp, #296]
  4f3b00:	str	x2, [sp, #320]
  4f3b04:	cmp	x7, x1
  4f3b08:	b.eq	4f3b4c <bpftime_map_delete_elem_helper+0x4a8>  // b.none
  4f3b0c:	add	x10, x10, x11
  4f3b10:	cmp	x1, #0x0
  4f3b14:	and	x8, x8, x10
  4f3b18:	add	x21, x8, x9
  4f3b1c:	ccmp	x1, x21, #0x4, ne	// ne = any
  4f3b20:	ccmp	x7, #0x0, #0x4, ne	// ne = any
  4f3b24:	b.eq	4f3b40 <bpftime_map_delete_elem_helper+0x49c>  // b.none
  4f3b28:	sub	x22, x21, x1
  4f3b2c:	mov	x0, x7
  4f3b30:	mov	x2, x22
  4f3b34:	bl	48b570 <memmove@plt>
  4f3b38:	ldr	x9, [x20, #16]
  4f3b3c:	add	x7, x0, x22
  4f3b40:	sub	x21, x21, x7
  4f3b44:	sub	x9, x9, x21
  4f3b48:	str	x9, [x20, #16]
  4f3b4c:	mov	x20, #0x0                   	// #0
  4f3b50:	mov	x0, x19
  4f3b54:	bl	48c720 <pthread_spin_unlock@plt>
  4f3b58:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f3b5c:	ldr	x0, [x19, #120]
  4f3b60:	add	x1, x19, #0x78
  4f3b64:	add	x19, x19, #0x74
  4f3b68:	cmp	x0, #0x1
  4f3b6c:	add	x1, x1, x0
  4f3b70:	csetm	x28, ne	// ne = any
  4f3b74:	mov	x0, x19
  4f3b78:	and	x28, x28, x1
  4f3b7c:	bl	48cea0 <pthread_spin_lock@plt>
  4f3b80:	ldr	x27, [x28]
  4f3b84:	cmp	x27, #0x4
  4f3b88:	b.eq	4f4d60 <bpftime_map_delete_elem_helper+0x16bc>  // b.none
  4f3b8c:	add	x2, x20, x27
  4f3b90:	mov	x0, #0x0                   	// #0
  4f3b94:	cbz	x27, 4f5724 <bpftime_map_delete_elem_helper+0x2080>
  4f3b98:	ldrb	w1, [x20], #1
  4f3b9c:	lsl	x3, x0, #5
  4f3ba0:	sub	x0, x3, x0
  4f3ba4:	add	x0, x1, x0
  4f3ba8:	cmp	x2, x20
  4f3bac:	b.ne	4f3b98 <bpftime_map_delete_elem_helper+0x4f4>  // b.any
  4f3bb0:	mov	x2, x0
  4f3bb4:	ldr	x23, [x28, #24]
  4f3bb8:	add	x1, x28, #0x38
  4f3bbc:	ldp	x25, x26, [x28, #8]
  4f3bc0:	umulh	x23, x23, x2
  4f3bc4:	ldr	x2, [x28, #56]
  4f3bc8:	msub	x23, x26, x23, x0
  4f3bcc:	mov	x0, x1
  4f3bd0:	add	x25, x27, x25
  4f3bd4:	add	x0, x0, x2
  4f3bd8:	cmp	x26, x23
  4f3bdc:	sub	x1, x23, x26
  4f3be0:	csel	x23, x1, x23, ls	// ls = plast
  4f3be4:	cmp	x2, #0x1
  4f3be8:	csetm	x24, ne	// ne = any
  4f3bec:	mov	x20, x23
  4f3bf0:	add	x25, x25, #0x4
  4f3bf4:	and	x24, x24, x0
  4f3bf8:	b	4f3c10 <bpftime_map_delete_elem_helper+0x56c>
  4f3bfc:	add	x20, x20, #0x1
  4f3c00:	cmp	x26, x20
  4f3c04:	csel	x20, x20, xzr, ne	// ne = any
  4f3c08:	cmp	x20, x23
  4f3c0c:	b.eq	4f3c58 <bpftime_map_delete_elem_helper+0x5b4>  // b.none
  4f3c10:	mul	x21, x20, x25
  4f3c14:	ldr	w0, [x24, x21]
  4f3c18:	cbz	w0, 4f3c58 <bpftime_map_delete_elem_helper+0x5b4>
  4f3c1c:	cmp	w0, #0x1
  4f3c20:	b.ne	4f3bfc <bpftime_map_delete_elem_helper+0x558>  // b.any
  4f3c24:	add	x1, x21, #0x4
  4f3c28:	add	x0, x24, x1
  4f3c2c:	cmp	x27, #0x4
  4f3c30:	b.eq	4f4df4 <bpftime_map_delete_elem_helper+0x1750>  // b.none
  4f3c34:	mov	x2, x27
  4f3c38:	mov	x1, x22
  4f3c3c:	bl	48d570 <memcmp@plt>
  4f3c40:	cbnz	w0, 4f3bfc <bpftime_map_delete_elem_helper+0x558>
  4f3c44:	ldr	x0, [x28, #40]
  4f3c48:	mov	w1, #0x2                   	// #2
  4f3c4c:	str	w1, [x24, x21]
  4f3c50:	sub	x0, x0, #0x1
  4f3c54:	str	x0, [x28, #40]
  4f3c58:	mov	x0, x19
  4f3c5c:	mov	x20, #0x0                   	// #0
  4f3c60:	bl	48c720 <pthread_spin_unlock@plt>
  4f3c64:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f3c68:	ldr	x0, [x19, #120]
  4f3c6c:	add	x1, x19, #0x78
  4f3c70:	add	x19, x19, #0x74
  4f3c74:	cmp	x0, #0x1
  4f3c78:	add	x1, x1, x0
  4f3c7c:	csetm	x21, ne	// ne = any
  4f3c80:	mov	x0, x19
  4f3c84:	and	x21, x21, x1
  4f3c88:	bl	48cea0 <pthread_spin_lock@plt>
  4f3c8c:	ldr	w0, [x21, #48]
  4f3c90:	cbz	w0, 4f3e80 <bpftime_map_delete_elem_helper+0x7dc>
  4f3c94:	ldr	w1, [x21, #40]
  4f3c98:	sub	w0, w0, #0x1
  4f3c9c:	ldr	w2, [x21, #4]
  4f3ca0:	mov	x20, #0x0                   	// #0
  4f3ca4:	add	w1, w1, #0x1
  4f3ca8:	str	w0, [x21, #48]
  4f3cac:	udiv	w0, w1, w2
  4f3cb0:	msub	w0, w0, w2, w1
  4f3cb4:	str	w0, [x21, #40]
  4f3cb8:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f3cbc:	ldr	x0, [x19, #120]
  4f3cc0:	add	x1, x19, #0x78
  4f3cc4:	add	x19, x19, #0x74
  4f3cc8:	cmp	x0, #0x1
  4f3ccc:	add	x1, x1, x0
  4f3cd0:	csetm	x22, ne	// ne = any
  4f3cd4:	mov	x0, x19
  4f3cd8:	and	x22, x22, x1
  4f3cdc:	add	x24, x22, #0x38
  4f3ce0:	bl	48cea0 <pthread_spin_lock@plt>
  4f3ce4:	mov	x0, x24
  4f3ce8:	bl	48cea0 <pthread_spin_lock@plt>
  4f3cec:	cbz	x20, 4f5aa4 <bpftime_map_delete_elem_helper+0x2400>
  4f3cf0:	ldr	w23, [x20]
  4f3cf4:	ldr	w0, [x22, #16]
  4f3cf8:	cmp	w23, w0
  4f3cfc:	b.hi	4f59e8 <bpftime_map_delete_elem_helper+0x2344>  // b.pmore
  4f3d00:	ldr	x28, [x22, #32]
  4f3d04:	add	x25, x22, #0x20
  4f3d08:	add	x21, sp, #0x140
  4f3d0c:	cmp	x28, #0x1
  4f3d10:	sub	x1, x25, x21
  4f3d14:	csetm	x26, ne	// ne = any
  4f3d18:	and	x0, x26, x1
  4f3d1c:	add	x0, x0, x28
  4f3d20:	cmp	x0, #0x1
  4f3d24:	b.ne	4f3de8 <bpftime_map_delete_elem_helper+0x744>  // b.any
  4f3d28:	mov	x27, #0x0                   	// #0
  4f3d2c:	nop
  4f3d30:	sub	x0, x25, x21
  4f3d34:	and	x0, x0, x26
  4f3d38:	add	x0, x0, x28
  4f3d3c:	cmp	x0, #0x1
  4f3d40:	b.eq	4f572c <bpftime_map_delete_elem_helper+0x2088>  // b.none
  4f3d44:	ldr	w0, [x27, #32]
  4f3d48:	cmp	w23, w0
  4f3d4c:	b.ne	4f572c <bpftime_map_delete_elem_helper+0x2088>  // b.any
  4f3d50:	mov	x2, x20
  4f3d54:	mov	x1, x27
  4f3d58:	mov	x0, x22
  4f3d5c:	bl	506500 <_ZNK7bpftime17lpm_trie_map_impl20longest_prefix_matchEPKNS_13lpm_trie_nodeEPKNS_16bpf_lpm_trie_keyE>
  4f3d60:	cmp	x0, w23, uxtw
  4f3d64:	b.ne	4f572c <bpftime_map_delete_elem_helper+0x2088>  // b.any
  4f3d68:	ldrb	w0, [x27, #40]
  4f3d6c:	tbnz	w0, #0, 4f572c <bpftime_map_delete_elem_helper+0x2088>
  4f3d70:	ldr	w1, [x22, #12]
  4f3d74:	mov	w0, #0x1                   	// #1
  4f3d78:	strb	w0, [x27, #40]
  4f3d7c:	add	x0, x27, #0x30
  4f3d80:	bl	550820 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvE11priv_resizeINS0_12value_init_tENS_11move_detail17integral_constantIjLj2EEEEEvmRKT_T0_.isra.0>
  4f3d84:	ldr	x0, [x22, #40]
  4f3d88:	cbz	x0, 4f3d94 <bpftime_map_delete_elem_helper+0x6f0>
  4f3d8c:	sub	x0, x0, #0x1
  4f3d90:	str	x0, [x22, #40]
  4f3d94:	mov	x20, #0x0                   	// #0
  4f3d98:	mov	x0, x24
  4f3d9c:	bl	48c720 <pthread_spin_unlock@plt>
  4f3da0:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f3da4:	add	x2, x20, x2, lsr #3
  4f3da8:	mvn	w1, w1
  4f3dac:	and	w1, w1, #0x7
  4f3db0:	ldrb	w0, [x2, #4]
  4f3db4:	asr	w0, w0, w1
  4f3db8:	ubfiz	x0, x0, #4, #1
  4f3dbc:	add	x25, x0, #0x8
  4f3dc0:	add	x0, x27, x0
  4f3dc4:	ldr	x28, [x0, #8]
  4f3dc8:	add	x25, x27, x25
  4f3dcc:	sub	x1, x25, x21
  4f3dd0:	cmp	x28, #0x1
  4f3dd4:	csetm	x26, ne	// ne = any
  4f3dd8:	and	x0, x26, x1
  4f3ddc:	add	x0, x0, x28
  4f3de0:	cmp	x0, #0x1
  4f3de4:	b.eq	4f572c <bpftime_map_delete_elem_helper+0x2088>  // b.none
  4f3de8:	and	x1, x1, x26
  4f3dec:	mov	x2, x20
  4f3df0:	add	x1, x1, x28
  4f3df4:	mov	x0, x22
  4f3df8:	cmp	x1, #0x1
  4f3dfc:	add	x1, x21, x1
  4f3e00:	csetm	x27, ne	// ne = any
  4f3e04:	and	x27, x27, x1
  4f3e08:	mov	x1, x27
  4f3e0c:	bl	506500 <_ZNK7bpftime17lpm_trie_map_impl20longest_prefix_matchEPKNS_13lpm_trie_nodeEPKNS_16bpf_lpm_trie_keyE>
  4f3e10:	ldr	w1, [x27, #32]
  4f3e14:	mov	w2, w1
  4f3e18:	cmp	x0, x2
  4f3e1c:	ccmp	w23, w1, #0x4, eq	// eq = none
  4f3e20:	b.eq	4f3d30 <bpftime_map_delete_elem_helper+0x68c>  // b.none
  4f3e24:	ldr	w0, [x22, #12]
  4f3e28:	cmp	x2, x0, lsl #3
  4f3e2c:	b.cc	4f3da4 <bpftime_map_delete_elem_helper+0x700>  // b.lo, b.ul, b.last
  4f3e30:	mov	x0, x27
  4f3e34:	mov	x25, #0x8                   	// #8
  4f3e38:	b	4f3dc4 <bpftime_map_delete_elem_helper+0x720>
  4f3e3c:	ldr	x0, [x19, #120]
  4f3e40:	add	x1, x19, #0x78
  4f3e44:	add	x19, x19, #0x74
  4f3e48:	cmp	x0, #0x1
  4f3e4c:	add	x1, x1, x0
  4f3e50:	csetm	x22, ne	// ne = any
  4f3e54:	mov	x0, x19
  4f3e58:	and	x22, x22, x1
  4f3e5c:	bl	48cea0 <pthread_spin_lock@plt>
  4f3e60:	add	x21, x22, #0x80
  4f3e64:	mov	x1, x20
  4f3e68:	mov	x0, x21
  4f3e6c:	ldr	x2, [x22, #104]
  4f3e70:	add	x2, x20, x2
  4f3e74:	bl	53b844 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvE6assignIPhEEvT_SG_PNS_11move_detail13disable_if_orIvNSH_7is_sameINSH_17integral_constantIjLj2EEENSK_IjLj0EEEEENSH_14is_convertibleISG_mEENS0_3dtl17is_input_iteratorISG_XsrNSQ_21has_iterator_categoryISG_EE5valueEEENSH_5bool_ILb0EEEE4typeE.constprop.0>
  4f3e78:	ldr	x0, [x22, #8]
  4f3e7c:	cbnz	x0, 4f43e0 <bpftime_map_delete_elem_helper+0xd3c>
  4f3e80:	bl	48c090 <__errno_location@plt>
  4f3e84:	mov	x20, #0xffffffffffffffff    	// #-1
  4f3e88:	mov	w1, #0x2                   	// #2
  4f3e8c:	str	w1, [x0]
  4f3e90:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f3e94:	ldr	x0, [x19, #120]
  4f3e98:	add	x1, x19, #0x78
  4f3e9c:	add	x19, x19, #0x74
  4f3ea0:	cmp	x0, #0x1
  4f3ea4:	add	x1, x1, x0
  4f3ea8:	csetm	x21, ne	// ne = any
  4f3eac:	mov	x0, x19
  4f3eb0:	and	x21, x21, x1
  4f3eb4:	bl	48cea0 <pthread_spin_lock@plt>
  4f3eb8:	cbz	x20, 4f5a48 <bpftime_map_delete_elem_helper+0x23a4>
  4f3ebc:	ldr	x0, [x21, #24]
  4f3ec0:	add	x22, x21, #0x10
  4f3ec4:	ldr	w20, [x20]
  4f3ec8:	cbnz	x0, 4f490c <bpftime_map_delete_elem_helper+0x1268>
  4f3ecc:	bl	48c090 <__errno_location@plt>
  4f3ed0:	mov	x20, #0xffffffffffffffff    	// #-1
  4f3ed4:	mov	w1, #0x2                   	// #2
  4f3ed8:	str	w1, [x0]
  4f3edc:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f3ee0:	add	x19, x19, #0x74
  4f3ee4:	mov	x20, #0xffffffffffffffff    	// #-1
  4f3ee8:	mov	x0, x19
  4f3eec:	bl	48cea0 <pthread_spin_lock@plt>
  4f3ef0:	bl	48c090 <__errno_location@plt>
  4f3ef4:	mov	x1, x0
  4f3ef8:	mov	w2, #0x5f                  	// #95
  4f3efc:	mov	x0, x19
  4f3f00:	str	w2, [x1]
  4f3f04:	bl	48c720 <pthread_spin_unlock@plt>
  4f3f08:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f3f0c:	ldr	x0, [x19, #120]
  4f3f10:	add	x19, x19, #0x78
  4f3f14:	cmp	x0, #0x1
  4f3f18:	add	x0, x19, x0
  4f3f1c:	csetm	x19, ne	// ne = any
  4f3f20:	and	x19, x19, x0
  4f3f24:	bl	48bb80 <sched_getcpu@plt>
  4f3f28:	ldr	w2, [x19, #144]
  4f3f2c:	cmp	w2, #0x4
  4f3f30:	b.eq	4f4378 <bpftime_map_delete_elem_helper+0xcd4>  // b.none
  4f3f34:	ldr	x1, [x19, #200]
  4f3f38:	add	x4, x19, #0xc8
  4f3f3c:	sbfiz	x3, x0, #5, #32
  4f3f40:	mov	w2, w2
  4f3f44:	cmp	x1, #0x1
  4f3f48:	add	x4, x4, x1
  4f3f4c:	csetm	x0, ne	// ne = any
  4f3f50:	mov	x1, x20
  4f3f54:	and	x20, x0, x4
  4f3f58:	add	x20, x20, x3
  4f3f5c:	add	x0, x20, #0x8
  4f3f60:	ldr	x3, [x20, #8]
  4f3f64:	cmp	x3, #0x1
  4f3f68:	add	x0, x0, x3
  4f3f6c:	csetm	x3, ne	// ne = any
  4f3f70:	and	x0, x3, x0
  4f3f74:	bl	48c120 <memcpy@plt>
  4f3f78:	add	x8, sp, #0xe0
  4f3f7c:	mov	x1, x20
  4f3f80:	mov	x0, x19
  4f3f84:	mov	x20, x8
  4f3f88:	bl	4fc634 <_ZNK5boost9unordered6detail5tableINS1_3mapINS_12interprocess9allocatorISt4pairIKNS_9container6vectorIhNS5_IhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEESI_ESG_EESI_SI_N7bpftime24per_cpu_bytes_vec_hasherENSM_23per_cpu_bytes_vec_equalEEEE4findISI_EENS1_15iterator_detail8iteratorINS1_4nodeISK_SD_EENS1_6bucketISV_SD_EEEERKT_>
  4f3f8c:	ldr	x0, [sp, #224]
  4f3f90:	cmp	x0, #0x1
  4f3f94:	add	x20, x20, x0
  4f3f98:	csetm	x0, ne	// ne = any
  4f3f9c:	tst	x0, x20
  4f3fa0:	b.eq	4f4388 <bpftime_map_delete_elem_helper+0xce4>  // b.none
  4f3fa4:	ldp	x3, x2, [sp, #232]
  4f3fa8:	add	x1, sp, #0xf8
  4f3fac:	sub	x20, x20, x1
  4f3fb0:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f3fb4:	add	x21, sp, #0x128
  4f3fb8:	add	x6, sp, #0x140
  4f3fbc:	add	x23, sp, #0x130
  4f3fc0:	cmp	x3, #0x1
  4f3fc4:	csetm	x8, ne	// ne = any
  4f3fc8:	cmp	x2, #0x1
  4f3fcc:	csetm	x9, ne	// ne = any
  4f3fd0:	and	x8, x8, x0
  4f3fd4:	cmp	x20, #0x1
  4f3fd8:	add	x0, sp, #0x110
  4f3fdc:	add	x8, x8, x3
  4f3fe0:	sub	x1, x1, x0
  4f3fe4:	mov	x3, #0xffffffffffffffe8    	// #-24
  4f3fe8:	csetm	x7, ne	// ne = any
  4f3fec:	and	x9, x9, x3
  4f3ff0:	cmp	x8, #0x1
  4f3ff4:	add	x9, x9, x2
  4f3ff8:	and	x7, x7, x1
  4f3ffc:	add	x7, x7, x20
  4f4000:	mov	x1, x3
  4f4004:	csetm	x5, ne	// ne = any
  4f4008:	cmp	x9, #0x1
  4f400c:	mov	x2, x1
  4f4010:	csetm	x3, ne	// ne = any
  4f4014:	and	x5, x5, x1
  4f4018:	cmp	x7, #0x1
  4f401c:	add	x5, x5, x8
  4f4020:	csetm	x1, ne	// ne = any
  4f4024:	and	x3, x3, x2
  4f4028:	sub	x0, x0, x21
  4f402c:	add	x3, x3, x9
  4f4030:	cmp	x5, #0x1
  4f4034:	and	x1, x1, x0
  4f4038:	csetm	x26, ne	// ne = any
  4f403c:	add	x24, x1, x7
  4f4040:	cmp	x3, #0x1
  4f4044:	csetm	x25, ne	// ne = any
  4f4048:	and	x26, x26, x2
  4f404c:	cmp	x24, #0x1
  4f4050:	add	x26, x26, x5
  4f4054:	csetm	x22, ne	// ne = any
  4f4058:	and	x25, x25, x2
  4f405c:	sub	x1, x21, x6
  4f4060:	add	x25, x25, x3
  4f4064:	cmp	x26, #0x1
  4f4068:	and	x1, x22, x1
  4f406c:	add	x1, x1, x24
  4f4070:	csetm	x28, ne	// ne = any
  4f4074:	cmp	x25, #0x1
  4f4078:	add	x0, sp, #0x148
  4f407c:	csetm	x27, ne	// ne = any
  4f4080:	cmp	x1, #0x1
  4f4084:	add	x2, x6, x1
  4f4088:	csetm	x10, ne	// ne = any
  4f408c:	sub	x4, x23, x0
  4f4090:	and	x10, x10, x2
  4f4094:	and	x4, x28, x4
  4f4098:	mov	x2, #0xffffffffffffffe8    	// #-24
  4f409c:	and	x2, x27, x2
  4f40a0:	add	x4, x4, x26
  4f40a4:	add	x2, x2, x25
  4f40a8:	stp	x20, x8, [sp, #248]
  4f40ac:	sub	x11, x10, x6
  4f40b0:	stp	x9, x7, [sp, #264]
  4f40b4:	add	x20, sp, #0x138
  4f40b8:	stp	x5, x3, [sp, #280]
  4f40bc:	stp	x24, x26, [sp, #296]
  4f40c0:	stp	x25, x1, [sp, #312]
  4f40c4:	stp	x4, x2, [sp, #328]
  4f40c8:	ldr	x1, [x10]
  4f40cc:	cmp	x1, #0x1
  4f40d0:	csetm	x5, ne	// ne = any
  4f40d4:	and	x5, x5, x11
  4f40d8:	add	x5, x5, x1
  4f40dc:	str	x5, [sp, #320]
  4f40e0:	cmp	x5, #0x1
  4f40e4:	b.eq	4f513c <bpftime_map_delete_elem_helper+0x1a98>  // b.none
  4f40e8:	add	x8, sp, #0xa0
  4f40ec:	add	x9, sp, #0xa8
  4f40f0:	sub	x4, x23, x8
  4f40f4:	sub	x3, x20, x9
  4f40f8:	and	x4, x4, x28
  4f40fc:	and	x3, x3, x27
  4f4100:	add	x4, x4, x26
  4f4104:	add	x3, x3, x25
  4f4108:	cmp	x4, #0x1
  4f410c:	add	x6, x8, x4
  4f4110:	csetm	x5, ne	// ne = any
  4f4114:	stp	x4, x3, [sp, #160]
  4f4118:	and	x6, x5, x6
  4f411c:	add	x1, x21, x24
  4f4120:	mov	x7, x6
  4f4124:	and	x2, x22, x1
  4f4128:	ldr	x0, [x6]
  4f412c:	cmp	x0, #0x1
  4f4130:	add	x1, x6, x0
  4f4134:	csetm	x0, ne	// ne = any
  4f4138:	and	x0, x0, x1
  4f413c:	cmp	x2, x0
  4f4140:	b.eq	4f416c <bpftime_map_delete_elem_helper+0xac8>  // b.none
  4f4144:	nop
  4f4148:	ldr	x1, [x0]
  4f414c:	mov	x6, x0
  4f4150:	mov	x7, x0
  4f4154:	cmp	x1, #0x1
  4f4158:	add	x1, x0, x1
  4f415c:	csetm	x0, ne	// ne = any
  4f4160:	and	x0, x0, x1
  4f4164:	cmp	x2, x0
  4f4168:	b.ne	4f4148 <bpftime_map_delete_elem_helper+0xaa4>  // b.any
  4f416c:	cmp	x3, #0x1
  4f4170:	add	x1, sp, #0xb8
  4f4174:	add	x26, sp, #0xb0
  4f4178:	csetm	x0, ne	// ne = any
  4f417c:	sub	x9, x9, x1
  4f4180:	sub	x1, x8, x26
  4f4184:	and	x5, x5, x1
  4f4188:	and	x0, x0, x9
  4f418c:	add	x0, x0, x3
  4f4190:	add	x5, x5, x4
  4f4194:	stp	x5, x0, [sp, #176]
  4f4198:	add	x20, sp, #0xc0
  4f419c:	add	x28, sp, #0x98
  4f41a0:	ldr	x0, [x6]
  4f41a4:	cmp	x0, #0x1
  4f41a8:	add	x0, x0, x7
  4f41ac:	csetm	x1, ne	// ne = any
  4f41b0:	and	x1, x1, x0
  4f41b4:	sub	x0, x1, x7
  4f41b8:	ldr	x2, [x1]
  4f41bc:	cmp	x2, #0x1
  4f41c0:	csetm	x1, ne	// ne = any
  4f41c4:	and	x0, x1, x0
  4f41c8:	add	x0, x0, x2
  4f41cc:	str	x0, [x6]
  4f41d0:	ldr	x0, [sp, #176]
  4f41d4:	cmp	x0, #0x1
  4f41d8:	add	x0, x26, x0
  4f41dc:	csetm	x1, ne	// ne = any
  4f41e0:	and	x0, x1, x0
  4f41e4:	ldr	x22, [x0]
  4f41e8:	cmp	x22, #0x1
  4f41ec:	b.eq	4f517c <bpftime_map_delete_elem_helper+0x1ad8>  // b.none
  4f41f0:	ldr	x3, [sp, #296]
  4f41f4:	sub	x0, x21, x28
  4f41f8:	add	x2, x19, #0x20
  4f41fc:	cmp	x3, #0x1
  4f4200:	csetm	x1, ne	// ne = any
  4f4204:	and	x1, x1, x0
  4f4208:	add	x1, x1, x3
  4f420c:	str	x1, [sp, #152]
  4f4210:	add	x3, x28, x1
  4f4214:	sub	x28, x28, x20
  4f4218:	ldr	x0, [x19, #32]
  4f421c:	cmp	x0, #0x1
  4f4220:	add	x2, x2, x0
  4f4224:	csetm	x0, ne	// ne = any
  4f4228:	and	x0, x0, x2
  4f422c:	cmp	x0, #0x0
  4f4230:	sub	x0, x0, x26
  4f4234:	csetm	x2, ne	// ne = any
  4f4238:	cmp	x1, #0x1
  4f423c:	csetm	x1, ne	// ne = any
  4f4240:	sub	x0, x0, #0x1
  4f4244:	and	x1, x1, x3
  4f4248:	and	x2, x2, x0
  4f424c:	add	x0, x1, #0x8
  4f4250:	add	x1, x2, #0x1
  4f4254:	sub	x2, x0, x20
  4f4258:	str	x1, [sp, #176]
  4f425c:	cmp	x2, #0x1
  4f4260:	csetm	x21, ne	// ne = any
  4f4264:	and	x21, x21, x0
  4f4268:	add	x0, x21, #0x20
  4f426c:	bl	4dcf20 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvED1Ev>
  4f4270:	mov	x0, x21
  4f4274:	bl	4dcf20 <_ZN5boost9container6vectorIhNS_12interprocess9allocatorIhNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEEvED1Ev>
  4f4278:	ldr	x3, [sp, #152]
  4f427c:	mov	x1, x20
  4f4280:	mov	x0, x26
  4f4284:	mov	x20, #0x0                   	// #0
  4f4288:	cmp	x3, #0x1
  4f428c:	csetm	x2, ne	// ne = any
  4f4290:	and	x2, x2, x28
  4f4294:	add	x2, x2, x3
  4f4298:	str	x2, [sp, #192]
  4f429c:	bl	550b94 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f42a0:	ldr	x0, [x19, #8]
  4f42a4:	sub	x0, x0, #0x1
  4f42a8:	str	x0, [x19, #8]
  4f42ac:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f42b0:	ldr	x0, [x19, #120]
  4f42b4:	add	x1, x19, #0x78
  4f42b8:	add	x19, x19, #0x74
  4f42bc:	cmp	x0, #0x1
  4f42c0:	add	x1, x1, x0
  4f42c4:	csetm	x21, ne	// ne = any
  4f42c8:	mov	x0, x19
  4f42cc:	and	x21, x21, x1
  4f42d0:	bl	48cea0 <pthread_spin_lock@plt>
  4f42d4:	ldr	w0, [x20]
  4f42d8:	tbnz	w0, #31, 4f5978 <bpftime_map_delete_elem_helper+0x22d4>
  4f42dc:	ldr	x1, [x21, #16]
  4f42e0:	sxtw	x0, w0
  4f42e4:	cmp	x0, x1
  4f42e8:	b.cs	4f5978 <bpftime_map_delete_elem_helper+0x22d4>  // b.hs, b.nlast
  4f42ec:	ldr	x1, [x21, #8]
  4f42f0:	add	x21, x21, #0x8
  4f42f4:	mov	w2, #0xffffffff            	// #-1
  4f42f8:	mov	x20, #0x0                   	// #0
  4f42fc:	cmp	x1, #0x1
  4f4300:	add	x21, x21, x1
  4f4304:	csetm	x1, ne	// ne = any
  4f4308:	and	x1, x1, x21
  4f430c:	str	w2, [x1, x0, lsl #2]
  4f4310:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f4314:	cmp	w0, #0x3ee
  4f4318:	b.ne	4f37ac <bpftime_map_delete_elem_helper+0x108>  // b.any
  4f431c:	ldr	x0, [x19, #120]
  4f4320:	add	x19, x19, #0x78
  4f4324:	mov	x1, x20
  4f4328:	cmp	x0, #0x1
  4f432c:	add	x19, x19, x0
  4f4330:	csetm	x0, ne	// ne = any
  4f4334:	and	x0, x0, x19
  4f4338:	ldr	w0, [x0, #8]
  4f433c:	bl	1b3bdb0 <bpf_map_delete_elem>
  4f4340:	sxtw	x20, w0
  4f4344:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f4348:	ldr	x0, [x19, #120]
  4f434c:	add	x19, x19, #0x78
  4f4350:	cmp	x0, #0x1
  4f4354:	add	x0, x19, x0
  4f4358:	csetm	x19, ne	// ne = any
  4f435c:	and	x19, x19, x0
  4f4360:	ldr	w0, [x19, #76]
  4f4364:	tbnz	w0, #31, 4f4d50 <bpftime_map_delete_elem_helper+0x16ac>
  4f4368:	mov	x1, x20
  4f436c:	bl	1b3bdb0 <bpf_map_delete_elem>
  4f4370:	sxtw	x20, w0
  4f4374:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f4378:	ldr	x0, [x19, #80]
  4f437c:	add	x28, x19, #0x48
  4f4380:	ldr	w20, [x20]
  4f4384:	cbnz	x0, 4f52a0 <bpftime_map_delete_elem_helper+0x1bfc>
  4f4388:	bl	48c090 <__errno_location@plt>
  4f438c:	mov	x20, #0xffffffffffffffff    	// #-1
  4f4390:	mov	w1, #0x2                   	// #2
  4f4394:	str	w1, [x0]
  4f4398:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f439c:	mov	x0, x19
  4f43a0:	bl	50aac0 <_ZN7bpftime26array_map_kernel_user_impl11init_map_fdEv>
  4f43a4:	tbz	w0, #0, 4f3944 <bpftime_map_delete_elem_helper+0x2a0>
  4f43a8:	ldr	x0, [x19, #56]
  4f43ac:	cbz	x0, 4f586c <bpftime_map_delete_elem_helper+0x21c8>
  4f43b0:	ldr	w1, [x19, #36]
  4f43b4:	ldr	w3, [x20]
  4f43b8:	cmp	w3, w1
  4f43bc:	b.cs	4f4388 <bpftime_map_delete_elem_helper+0xce4>  // b.hs, b.nlast
  4f43c0:	ldr	w4, [x19, #32]
  4f43c4:	mov	w1, #0x0                   	// #0
  4f43c8:	mov	x20, #0x0                   	// #0
  4f43cc:	mov	w2, w4
  4f43d0:	mul	w3, w3, w4
  4f43d4:	add	x0, x0, x3
  4f43d8:	bl	48bc60 <memset@plt>
  4f43dc:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f43e0:	add	x20, x22, #0x20
  4f43e4:	mov	x0, x21
  4f43e8:	bl	54a960 <_ZNK7bpftime16bytes_vec_hasherclERKN5boost9container6vectorIhNS1_12interprocess9allocatorIhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEE.isra.0>
  4f43ec:	ldr	x1, [x20, #8]
  4f43f0:	bl	4fc5e0 <_ZN5boost9unordered6detail15prime_fmod_sizeIvE8positionEmm>
  4f43f4:	add	x23, sp, #0x128
  4f43f8:	mov	x1, x0
  4f43fc:	mov	x8, x23
  4f4400:	mov	x0, x20
  4f4404:	bl	4e20f0 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EENS9_ISN_SH_EENS1_15prime_fmod_sizeIvEEE2atEm>
  4f4408:	add	x20, sp, #0xc0
  4f440c:	ldr	x0, [sp, #296]
  4f4410:	cmp	x0, #0x1
  4f4414:	add	x1, x23, x0
  4f4418:	csetm	x0, ne	// ne = any
  4f441c:	and	x0, x0, x1
  4f4420:	sub	x1, x0, x20
  4f4424:	ldr	x0, [x0]
  4f4428:	cmp	x0, #0x1
  4f442c:	csetm	x27, ne	// ne = any
  4f4430:	and	x27, x27, x1
  4f4434:	add	x27, x27, x0
  4f4438:	str	x27, [sp, #192]
  4f443c:	cmp	x27, #0x1
  4f4440:	b.eq	4f3e80 <bpftime_map_delete_elem_helper+0x7dc>  // b.none
  4f4444:	ldr	x4, [x21, #16]
  4f4448:	b	4f4470 <bpftime_map_delete_elem_helper+0xdcc>
  4f444c:	ldr	x0, [x28]
  4f4450:	sub	x28, x28, x20
  4f4454:	cmp	x0, #0x1
  4f4458:	csetm	x27, ne	// ne = any
  4f445c:	and	x27, x27, x28
  4f4460:	add	x27, x27, x0
  4f4464:	str	x27, [sp, #192]
  4f4468:	cmp	x27, #0x1
  4f446c:	b.eq	4f3e80 <bpftime_map_delete_elem_helper+0x7dc>  // b.none
  4f4470:	add	x28, x20, x27
  4f4474:	add	x0, x28, #0x8
  4f4478:	ldr	x1, [x0, #16]
  4f447c:	cmp	x4, x1
  4f4480:	b.ne	4f444c <bpftime_map_delete_elem_helper+0xda8>  // b.any
  4f4484:	ldr	x5, [x22, #136]
  4f4488:	add	x24, sp, #0x110
  4f448c:	add	x3, x22, #0x88
  4f4490:	add	x25, sp, #0xf8
  4f4494:	cmp	x5, #0x1
  4f4498:	sub	x2, x3, x24
  4f449c:	csetm	x1, ne	// ne = any
  4f44a0:	sub	x3, x3, x25
  4f44a4:	and	x2, x1, x2
  4f44a8:	and	x1, x1, x3
  4f44ac:	add	x2, x2, x5
  4f44b0:	add	x1, x1, x5
  4f44b4:	cmp	x2, #0x1
  4f44b8:	add	x21, sp, #0x140
  4f44bc:	csetm	x3, ne	// ne = any
  4f44c0:	cmp	x1, #0x1
  4f44c4:	sub	x7, x24, x21
  4f44c8:	sub	x6, x25, x24
  4f44cc:	csetm	x5, ne	// ne = any
  4f44d0:	add	x1, x4, x1
  4f44d4:	and	x3, x3, x7
  4f44d8:	and	x5, x5, x6
  4f44dc:	add	x2, x3, x2
  4f44e0:	add	x1, x5, x1
  4f44e4:	str	x1, [sp, #272]
  4f44e8:	add	x0, x0, #0x8
  4f44ec:	str	x2, [sp, #320]
  4f44f0:	add	x26, sp, #0xe0
  4f44f4:	sub	x3, x0, x26
  4f44f8:	sub	x7, x26, x25
  4f44fc:	ldr	x6, [x28, #16]
  4f4500:	mov	x0, x21
  4f4504:	mov	x2, x25
  4f4508:	mov	x1, x24
  4f450c:	cmp	x6, #0x1
  4f4510:	str	x4, [sp]
  4f4514:	csetm	x5, ne	// ne = any
  4f4518:	and	x3, x5, x3
  4f451c:	add	x3, x3, x6
  4f4520:	cmp	x3, #0x1
  4f4524:	csetm	x5, ne	// ne = any
  4f4528:	and	x5, x5, x7
  4f452c:	add	x3, x5, x3
  4f4530:	str	x3, [sp, #248]
  4f4534:	bl	506384 <_ZN5boost9intrusive10algo_equalINS_9container12vec_iteratorINS_12interprocess10offset_ptrIhlmLm0EEELb1EEES7_EEbT_S8_T0_>
  4f4538:	ldr	x4, [sp]
  4f453c:	tbz	w0, #0, 4f444c <bpftime_map_delete_elem_helper+0xda8>
  4f4540:	sub	x2, x20, x24
  4f4544:	mov	x1, x23
  4f4548:	add	x2, x2, x27
  4f454c:	mov	x0, x21
  4f4550:	str	x2, [sp, #272]
  4f4554:	add	x28, sp, #0xe8
  4f4558:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f455c:	ldr	x4, [sp, #272]
  4f4560:	sub	x3, x24, x26
  4f4564:	mov	x1, x21
  4f4568:	mov	x0, x28
  4f456c:	cmp	x4, #0x1
  4f4570:	csetm	x2, ne	// ne = any
  4f4574:	and	x2, x2, x3
  4f4578:	add	x2, x2, x4
  4f457c:	str	x2, [sp, #224]
  4f4580:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f4584:	ldr	x1, [sp, #224]
  4f4588:	cmp	x1, #0x1
  4f458c:	add	x1, x26, x1
  4f4590:	csetm	x0, ne	// ne = any
  4f4594:	ands	x0, x0, x1
  4f4598:	b.eq	4f3e80 <bpftime_map_delete_elem_helper+0x7dc>  // b.none
  4f459c:	ldr	x4, [x0, #72]
  4f45a0:	add	x3, x0, #0x48
  4f45a4:	sub	x3, x3, x21
  4f45a8:	add	x2, x0, #0x50
  4f45ac:	cmp	x4, #0x1
  4f45b0:	add	x27, sp, #0x148
  4f45b4:	csetm	x1, ne	// ne = any
  4f45b8:	sub	x2, x2, x27
  4f45bc:	and	x1, x1, x3
  4f45c0:	add	x1, x1, x4
  4f45c4:	str	x1, [sp, #320]
  4f45c8:	ldr	x1, [x0, #80]
  4f45cc:	cmp	x1, #0x1
  4f45d0:	csetm	x0, ne	// ne = any
  4f45d4:	and	x0, x0, x2
  4f45d8:	add	x0, x0, x1
  4f45dc:	str	x0, [sp, #328]
  4f45e0:	cmp	x0, #0x1
  4f45e4:	add	x0, x27, x0
  4f45e8:	csetm	x1, ne	// ne = any
  4f45ec:	ands	x1, x1, x0
  4f45f0:	b.ne	4f4e08 <bpftime_map_delete_elem_helper+0x1764>  // b.any
  4f45f4:	mov	x1, x21
  4f45f8:	mov	x0, x22
  4f45fc:	bl	505b40 <_ZN7bpftime21lru_var_hash_map_impl11evict_entryEN5boost12interprocess10shared_ptrINS_18lru_linklist_entryENS2_9allocatorIvNS2_15segment_managerIcNS2_15rbtree_best_fitINS2_12mutex_familyENS2_10offset_ptrIvlmLm0EEELm0EEENS2_10iset_indexEEEEENS2_7deleterIS4_SD_EEEE>
  4f4600:	mov	x0, x21
  4f4604:	bl	4df5c0 <_ZN5boost12interprocess9ipcdetail12shared_countIN7bpftime18lru_linklist_entryENS0_9allocatorIvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIS4_SD_EEED1Ev>
  4f4608:	ldp	x3, x2, [sp, #224]
  4f460c:	sub	x26, x26, x25
  4f4610:	ldr	x0, [sp, #240]
  4f4614:	add	x1, sp, #0x100
  4f4618:	sub	x28, x28, x1
  4f461c:	sub	x25, x25, x23
  4f4620:	sub	x23, x23, x21
  4f4624:	cmp	x3, #0x1
  4f4628:	csetm	x7, ne	// ne = any
  4f462c:	cmp	x2, #0x1
  4f4630:	and	x7, x7, x26
  4f4634:	csetm	x6, ne	// ne = any
  4f4638:	cmp	x0, #0x1
  4f463c:	add	x7, x7, x3
  4f4640:	and	x6, x6, x28
  4f4644:	csetm	x5, ne	// ne = any
  4f4648:	add	x6, x6, x2
  4f464c:	cmp	x7, #0x1
  4f4650:	mov	x2, #0xffffffffffffffe8    	// #-24
  4f4654:	and	x5, x5, x2
  4f4658:	add	x5, x5, x0
  4f465c:	csetm	x4, ne	// ne = any
  4f4660:	add	x0, sp, #0x130
  4f4664:	cmp	x6, #0x1
  4f4668:	sub	x1, x1, x0
  4f466c:	and	x4, x4, x25
  4f4670:	csetm	x11, ne	// ne = any
  4f4674:	cmp	x5, #0x1
  4f4678:	add	x4, x4, x7
  4f467c:	and	x11, x11, x1
  4f4680:	csetm	x8, ne	// ne = any
  4f4684:	add	x11, x11, x6
  4f4688:	cmp	x4, #0x1
  4f468c:	mov	x1, #0xffffffffffffffd0    	// #-48
  4f4690:	and	x8, x8, x1
  4f4694:	csetm	x26, ne	// ne = any
  4f4698:	add	x8, x8, x5
  4f469c:	cmp	x11, #0x1
  4f46a0:	and	x26, x26, x23
  4f46a4:	sub	x0, x0, x27
  4f46a8:	csetm	x3, ne	// ne = any
  4f46ac:	cmp	x8, #0x1
  4f46b0:	add	x26, x26, x4
  4f46b4:	and	x3, x3, x0
  4f46b8:	csetm	x2, ne	// ne = any
  4f46bc:	add	x3, x3, x11
  4f46c0:	cmp	x26, #0x1
  4f46c4:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f46c8:	and	x2, x2, x0
  4f46cc:	csetm	x1, ne	// ne = any
  4f46d0:	add	x2, x2, x8
  4f46d4:	cmp	x3, #0x1
  4f46d8:	sub	x0, x21, x20
  4f46dc:	add	x23, sp, #0xc8
  4f46e0:	csetm	x10, ne	// ne = any
  4f46e4:	and	x0, x1, x0
  4f46e8:	cmp	x2, #0x1
  4f46ec:	add	x28, sp, #0x150
  4f46f0:	add	x25, sp, #0xd0
  4f46f4:	stp	x7, x6, [sp, #248]
  4f46f8:	sub	x6, x27, x23
  4f46fc:	add	x0, x0, x26
  4f4700:	csetm	x9, ne	// ne = any
  4f4704:	and	x6, x10, x6
  4f4708:	str	x5, [sp, #264]
  4f470c:	sub	x5, x28, x25
  4f4710:	cmp	x0, #0x1
  4f4714:	add	x6, x6, x3
  4f4718:	and	x5, x9, x5
  4f471c:	add	x5, x5, x2
  4f4720:	str	x0, [sp, #192]
  4f4724:	add	x0, x20, x0
  4f4728:	stp	x6, x5, [sp, #200]
  4f472c:	csetm	x6, ne	// ne = any
  4f4730:	and	x0, x6, x0
  4f4734:	stp	x4, x11, [sp, #296]
  4f4738:	sub	x5, x0, x20
  4f473c:	stp	x8, x26, [sp, #312]
  4f4740:	stp	x3, x2, [sp, #328]
  4f4744:	ldr	x4, [x0]
  4f4748:	cmp	x4, #0x1
  4f474c:	csetm	x0, ne	// ne = any
  4f4750:	and	x0, x0, x5
  4f4754:	add	x0, x0, x4
  4f4758:	str	x0, [sp, #192]
  4f475c:	cmp	x0, #0x1
  4f4760:	b.eq	4f5094 <bpftime_map_delete_elem_helper+0x19f0>  // b.none
  4f4764:	add	x7, sp, #0xa0
  4f4768:	add	x8, sp, #0xa8
  4f476c:	sub	x4, x27, x7
  4f4770:	sub	x6, x28, x8
  4f4774:	and	x4, x4, x10
  4f4778:	and	x6, x6, x9
  4f477c:	add	x4, x4, x3
  4f4780:	add	x6, x6, x2
  4f4784:	cmp	x4, #0x1
  4f4788:	add	x2, x7, x4
  4f478c:	csetm	x9, ne	// ne = any
  4f4790:	stp	x4, x6, [sp, #160]
  4f4794:	and	x2, x9, x2
  4f4798:	add	x3, x21, x26
  4f479c:	mov	x5, x2
  4f47a0:	and	x3, x3, x1
  4f47a4:	ldr	x0, [x2]
  4f47a8:	cmp	x0, #0x1
  4f47ac:	add	x1, x0, x2
  4f47b0:	csetm	x0, ne	// ne = any
  4f47b4:	and	x0, x0, x1
  4f47b8:	cmp	x3, x0
  4f47bc:	b.eq	4f47e4 <bpftime_map_delete_elem_helper+0x1140>  // b.none
  4f47c0:	ldr	x1, [x0]
  4f47c4:	mov	x2, x0
  4f47c8:	mov	x5, x0
  4f47cc:	cmp	x1, #0x1
  4f47d0:	add	x1, x0, x1
  4f47d4:	csetm	x0, ne	// ne = any
  4f47d8:	and	x0, x0, x1
  4f47dc:	cmp	x3, x0
  4f47e0:	b.ne	4f47c0 <bpftime_map_delete_elem_helper+0x111c>  // b.any
  4f47e4:	cmp	x6, #0x1
  4f47e8:	add	x26, sp, #0xb0
  4f47ec:	add	x27, sp, #0xb8
  4f47f0:	csetm	x1, ne	// ne = any
  4f47f4:	sub	x8, x8, x27
  4f47f8:	sub	x7, x7, x26
  4f47fc:	and	x1, x1, x8
  4f4800:	and	x0, x9, x7
  4f4804:	add	x1, x1, x6
  4f4808:	add	x0, x0, x4
  4f480c:	stp	x0, x1, [sp, #176]
  4f4810:	ldr	x0, [x2]
  4f4814:	cmp	x0, #0x1
  4f4818:	add	x0, x0, x5
  4f481c:	csetm	x1, ne	// ne = any
  4f4820:	and	x1, x1, x0
  4f4824:	sub	x0, x1, x5
  4f4828:	ldr	x3, [x1]
  4f482c:	cmp	x3, #0x1
  4f4830:	csetm	x1, ne	// ne = any
  4f4834:	and	x0, x1, x0
  4f4838:	add	x0, x0, x3
  4f483c:	str	x0, [x2]
  4f4840:	ldr	x0, [sp, #176]
  4f4844:	cmp	x0, #0x1
  4f4848:	add	x0, x26, x0
  4f484c:	csetm	x1, ne	// ne = any
  4f4850:	and	x0, x1, x0
  4f4854:	ldr	x28, [x0]
  4f4858:	cmp	x28, #0x1
  4f485c:	b.eq	4f4f78 <bpftime_map_delete_elem_helper+0x18d4>  // b.none
  4f4860:	ldr	x3, [sp, #320]
  4f4864:	sub	x21, x21, x24
  4f4868:	mov	x1, x24
  4f486c:	mov	x0, x22
  4f4870:	cmp	x3, #0x1
  4f4874:	csetm	x2, ne	// ne = any
  4f4878:	and	x2, x2, x21
  4f487c:	add	x2, x2, x3
  4f4880:	str	x2, [sp, #272]
  4f4884:	bl	4e1e70 <_ZN5boost9unordered6detail5tableINS1_3mapINS_12interprocess9allocatorISt4pairIKNS_9container6vectorIhNS5_IhNS4_15segment_managerIcNS4_15rbtree_best_fitINS4_12mutex_familyENS4_10offset_ptrIvlmLm0EEELm0EEENS4_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESG_EESI_SL_NSK_16bytes_vec_hasherESt8equal_toISI_EEEE11delete_nodeENSC_INS1_4nodeISM_SD_EElmLm0EEE>
  4f4888:	ldr	x1, [x22, #8]
  4f488c:	adrp	x2, 1d3f000 <_ZZN12_GLOBAL__N_115AArch64FastISel9selectCmpEPKN4llvm11InstructionEE13CondCodeTable+0x500>
  4f4890:	fmov	d2, x23
  4f4894:	fmov	d0, x26
  4f4898:	sub	x1, x1, #0x1
  4f489c:	str	x1, [x22, #8]
  4f48a0:	ldr	q1, [x2, #3984]
  4f48a4:	mov	v2.d[1], x25
  4f48a8:	ldur	q3, [sp, #200]
  4f48ac:	mov	v0.d[1], x27
  4f48b0:	ldr	x4, [sp, #192]
  4f48b4:	add	x0, sp, #0x98
  4f48b8:	cmeq	v1.2d, v3.2d, v1.2d
  4f48bc:	sub	x20, x20, x0
  4f48c0:	sub	v0.2d, v2.2d, v0.2d
  4f48c4:	cmp	x4, #0x1
  4f48c8:	csetm	x2, ne	// ne = any
  4f48cc:	sub	x3, x0, x24
  4f48d0:	and	x2, x2, x20
  4f48d4:	add	x0, sp, #0x118
  4f48d8:	add	x2, x2, x4
  4f48dc:	str	x2, [sp, #152]
  4f48e0:	bic	v0.16b, v0.16b, v1.16b
  4f48e4:	cmp	x2, #0x1
  4f48e8:	csetm	x4, ne	// ne = any
  4f48ec:	mov	x1, x26
  4f48f0:	and	x3, x4, x3
  4f48f4:	add	x2, x3, x2
  4f48f8:	str	x2, [sp, #272]
  4f48fc:	add	v0.2d, v0.2d, v3.2d
  4f4900:	str	q0, [sp, #176]
  4f4904:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f4908:	b	4f3b4c <bpftime_map_delete_elem_helper+0x4a8>
  4f490c:	add	x0, x21, #0x30
  4f4910:	mov	x1, x0
  4f4914:	str	x1, [sp, #32]
  4f4918:	mov	w0, w20
  4f491c:	ldr	x1, [x1, #8]
  4f4920:	cmp	x1, #0x1c
  4f4924:	b.hi	4f5948 <bpftime_map_delete_elem_helper+0x22a4>  // b.pmore
  4f4928:	adrp	x3, 1d0c000 <_ZTSSt23_Sp_counted_ptr_inplaceIN5boost12interprocess27basic_managed_shared_memoryIcNS1_15rbtree_best_fitINS1_12mutex_familyENS1_10offset_ptrIvlmLm0EEELm0EEENS1_10iset_indexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x58>
  4f492c:	add	x3, x3, #0x978
  4f4930:	adrp	x2, 1d0c000 <_ZTSSt23_Sp_counted_ptr_inplaceIN5boost12interprocess27basic_managed_shared_memoryIcNS1_15rbtree_best_fitINS1_12mutex_familyENS1_10offset_ptrIvlmLm0EEELm0EEENS1_10iset_indexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0x58>
  4f4934:	add	x2, x2, #0xa60
  4f4938:	ldr	x3, [x3, x1, lsl #3]
  4f493c:	ldr	x1, [x2, x1, lsl #3]
  4f4940:	mul	x0, x0, x3
  4f4944:	mov	w1, w1
  4f4948:	umulh	x1, x0, x1
  4f494c:	ldr	x0, [sp, #32]
  4f4950:	add	x23, sp, #0x128
  4f4954:	mov	x8, x23
  4f4958:	add	x25, sp, #0xf8
  4f495c:	bl	4e2290 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKjNS_9container6vectorImNS_12interprocess9allocatorImNS9_15segment_managerIcNS9_15rbtree_best_fitINS9_12mutex_familyENS9_10offset_ptrIvlmLm0EEELm0EEENS9_10iset_indexEEEEEvEEESF_EESF_EENSA_ISL_SI_EENS1_15prime_fmod_sizeIvEEE2atEm>
  4f4960:	ldr	x1, [sp, #296]
  4f4964:	cmp	x1, #0x1
  4f4968:	add	x0, x23, x1
  4f496c:	csetm	x8, ne	// ne = any
  4f4970:	and	x0, x8, x0
  4f4974:	sub	x2, x0, x25
  4f4978:	ldr	x0, [x0]
  4f497c:	cmp	x0, #0x1
  4f4980:	csetm	x3, ne	// ne = any
  4f4984:	and	x3, x3, x2
  4f4988:	add	x3, x3, x0
  4f498c:	str	x3, [sp, #248]
  4f4990:	cmp	x3, #0x1
  4f4994:	b.eq	4f3ecc <bpftime_map_delete_elem_helper+0x828>  // b.none
  4f4998:	add	x0, x25, x3
  4f499c:	ldr	w2, [x0, #8]
  4f49a0:	cmp	w20, w2
  4f49a4:	b.ne	4f5880 <bpftime_map_delete_elem_helper+0x21dc>  // b.any
  4f49a8:	ldr	x10, [sp, #304]
  4f49ac:	add	x27, sp, #0x110
  4f49b0:	sub	x9, x25, x27
  4f49b4:	add	x2, sp, #0xe0
  4f49b8:	add	x3, x9, x3
  4f49bc:	cmp	x10, #0x1
  4f49c0:	add	x28, sp, #0x140
  4f49c4:	csetm	x4, ne	// ne = any
  4f49c8:	add	x20, sp, #0x130
  4f49cc:	cmp	x3, #0x1
  4f49d0:	add	x26, sp, #0x148
  4f49d4:	sub	x0, x27, x2
  4f49d8:	sub	x5, x23, x28
  4f49dc:	csetm	x6, ne	// ne = any
  4f49e0:	sub	x24, x20, x26
  4f49e4:	and	x8, x8, x5
  4f49e8:	and	x6, x6, x0
  4f49ec:	and	x4, x4, x24
  4f49f0:	add	x8, x8, x1
  4f49f4:	add	x4, x4, x10
  4f49f8:	add	x6, x6, x3
  4f49fc:	add	x0, sp, #0xe8
  4f4a00:	mov	x1, x28
  4f4a04:	stp	x0, x5, [sp]
  4f4a08:	stp	x2, x9, [sp, #16]
  4f4a0c:	str	x6, [sp, #224]
  4f4a10:	str	x3, [sp, #272]
  4f4a14:	stp	x8, x4, [sp, #320]
  4f4a18:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f4a1c:	ldp	x2, x9, [sp, #16]
  4f4a20:	ldr	x1, [sp, #224]
  4f4a24:	ldp	x0, x5, [sp]
  4f4a28:	cmp	x1, #0x1
  4f4a2c:	add	x2, x2, x1
  4f4a30:	csetm	x1, ne	// ne = any
  4f4a34:	tst	x1, x2
  4f4a38:	b.eq	4f3ecc <bpftime_map_delete_elem_helper+0x828>  // b.none
  4f4a3c:	ldp	x4, x3, [sp, #232]
  4f4a40:	add	x1, sp, #0x100
  4f4a44:	sub	x2, x2, x25
  4f4a48:	sub	x0, x0, x1
  4f4a4c:	cmp	x4, #0x1
  4f4a50:	csetm	x11, ne	// ne = any
  4f4a54:	cmp	x3, #0x1
  4f4a58:	and	x11, x11, x0
  4f4a5c:	csetm	x12, ne	// ne = any
  4f4a60:	cmp	x2, #0x1
  4f4a64:	add	x11, x11, x4
  4f4a68:	csetm	x10, ne	// ne = any
  4f4a6c:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f4a70:	and	x12, x12, x0
  4f4a74:	cmp	x11, #0x1
  4f4a78:	add	x12, x12, x3
  4f4a7c:	and	x10, x10, x9
  4f4a80:	add	x0, sp, #0x118
  4f4a84:	add	x10, x10, x2
  4f4a88:	sub	x1, x1, x0
  4f4a8c:	csetm	x9, ne	// ne = any
  4f4a90:	cmp	x12, #0x1
  4f4a94:	and	x9, x9, x1
  4f4a98:	csetm	x8, ne	// ne = any
  4f4a9c:	cmp	x10, #0x1
  4f4aa0:	add	x9, x9, x11
  4f4aa4:	sub	x4, x27, x23
  4f4aa8:	mov	x1, #0xffffffffffffffe8    	// #-24
  4f4aac:	csetm	x27, ne	// ne = any
  4f4ab0:	and	x8, x8, x1
  4f4ab4:	cmp	x9, #0x1
  4f4ab8:	add	x8, x8, x12
  4f4abc:	and	x27, x27, x4
  4f4ac0:	add	x27, x27, x10
  4f4ac4:	sub	x0, x0, x20
  4f4ac8:	csetm	x4, ne	// ne = any
  4f4acc:	cmp	x8, #0x1
  4f4ad0:	and	x4, x4, x0
  4f4ad4:	csetm	x1, ne	// ne = any
  4f4ad8:	cmp	x27, #0x1
  4f4adc:	add	x4, x4, x9
  4f4ae0:	csetm	x25, ne	// ne = any
  4f4ae4:	mov	x0, #0xffffffffffffffe8    	// #-24
  4f4ae8:	and	x1, x1, x0
  4f4aec:	cmp	x4, #0x1
  4f4af0:	add	x1, x1, x8
  4f4af4:	and	x0, x25, x5
  4f4af8:	add	x0, x0, x27
  4f4afc:	csetm	x6, ne	// ne = any
  4f4b00:	cmp	x1, #0x1
  4f4b04:	stp	x2, x11, [sp, #248]
  4f4b08:	csetm	x5, ne	// ne = any
  4f4b0c:	cmp	x0, #0x1
  4f4b10:	add	x2, x28, x0
  4f4b14:	csetm	x7, ne	// ne = any
  4f4b18:	and	x3, x6, x24
  4f4b1c:	and	x7, x7, x2
  4f4b20:	mov	x2, #0xffffffffffffffe8    	// #-24
  4f4b24:	and	x2, x5, x2
  4f4b28:	add	x3, x3, x4
  4f4b2c:	add	x2, x2, x1
  4f4b30:	stp	x12, x10, [sp, #264]
  4f4b34:	stp	x9, x8, [sp, #280]
  4f4b38:	sub	x8, x7, x28
  4f4b3c:	add	x9, sp, #0x138
  4f4b40:	stp	x27, x4, [sp, #296]
  4f4b44:	stp	x1, x0, [sp, #312]
  4f4b48:	str	x3, [sp, #328]
  4f4b4c:	str	x2, [sp, #336]
  4f4b50:	ldr	x2, [x7]
  4f4b54:	cmp	x2, #0x1
  4f4b58:	csetm	x0, ne	// ne = any
  4f4b5c:	and	x0, x0, x8
  4f4b60:	add	x0, x0, x2
  4f4b64:	str	x0, [sp, #320]
  4f4b68:	cmp	x0, #0x1
  4f4b6c:	b.eq	4f50e8 <bpftime_map_delete_elem_helper+0x1a44>  // b.none
  4f4b70:	add	x7, sp, #0xa0
  4f4b74:	add	x8, sp, #0xa8
  4f4b78:	sub	x20, x20, x7
  4f4b7c:	sub	x0, x9, x8
  4f4b80:	and	x20, x20, x6
  4f4b84:	and	x0, x0, x5
  4f4b88:	add	x20, x20, x4
  4f4b8c:	add	x5, x0, x1
  4f4b90:	cmp	x20, #0x1
  4f4b94:	add	x2, x7, x20
  4f4b98:	csetm	x6, ne	// ne = any
  4f4b9c:	stp	x20, x5, [sp, #160]
  4f4ba0:	and	x2, x6, x2
  4f4ba4:	add	x27, x23, x27
  4f4ba8:	mov	x4, x2
  4f4bac:	and	x3, x25, x27
  4f4bb0:	ldr	x1, [x2]
  4f4bb4:	b	4f4bc4 <bpftime_map_delete_elem_helper+0x1520>
  4f4bb8:	ldr	x1, [x0]
  4f4bbc:	mov	x2, x0
  4f4bc0:	mov	x4, x0
  4f4bc4:	cmp	x1, #0x1
  4f4bc8:	add	x1, x1, x2
  4f4bcc:	csetm	x0, ne	// ne = any
  4f4bd0:	and	x0, x0, x1
  4f4bd4:	cmp	x3, x0
  4f4bd8:	b.ne	4f4bb8 <bpftime_map_delete_elem_helper+0x1514>  // b.any
  4f4bdc:	cmp	x5, #0x1
  4f4be0:	add	x3, sp, #0xb8
  4f4be4:	add	x26, sp, #0xb0
  4f4be8:	csetm	x1, ne	// ne = any
  4f4bec:	sub	x8, x8, x3
  4f4bf0:	sub	x3, x7, x26
  4f4bf4:	and	x0, x6, x3
  4f4bf8:	and	x1, x1, x8
  4f4bfc:	add	x1, x1, x5
  4f4c00:	add	x0, x0, x20
  4f4c04:	stp	x0, x1, [sp, #176]
  4f4c08:	ldr	x0, [x2]
  4f4c0c:	cmp	x0, #0x1
  4f4c10:	add	x0, x0, x4
  4f4c14:	csetm	x1, ne	// ne = any
  4f4c18:	and	x1, x1, x0
  4f4c1c:	sub	x0, x1, x4
  4f4c20:	ldr	x3, [x1]
  4f4c24:	cmp	x3, #0x1
  4f4c28:	csetm	x1, ne	// ne = any
  4f4c2c:	and	x0, x1, x0
  4f4c30:	add	x0, x0, x3
  4f4c34:	str	x0, [x2]
  4f4c38:	ldr	x0, [sp, #176]
  4f4c3c:	cmp	x0, #0x1
  4f4c40:	add	x0, x26, x0
  4f4c44:	csetm	x1, ne	// ne = any
  4f4c48:	and	x0, x1, x0
  4f4c4c:	ldr	x4, [x0]
  4f4c50:	cmp	x4, #0x1
  4f4c54:	b.eq	4f4e4c <bpftime_map_delete_elem_helper+0x17a8>  // b.none
  4f4c58:	add	x20, sp, #0xc0
  4f4c5c:	add	x25, sp, #0x90
  4f4c60:	add	x28, sp, #0x98
  4f4c64:	add	x27, sp, #0x88
  4f4c68:	ldr	x0, [sp, #296]
  4f4c6c:	sub	x23, x23, x27
  4f4c70:	ldr	x1, [sp, #32]
  4f4c74:	cmp	x0, #0x1
  4f4c78:	csetm	x4, ne	// ne = any
  4f4c7c:	and	x4, x4, x23
  4f4c80:	add	x4, x4, x0
  4f4c84:	str	x4, [sp, #136]
  4f4c88:	add	x3, x27, x4
  4f4c8c:	ldr	x0, [x21, #48]
  4f4c90:	cmp	x0, #0x1
  4f4c94:	add	x24, x1, x0
  4f4c98:	csetm	x0, ne	// ne = any
  4f4c9c:	and	x0, x0, x24
  4f4ca0:	cmp	x0, #0x0
  4f4ca4:	sub	x0, x0, x25
  4f4ca8:	sub	x1, x0, #0x1
  4f4cac:	csetm	x0, ne	// ne = any
  4f4cb0:	and	x0, x0, x1
  4f4cb4:	add	x0, x0, #0x1
  4f4cb8:	str	x0, [sp, #144]
  4f4cbc:	cmp	x0, #0x1
  4f4cc0:	add	x0, x25, x0
  4f4cc4:	csetm	x1, ne	// ne = any
  4f4cc8:	and	x0, x1, x0
  4f4ccc:	cmp	x0, #0x0
  4f4cd0:	sub	x0, x0, x28
  4f4cd4:	csetm	x2, ne	// ne = any
  4f4cd8:	cmp	x4, #0x1
  4f4cdc:	csetm	x5, ne	// ne = any
  4f4ce0:	sub	x0, x0, #0x1
  4f4ce4:	and	x1, x5, x3
  4f4ce8:	and	x2, x2, x0
  4f4cec:	add	x0, x1, #0x8
  4f4cf0:	add	x1, x2, #0x1
  4f4cf4:	sub	x26, x0, x26
  4f4cf8:	str	x1, [sp, #152]
  4f4cfc:	cmp	x26, #0x1
  4f4d00:	str	x26, [sp, #176]
  4f4d04:	csetm	x1, ne	// ne = any
  4f4d08:	and	x1, x1, x0
  4f4d0c:	add	x0, x1, #0x8
  4f4d10:	ldr	x2, [x0, #24]
  4f4d14:	cbnz	x2, 4f4e14 <bpftime_map_delete_elem_helper+0x1770>
  4f4d18:	sub	x2, x27, x20
  4f4d1c:	mov	x1, x20
  4f4d20:	and	x2, x2, x5
  4f4d24:	mov	x0, x25
  4f4d28:	add	x2, x2, x4
  4f4d2c:	str	x2, [sp, #192]
  4f4d30:	bl	550b94 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f4d34:	ldr	x0, [x22, #8]
  4f4d38:	mov	x20, #0x0                   	// #0
  4f4d3c:	sub	x0, x0, #0x1
  4f4d40:	str	x0, [x22, #8]
  4f4d44:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f4d48:	mov	x20, #0xffffffffffffffea    	// #-22
  4f4d4c:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f4d50:	mov	x0, x19
  4f4d54:	bl	509060 <_ZN7bpftime25hash_map_kernel_user_impl11init_map_fdEv>
  4f4d58:	ldr	w0, [x19, #76]
  4f4d5c:	b	4f4368 <bpftime_map_delete_elem_helper+0xcc4>
  4f4d60:	ldrb	w3, [x20, #1]
  4f4d64:	mov	x4, #0x745f                	// #29791
  4f4d68:	ldrb	w0, [x20]
  4f4d6c:	mov	w2, #0x1f                  	// #31
  4f4d70:	ldrb	w6, [x20, #2]
  4f4d74:	lsl	x1, x3, #4
  4f4d78:	ldrb	w5, [x20, #3]
  4f4d7c:	sub	x1, x1, x3
  4f4d80:	add	x1, x3, x1, lsl #6
  4f4d84:	umaddl	x2, w2, w6, x5
  4f4d88:	madd	x0, x0, x4, x1
  4f4d8c:	add	x0, x0, x2
  4f4d90:	mov	x2, x0
  4f4d94:	b	4f3bb4 <bpftime_map_delete_elem_helper+0x510>
  4f4d98:	bl	506894 <_ZN6spdlog18default_logger_rawEv>
  4f4d9c:	mov	x20, #0xffffffffffffffff    	// #-1
  4f4da0:	mov	w3, #0x327                 	// #807
  4f4da4:	adrp	x2, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f4da8:	add	x2, x2, #0xf58
  4f4dac:	str	x2, [sp, #96]
  4f4db0:	str	w3, [sp, #104]
  4f4db4:	add	x1, sp, #0x30
  4f4db8:	ldr	w7, [x19, #72]
  4f4dbc:	adrp	x6, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f4dc0:	ldp	x8, x9, [sp, #96]
  4f4dc4:	add	x6, x6, #0xe98
  4f4dc8:	stp	x8, x9, [sp, #48]
  4f4dcc:	add	x5, sp, #0x5c
  4f4dd0:	mov	x4, #0x2e                  	// #46
  4f4dd4:	str	w7, [sp, #92]
  4f4dd8:	adrp	x3, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f4ddc:	str	x6, [sp, #112]
  4f4de0:	add	x3, x3, #0xea8
  4f4de4:	str	x6, [x1, #16]
  4f4de8:	mov	w2, #0x4                   	// #4
  4f4dec:	bl	4c98f0 <_ZN6spdlog6logger4log_IJRKiEEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_>
  4f4df0:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f4df4:	ldr	w1, [x24, x1]
  4f4df8:	ldr	w0, [x22]
  4f4dfc:	cmp	w1, w0
  4f4e00:	b.ne	4f3bfc <bpftime_map_delete_elem_helper+0x558>  // b.any
  4f4e04:	b	4f3c44 <bpftime_map_delete_elem_helper+0x5a0>
  4f4e08:	mov	w0, #0x1                   	// #1
  4f4e0c:	bl	1d092e0 <__aarch64_ldadd4_sync>
  4f4e10:	b	4f45f4 <bpftime_map_delete_elem_helper+0xf50>
  4f4e14:	ldr	x4, [x1, #16]
  4f4e18:	add	x3, x0, #0x8
  4f4e1c:	sub	x3, x3, x20
  4f4e20:	mov	x1, x20
  4f4e24:	cmp	x4, #0x1
  4f4e28:	csetm	x2, ne	// ne = any
  4f4e2c:	and	x2, x2, x3
  4f4e30:	add	x2, x2, x4
  4f4e34:	str	x2, [sp, #192]
  4f4e38:	bl	550b94 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f4e3c:	ldr	x4, [sp, #136]
  4f4e40:	cmp	x4, #0x1
  4f4e44:	csetm	x5, ne	// ne = any
  4f4e48:	b	4f4d18 <bpftime_map_delete_elem_helper+0x1674>
  4f4e4c:	add	x20, sp, #0xc0
  4f4e50:	mov	x1, x26
  4f4e54:	mov	x0, x20
  4f4e58:	str	x4, [sp]
  4f4e5c:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f4e60:	add	x25, sp, #0x90
  4f4e64:	ldp	x3, x0, [sp, #192]
  4f4e68:	sub	x5, x20, x25
  4f4e6c:	add	x28, sp, #0x98
  4f4e70:	add	x27, sp, #0x88
  4f4e74:	ldr	x4, [sp]
  4f4e78:	cmp	x3, #0x1
  4f4e7c:	csetm	x1, ne	// ne = any
  4f4e80:	cmp	x0, #0x1
  4f4e84:	csetm	x2, ne	// ne = any
  4f4e88:	and	x1, x1, x5
  4f4e8c:	and	x2, x2, #0x30
  4f4e90:	add	x1, x1, x3
  4f4e94:	add	x2, x2, x0
  4f4e98:	stp	x1, x2, [sp, #144]
  4f4e9c:	cmp	x2, #0x1
  4f4ea0:	csetm	x6, ne	// ne = any
  4f4ea4:	add	x3, x28, x2
  4f4ea8:	and	x3, x6, x3
  4f4eac:	cmp	x1, #0x1
  4f4eb0:	cset	x0, eq	// eq = none
  4f4eb4:	add	x1, x25, x1
  4f4eb8:	sub	x0, x0, #0x1
  4f4ebc:	ldp	x5, x7, [x3]
  4f4ec0:	and	x0, x0, x1
  4f4ec4:	cmp	x5, #0x1
  4f4ec8:	add	x5, x3, x5
  4f4ecc:	csetm	x1, ne	// ne = any
  4f4ed0:	and	x1, x1, x5
  4f4ed4:	sub	x0, x0, x1
  4f4ed8:	asr	x0, x0, #3
  4f4edc:	lsl	x0, x4, x0
  4f4ee0:	bic	x0, x7, x0
  4f4ee4:	str	x0, [x3, #8]
  4f4ee8:	cbnz	x0, 4f4c68 <bpftime_map_delete_elem_helper+0x15c4>
  4f4eec:	sub	x0, x28, x27
  4f4ef0:	dup	v0.2d, x4
  4f4ef4:	and	x0, x6, x0
  4f4ef8:	add	x0, x0, x2
  4f4efc:	cmp	x0, #0x1
  4f4f00:	add	x0, x27, x0
  4f4f04:	csetm	x1, ne	// ne = any
  4f4f08:	and	x0, x1, x0
  4f4f0c:	add	x3, x0, #0x10
  4f4f10:	add	x5, x0, #0x18
  4f4f14:	ldp	x1, x4, [x0, #16]
  4f4f18:	cmp	x1, #0x1
  4f4f1c:	add	x1, x1, x3
  4f4f20:	csetm	x2, ne	// ne = any
  4f4f24:	cmp	x4, #0x1
  4f4f28:	and	x2, x2, x1
  4f4f2c:	csetm	x1, ne	// ne = any
  4f4f30:	sub	x6, x0, x2
  4f4f34:	and	x1, x1, x6
  4f4f38:	add	x1, x1, x4
  4f4f3c:	str	x1, [x2, #24]
  4f4f40:	ldp	x4, x1, [x0, #16]
  4f4f44:	cmp	x1, #0x1
  4f4f48:	add	x1, x5, x1
  4f4f4c:	csetm	x2, ne	// ne = any
  4f4f50:	cmp	x4, #0x1
  4f4f54:	and	x2, x2, x1
  4f4f58:	csetm	x1, ne	// ne = any
  4f4f5c:	add	x5, x2, #0x10
  4f4f60:	sub	x3, x3, x5
  4f4f64:	and	x1, x1, x3
  4f4f68:	add	x1, x1, x4
  4f4f6c:	str	x1, [x2, #16]
  4f4f70:	str	q0, [x0, #16]
  4f4f74:	b	4f4c68 <bpftime_map_delete_elem_helper+0x15c4>
  4f4f78:	mov	x1, x26
  4f4f7c:	mov	x0, x24
  4f4f80:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f4f84:	ldp	x2, x3, [sp, #272]
  4f4f88:	add	x1, sp, #0x90
  4f4f8c:	sub	x5, x24, x1
  4f4f90:	add	x4, sp, #0x98
  4f4f94:	cmp	x2, #0x1
  4f4f98:	csetm	x0, ne	// ne = any
  4f4f9c:	cmp	x3, #0x1
  4f4fa0:	and	x0, x0, x5
  4f4fa4:	cset	x5, ne	// ne = any
  4f4fa8:	add	x0, x0, x2
  4f4fac:	add	x5, x3, x5, lsl #7
  4f4fb0:	add	x2, x1, x0
  4f4fb4:	cmp	x5, #0x1
  4f4fb8:	add	x1, x4, x5
  4f4fbc:	csetm	x6, ne	// ne = any
  4f4fc0:	cmp	x0, #0x1
  4f4fc4:	and	x1, x6, x1
  4f4fc8:	stp	x0, x5, [sp, #144]
  4f4fcc:	cset	x0, eq	// eq = none
  4f4fd0:	sub	x0, x0, #0x1
  4f4fd4:	and	x0, x0, x2
  4f4fd8:	ldp	x2, x7, [x1]
  4f4fdc:	cmp	x2, #0x1
  4f4fe0:	add	x2, x1, x2
  4f4fe4:	csetm	x3, ne	// ne = any
  4f4fe8:	and	x2, x3, x2
  4f4fec:	sub	x0, x0, x2
  4f4ff0:	asr	x0, x0, #3
  4f4ff4:	lsl	x0, x28, x0
  4f4ff8:	bic	x0, x7, x0
  4f4ffc:	str	x0, [x1, #8]
  4f5000:	cbnz	x0, 4f4860 <bpftime_map_delete_elem_helper+0x11bc>
  4f5004:	add	x1, sp, #0x88
  4f5008:	dup	v0.2d, x28
  4f500c:	sub	x4, x4, x1
  4f5010:	and	x0, x6, x4
  4f5014:	add	x0, x0, x5
  4f5018:	cmp	x0, #0x1
  4f501c:	add	x1, x1, x0
  4f5020:	csetm	x0, ne	// ne = any
  4f5024:	and	x0, x0, x1
  4f5028:	add	x3, x0, #0x10
  4f502c:	add	x4, x0, #0x18
  4f5030:	ldp	x1, x5, [x0, #16]
  4f5034:	cmp	x1, #0x1
  4f5038:	add	x1, x1, x3
  4f503c:	csetm	x2, ne	// ne = any
  4f5040:	cmp	x5, #0x1
  4f5044:	and	x2, x2, x1
  4f5048:	csetm	x1, ne	// ne = any
  4f504c:	sub	x6, x0, x2
  4f5050:	and	x1, x1, x6
  4f5054:	add	x1, x1, x5
  4f5058:	str	x1, [x2, #24]
  4f505c:	ldp	x5, x1, [x0, #16]
  4f5060:	cmp	x1, #0x1
  4f5064:	add	x1, x4, x1
  4f5068:	csetm	x2, ne	// ne = any
  4f506c:	cmp	x5, #0x1
  4f5070:	and	x2, x2, x1
  4f5074:	csetm	x1, ne	// ne = any
  4f5078:	add	x4, x2, #0x10
  4f507c:	sub	x3, x3, x4
  4f5080:	and	x1, x1, x3
  4f5084:	add	x1, x1, x5
  4f5088:	str	x1, [x2, #16]
  4f508c:	str	q0, [x0, #16]
  4f5090:	b	4f4860 <bpftime_map_delete_elem_helper+0x11bc>
  4f5094:	mov	x0, x23
  4f5098:	stp	x3, x2, [sp]
  4f509c:	stp	x1, x10, [sp, #16]
  4f50a0:	str	x9, [sp, #32]
  4f50a4:	bl	4e2770 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEE9incrementEv>
  4f50a8:	ldr	x0, [sp, #200]
  4f50ac:	ldp	x3, x2, [sp]
  4f50b0:	cmp	x0, #0x1
  4f50b4:	add	x4, x0, x23
  4f50b8:	csetm	x0, ne	// ne = any
  4f50bc:	and	x0, x0, x4
  4f50c0:	sub	x5, x0, x20
  4f50c4:	ldp	x1, x10, [sp, #16]
  4f50c8:	ldr	x4, [x0]
  4f50cc:	ldr	x9, [sp, #32]
  4f50d0:	cmp	x4, #0x1
  4f50d4:	csetm	x0, ne	// ne = any
  4f50d8:	and	x0, x0, x5
  4f50dc:	add	x0, x0, x4
  4f50e0:	str	x0, [sp, #192]
  4f50e4:	b	4f4764 <bpftime_map_delete_elem_helper+0x10c0>
  4f50e8:	mov	x0, x26
  4f50ec:	stp	x4, x9, [sp]
  4f50f0:	stp	x1, x6, [sp, #16]
  4f50f4:	str	x5, [sp, #40]
  4f50f8:	bl	4e28e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKjNS_9container6vectorImNS_12interprocess9allocatorImNS9_15segment_managerIcNS9_15rbtree_best_fitINS9_12mutex_familyENS9_10offset_ptrIvlmLm0EEELm0EEENS9_10iset_indexEEEEEvEEESF_EESF_EEE9incrementEv>
  4f50fc:	ldr	x0, [sp, #328]
  4f5100:	ldp	x4, x9, [sp]
  4f5104:	cmp	x0, #0x1
  4f5108:	add	x2, x0, x26
  4f510c:	csetm	x0, ne	// ne = any
  4f5110:	and	x0, x0, x2
  4f5114:	sub	x28, x0, x28
  4f5118:	ldp	x1, x6, [sp, #16]
  4f511c:	ldr	x2, [x0]
  4f5120:	ldr	x5, [sp, #40]
  4f5124:	cmp	x2, #0x1
  4f5128:	csetm	x0, ne	// ne = any
  4f512c:	and	x0, x0, x28
  4f5130:	add	x0, x0, x2
  4f5134:	str	x0, [sp, #320]
  4f5138:	b	4f4b70 <bpftime_map_delete_elem_helper+0x14cc>
  4f513c:	stp	x0, x6, [sp]
  4f5140:	bl	4e2a50 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEESJ_ESE_EESE_EEE9incrementEv>
  4f5144:	ldp	x0, x6, [sp]
  4f5148:	ldr	x1, [sp, #328]
  4f514c:	cmp	x1, #0x1
  4f5150:	add	x1, x1, x0
  4f5154:	csetm	x0, ne	// ne = any
  4f5158:	and	x0, x0, x1
  4f515c:	sub	x6, x0, x6
  4f5160:	ldr	x1, [x0]
  4f5164:	cmp	x1, #0x1
  4f5168:	csetm	x0, ne	// ne = any
  4f516c:	and	x0, x0, x6
  4f5170:	add	x0, x0, x1
  4f5174:	str	x0, [sp, #320]
  4f5178:	b	4f40e8 <bpftime_map_delete_elem_helper+0xa44>
  4f517c:	mov	x1, x26
  4f5180:	mov	x0, x20
  4f5184:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f5188:	ldp	x5, x3, [sp, #192]
  4f518c:	add	x0, sp, #0xc8
  4f5190:	sub	x0, x0, x28
  4f5194:	add	x4, sp, #0x90
  4f5198:	sub	x6, x20, x4
  4f519c:	cmp	x5, #0x1
  4f51a0:	csetm	x2, ne	// ne = any
  4f51a4:	cmp	x3, #0x1
  4f51a8:	csetm	x1, ne	// ne = any
  4f51ac:	and	x2, x2, x6
  4f51b0:	and	x1, x1, x0
  4f51b4:	add	x2, x2, x5
  4f51b8:	add	x1, x1, x3
  4f51bc:	stp	x2, x1, [sp, #144]
  4f51c0:	cmp	x1, #0x1
  4f51c4:	csetm	x5, ne	// ne = any
  4f51c8:	add	x3, x28, x1
  4f51cc:	and	x3, x5, x3
  4f51d0:	cmp	x2, #0x1
  4f51d4:	add	x4, x4, x2
  4f51d8:	cset	x0, eq	// eq = none
  4f51dc:	sub	x0, x0, #0x1
  4f51e0:	ldp	x2, x6, [x3]
  4f51e4:	and	x0, x0, x4
  4f51e8:	cmp	x2, #0x1
  4f51ec:	add	x2, x3, x2
  4f51f0:	csetm	x4, ne	// ne = any
  4f51f4:	and	x2, x4, x2
  4f51f8:	sub	x0, x0, x2
  4f51fc:	asr	x0, x0, #3
  4f5200:	lsl	x0, x22, x0
  4f5204:	bic	x0, x6, x0
  4f5208:	str	x0, [x3, #8]
  4f520c:	cbnz	x0, 4f41f0 <bpftime_map_delete_elem_helper+0xb4c>
  4f5210:	add	x2, sp, #0x88
  4f5214:	dup	v0.2d, x22
  4f5218:	sub	x0, x28, x2
  4f521c:	and	x0, x5, x0
  4f5220:	add	x0, x0, x1
  4f5224:	cmp	x0, #0x1
  4f5228:	add	x2, x2, x0
  4f522c:	csetm	x0, ne	// ne = any
  4f5230:	and	x0, x0, x2
  4f5234:	add	x3, x0, #0x10
  4f5238:	add	x4, x0, #0x18
  4f523c:	ldp	x1, x5, [x0, #16]
  4f5240:	cmp	x1, #0x1
  4f5244:	add	x1, x1, x3
  4f5248:	csetm	x2, ne	// ne = any
  4f524c:	cmp	x5, #0x1
  4f5250:	and	x2, x2, x1
  4f5254:	csetm	x1, ne	// ne = any
  4f5258:	sub	x6, x0, x2
  4f525c:	and	x1, x1, x6
  4f5260:	add	x1, x1, x5
  4f5264:	str	x1, [x2, #24]
  4f5268:	ldp	x5, x1, [x0, #16]
  4f526c:	cmp	x1, #0x1
  4f5270:	add	x1, x4, x1
  4f5274:	csetm	x2, ne	// ne = any
  4f5278:	cmp	x5, #0x1
  4f527c:	and	x2, x2, x1
  4f5280:	csetm	x1, ne	// ne = any
  4f5284:	add	x4, x2, #0x10
  4f5288:	sub	x3, x3, x4
  4f528c:	and	x1, x1, x3
  4f5290:	add	x1, x1, x5
  4f5294:	str	x1, [x2, #16]
  4f5298:	str	q0, [x0, #16]
  4f529c:	b	4f41f0 <bpftime_map_delete_elem_helper+0xb4c>
  4f52a0:	ldr	x1, [x19, #112]
  4f52a4:	add	x22, x19, #0x68
  4f52a8:	add	x23, sp, #0x128
  4f52ac:	mov	w0, w20
  4f52b0:	add	x25, sp, #0xf8
  4f52b4:	bl	4fc5e0 <_ZN5boost9unordered6detail15prime_fmod_sizeIvE8positionEmm>
  4f52b8:	mov	x1, x0
  4f52bc:	mov	x8, x23
  4f52c0:	mov	x0, x22
  4f52c4:	bl	4e25d0 <_ZNK5boost9unordered6detail20grouped_bucket_arrayINS1_6bucketINS1_4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEESD_EENSB_9allocatorISA_NSB_15segment_managerIcNSB_15rbtree_best_fitINSB_12mutex_familyESD_Lm0EEENSB_10iset_indexEEEEENS1_15prime_fmod_sizeIvEEE2atEm>
  4f52c8:	ldr	x0, [sp, #296]
  4f52cc:	cmp	x0, #0x1
  4f52d0:	add	x1, x23, x0
  4f52d4:	csetm	x0, ne	// ne = any
  4f52d8:	and	x0, x0, x1
  4f52dc:	sub	x2, x0, x25
  4f52e0:	ldr	x0, [x0]
  4f52e4:	cmp	x0, #0x1
  4f52e8:	csetm	x1, ne	// ne = any
  4f52ec:	and	x1, x1, x2
  4f52f0:	add	x1, x1, x0
  4f52f4:	str	x1, [sp, #248]
  4f52f8:	cmp	x1, #0x1
  4f52fc:	b.eq	4f4388 <bpftime_map_delete_elem_helper+0xce4>  // b.none
  4f5300:	add	x0, x25, x1
  4f5304:	ldr	w2, [x0, #8]
  4f5308:	cmp	w20, w2
  4f530c:	b.ne	4f598c <bpftime_map_delete_elem_helper+0x22e8>  // b.any
  4f5310:	add	x20, sp, #0x110
  4f5314:	add	x21, sp, #0x140
  4f5318:	sub	x24, x25, x20
  4f531c:	mov	x0, x21
  4f5320:	add	x2, x24, x1
  4f5324:	mov	x1, x23
  4f5328:	str	x2, [sp, #272]
  4f532c:	add	x26, sp, #0xc0
  4f5330:	add	x27, sp, #0xc8
  4f5334:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f5338:	ldr	x3, [sp, #272]
  4f533c:	sub	x4, x20, x26
  4f5340:	mov	x0, x27
  4f5344:	mov	x1, x21
  4f5348:	cmp	x3, #0x1
  4f534c:	csetm	x2, ne	// ne = any
  4f5350:	and	x2, x2, x4
  4f5354:	add	x2, x2, x3
  4f5358:	str	x2, [sp, #192]
  4f535c:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f5360:	ldr	x0, [sp, #192]
  4f5364:	cmp	x0, #0x1
  4f5368:	add	x3, x26, x0
  4f536c:	csetm	x5, ne	// ne = any
  4f5370:	ands	x5, x5, x3
  4f5374:	b.eq	4f4388 <bpftime_map_delete_elem_helper+0xce4>  // b.none
  4f5378:	ldp	x6, x4, [sp, #200]
  4f537c:	add	x1, sp, #0xe8
  4f5380:	sub	x27, x27, x1
  4f5384:	add	x2, sp, #0xe0
  4f5388:	sub	x20, x20, x21
  4f538c:	add	x26, sp, #0xb0
  4f5390:	cmp	x6, #0x1
  4f5394:	csetm	x0, ne	// ne = any
  4f5398:	cmp	x4, #0x1
  4f539c:	and	x14, x0, x27
  4f53a0:	csetm	x13, ne	// ne = any
  4f53a4:	sub	x0, x3, x2
  4f53a8:	add	x14, x14, x6
  4f53ac:	cmp	x0, #0x1
  4f53b0:	and	x13, x13, #0xffffffffffffffe0
  4f53b4:	add	x13, x13, x4
  4f53b8:	sub	x2, x2, x25
  4f53bc:	csetm	x12, ne	// ne = any
  4f53c0:	add	x3, sp, #0x100
  4f53c4:	cmp	x14, #0x1
  4f53c8:	sub	x1, x1, x3
  4f53cc:	and	x12, x12, x2
  4f53d0:	csetm	x11, ne	// ne = any
  4f53d4:	cmp	x13, #0x1
  4f53d8:	add	x12, x12, x0
  4f53dc:	and	x11, x11, x1
  4f53e0:	csetm	x2, ne	// ne = any
  4f53e4:	add	x11, x11, x14
  4f53e8:	cmp	x12, #0x1
  4f53ec:	mov	x1, #0xffffffffffffffe8    	// #-24
  4f53f0:	and	x2, x2, x1
  4f53f4:	add	x2, x2, x13
  4f53f8:	csetm	x4, ne	// ne = any
  4f53fc:	add	x1, sp, #0x118
  4f5400:	cmp	x11, #0x1
  4f5404:	sub	x3, x3, x1
  4f5408:	and	x4, x4, x24
  4f540c:	csetm	x9, ne	// ne = any
  4f5410:	cmp	x2, #0x1
  4f5414:	add	x4, x4, x12
  4f5418:	and	x9, x9, x3
  4f541c:	csetm	x8, ne	// ne = any
  4f5420:	add	x9, x9, x11
  4f5424:	cmp	x4, #0x1
  4f5428:	mov	x3, #0xffffffffffffffe8    	// #-24
  4f542c:	and	x8, x8, x3
  4f5430:	csetm	x7, ne	// ne = any
  4f5434:	add	x8, x8, x2
  4f5438:	cmp	x9, #0x1
  4f543c:	add	x27, sp, #0x148
  4f5440:	csetm	x6, ne	// ne = any
  4f5444:	sub	x1, x1, x27
  4f5448:	cmp	x8, #0x1
  4f544c:	and	x7, x7, x20
  4f5450:	and	x6, x6, x1
  4f5454:	add	x7, x7, x4
  4f5458:	csetm	x3, ne	// ne = any
  4f545c:	add	x6, x6, x9
  4f5460:	cmp	x7, #0x1
  4f5464:	mov	x1, #0xffffffffffffffd0    	// #-48
  4f5468:	and	x3, x3, x1
  4f546c:	add	x3, x3, x8
  4f5470:	csetm	x10, ne	// ne = any
  4f5474:	add	x24, sp, #0xa0
  4f5478:	cmp	x6, #0x1
  4f547c:	csetm	x15, ne	// ne = any
  4f5480:	sub	x1, x27, x24
  4f5484:	cmp	x3, #0x1
  4f5488:	and	x15, x15, x1
  4f548c:	csetm	x1, ne	// ne = any
  4f5490:	ldr	w25, [x5, #24]
  4f5494:	add	x15, x15, x6
  4f5498:	stp	x0, x14, [sp, #224]
  4f549c:	mov	x0, #0xa8                  	// #168
  4f54a0:	and	x1, x1, x0
  4f54a4:	cmp	x15, #0x1
  4f54a8:	add	x1, x1, x3
  4f54ac:	add	x5, sp, #0x78
  4f54b0:	sub	x16, x21, x5
  4f54b4:	csetm	x0, ne	// ne = any
  4f54b8:	cmp	x1, #0x1
  4f54bc:	and	x10, x10, x16
  4f54c0:	sub	x16, x24, x26
  4f54c4:	stp	x13, x12, [sp, #240]
  4f54c8:	csetm	x13, ne	// ne = any
  4f54cc:	add	x10, x10, x7
  4f54d0:	and	x0, x0, x16
  4f54d4:	and	x13, x13, #0xfffffffffffffff0
  4f54d8:	add	x0, x0, x15
  4f54dc:	cmp	x10, #0x1
  4f54e0:	add	x12, sp, #0x80
  4f54e4:	stp	x1, x0, [sp, #168]
  4f54e8:	add	x1, x13, x1
  4f54ec:	str	x1, [sp, #184]
  4f54f0:	csetm	x1, ne	// ne = any
  4f54f4:	cmp	x0, #0x1
  4f54f8:	stp	x11, x2, [sp, #256]
  4f54fc:	sub	x2, x5, x12
  4f5500:	add	x0, x26, x0
  4f5504:	and	x1, x1, x2
  4f5508:	csetm	x2, ne	// ne = any
  4f550c:	add	x1, x1, x10
  4f5510:	and	x2, x2, x0
  4f5514:	str	x10, [sp, #120]
  4f5518:	str	x1, [sp, #128]
  4f551c:	add	x11, x12, x1
  4f5520:	str	x15, [sp, #160]
  4f5524:	stp	x4, x9, [sp, #272]
  4f5528:	mov	x4, x2
  4f552c:	str	x8, [sp, #288]
  4f5530:	stp	x7, x6, [sp, #320]
  4f5534:	str	x3, [sp, #336]
  4f5538:	ldr	x0, [x2]
  4f553c:	cmp	x0, #0x1
  4f5540:	add	x6, x2, x0
  4f5544:	csetm	x0, ne	// ne = any
  4f5548:	cmp	x1, #0x1
  4f554c:	csetm	x3, ne	// ne = any
  4f5550:	and	x0, x0, x6
  4f5554:	and	x3, x3, x11
  4f5558:	cmp	x3, x0
  4f555c:	b.eq	4f5584 <bpftime_map_delete_elem_helper+0x1ee0>  // b.none
  4f5560:	ldr	x1, [x0]
  4f5564:	mov	x2, x0
  4f5568:	mov	x4, x0
  4f556c:	cmp	x1, #0x1
  4f5570:	add	x1, x0, x1
  4f5574:	csetm	x0, ne	// ne = any
  4f5578:	and	x0, x0, x1
  4f557c:	cmp	x0, x3
  4f5580:	b.ne	4f5560 <bpftime_map_delete_elem_helper+0x1ebc>  // b.any
  4f5584:	ldr	x1, [x3]
  4f5588:	sub	x3, x3, x4
  4f558c:	add	x20, sp, #0x130
  4f5590:	cmp	x1, #0x1
  4f5594:	csetm	x0, ne	// ne = any
  4f5598:	and	x0, x0, x3
  4f559c:	add	x0, x0, x1
  4f55a0:	str	x0, [x2]
  4f55a4:	ldr	x0, [sp, #176]
  4f55a8:	cmp	x0, #0x1
  4f55ac:	add	x0, x26, x0
  4f55b0:	csetm	x1, ne	// ne = any
  4f55b4:	and	x0, x1, x0
  4f55b8:	ldr	x2, [x0]
  4f55bc:	cmp	x2, #0x1
  4f55c0:	b.eq	4f5740 <bpftime_map_delete_elem_helper+0x209c>  // b.none
  4f55c4:	ldr	x0, [x28, #8]
  4f55c8:	sub	x5, x5, x23
  4f55cc:	ldr	x6, [sp, #120]
  4f55d0:	sub	x0, x0, #0x1
  4f55d4:	str	x0, [x28, #8]
  4f55d8:	add	x2, x19, #0x140
  4f55dc:	mov	w28, #0x1                   	// #1
  4f55e0:	add	x4, x19, #0x148
  4f55e4:	ldr	x0, [x19, #104]
  4f55e8:	strb	w28, [sp, #312]
  4f55ec:	cmp	x0, #0x1
  4f55f0:	add	x22, x22, x0
  4f55f4:	csetm	x0, ne	// ne = any
  4f55f8:	and	x0, x0, x22
  4f55fc:	cmp	x0, #0x0
  4f5600:	sub	x0, x0, x26
  4f5604:	sub	x0, x0, #0x1
  4f5608:	csetm	x1, ne	// ne = any
  4f560c:	and	x1, x1, x0
  4f5610:	cmp	x6, #0x1
  4f5614:	csetm	x3, ne	// ne = any
  4f5618:	add	x0, x1, #0x1
  4f561c:	cmp	x1, #0x0
  4f5620:	add	x1, x26, x0
  4f5624:	csetm	x0, ne	// ne = any
  4f5628:	and	x3, x3, x5
  4f562c:	and	x0, x0, x1
  4f5630:	add	x3, x3, x6
  4f5634:	cmp	x0, #0x0
  4f5638:	sub	x0, x0, x20
  4f563c:	sub	x0, x0, #0x1
  4f5640:	csetm	x1, ne	// ne = any
  4f5644:	and	x1, x1, x0
  4f5648:	add	x1, x1, #0x1
  4f564c:	stp	x3, x1, [sp, #296]
  4f5650:	ldp	x5, x6, [x2, #16]
  4f5654:	ldr	x1, [x19, #328]
  4f5658:	cmp	x1, #0x1
  4f565c:	add	x0, x4, x1
  4f5660:	csetm	x4, ne	// ne = any
  4f5664:	add	x1, x5, x5, lsl #1
  4f5668:	and	x0, x4, x0
  4f566c:	add	x1, x0, x1, lsl #3
  4f5670:	cmp	x5, x6
  4f5674:	b.eq	4f58a8 <bpftime_map_delete_elem_helper+0x2204>  // b.none
  4f5678:	cmp	x1, #0x0
  4f567c:	sub	x1, x1, x21
  4f5680:	sub	x1, x1, #0x1
  4f5684:	csetm	x0, ne	// ne = any
  4f5688:	and	x0, x0, x1
  4f568c:	add	x0, x0, #0x1
  4f5690:	str	x0, [sp, #320]
  4f5694:	cmp	x0, #0x1
  4f5698:	add	x0, x21, x0
  4f569c:	csetm	x1, ne	// ne = any
  4f56a0:	cmp	x3, #0x1
  4f56a4:	and	x1, x1, x0
  4f56a8:	csetm	x4, ne	// ne = any
  4f56ac:	mov	x21, x1
  4f56b0:	sub	x23, x23, x1
  4f56b4:	and	x4, x4, x23
  4f56b8:	add	x3, x4, x3
  4f56bc:	str	x3, [x1]
  4f56c0:	str	xzr, [x21, #8]!
  4f56c4:	strb	wzr, [x21, #8]
  4f56c8:	ldrb	w0, [sp, #312]
  4f56cc:	tbnz	w0, #0, 4f5708 <bpftime_map_delete_elem_helper+0x2064>
  4f56d0:	ldr	x0, [x2, #16]
  4f56d4:	add	x0, x0, #0x1
  4f56d8:	str	x0, [x2, #16]
  4f56dc:	ldr	x0, [x19, #296]
  4f56e0:	add	x1, x19, #0x128
  4f56e4:	ldr	w2, [x19, #352]
  4f56e8:	mov	x20, #0x0                   	// #0
  4f56ec:	cmp	x0, #0x1
  4f56f0:	add	x1, x1, x0
  4f56f4:	csetm	x0, ne	// ne = any
  4f56f8:	and	x0, x0, x1
  4f56fc:	str	w2, [x0, w25, uxtw #2]
  4f5700:	str	w25, [x19, #352]
  4f5704:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f5708:	mov	x1, x20
  4f570c:	mov	x0, x21
  4f5710:	str	x2, [sp]
  4f5714:	bl	4dccc4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_EC1ERKSI_>
  4f5718:	ldr	x2, [sp]
  4f571c:	strb	w28, [x21, #8]
  4f5720:	b	4f56d0 <bpftime_map_delete_elem_helper+0x202c>
  4f5724:	mov	x2, #0x0                   	// #0
  4f5728:	b	4f3bb4 <bpftime_map_delete_elem_helper+0x510>
  4f572c:	bl	48c090 <__errno_location@plt>
  4f5730:	mov	x20, #0xffffffffffffffff    	// #-1
  4f5734:	mov	w1, #0x2                   	// #2
  4f5738:	str	w1, [x0]
  4f573c:	b	4f3d98 <bpftime_map_delete_elem_helper+0x6f4>
  4f5740:	mov	x1, x26
  4f5744:	mov	x0, x23
  4f5748:	stp	x5, x2, [sp]
  4f574c:	bl	4df4e0 <_ZN5boost9unordered6detail23grouped_bucket_iteratorINS1_6bucketINS1_4nodeISt4pairIKNS_9container6vectorIhNS_12interprocess9allocatorIhNS8_15segment_managerIcNS8_15rbtree_best_fitINS8_12mutex_familyENS8_10offset_ptrIvlmLm0EEELm0EEENS8_10iset_indexEEEEEvEEN7bpftime14hash_map_valueEESE_EESE_EEEC1ERKSQ_>
  4f5750:	add	x7, sp, #0x98
  4f5754:	ldp	x4, x0, [sp, #296]
  4f5758:	sub	x8, x20, x7
  4f575c:	add	x6, sp, #0x90
  4f5760:	sub	x9, x23, x6
  4f5764:	ldp	x5, x2, [sp]
  4f5768:	cmp	x4, #0x1
  4f576c:	csetm	x3, ne	// ne = any
  4f5770:	cmp	x0, #0x1
  4f5774:	csetm	x1, ne	// ne = any
  4f5778:	and	x3, x3, x9
  4f577c:	and	x1, x1, x8
  4f5780:	add	x3, x3, x4
  4f5784:	add	x1, x1, x0
  4f5788:	stp	x3, x1, [sp, #144]
  4f578c:	cmp	x1, #0x1
  4f5790:	csetm	x8, ne	// ne = any
  4f5794:	add	x4, x7, x1
  4f5798:	and	x4, x8, x4
  4f579c:	cmp	x3, #0x1
  4f57a0:	add	x6, x6, x3
  4f57a4:	cset	x0, eq	// eq = none
  4f57a8:	sub	x0, x0, #0x1
  4f57ac:	ldp	x3, x9, [x4]
  4f57b0:	and	x0, x0, x6
  4f57b4:	cmp	x3, #0x1
  4f57b8:	add	x3, x4, x3
  4f57bc:	csetm	x6, ne	// ne = any
  4f57c0:	and	x3, x6, x3
  4f57c4:	sub	x0, x0, x3
  4f57c8:	asr	x0, x0, #3
  4f57cc:	lsl	x0, x2, x0
  4f57d0:	bic	x0, x9, x0
  4f57d4:	str	x0, [x4, #8]
  4f57d8:	cbnz	x0, 4f55c4 <bpftime_map_delete_elem_helper+0x1f20>
  4f57dc:	add	x3, sp, #0x88
  4f57e0:	dup	v0.2d, x2
  4f57e4:	sub	x7, x7, x3
  4f57e8:	and	x0, x8, x7
  4f57ec:	add	x0, x0, x1
  4f57f0:	cmp	x0, #0x1
  4f57f4:	add	x3, x3, x0
  4f57f8:	csetm	x0, ne	// ne = any
  4f57fc:	and	x0, x0, x3
  4f5800:	add	x3, x0, #0x10
  4f5804:	add	x4, x0, #0x18
  4f5808:	ldp	x1, x6, [x0, #16]
  4f580c:	cmp	x1, #0x1
  4f5810:	add	x1, x1, x3
  4f5814:	csetm	x2, ne	// ne = any
  4f5818:	cmp	x6, #0x1
  4f581c:	and	x2, x2, x1
  4f5820:	csetm	x1, ne	// ne = any
  4f5824:	sub	x7, x0, x2
  4f5828:	and	x1, x1, x7
  4f582c:	add	x1, x1, x6
  4f5830:	str	x1, [x2, #24]
  4f5834:	ldp	x6, x1, [x0, #16]
  4f5838:	cmp	x1, #0x1
  4f583c:	add	x1, x4, x1
  4f5840:	csetm	x2, ne	// ne = any
  4f5844:	cmp	x6, #0x1
  4f5848:	and	x2, x2, x1
  4f584c:	csetm	x1, ne	// ne = any
  4f5850:	add	x4, x2, #0x10
  4f5854:	sub	x3, x3, x4
  4f5858:	and	x1, x1, x3
  4f585c:	add	x1, x1, x6
  4f5860:	str	x1, [x2, #16]
  4f5864:	str	q0, [x0, #16]
  4f5868:	b	4f55c4 <bpftime_map_delete_elem_helper+0x1f20>
  4f586c:	ldr	w0, [x19, #40]
  4f5870:	mov	x1, x20
  4f5874:	bl	1b3bdb0 <bpf_map_delete_elem>
  4f5878:	sxtw	x20, w0
  4f587c:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f5880:	ldr	x2, [x25, x3]
  4f5884:	sub	x0, x0, x25
  4f5888:	cmp	x2, #0x1
  4f588c:	csetm	x3, ne	// ne = any
  4f5890:	and	x3, x3, x0
  4f5894:	add	x3, x3, x2
  4f5898:	str	x3, [sp, #248]
  4f589c:	cmp	x3, #0x1
  4f58a0:	b.ne	4f4998 <bpftime_map_delete_elem_helper+0x12f4>  // b.any
  4f58a4:	b	4f3ecc <bpftime_map_delete_elem_helper+0x828>
  4f58a8:	mov	x0, x2
  4f58ac:	mov	x8, x21
  4f58b0:	mov	x2, x23
  4f58b4:	bl	53a320 <_ZN5boost9container6vectorINS_9unordered15node_handle_mapINS2_6detail4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEEjSA_NSC_9allocatorISB_NSC_15segment_managerIcNSC_15rbtree_best_fitINSC_12mutex_familyESE_Lm0EEENSC_10iset_indexEEEEEEENSG_ISO_SM_EEvE37priv_insert_forward_range_no_capacityINS0_3dtl20insert_emplace_proxyISP_JSO_EEEEENS0_12vec_iteratorINSD_ISO_lmLm0EEELb0EEEPSO_mT_NS_11move_detail17integral_constantIjLj2EEE.constprop.0>
  4f58b8:	ldr	x0, [sp, #296]
  4f58bc:	cmp	x0, #0x1
  4f58c0:	b.eq	4f56dc <bpftime_map_delete_elem_helper+0x2038>  // b.none
  4f58c4:	ldr	x2, [sp, #304]
  4f58c8:	add	x0, x23, x0
  4f58cc:	sub	x1, x0, x26
  4f58d0:	add	x21, sp, #0x98
  4f58d4:	cmp	x2, #0x1
  4f58d8:	add	x2, x20, x2
  4f58dc:	csetm	x0, ne	// ne = any
  4f58e0:	sub	x3, x26, x27
  4f58e4:	and	x0, x0, x2
  4f58e8:	cmp	x0, #0x0
  4f58ec:	sub	x0, x0, x21
  4f58f0:	csetm	x2, ne	// ne = any
  4f58f4:	cmp	x1, #0x1
  4f58f8:	sub	x0, x0, #0x1
  4f58fc:	csetm	x20, ne	// ne = any
  4f5900:	and	x0, x2, x0
  4f5904:	and	x20, x20, x3
  4f5908:	add	x0, x0, #0x1
  4f590c:	add	x20, x20, x1
  4f5910:	str	x0, [sp, #152]
  4f5914:	stp	x21, x20, [sp, #320]
  4f5918:	cmp	x20, #0x1
  4f591c:	b.eq	4f56dc <bpftime_map_delete_elem_helper+0x2038>  // b.none
  4f5920:	mov	x0, x24
  4f5924:	mov	x1, x21
  4f5928:	bl	4dccc4 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_EC1ERKSI_>
  4f592c:	sub	x2, x27, x26
  4f5930:	mov	x1, x26
  4f5934:	add	x2, x2, x20
  4f5938:	mov	x0, x21
  4f593c:	str	x2, [sp, #176]
  4f5940:	bl	550b94 <_ZN5boost12interprocess9allocatorINS0_9ipcdetail18sp_counted_impl_pdINS1_IvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIN7bpftime18lru_linklist_entryESB_EEEESB_E10deallocateERKNS7_ISH_lmLm0EEEm.isra.0>
  4f5944:	b	4f56dc <bpftime_map_delete_elem_helper+0x2038>
  4f5948:	sub	x1, x1, #0x1d
  4f594c:	adrp	x2, 2b0c000 <_ZTVN5boost12interprocess9ipcdetail17placement_destroyIN7bpftime15handler_managerEEE+0x28>
  4f5950:	add	x2, x2, #0x3c8
  4f5954:	ldr	x1, [x2, x1, lsl #3]
  4f5958:	blr	x1
  4f595c:	mov	x1, x0
  4f5960:	b	4f494c <bpftime_map_delete_elem_helper+0x12a8>
  4f5964:	bl	48c090 <__errno_location@plt>
  4f5968:	mov	x20, #0xffffffffffffffff    	// #-1
  4f596c:	mov	w1, #0x16                  	// #22
  4f5970:	str	w1, [x0]
  4f5974:	b	4f3700 <bpftime_map_delete_elem_helper+0x5c>
  4f5978:	bl	48c090 <__errno_location@plt>
  4f597c:	mov	x20, #0xffffffffffffffff    	// #-1
  4f5980:	mov	w1, #0x16                  	// #22
  4f5984:	str	w1, [x0]
  4f5988:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f598c:	ldr	x2, [x25, x1]
  4f5990:	sub	x0, x0, x25
  4f5994:	cmp	x2, #0x1
  4f5998:	csetm	x1, ne	// ne = any
  4f599c:	and	x1, x1, x0
  4f59a0:	add	x1, x1, x2
  4f59a4:	str	x1, [sp, #248]
  4f59a8:	cmp	x1, #0x1
  4f59ac:	b.ne	4f5300 <bpftime_map_delete_elem_helper+0x1c5c>  // b.any
  4f59b0:	b	4f4388 <bpftime_map_delete_elem_helper+0xce4>
  4f59b4:	mov	x20, x0
  4f59b8:	mov	x0, x24
  4f59bc:	bl	48c720 <pthread_spin_unlock@plt>
  4f59c0:	mov	x0, x19
  4f59c4:	bl	48c720 <pthread_spin_unlock@plt>
  4f59c8:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f59cc:	ldr	x0, [x0, #1160]
  4f59d0:	ldr	x2, [sp, #344]
  4f59d4:	ldr	x1, [x0]
  4f59d8:	subs	x2, x2, x1
  4f59dc:	mov	x1, #0x0                   	// #0
  4f59e0:	b.eq	4f5b90 <bpftime_map_delete_elem_helper+0x24ec>  // b.none
  4f59e4:	bl	48d490 <__stack_chk_fail@plt>
  4f59e8:	bl	506894 <_ZN6spdlog18default_logger_rawEv>
  4f59ec:	mov	w3, #0x20c                 	// #524
  4f59f0:	adrp	x2, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f59f4:	add	x2, x2, #0xc88
  4f59f8:	str	x2, [sp, #96]
  4f59fc:	str	w3, [sp, #104]
  4f5a00:	add	x1, sp, #0x30
  4f5a04:	adrp	x6, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f5a08:	add	x6, x6, #0xd78
  4f5a0c:	ldp	x8, x9, [sp, #96]
  4f5a10:	stp	x8, x9, [sp, #48]
  4f5a14:	adrp	x2, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f5a18:	str	x6, [sp, #112]
  4f5a1c:	add	x5, x22, #0x10
  4f5a20:	str	x6, [x1, #16]
  4f5a24:	mov	x4, x20
  4f5a28:	add	x2, x2, #0xe50
  4f5a2c:	mov	x3, #0x40                  	// #64
  4f5a30:	bl	53c8b0 <_ZN6spdlog6logger4log_IJRjS2_EEEvNS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEEDpOT_.constprop.0>
  4f5a34:	bl	48c090 <__errno_location@plt>
  4f5a38:	mov	x20, #0xffffffffffffffff    	// #-1
  4f5a3c:	mov	w1, #0x16                  	// #22
  4f5a40:	str	w1, [x0]
  4f5a44:	b	4f3d98 <bpftime_map_delete_elem_helper+0x6f4>
  4f5a48:	bl	506894 <_ZN6spdlog18default_logger_rawEv>
  4f5a4c:	mov	w3, #0x49                  	// #73
  4f5a50:	adrp	x2, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f5a54:	add	x2, x2, #0xb30
  4f5a58:	str	x2, [sp, #96]
  4f5a5c:	str	w3, [sp, #104]
  4f5a60:	add	x1, sp, #0x30
  4f5a64:	adrp	x5, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f5a68:	add	x5, x5, #0xd78
  4f5a6c:	ldp	x6, x7, [sp, #96]
  4f5a70:	stp	x6, x7, [sp, #48]
  4f5a74:	adrp	x3, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f5a78:	str	x5, [sp, #112]
  4f5a7c:	add	x3, x3, #0xb98
  4f5a80:	str	x5, [x1, #16]
  4f5a84:	mov	x4, #0x30                  	// #48
  4f5a88:	mov	w2, #0x4                   	// #4
  4f5a8c:	bl	4b40c0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  4f5a90:	bl	48c090 <__errno_location@plt>
  4f5a94:	mov	x20, #0xffffffffffffffff    	// #-1
  4f5a98:	mov	w1, #0x16                  	// #22
  4f5a9c:	str	w1, [x0]
  4f5aa0:	b	4f3b50 <bpftime_map_delete_elem_helper+0x4ac>
  4f5aa4:	bl	506894 <_ZN6spdlog18default_logger_rawEv>
  4f5aa8:	mov	w3, #0x202                 	// #514
  4f5aac:	adrp	x2, 1d36000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xe838>
  4f5ab0:	add	x2, x2, #0xc88
  4f5ab4:	str	x2, [sp, #96]
  4f5ab8:	str	w3, [sp, #104]
  4f5abc:	add	x1, sp, #0x30
  4f5ac0:	adrp	x5, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f5ac4:	add	x5, x5, #0xd78
  4f5ac8:	ldp	x6, x7, [sp, #96]
  4f5acc:	stp	x6, x7, [sp, #48]
  4f5ad0:	adrp	x3, 1d37000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf838>
  4f5ad4:	str	x5, [sp, #112]
  4f5ad8:	add	x3, x3, #0xe18
  4f5adc:	str	x5, [x1, #16]
  4f5ae0:	mov	x4, #0x33                  	// #51
  4f5ae4:	mov	w2, #0x4                   	// #4
  4f5ae8:	bl	4b40c0 <_ZN6spdlog6logger3logENS_10source_locENS_5level10level_enumEN3fmt3v1117basic_string_viewIcEE>
  4f5aec:	b	4f5a34 <bpftime_map_delete_elem_helper+0x2390>
  4f5af0:	mov	x19, x0
  4f5af4:	mov	x0, x23
  4f5af8:	bl	4e1d90 <_ZN5boost9unordered15node_handle_mapINS0_6detail4nodeISt4pairIKjN7bpftime21per_cpu_hash_map_impl16uint32_value_refEENS_12interprocess10offset_ptrIvlmLm0EEEEEjS8_NSA_9allocatorIS9_NSA_15segment_managerIcNSA_15rbtree_best_fitINSA_12mutex_familyESC_Lm0EEENSA_10iset_indexEEEEEED1Ev>
  4f5afc:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f5b00:	ldr	x0, [x0, #1160]
  4f5b04:	ldr	x2, [sp, #344]
  4f5b08:	ldr	x1, [x0]
  4f5b0c:	subs	x2, x2, x1
  4f5b10:	mov	x1, #0x0                   	// #0
  4f5b14:	b.ne	4f59e4 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f5b18:	mov	x0, x19
  4f5b1c:	bl	48c590 <_Unwind_Resume@plt>
  4f5b20:	adrp	x1, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f5b24:	ldr	x1, [x1, #1160]
  4f5b28:	ldr	x3, [sp, #344]
  4f5b2c:	ldr	x2, [x1]
  4f5b30:	subs	x3, x3, x2
  4f5b34:	mov	x2, #0x0                   	// #0
  4f5b38:	b.ne	4f59e4 <bpftime_map_delete_elem_helper+0x2340>  // b.any
  4f5b3c:	cmp	w0, #0xff
  4f5b40:	cset	w0, eq	// eq = none
  4f5b44:	bl	48da54 <_ZSt26__throw_bad_variant_accessb>
  4f5b48:	mov	x1, x0
  4f5b4c:	mov	x0, x19
  4f5b50:	mov	x19, x1
  4f5b54:	bl	48c720 <pthread_spin_unlock@plt>
  4f5b58:	adrp	x0, 2e4f000 <_GLOBAL_OFFSET_TABLE_+0x3468>
  4f5b5c:	ldr	x0, [x0, #1160]
  4f5b60:	ldr	x2, [sp, #344]
  4f5b64:	ldr	x1, [x0]
  4f5b68:	subs	x2, x2, x1
  4f5b6c:	mov	x1, #0x0                   	// #0
  4f5b70:	b.eq	4f5b18 <bpftime_map_delete_elem_helper+0x2474>  // b.none
  4f5b74:	b	4f59e4 <bpftime_map_delete_elem_helper+0x2340>
  4f5b78:	mov	x20, x0
  4f5b7c:	b	4f59c0 <bpftime_map_delete_elem_helper+0x231c>
  4f5b80:	mov	x20, x0
  4f5b84:	mov	x0, x21
  4f5b88:	bl	4df5c0 <_ZN5boost12interprocess9ipcdetail12shared_countIN7bpftime18lru_linklist_entryENS0_9allocatorIvNS0_15segment_managerIcNS0_15rbtree_best_fitINS0_12mutex_familyENS0_10offset_ptrIvlmLm0EEELm0EEENS0_10iset_indexEEEEENS0_7deleterIS4_SD_EEED1Ev>
  4f5b8c:	b	4f59c0 <bpftime_map_delete_elem_helper+0x231c>
  4f5b90:	mov	x0, x20
  4f5b94:	bl	48c590 <_Unwind_Resume@plt>

Disassembly of section .fini:
