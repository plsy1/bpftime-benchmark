#include "spdlog/spdlog.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <fstream>
#include <sys/mman.h>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include <string>
#include <cinttypes>
#if defined(__aarch64__)
#include <elf.h>
#endif
#if defined(__x86_64__)
#include <cpuid.h>
#endif
#include "text_segment_transformer.hpp"
#include <frida-gum.h>
/*
Function arguments are passed using the following order:
- RDI
- RSI
- RDX
- RCX
- R8
- R9
- ...stack

- RAX: return value

While syscall args are passed using
- RAX:  syscall_nr
- RDI:  arg1
- RSI:  arg2
- RDX:  arg3
- R10:  arg4
- R8:   arg5
- R9:   arg6

- RAX: return value

*/

extern "C" void syscall_hooker_asm();
extern "C" int64_t call_orig_syscall(int64_t sys_nr, int64_t arg1, int64_t arg2,
				     int64_t arg3, int64_t arg4, int64_t arg5,
				     int64_t arg6);
extern "C" void syscall_addr();

static const int NR_syscalls = 512;
static syscall_hooker_func_t call_hook = &call_orig_syscall;

#if defined(__x86_64__)
// XSAVE components the hook preserves: x87, SSE, AVX, opmask, ZMM_Hi256 and
// Hi16_ZMM (bits 0-2, 5-7). The mask is ANDed with XCR0; AMX tiles are left
// out, as glibc's lazy-binding trampoline does.
#define BPFTIME_XSAVE_MASK 0xe7
#define BPFTIME_XSAVE_MASK_STR "0xe7"
// Bytes the hook reserves for XSAVE, set by setup_syscall_tracer from CPUID
// before any site is rewritten. Defined in the asm below: only the asm reads
// it, and GCC's LTO removed a C++ definition it saw no read of (hidden: the
// asm addresses it rip-relative).
extern "C" __attribute__((visibility("hidden"))) uint64_t
	bpftime_xsave_area_size;
