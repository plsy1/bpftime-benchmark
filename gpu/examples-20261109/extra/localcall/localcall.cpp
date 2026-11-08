// Does a bpf-to-bpf call give the callee a frame that overlaps the caller's?
// main stores a sentinel at r10-128, calls a subprogram that zeroes r10-8..r10-64
// of its own frame, then returns the sentinel. Correct result: 0x5a5a5a5a.
#include "llvmbpf.hpp"
#include <cstdint>
#include <cstdio>
#include <new>
alignas(64) static unsigned char vmbuf[1 << 20];
struct insn { uint8_t code; uint8_t dst : 4; uint8_t src : 4; int16_t off; int32_t imm; };
static int run(bool compat, int sentinel_off)
{
	insn code[32]; int n = 0;
	code[n++] = { 0xb7, 1, 0, 0, 0x5a5a5a5a };          // r1 = sentinel
	code[n++] = { 0x7b, 10, 1, (int16_t)sentinel_off, 0 }; // *(u64*)(r10+off) = r1
	code[n++] = { 0x85, 0, 1, 0, 2 };                    // call pc+2+1 (local)
	code[n++] = { 0x79, 0, 10, (int16_t)sentinel_off, 0 }; // r0 = *(u64*)(r10+off)
	code[n++] = { 0x95, 0, 0, 0, 0 };                    // exit
	code[n++] = { 0xb7, 1, 0, 0, 0 };                    // sub: r1 = 0
	for (int k = 1; k <= 8; k++)
		code[n++] = { 0x7b, 10, 1, (int16_t)(-8 * k), 0 }; // *(u64*)(r10-8k) = 0
	code[n++] = { 0xb7, 0, 0, 0, 0 };
	code[n++] = { 0x95, 0, 0, 0, 0 };
	auto &vm = *new (vmbuf) bpftime::llvmbpf_vm(); // placement: size-agnostic
	vm.set_kernel_compatible_mode(compat);
	if (vm.load_code(code, n * sizeof(insn)) != 0) { printf("load failed: %s\n", vm.get_error_message().c_str()); return 2; }
	if (!vm.compile()) { printf("compile failed: %s\n", vm.get_error_message().c_str()); return 2; }
	uint64_t mem[4] = {}, ret = 0;
	if (vm.exec(mem, sizeof(mem), ret) != 0) { printf("exec failed\n"); return 2; }
	printf("kernel_compatible=%d sentinel at r10%d -> returned 0x%llx (%s)\n", compat, sentinel_off,
	       (unsigned long long)ret, ret == 0x5a5a5a5a ? "intact" : "CLOBBERED");
	return ret == 0x5a5a5a5a ? 0 : 1;
}
int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("sizeof(llvmbpf_vm) seen here = %zu\n", sizeof(bpftime::llvmbpf_vm));
	int r = 0;
	r |= run(false, -128);
	r |= run(false, -48);
	r |= run(true, -128);
	return r;
}
