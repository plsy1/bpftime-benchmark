// Hand-written edge cases for bpftime::reachable_instructions (analysis only,
// no VM). Each case states what the eBPF semantics make the guarded `call 158`
// at the marked pc: reachable (R) or unreachable (U). A mismatch where the
// analysis says U but the semantics say R would be live code pruned.
#include "bpftime_prog_reachability.hpp"
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using namespace bpftime;

static ebpf_inst I(uint8_t code, uint8_t dst, uint8_t src, int16_t off,
		   int32_t imm)
{
	ebpf_inst i{};
	i.code = code;
	i.dst_reg = dst;
	i.src_reg = src;
	i.off = off;
	i.imm = imm;
	return i;
}
static const ebpf_inst CALL = I(0x85, 0, 0, 0, 158);
static const ebpf_inst EXIT = I(0x95, 0, 0, 0, 0);
static const ebpf_inst R0_7 = I(0xb7, 0, 0, 0, 7);
constexpr int FD = 5;

static constant_map_value rodata(const std::vector<uint8_t> &bytes)
{
	return [&bytes](int fd) -> std::optional<std::span<const uint8_t> > {
		if (fd != FD)
			return std::nullopt;
		return std::span<const uint8_t>(bytes);
	};
}
static int failures = 0;
static void check(const std::string &name, const std::vector<ebpf_inst> &code,
		  size_t call_pc, bool expect_reachable,
		  const std::vector<uint8_t> &bytes = {})
{
	auto r = reachable_instructions(code, rodata(bytes));
	bool got = r[call_pc];
	const char *verdict = got == expect_reachable ? "ok  " : "MISMATCH";
	if (got != expect_reachable)
		failures++;
	printf("%s %-60s expected %s got %s\n", verdict, name.c_str(),
	       expect_reachable ? "R" : "U", got ? "R" : "U");
}