// File scope: LTO dropped asm inside an unreferenced function (see aarch64).
__asm__(".pushsection .data\n\t"
	".p2align 3\n\t"
	".globl bpftime_xsave_area_size\n\t"
	".hidden bpftime_xsave_area_size\n\t"
	"bpftime_xsave_area_size:\n\t"
	".quad 0\n\t"
	".popsection\n\t"
	".pushsection .text\n\t"
	".globl syscall_hooker_asm\n\t"
		"syscall_hooker_asm:\n\t"
		"pop %rax\n\t" // Restore the saved rax, which is the syscall
			       // number
		"cmp $15, %rax\n\t" // Special handing for rt_sigreturn, they
				    // don't need to be traced
		"je handle_sigreturn\n\t"
		// clone and clone3 return TWICE. The child resumes at the `ret`
		// of call_orig_syscall, but on its own fresh stack, where no
		// return address was ever pushed -- it pops whatever the thread
		// stack happens to hold and jumps there. They therefore cannot
		// be traced through the C hook at all; send them down the
		// untraced path, after seeding the child stack so that the
		// child's `ret` lands where the replaced `syscall` instruction
		// would have continued.
		"cmp $56, %rax\n\t" // SYS_clone
		"je handle_clone\n\t"
		"cmp $435, %rax\n\t" // SYS_clone3
		"je handle_clone3\n\t"
		// The kernel changes only rax, rcx and r11 across `syscall`;
		// code after an inline system call keeps using the argument
		// registers and vector registers (glibc's __libc_sigaction
		// reads its oact pointer from r8 afterwards). Save and restore
		// all of them around the C hook, below the caller's 128-byte
		// red zone: the rewritten `call *%rax` already stored its
		// return address at its top, nothing else of it may change.
		"lea -120(%rsp), %rsp\n\t"
		"pushq %rbp\n\t"
		"movq %rsp, %rbp\n\t"
		"subq $64, %rsp\n\t"
		"movq %rax, -8(%rbp)\n\t" // syscall number
		"movq %rdi, -16(%rbp)\n\t"
		"movq %rsi, -24(%rbp)\n\t"
		"movq %rdx, -32(%rbp)\n\t"
		"movq %r10, -40(%rbp)\n\t"
		"movq %r8, -48(%rbp)\n\t"
		"movq %r9, -56(%rbp)\n\t"
		// Vector, opmask and x87 state: XSAVE into a 64-byte aligned
		// area sized at setup. The header must be zero for XRSTOR.
		"subq bpftime_xsave_area_size(%rip), %rsp\n\t"
		"andq $-64, %rsp\n\t"
		"xorl %eax, %eax\n\t"
		"movq %rax, 512(%rsp)\n\t"
		"movq %rax, 520(%rsp)\n\t"
		"movq %rax, 528(%rsp)\n\t"
		"movq %rax, 536(%rsp)\n\t"
		"movq %rax, 544(%rsp)\n\t"
		"movq %rax, 552(%rsp)\n\t"
		"movq %rax, 560(%rsp)\n\t"
		"movq %rax, 568(%rsp)\n\t"
		"movl $" BPFTIME_XSAVE_MASK_STR ", %eax\n\t"
		"xorl %edx, %edx\n\t"
		"xsave (%rsp)\n\t"
		// syscall_hooker_cxx(nr, arg1, ..., arg6), arg6 on the stack
		"movq -8(%rbp), %rdi\n\t"
		"movq -16(%rbp), %rsi\n\t"
		"movq -24(%rbp), %rdx\n\t"
		"movq -32(%rbp), %rcx\n\t"
		"movq -40(%rbp), %r8\n\t"
		"movq -48(%rbp), %r9\n\t"
		"subq $8, %rsp\n\t" // 16-byte alignment at the call
		"pushq -56(%rbp)\n\t"
		"call syscall_hooker_cxx\n\t"
		"addq $16, %rsp\n\t"
		"movq %rax, %r11\n\t" // the result; `syscall` clobbers r11
		"movl $" BPFTIME_XSAVE_MASK_STR ", %eax\n\t"
		"xorl %edx, %edx\n\t"
		"xrstor (%rsp)\n\t"
		"movq %r11, %rax\n\t"
		"movq -16(%rbp), %rdi\n\t"
		"movq -24(%rbp), %rsi\n\t"
		"movq -32(%rbp), %rdx\n\t"
		"movq -40(%rbp), %r10\n\t"
		"movq -48(%rbp), %r8\n\t"
		"movq -56(%rbp), %r9\n\t"
		"leave\n\t"
		"lea 120(%rsp), %rsp\n\t"
		"ret\n\t"
	".globl call_orig_syscall\n\t"
		"call_orig_syscall:\n\t"
		"movq %rdi, %rax \n\t"
		"movq %rsi, %rdi \n\t"
		"movq %rdx, %rsi \n\t"
		"movq %rcx, %rdx \n\t"
		"movq %r8, %r10 \n\t"
		"movq %r9, %r8 \n\t"
		"movq 8(%rsp),%r9 \n\t"
		"handle_sigreturn:\n\t"
		// "addq $8, %rsp\n\t"
		"syscall_addr:\n\t"
		"syscall\n\t"
		"ret\n\t"
		// Reached only by an explicit jump from the dispatcher, never
		// by fall-through: call_orig_syscall above must continue
		// straight into `syscall`. Registers are still in syscall ABI
		// order and the application's return address is at (%rsp),
		// because only the pushed syscall number has been popped.
		// %rcx and %r11 are free scratch: the `syscall` destroys both.
		"handle_clone:\n\t"
		"testq %rsi, %rsi\n\t" // no child stack: fork-like, the child
					// keeps the parent's stack layout and
					// the ordinary `ret` is already correct
		"jz handle_sigreturn\n\t"
		"movq (%rsp), %r11\n\t" // application return address
		"subq $8, %rsi\n\t"
		"movq %r11, (%rsi)\n\t" // the child pops this in `ret`
		"jmp handle_sigreturn\n\t"
		"handle_clone3:\n\t"
		"movq 40(%rdi), %rcx\n\t" // clone_args.stack
		"testq %rcx, %rcx\n\t"
		"jz handle_sigreturn\n\t"
		"cmpq $8, 48(%rdi)\n\t" // clone_args.stack_size
		"jb handle_sigreturn\n\t"
		"addq 48(%rdi), %rcx\n\t" // top of the child stack
		"movq (%rsp), %r11\n\t"
		"subq $8, %rcx\n\t"
		"movq %r11, (%rcx)\n\t"
		"subq $8, 48(%rdi)\n\t" // the kernel starts the child one
					 // slot lower, on the seeded address
		"jmp handle_sigreturn\n\t"
	".popsection\n\t");
