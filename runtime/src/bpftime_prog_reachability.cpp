#include "bpftime_prog_reachability.hpp"
#include <array>
#include <cstring>
#include <deque>

using namespace bpftime;

namespace
{
constexpr uint8_t CLS_LD = 0x00, CLS_LDX = 0x01, CLS_ST = 0x02,
		  CLS_STX = 0x03, CLS_ALU = 0x04, CLS_JMP = 0x05,
		  CLS_JMP32 = 0x06, CLS_ALU64 = 0x07;
constexpr uint8_t SRC_X = 0x08;
constexpr uint8_t OP_JA = 0x00, OP_CALL = 0x80, OP_EXIT = 0x90,
		  OP_MOV = 0xb0, OP_ADD = 0x00;
constexpr uint8_t LDDW = 0x18, MODE_MEM = 0x60, MODE_ATOMIC = 0xc0;
// ldimm64 and call src_reg values (linux/bpf.h)
constexpr uint8_t PSEUDO_MAP_VALUE = 2, PSEUDO_FUNC = 4, PSEUDO_CALL = 1;
constexpr int32_t ATOMIC_FETCH = 0x01, ATOMIC_CMPXCHG = 0xf1;

enum class kind : uint8_t { unknown, scalar, map_value };
struct reg {
	kind k = kind::unknown;
	// The scalar, or the offset into the constant map's value.
	uint64_t value = 0;
	int fd = -1;
	bool operator==(const reg &) const = default;
};
using regs = std::array<reg, 11>;

reg scalar(uint64_t value)
{
	return reg{ kind::scalar, value, -1 };
}

// Registers that differ become unknown. True if `into` changed.
bool join(regs &into, const regs &from)
{
	bool changed = false;
	for (size_t i = 0; i < into.size(); i++) {
		if (into[i] != from[i] && into[i].k != kind::unknown) {
			into[i] = reg{};
			changed = true;
		}
	}
	return changed;
}

std::optional<uint64_t> alu(uint8_t op, uint64_t a, uint64_t b, bool is64)
{
	if (!is64) {
		a = (uint32_t)a;
		b = (uint32_t)b;
	}
	uint64_t r;
	switch (op) {
	case 0x00:
		r = a + b;
		break;
	case 0x10:
		r = a - b;
		break;
	case 0x20:
		r = a * b;
		break;
	case 0x40:
		r = a | b;
		break;
	case 0x50:
		r = a & b;
		break;
	case 0x60:
		r = is64 ? a << (b & 63) : (uint64_t)((uint32_t)a << (b & 31));
		break;
	case 0x70:
		r = is64 ? a >> (b & 63) : (uint64_t)((uint32_t)a >> (b & 31));
		break;
	case 0xa0:
		r = a ^ b;
		break;
	default: // div, mod, neg, arsh, byte swaps: not modeled
		return std::nullopt;
	}
	return is64 ? r : (uint64_t)(uint32_t)r;
}

std::optional<bool> taken(uint8_t op, uint64_t a, uint64_t b, bool is64)
{
	if (!is64) {
		a = (uint32_t)a;
		b = (uint32_t)b;
	}
	const int64_t sa = is64 ? (int64_t)a : (int64_t)(int32_t)a;
	const int64_t sb = is64 ? (int64_t)b : (int64_t)(int32_t)b;
	switch (op) {
	case 0x10:
		return a == b;
	case 0x20:
		return a > b;
	case 0x30:
		return a >= b;
	case 0x40:
		return (a & b) != 0;
	case 0x50:
		return a != b;
	case 0x60:
		return sa > sb;
	case 0x70:
		return sa >= sb;
	case 0xa0:
		return a < b;
	case 0xb0:
		return a <= b;
	case 0xc0:
		return sa < sb;
	case 0xd0:
		return sa <= sb;
	}
	return std::nullopt;
}
} // namespace