int main()
{
	// Guard pattern: <setup>; if r1 == 0 goto +1; call 158; r0 = 7; exit
	auto guarded = [](std::vector<ebpf_inst> setup, ebpf_inst jump) {
		setup.push_back(jump);
		setup.push_back(CALL);
		setup.push_back(R0_7);
		setup.push_back(EXIT);
		return setup;
	};
	// 1. alu32 wrap: w1 = 0xffffffff; w1 += 1 -> r1 = 0 -> jeq taken -> U
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, -1), I(0x04, 1, 0, 0, 1) },
				 I(0x15, 1, 0, 1, 0));
		check("alu32 wrap to zero, jeq64 0 taken", c, 3, false);
	}
	// 2. mov32 w1 = -1 -> r1 = 0xffffffff; jeq64 r1, -1 (sign-extended): not
	// equal -> fallthrough -> R
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, -1) }, I(0x15, 1, 0, 1, -1));
		check("mov32 -1 vs jeq64 -1 (not equal)", c, 2, true);
	}
	// 3. same but JMP32 jeq: lower 32 bits equal -> taken -> U
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, -1) }, I(0x16, 1, 0, 1, -1));
		check("mov32 -1 vs jeq32 -1 (equal)", c, 2, false);
	}
	// 4. jsgt64 negative: r1 = -5; if r1 s> -10 -> taken -> U
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, -5) }, I(0x65, 1, 0, 1, -10));
		check("jsgt64 -5 > -10 taken", c, 2, false);
	}
	// 5. jsgt32 negative: w1 = -5; if w1 s> -10 -> taken -> U
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, -5) }, I(0x66, 1, 0, 1, -10));
		check("jsgt32 -5 > -10 taken", c, 2, false);
	}
	// 6. jgt64 unsigned with negative imm: 5 > 0xffff..ff false -> R
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 5) }, I(0x25, 1, 0, 1, -1));
		check("jgt64 5 > -1 (unsigned, false)", c, 2, true);
	}
	// 7. rodata u16 at offset 2 via pointer add; bytes {0,0,0x12,0x34} ->
	// 0x3412 -> jeq taken -> U
	{
		std::vector<uint8_t> b{ 0, 0, 0x12, 0x34, 0, 0, 0, 0 };
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0x07, 1, 0, 0, 2), I(0x69, 2, 1, 0, 0) },
				 I(0x15, 2, 0, 1, 0x3412));
		check("rodata u16 @2 via add, little-endian", c, 5, false, b);
	}
	// 8. rodata via negative LDX offset after add: r1 += 8; *(u32*)(r1-8)
	{
		std::vector<uint8_t> b{ 1, 0, 0, 0, 0, 0, 0, 0 };
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0x07, 1, 0, 0, 8), I(0x61, 2, 1, -8, 0) },
				 I(0x15, 2, 0, 1, 1));
		check("rodata u32 @0 via add 8 and off -8", c, 5, false, b);
	}
	// 9. ldimm64 second-half offset: value+4 -> u8 at 4 = 9 -> jeq 9 -> U
	{
		std::vector<uint8_t> b{ 0, 0, 0, 0, 9, 0, 0, 0 };
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 4),
				   I(0x71, 1, 1, 0, 0) },
				 I(0x15, 1, 0, 1, 9));
		check("rodata ldimm64 with value offset 4", c, 4, false, b);
	}
	// 10. load out of bounds (offset 8 of an 8-byte value) -> unknown -> R
	{
		std::vector<uint8_t> b(8, 0);
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0x71, 1, 1, 8, 0) },
				 I(0x15, 1, 0, 1, 0));
		check("rodata load past the value -> unknown", c, 4, true, b);
	}
	// 11. LDX MEMSX (sign-extending load) -> unknown -> R, even though a
	// plain load would decide it
	{
		std::vector<uint8_t> b{ 0xff, 0, 0, 0, 0, 0, 0, 0 };
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0x91, 1, 1, 0, 0) },
				 I(0x15, 1, 0, 1, -1));
		check("ldsx8 -> unknown (both ways)", c, 4, true, b);
	}
	// 12. movsx (off=8): r1 = (s8)r2 where r2 = 0xff -> would be -1; unknown
	{
		auto c = guarded({ I(0xb7, 2, 0, 0, 0xff), I(0xbf, 1, 2, 8, 0) },
				 I(0x15, 1, 0, 1, -1));
		check("movsx -> unknown (both ways)", c, 3, true);
	}
	// 13. bswap16 (ALU64|END, imm 16): unknown
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 0x1234), I(0xd7, 1, 0, 0, 16) },
				 I(0x15, 1, 0, 1, 0x3412));
		check("bswap16 -> unknown", c, 3, true);
	}
	// 14. gotol (JMP32|JA, imm offset) skips the call -> U
	{
		std::vector<ebpf_inst> c{ I(0x06, 0, 0, 0, 1), CALL, R0_7, EXIT };
		check("gotol skips call", c, 1, false);
	}
	// 15. ja with off skips the call -> U
	{
		std::vector<ebpf_inst> c{ I(0x05, 0, 0, 1, 0), CALL, R0_7, EXIT };
		check("ja skips call", c, 1, false);
	}
	// 16. loop: r1 = 0; L: r1 += 1; if r1 < 10 goto L; then call guarded by
	// r1 == 10? After the loop r1 is unknown (joined) -> R
	{
		std::vector<ebpf_inst> c{ I(0xb7, 1, 0, 0, 0),
					  I(0x07, 1, 0, 0, 1),
					  I(0xa5, 1, 0, -2, 10),
					  I(0x15, 1, 0, 1, 10),
					  CALL,
					  R0_7,
					  EXIT };
		check("loop counter unknown after join", c, 4, true);
	}
	// 17. subprogram containing the call, called only from dead code -> U
	{
		std::vector<uint8_t> b{ 0 };
		std::vector<ebpf_inst> c{
			I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
			I(0x71, 1, 1, 0, 0),
			I(0x15, 1, 0, 1, 0), // if r1 == 0 skip the call
			I(0x85, 0, 1, 0, 2), // call subprog at pc 7
			R0_7, EXIT,
			CALL, // pc 7: subprogram body
			EXIT
		};
		check("subprogram only called from dead code", c, 7, false, b);
	}
	// 18. same but the option on -> R
	{
		std::vector<uint8_t> b{ 1 };
		std::vector<ebpf_inst> c{
			I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
			I(0x71, 1, 1, 0, 0), I(0x15, 1, 0, 1, 0),
			I(0x85, 0, 1, 0, 2), R0_7, EXIT, CALL, EXIT
		};
		check("subprogram called from live code", c, 7, true, b);
	}
	// 19. atomic cmpxchg clobbers r0: r0 = 0; cmpxchg; if r0 == 0 -> both
	{
		std::vector<ebpf_inst> c{ I(0xb7, 0, 0, 0, 0),
					  I(0xb7, 1, 0, 0, 0),
					  I(0xdb, 10, 1, -8, 0xf1),
					  I(0x15, 0, 0, 1, 0),
					  CALL,
					  R0_7,
					  EXIT };
		check("cmpxchg makes r0 unknown", c, 4, true);
	}
	// 20. 32-bit shifts: w1 = 1; w1 <<= 31; w1 <<= 1 -> 0 -> U
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, 1), I(0x64, 1, 0, 0, 31),
				   I(0x64, 1, 0, 0, 1) },
				 I(0x15, 1, 0, 1, 0));
		check("alu32 shifts wrap", c, 4, false);
	}
	// 21. 64-bit sub: r1 = 0; r1 -= 1; if r1 == -1 taken -> U
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 0), I(0x17, 1, 0, 0, 1) },
				 I(0x15, 1, 0, 1, -1));
		check("alu64 sub wraps to -1", c, 3, false);
	}
	// 22. ldimm64 scalar 0x8000000000000000; jslt 0 taken -> U
	{
		auto c = guarded({ I(0x18, 1, 0, 0, 0), I(0, 0, 0, 0, (int32_t)0x80000000) },
				 I(0xc5, 1, 0, 1, 0));
		check("ldimm64 negative scalar jslt 0", c, 3, false);
	}
	// 23. map_value pointer compared with 0: not a scalar -> both -> R
	{
		std::vector<uint8_t> b{ 0 };
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0) },
				 I(0x15, 1, 0, 1, 0));
		check("map_value pointer null check undecided", c, 3, true, b);
	}
	// 24. helper call clobbers r1..r5 but keeps r6..r9
	{
		std::vector<uint8_t> b{ 0 };
		auto c = guarded({ I(0x18, 6, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0x71, 6, 6, 0, 0), I(0x85, 0, 0, 0, 5) },
				 I(0x15, 6, 0, 1, 0));
		check("r6 constant survives a helper call", c, 5, false, b);
	}
	// 25. bpf-to-bpf call keeps r6 (callee-saved)
	{
		std::vector<uint8_t> b{ 0 };
		std::vector<ebpf_inst> c{
			I(0x18, 6, 2, 0, FD), I(0, 0, 0, 0, 0),
			I(0x71, 6, 6, 0, 0),
			I(0x85, 0, 1, 0, 4), // call pc 8
			I(0x15, 6, 0, 1, 0), CALL, R0_7, EXIT,
			I(0xb7, 6, 0, 0, 1), EXIT // callee writes r6 = 1 (own frame)
		};
		check("r6 constant survives a bpf-to-bpf call", c, 5, false, b);
	}
	// 26. callback (ldimm64 src 4) body with the call is an entry -> R
	{
		// target = pc + imm + 1 = 0 + 3 + 1 = 4
		std::vector<ebpf_inst> c{ I(0x18, 2, 4, 0, 3), I(0, 0, 0, 0, 0),
					  R0_7, EXIT, CALL, EXIT };
		check("callback body reachable", c, 4, true);
	}
	// 27. may_goto (JMP|JCOND 0xe5, src 0) undecided -> both
	{
		std::vector<ebpf_inst> c{ I(0xb7, 0, 0, 0, 0), I(0xe5, 0, 0, 1, 0),
					  CALL, R0_7, EXIT };
		check("may_goto undecided", c, 2, true);
	}
	// 28. JMP32 call -> everything reachable (fallback)
	{
		std::vector<ebpf_inst> c{ I(0xb7, 1, 0, 0, 0), I(0x15, 1, 0, 1, 0),
					  CALL, I(0x86, 0, 0, 0, 1), EXIT };
		check("unknown instruction class -> everything", c, 2, true);
	}
	// 29. and with negative imm 64: r1 = 0x1ff; r1 &= -256 -> 0x100 -> jeq
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 0x1ff), I(0x57, 1, 0, 0, -256) },
				 I(0x15, 1, 0, 1, 0x100));
		check("and64 sign-extended imm", c, 3, false);
	}
	// 30. and32 with negative imm: w1 = 0x1ff; w1 &= -256 -> 0x100 (zero-ext)
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, 0x1ff), I(0x54, 1, 0, 0, -256) },
				 I(0x15, 1, 0, 1, 0x100));
		check("and32 imm zero-extended result", c, 3, false);
	}
	// 31. mov32 from a 64-bit value truncates: r2 = 0x1_0000_0001; w1 = w2
	{
		auto c = guarded({ I(0x18, 2, 0, 0, 1), I(0, 0, 0, 0, 1),
				   I(0xbc, 1, 2, 0, 0) },
				 I(0x15, 1, 0, 1, 1));
		check("mov32 truncates", c, 4, false);
	}
	// 32. conditional on r10 (stack pointer, unknown) -> both
	{
		std::vector<ebpf_inst> c{ I(0x15, 10, 0, 1, 0), CALL, R0_7, EXIT };
		check("r10 unknown", c, 1, true);
	}
	// 33. spill/reload: *(u64*)(r10-8) = r1 (=0); r1 = *(u64*)(r10-8) -> unknown
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 0), I(0x7b, 10, 1, -8, 0),
				   I(0x79, 1, 10, -8, 0) },
				 I(0x15, 1, 0, 1, 0));
		check("spilled constant unknown after reload", c, 4, true);
	}
	// 34. ST (store imm) does not touch registers; still decided
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 0), I(0x7a, 10, 0, -8, 5) },
				 I(0x15, 1, 0, 1, 0));
		check("st imm keeps registers", c, 3, false);
	}
	// 35. two-register compare, both known
	{
		auto c = guarded({ I(0xb7, 1, 0, 0, 3), I(0xb7, 2, 0, 0, 3) },
				 I(0x1d, 1, 2, 1, 0));
		check("jeq reg,reg decided", c, 3, false);
	}
	// 36. JMP32 compare ignores upper bits: r1 = 0x1_0000_0000; if w1 == 0
	{
		auto c = guarded({ I(0x18, 1, 0, 0, 0), I(0, 0, 0, 0, 1) },
				 I(0x16, 1, 0, 1, 0));
		check("jeq32 ignores upper bits", c, 3, false);
	}
	// 37. rsh32 by 0 then jeq64 with imm -1 (sign-extended): r1 =
	// 0x00000000ffffffff != 0xffffffffffffffff -> fallthrough -> R; the
	// JMP32 form compares the low 32 bits -> taken -> U
	{
		auto c = guarded({ I(0xb4, 1, 0, 0, -1), I(0x74, 1, 0, 0, 0) },
				 I(0x15, 1, 0, 1, -1));
		check("rsh32 by 0, jeq64 -1 not equal", c, 3, true);
		auto d = guarded({ I(0xb4, 1, 0, 0, -1), I(0x74, 1, 0, 0, 0) },
				 I(0x16, 1, 0, 1, -1));
		check("rsh32 by 0, jeq32 -1 equal", d, 3, false);
	}
	// 38. rodata DW with value 0x0102030405060708 LE, jeq via ldimm64 compare
	{
		std::vector<uint8_t> b{ 8, 7, 6, 5, 4, 3, 2, 1 };
		std::vector<ebpf_inst> c{ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
					  I(0x79, 1, 1, 0, 0),
					  I(0x18, 2, 0, 0, 0x05060708),
					  I(0, 0, 0, 0, 0x01020304),
					  I(0x1d, 1, 2, 1, 0),
					  CALL, R0_7, EXIT };
		check("rodata dw little-endian equals ldimm64", c, 6, false, b);
	}
	// 39. ALU on a map_value pointer other than add imm -> unknown pointer
	// then load -> unknown -> R
	{
		std::vector<uint8_t> b{ 0, 0, 0, 0, 0, 0, 0, 0 };
		auto c = guarded({ I(0x18, 1, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0xb7, 2, 0, 0, 0), I(0x0f, 1, 2, 0, 0),
				   I(0x71, 1, 1, 0, 0) },
				 I(0x15, 1, 0, 1, 0));
		check("pointer add reg -> unknown", c, 6, true, b);
	}
	// 40. tail call (helper 12) then fallthrough: continues (conservative)
	{
		std::vector<uint8_t> b{ 0 };
		auto c = guarded({ I(0x18, 6, 2, 0, FD), I(0, 0, 0, 0, 0),
				   I(0x71, 6, 6, 0, 0), I(0x85, 0, 0, 0, 12) },
				 I(0x15, 6, 0, 1, 0));
		check("tail call fallthrough still analysed", c, 5, false, b);
	}
	printf("failures: %d\n", failures);
	return failures ? 1 : 0;
}