#elif defined(__aarch64__)
/*
Every `svc #imm` found in executable code is replaced by `b gate`, where the
gate is a per-site stub in a trampoline region allocated within the +-128 MiB
reach of the branch (the scheme of svc-hook, Middleware 2025, and ASC-Hook;
zpoline's page-zero trick needs an unaligned `call *%rax`, which aarch64 does
not have). A region starts with a common jump to syscall_hooker_asm, followed
by one 32-byte gate per site:

  +0   stp x16, x17, [sp, #-16]!   save the two registers the gate uses
  +4   adr x16, gate               identify the site
  +8   b   region                  -> ldr x17, =syscall_hooker_asm; br x17
  +12  ldp x16, x17, [sp], #16     return path: everything else restored
  +16  b   site + 4
  +20  ldp x16, x17, [sp], #16     direct path: every register as at the svc
  +24  svc #0
  +28  b   site + 4

The kernel preserves every register except x0 across a system call, so the
hook saves x0-x30 (x16 holds the gate), NZCV, FPSR and q0-q31 on the
application stack (aarch64 Linux has no red zone) and restores them all; x0
becomes the result. rt_sigreturn, clone and clone3 take the direct path: the
system call is issued from the gate with the application's registers and sp,
so a child resuming on a fresh stack returns to the site by a plain branch,
without any frame of the C hook.
*/
static constexpr uint32_t GATE_SIZE = 32;
static constexpr uint32_t GATE_RETURN_OFFSET = 12;
static constexpr uint32_t GATE_DIRECT_OFFSET = 20;
static constexpr uint32_t REGION_HEADER_SIZE = 16;
static_assert(GATE_RETURN_OFFSET == 12 && GATE_DIRECT_OFFSET == 20,
	      "syscall_hooker_asm adds these offsets as immediates");