std::vector<bool>
bpftime::reachable_instructions(const std::vector<ebpf_inst> &insns,
				const constant_map_value &constant)
{
	const size_t n = insns.size();
	const std::vector<bool> everything(n, true);
	std::vector<std::optional<regs> > at(n);
	std::deque<size_t> work;
	auto enter = [&](int64_t pc, const regs &state) {
		if (pc < 0 || (size_t)pc >= n)
			return;
		if (!at[pc]) {
			at[pc] = state;
			work.push_back(pc);
		} else if (join(*at[pc], state)) {
			work.push_back(pc);
		}
	};
	enter(0, regs{});
	// Joins only turn registers unknown, so this ends; the bound is a guard.
	size_t steps = 0;
	while (!work.empty()) {
		if (++steps > 64 * n + 1024)
			return everything;
		const size_t pc = work.front();
		work.pop_front();
		regs s = *at[pc];
		const ebpf_inst &in = insns[pc];
		const uint8_t cls = in.code & 0x07;
		const uint8_t dst = in.dst_reg, src = in.src_reg;
		if (dst > 10 || src > 10)
			return everything;
		switch (cls) {
		case CLS_LD: {
			if (in.code != LDDW) {
				// LD_ABS / LD_IND: call-like, result in r0
				for (int i = 0; i <= 5; i++)
					s[i] = reg{};
				enter(pc + 1, s);
				break;
			}
			if (pc + 1 >= n)
				return everything;
			const uint64_t imm64 =
				(uint32_t)in.imm |
				((uint64_t)(uint32_t)insns[pc + 1].imm << 32);
			if (src == 0) {
				s[dst] = scalar(imm64);
			} else if (src == PSEUDO_MAP_VALUE && constant(in.imm)) {
				s[dst] = reg{ kind::map_value,
					      (uint32_t)insns[pc + 1].imm, in.imm };
			} else {
				// A callback's address: it can run.
				if (src == PSEUDO_FUNC)
					enter((int64_t)pc + in.imm + 1, regs{});
				s[dst] = reg{};
			}
			enter(pc + 2, s);
			break;
		}
		case CLS_LDX: {
			reg r;
			const uint8_t size = in.code & 0x18;
			if ((in.code & 0xe0) == MODE_MEM &&
			    s[src].k == kind::map_value) {
				const size_t bytes = size == 0x00 ? 4 :
						     size == 0x08 ? 2 :
						     size == 0x10 ? 1 :
								    8;
				const auto value = constant(s[src].fd);
				const int64_t offset = (int64_t)s[src].value + in.off;
				if (value && offset >= 0 &&
				    (uint64_t)offset + bytes <= value->size()) {
					uint64_t loaded = 0;
					memcpy(&loaded, value->data() + offset, bytes);
					r = scalar(loaded);
				}
			}
			s[dst] = r;
			enter(pc + 1, s);
			break;
		}
		case CLS_ST:
			enter(pc + 1, s);
			break;
		case CLS_STX:
			if ((in.code & 0xe0) == MODE_ATOMIC) {
				if (in.imm & ATOMIC_FETCH)
					s[src] = reg{};
				if (in.imm == ATOMIC_CMPXCHG)
					s[0] = reg{};
			}
			enter(pc + 1, s);
			break;
		case CLS_ALU:
		case CLS_ALU64: {
			const bool is64 = cls == CLS_ALU64;
			const uint8_t op = in.code & 0xf0;
			const bool x = in.code & SRC_X;
			const uint64_t k = is64 ? (uint64_t)(int64_t)in.imm :
						  (uint64_t)(uint32_t)in.imm;
			reg r;
			if (in.off != 0) {
				// signed div/mod, sign-extending moves
			} else if (op == OP_MOV) {
				if (!x)
					r = scalar(k);
				else if (is64)
					r = s[src];
				else if (s[src].k == kind::scalar)
					r = scalar((uint32_t)s[src].value);
			} else if (s[dst].k == kind::scalar) {
				std::optional<uint64_t> b;
				if (!x)
					b = k;
				else if (s[src].k == kind::scalar)
					b = s[src].value;
				if (b)
					if (auto v = alu(op, s[dst].value, *b, is64))
						r = scalar(*v);
			} else if (is64 && op == OP_ADD && !x &&
				   s[dst].k == kind::map_value) {
				r = s[dst];
				r.value += k;
			}
			s[dst] = r;
			enter(pc + 1, s);
			break;
		}
		case CLS_JMP:
		case CLS_JMP32: {
			const bool is64 = cls == CLS_JMP;
			const uint8_t op = in.code & 0xf0;
			if (op == OP_JA) {
				// JMP32's ja (gotol) takes its offset in imm.
				enter((int64_t)pc + 1 + (is64 ? in.off : in.imm), s);
				break;
			}
			if (op == OP_EXIT)
				break;
			if (op == OP_CALL) {
				if (!is64)
					return everything;
				if (src == PSEUDO_CALL)
					enter((int64_t)pc + in.imm + 1, regs{});
				for (int i = 0; i <= 5; i++)
					s[i] = reg{};
				enter(pc + 1, s);
				break;
			}
			std::optional<bool> decided;
			if (s[dst].k == kind::scalar) {
				std::optional<uint64_t> b;
				if (!(in.code & SRC_X))
					b = is64 ? (uint64_t)(int64_t)in.imm :
						   (uint64_t)(uint32_t)in.imm;
				else if (s[src].k == kind::scalar)
					b = s[src].value;
				if (b)
					decided = taken(op, s[dst].value, *b, is64);
			}
			if (!decided || *decided)
				enter((int64_t)pc + 1 + in.off, s);
			if (!decided || !*decided)
				enter(pc + 1, s);
			break;
		}
		default:
			return everything;
		}
	}
	std::vector<bool> reachable(n, false);
	for (size_t pc = 0; pc < n; pc++) {
		if (!at[pc])
			continue;
		reachable[pc] = true;
		// An ldimm64's second half is data.
		if (insns[pc].code == LDDW && pc + 1 < n)
			reachable[pc + 1] = true;
	}
	return reachable;
}