// File-scope asm: LTO drops an unreferenced holder function in executables.
__asm__(".pushsection .text\n\t"
	".p2align 2\n\t"
	".globl syscall_hooker_asm\n\t"
		".type syscall_hooker_asm, %function\n\t"
		"syscall_hooker_asm:\n\t"
		"hint #36\n\t" // bti j: entered by `br x17`
		"sub sp, sp, #768\n\t"
		"stp x0, x1, [sp, #0]\n\t"
		"stp x2, x3, [sp, #16]\n\t"
		"stp x4, x5, [sp, #32]\n\t"
		"stp x6, x7, [sp, #48]\n\t"
		"stp x8, x9, [sp, #64]\n\t"
		"stp x10, x11, [sp, #80]\n\t"
		"stp x12, x13, [sp, #96]\n\t"
		"stp x14, x15, [sp, #112]\n\t"
		"stp x16, x18, [sp, #128]\n\t" // x16: the gate
		"stp x19, x20, [sp, #144]\n\t"
		"stp x21, x22, [sp, #160]\n\t"
		"stp x23, x24, [sp, #176]\n\t"
		"stp x25, x26, [sp, #192]\n\t"
		"stp x27, x28, [sp, #208]\n\t"
		"stp x29, x30, [sp, #224]\n\t"
		"mrs x9, nzcv\n\t"
		"mrs x10, fpsr\n\t"
		"stp x9, x10, [sp, #240]\n\t"
		"add x9, sp, #256\n\t"
		"stp q0, q1, [x9, #0]\n\t"
		"stp q2, q3, [x9, #32]\n\t"
		"stp q4, q5, [x9, #64]\n\t"
		"stp q6, q7, [x9, #96]\n\t"
		"stp q8, q9, [x9, #128]\n\t"
		"stp q10, q11, [x9, #160]\n\t"
		"stp q12, q13, [x9, #192]\n\t"
		"stp q14, q15, [x9, #224]\n\t"
		"stp q16, q17, [x9, #256]\n\t"
		"stp q18, q19, [x9, #288]\n\t"
		"stp q20, q21, [x9, #320]\n\t"
		"stp q22, q23, [x9, #352]\n\t"
		"stp q24, q25, [x9, #384]\n\t"
		"stp q26, q27, [x9, #416]\n\t"
		"stp q28, q29, [x9, #448]\n\t"
		"stp q30, q31, [x9, #480]\n\t"
		"cmp x8, #139\n\t" // rt_sigreturn
		"b.eq 2f\n\t"
		"cmp x8, #220\n\t" // clone
		"b.eq 2f\n\t"
		"cmp x8, #435\n\t" // clone3
		"b.eq 2f\n\t"
		"mov x6, x5\n\t"
		"mov x5, x4\n\t"
		"mov x4, x3\n\t"
		"mov x3, x2\n\t"
		"mov x2, x1\n\t"
		"mov x1, x0\n\t"
		"mov x0, x8\n\t"
		"bl syscall_hooker_cxx\n\t"
		"str x0, [sp, #0]\n\t"
		"mov x17, #12\n\t" // GATE_RETURN_OFFSET
		"b 3f\n\t"
		"2:\n\t"
		"mov x17, #20\n\t" // GATE_DIRECT_OFFSET
		"3:\n\t"
		"add x9, sp, #256\n\t"
		"ldp q0, q1, [x9, #0]\n\t"
		"ldp q2, q3, [x9, #32]\n\t"
		"ldp q4, q5, [x9, #64]\n\t"
		"ldp q6, q7, [x9, #96]\n\t"
		"ldp q8, q9, [x9, #128]\n\t"
		"ldp q10, q11, [x9, #160]\n\t"
		"ldp q12, q13, [x9, #192]\n\t"
		"ldp q14, q15, [x9, #224]\n\t"
		"ldp q16, q17, [x9, #256]\n\t"
		"ldp q18, q19, [x9, #288]\n\t"
		"ldp q20, q21, [x9, #320]\n\t"
		"ldp q22, q23, [x9, #352]\n\t"
		"ldp q24, q25, [x9, #384]\n\t"
		"ldp q26, q27, [x9, #416]\n\t"
		"ldp q28, q29, [x9, #448]\n\t"
		"ldp q30, q31, [x9, #480]\n\t"
		"ldp x9, x10, [sp, #240]\n\t"
		"msr nzcv, x9\n\t"
		"msr fpsr, x10\n\t"
		"ldp x0, x1, [sp, #0]\n\t"
		"ldp x2, x3, [sp, #16]\n\t"
		"ldp x4, x5, [sp, #32]\n\t"
		"ldp x6, x7, [sp, #48]\n\t"
		"ldp x8, x9, [sp, #64]\n\t"
		"ldp x10, x11, [sp, #80]\n\t"
		"ldp x12, x13, [sp, #96]\n\t"
		"ldp x14, x15, [sp, #112]\n\t"
		"ldp x16, x18, [sp, #128]\n\t"
		"ldp x19, x20, [sp, #144]\n\t"
		"ldp x21, x22, [sp, #160]\n\t"
		"ldp x23, x24, [sp, #176]\n\t"
		"ldp x25, x26, [sp, #192]\n\t"
		"ldp x27, x28, [sp, #208]\n\t"
		"ldp x29, x30, [sp, #224]\n\t"
		"add x16, x16, x17\n\t"
		"add sp, sp, #768\n\t"
		"br x16\n\t"
		".size syscall_hooker_asm, .-syscall_hooker_asm\n\t"
	".popsection\n\t");

__asm__(".pushsection .text\n\t"
	".p2align 2\n\t"
	".globl call_orig_syscall\n\t"
		".type call_orig_syscall, %function\n\t"
		"call_orig_syscall:\n\t"
		"hint #34\n\t" // bti c: the default hook, called through a pointer
		"mov x8, x0\n\t"
		"mov x0, x1\n\t"
		"mov x1, x2\n\t"
		"mov x2, x3\n\t"
		"mov x3, x4\n\t"
		"mov x4, x5\n\t"
		"mov x5, x6\n\t"
		".globl syscall_addr\n\t"
		"syscall_addr:\n\t"
		"svc #0\n\t"
		"ret\n\t"
		".size call_orig_syscall, .-call_orig_syscall\n\t"
	".popsection\n\t");
#else
#error "Unsupported architecture"
#endif

// Called only from the assembly hook: keep it through LTO.
extern "C" __attribute__((used)) int64_t syscall_hooker_cxx(int64_t sys_nr, int64_t arg1,
				      int64_t arg2, int64_t arg3, int64_t arg4,
				      int64_t arg5, int64_t arg6)
{
	return call_hook(sys_nr, arg1, arg2, arg3, arg4, arg5, arg6);
}

#if defined(__x86_64__)
static inline void rewrite_segment(uint8_t *code, size_t len, int perm)
{
	// Set the pages to be writable
	if (int err = mprotect(code, len, PROT_READ | PROT_WRITE | PROT_EXEC);
	    err < 0) {
		SPDLOG_ERROR(
			"Failed to change the protect status of the rewriting page {:x}",
			(uintptr_t)code);
		exit(1);
	}
	csh cs_handle;
	cs_err ret;
	ret = cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle);
	if (ret != CS_ERR_OK) {
		SPDLOG_ERROR("Failed to open capstone instance: {}, {}",
			      (int)ret, cs_strerror(ret));
		exit(1);
	}
	const uint8_t *curr_code = code;
	size_t size = len;
	uint64_t curr_addr = (uint64_t)(uintptr_t)curr_code;
	cs_insn curr_insn;
	memset(&curr_insn, 0, sizeof(curr_insn));
	while (curr_addr < (uintptr_t)code + len) {
		auto ok = cs_disasm_iter(cs_handle, &curr_code, &size,
					 &curr_addr, &curr_insn);
		if (!ok) {
			break;
		}
		auto insn_name =
			std::string(cs_insn_name(cs_handle, curr_insn.id));
		if (insn_name == "syscall" || insn_name == "sysenter") {
			if (curr_insn.address != (uintptr_t)&syscall_addr) {
				uint8_t *curr_pos =
					(uint8_t *)(uintptr_t)curr_insn.address;
				SPDLOG_TRACE("Rewrite syscall insn at {}",
					      (void *)curr_pos);
				curr_pos[0] = 0xff;
				curr_pos[1] = 0xd0;
			}
		}
	}
	cs_close(&cs_handle);
	if (int err = mprotect(code, len, perm); err < 0) {
		SPDLOG_ERROR(
			"Failed to change the protect status of the rewriting page {:x}",
			(uintptr_t)code);
		exit(1);
	}
}

#endif

struct MapEntry {
	uint64_t begin, end;
	char w, r, x;
	int get_perm() const
	{
		int ret = 0;
		if (w == 'w')
			ret |= PROT_WRITE;
		if (r == 'r')
			ret |= PROT_READ;
		if (x == 'x')
			ret |= PROT_EXEC;
		return ret;
	}
};
namespace bpftime
{

syscall_hooker_func_t get_call_hook()
{
	return call_hook;
}
void set_call_hook(syscall_hooker_func_t hook)
{
	call_hook = hook;
}

#if defined(__x86_64__)
// Size of the XSAVE area for BPFTIME_XSAVE_MASK (standard format: legacy
// region, header, then each component at its CPUID offset), or 0 without
// OS-enabled XSAVE.
static uint64_t xsave_area_size()
{
	unsigned eax, ebx, ecx, edx;
	if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx) || !(ecx & bit_OSXSAVE))
		return 0;
	uint64_t end = 512 + 64;
	for (unsigned i = 2; i < 8; i++) {
		if (!((BPFTIME_XSAVE_MASK >> i) & 1))
			continue;
		__cpuid_count(0xd, i, eax, ebx, ecx, edx);
		if (eax != 0)
			end = std::max<uint64_t>(end, (uint64_t)ebx + eax);
	}
	return (end + 63) & ~(uint64_t)63;
}

void setup_syscall_tracer()
{
	bpftime_xsave_area_size = xsave_area_size();
	if (bpftime_xsave_area_size == 0) {
		SPDLOG_ERROR(
			"No OS-enabled XSAVE: the system call hook could not preserve vector registers; not rewriting");
		return;
	}
	// Setup page mappings

	if (auto mmap_addr =
		    mmap(0x0, 0x1000, PROT_EXEC | PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
	    mmap_addr == MAP_FAILED) {
		SPDLOG_ERROR("Failed to perform mmap: errno={}, message={}",
			      errno, strerror(errno));
		exit(1);
	}
	// Setup jumpings
	for (int i = 0; i < NR_syscalls; i++) {
		// 0x90; nop
		*((char *)(uintptr_t)(i)) = 0x90;
	}
	// Jump to the syscall handler function after the nop-s
	/*
	50
	push %rax;

	48 b8 88 77 66 55 44 33 22 11
	movabs $0x1122334455667788, %rax; // The constant is the address
	of syscall_hooker_asm

	ff e0
	jmp *%rax;

	*/
	std::vector<uint8_t> codes;
	codes.push_back(0x50);
	codes.push_back(0x48);
	codes.push_back(0xb8);
	for (int i = 0; i < 8; i++) {
		codes.push_back(
			(uint8_t)((((uint64_t)(uintptr_t)syscall_hooker_asm) >>
				   (8 * i)) &
				  0xff));
	}
	codes.push_back(0xff);
	codes.push_back(0xe0);
	std::copy(codes.begin(), codes.end(),
		  (uint8_t *)(uintptr_t)(0 + NR_syscalls));
	// Set the page to execute-only. Keep normal behavior of
	// dereferencing null-pointers
	if (int err = mprotect(0, 0x1000, PROT_EXEC); err < 0) {
		SPDLOG_ERROR(
			"Failed to set execute only of 0-started page: {}",
			errno);
		exit(1);
	}

	SPDLOG_INFO("Page zero setted up..");
	// Scan for /proc/self/maps

	std::vector<MapEntry> entries;
	std::ifstream ifs("/proc/self/maps");
	while (ifs) {
		std::string line;
		std::getline(ifs, line);

		MapEntry curr;
		char *path_buf;
		int cnt = sscanf(line.c_str(),
				 "%" SCNx64 "-%" SCNx64
				 " %c%c%c%*c %*x %*x:%*x %*d %ms",
				 &curr.begin, &curr.end, &curr.r, &curr.w,
				 &curr.x, &path_buf);
		if (cnt < 5)
			continue;
		if (cnt == 6) {
			std::string buf = path_buf;
			free(path_buf);
			if (buf == "[stack]" || buf == "[vsyscall]") {
				continue;
			}
		}

		entries.push_back(curr);
	}
	SPDLOG_INFO("Rewriting executable segments..");
	// Hack the executable mappings
	for (const auto &map : entries) {
		if (map.x == 'x') {
			if (map.begin == 0) {
				// Skip pages that we mapped
				continue;
			}
			SPDLOG_DEBUG("Rewriting segment from {:x} to {:x}",
				      map.begin, map.end);
			rewrite_segment((uint8_t *)(uintptr_t)(map.begin),
					map.end - map.begin, map.get_perm());
		}
	}
}
#elif defined(__aarch64__)
namespace
{
struct ExecMap {
	uint64_t begin, end, offset;
	int prot;
	std::string path;
};

std::vector<ExecMap> read_maps()
{
	std::vector<ExecMap> result;
	std::ifstream ifs("/proc/self/maps");
	std::string line;
	while (std::getline(ifs, line)) {
		ExecMap map;
		char perms[5] = {};
		int path_pos = 0;
		if (sscanf(line.c_str(),
			   "%" SCNx64 "-%" SCNx64 " %4s %" SCNx64
			   " %*s %*d %n",
			   &map.begin, &map.end, perms, &map.offset,
			   &path_pos) < 4)
			continue;
		map.prot = (perms[0] == 'r' ? PROT_READ : 0) |
			   (perms[1] == 'w' ? PROT_WRITE : 0) |
			   (perms[2] == 'x' ? PROT_EXEC : 0);
		if (path_pos > 0 && (size_t)path_pos < line.size())
			map.path = line.substr(path_pos);
		result.push_back(std::move(map));
	}
	return result;
}

bool is_svc(uint32_t insn)
{
	return (insn & 0xffe0001fu) == 0xd4000001u;
}

uint32_t encode_b(uint64_t from, uint64_t to)
{
	return 0x14000000u | (uint32_t)(((int64_t)(to - from) >> 2) & 0x3ffffff);
}

bool b_reaches(uint64_t from, uint64_t to)
{
	int64_t off = (int64_t)(to - from);
	return off >= -0x8000000ll && off <= 0x7fffffcll;
}

// File ranges of the executable sections (SHF_EXECINSTR) of an ELF file, so
// that constants in a read-only section sharing the executable segment are
// never taken for instructions. Empty when the file has no section headers.
std::vector<std::pair<uint64_t, uint64_t> > exec_sections(const std::string &path)
{
	std::vector<std::pair<uint64_t, uint64_t> > result;
	int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return result;
	Elf64_Ehdr ehdr;
	if (pread(fd, &ehdr, sizeof(ehdr), 0) == (ssize_t)sizeof(ehdr) &&
	    memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0 &&
	    ehdr.e_ident[EI_CLASS] == ELFCLASS64 &&
	    ehdr.e_shentsize == sizeof(Elf64_Shdr) && ehdr.e_shnum > 0) {
		std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
		size_t bytes = shdrs.size() * sizeof(Elf64_Shdr);
		if (pread(fd, shdrs.data(), bytes, ehdr.e_shoff) ==
		    (ssize_t)bytes) {
			for (const auto &sh : shdrs) {
				if (sh.sh_type == SHT_PROGBITS &&
				    (sh.sh_flags & SHF_EXECINSTR))
					result.emplace_back(sh.sh_offset,
							    sh.sh_offset +
								    sh.sh_size);
			}
		}
	}
	close(fd);
	return result;
}

// Addresses of the svc instructions of one executable mapping.
std::vector<uint64_t> find_svcs(const ExecMap &map)
{
	std::vector<std::pair<uint64_t, uint64_t> > ranges;
	if (!map.path.empty() && map.path[0] == '/') {
		auto sections = exec_sections(map.path);
		for (auto [lo, hi] : sections) {
			uint64_t file_end = map.offset + (map.end - map.begin);
			lo = std::max(lo, map.offset);
			hi = std::min(hi, file_end);
			if (lo < hi)
				ranges.emplace_back(map.begin + (lo - map.offset),
						    map.begin + (hi - map.offset));
		}
		if (ranges.empty() && !sections.empty())
			return {}; // no executable section in this mapping
	}
	if (ranges.empty())
		ranges.emplace_back(map.begin, map.end);
	bool unreadable = !(map.prot & PROT_READ);
	if (unreadable &&
	    mprotect((void *)map.begin, map.end - map.begin,
		     PROT_READ | PROT_EXEC) < 0)
		return {};
	std::vector<uint64_t> result;
	for (auto [lo, hi] : ranges) {
		for (uint64_t addr = (lo + 3) & ~3ull; addr + 4 <= hi;
		     addr += 4) {
			if (is_svc(*(const uint32_t *)addr) &&
			    addr != (uint64_t)&syscall_addr)
				result.push_back(addr);
		}
	}
	if (unreadable)
		mprotect((void *)map.begin, map.end - map.begin, map.prot);
	std::sort(result.begin(), result.end());
	return result;
}

// Map `size` bytes so that every address in it reaches every address in
// [lo, hi] with a `b` (with margin), without replacing an existing mapping.
void *map_near(uint64_t lo, uint64_t hi, size_t size)
{
	const uint64_t reach = 0x7000000; // 112 MiB, below the 128 MiB of `b`
	const uint64_t page = (uint64_t)sysconf(_SC_PAGESIZE);
	uint64_t win_lo = hi > reach ? ((hi - reach + page - 1) & ~(page - 1)) :
				       page;
	uint64_t win_hi = lo + reach - size;
	auto maps = read_maps();
	std::sort(maps.begin(), maps.end(),
		  [](auto &a, auto &b) { return a.begin < b.begin; });
	// Gaps between mappings, closest to the code first.
	std::vector<std::pair<uint64_t, uint64_t> > gaps;
	uint64_t prev_end = page;
	for (const auto &m : maps) {
		if (m.begin > prev_end)
			gaps.emplace_back(prev_end, m.begin);
		prev_end = std::max(prev_end, m.end);
	}
	std::vector<uint64_t> candidates;
	for (auto [g_lo, g_hi] : gaps) {
		uint64_t a = std::max(g_lo, win_lo);
		uint64_t b_end = std::min(g_hi, win_hi + size);
		if (a + size > b_end)
			continue;
		// Lowest fitting address at or above the code, else highest below.
		if (a >= lo)
			candidates.push_back(a);
		else
			candidates.push_back(std::min(b_end - size, lo) &
					     ~(page - 1));
	}
	std::sort(candidates.begin(), candidates.end(),
		  [&](uint64_t a, uint64_t b) {
			  auto dist = [&](uint64_t x) {
				  return x >= lo ? x - lo : lo - x;
			  };
			  return dist(a) < dist(b);
		  });
	for (uint64_t addr : candidates) {
		if (addr < win_lo || addr > win_hi)
			continue;
		void *ptr = mmap((void *)addr, size, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS |
					 MAP_FIXED_NOREPLACE,
				 -1, 0);
		if (ptr == MAP_FAILED)
			continue;
		if ((uint64_t)ptr == addr)
			return ptr;
		munmap(ptr, size);
	}
	return nullptr;
}

// Builds the region for `sites` (all within reach of one region) and
// rewrites them. Returns the number of rewritten sites.
size_t install_region(const std::vector<uint64_t> &sites, int code_prot)
{
	const uint64_t page = (uint64_t)sysconf(_SC_PAGESIZE);
	size_t size = (REGION_HEADER_SIZE + GATE_SIZE * sites.size() +
		       page - 1) &
		      ~(page - 1);
	auto *region = (uint32_t *)map_near(sites.front(), sites.back(), size);
	if (region == nullptr) {
		SPDLOG_WARN(
			"No trampoline space within reach of {:x}-{:x}; {} svc instructions are not traced",
			sites.front(), sites.back(), sites.size());
		return 0;
	}
	const uint64_t base = (uint64_t)region;
	region[0] = 0x58000051u; // ldr x17, #8
	region[1] = 0xd61f0220u; // br x17
	uint64_t hook = (uint64_t)&syscall_hooker_asm;
	memcpy(&region[2], &hook, sizeof(hook));
	for (size_t i = 0; i < sites.size(); i++) {
		uint64_t gate = base + REGION_HEADER_SIZE + GATE_SIZE * i;
		uint32_t *g = (uint32_t *)gate;
		g[0] = 0xa9bf47f0u; // stp x16, x17, [sp, #-16]!
		g[1] = 0x10fffff0u; // adr x16, gate (pc - 4)
		g[2] = encode_b(gate + 8, base);
		g[3] = 0xa8c147f0u; // ldp x16, x17, [sp], #16
		g[4] = encode_b(gate + 16, sites[i] + 4);
		g[5] = 0xa8c147f0u; // ldp x16, x17, [sp], #16
		g[6] = 0xd4000001u; // svc #0
		g[7] = encode_b(gate + 28, sites[i] + 4);
	}
	__builtin___clear_cache((char *)region, (char *)region + size);
	if (mprotect(region, size, PROT_READ | PROT_EXEC) < 0) {
		SPDLOG_WARN("Failed to make the trampoline executable: {}",
			    strerror(errno));
		munmap(region, size);
		return 0;
	}
	// Each site is one aligned word: svc -> b is a permitted concurrent
	// modification, so threads already running stay correct.
	size_t rewritten = 0;
	uint64_t open_page = 0;
	for (size_t i = 0; i < sites.size(); i++) {
		uint64_t site = sites[i];
		uint64_t gate = base + REGION_HEADER_SIZE + GATE_SIZE * i;
		uint64_t site_page = site & ~(page - 1);
		if (site_page != open_page) {
			if (open_page != 0)
				mprotect((void *)open_page, page, code_prot);
			open_page = 0;
			if (mprotect((void *)site_page, page,
				     PROT_READ | PROT_WRITE | PROT_EXEC) < 0) {
				SPDLOG_WARN(
					"Cannot write code page {:x}: {}",
					site_page, strerror(errno));
				continue;
			}
			open_page = site_page;
		}
		__atomic_store_n((uint32_t *)site, encode_b(site, gate),
				 __ATOMIC_RELEASE);
		__builtin___clear_cache((char *)site, (char *)site + 4);
		rewritten++;
	}
	if (open_page != 0)
		mprotect((void *)open_page, page, code_prot);
	return rewritten;
}
} // namespace

void setup_syscall_tracer()
{
	size_t found = 0, rewritten = 0;
	for (const auto &map : read_maps()) {
		if (!(map.prot & PROT_EXEC) || map.path == "[vdso]" ||
		    map.path == "[vsyscall]" || map.path == "[stack]" ||
		    map.path == "[uprobes]")
			continue;
		auto sites = find_svcs(map);
		if (sites.empty())
			continue;
		found += sites.size();
		// One region per 64 MiB of code keeps every gate in reach.
		size_t begin = 0;
		while (begin < sites.size()) {
			size_t end = begin;
			while (end < sites.size() &&
			       sites[end] - sites[begin] < 0x4000000)
				end++;
			std::vector<uint64_t> chunk(sites.begin() + begin,
						    sites.begin() + end);
			rewritten += install_region(chunk, map.prot);
			begin = end;
		}
	}
	SPDLOG_INFO("Rewrote {} of {} svc instructions", rewritten, found);
}
#endif

} // namespace bpftime
