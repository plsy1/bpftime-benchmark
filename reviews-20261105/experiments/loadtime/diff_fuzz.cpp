// Differential test: bpftime::reachable_instructions against the LLVM JIT.
// Random programs on scalars (mode A) or on bytes loaded from a frozen
// BPF_F_RDONLY_PROG array (mode B). Every conditional branch is followed by
// two marker arms (r0 |= bit): the analysis's verdict "arm unreachable" must
// never be contradicted by the JIT setting that arm's bit. Mode B also builds
// a variant with `call 158` (no such helper) placed in every arm the analysis
// found unreachable and loads it through bpftime_prog_load: it must compile
// (the real frozen-constant lookup must agree) and return the same r0.
#include "bpftime_config.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_prog_reachability.hpp"
#include "bpftime_shm.hpp"
#include <boost/interprocess/shared_memory_object.hpp>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <linux/bpf.h>
#include <random>
#include <string>
#include <unistd.h>
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

struct generator {
	std::mt19937_64 rng;
	std::vector<ebpf_inst> code;
	// (fall arm marker pc, taken arm marker pc, fall nop pc, taken nop pc)
	struct marker {
		size_t fall, taken, fall_nop, taken_nop;
	};
	std::vector<marker> markers;
	int rodata_fd = -1;
	int value_size = 0;
	int64_t ptr_off = 0; // tracked offset of r6 into the value
	bool have_ptr = false;

	explicit generator(uint64_t seed) : rng(seed)
	{
	}
	uint64_t pick(uint64_t n)
	{
		return rng() % n;
	}
	int32_t imm()
	{
		switch (pick(10)) {
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return -1;
		case 3:
			return (int32_t)pick(33) - 16;
		case 4:
			return INT32_MIN;
		case 5:
			return INT32_MAX;
		case 6:
			return (int32_t)(uint32_t)rng();
		case 7:
			return (int32_t)0x80000000u;
		case 8:
			return (int32_t)pick(256);
		default:
			return (int32_t)(uint32_t)rng();
		}
	}
	uint8_t vreg()
	{
		return 1 + pick(5); // r1..r5
	}
	void init_regs()
	{
		code.push_back(I(0xb7, 0, 0, 0, 0)); // r0 = 0
		for (uint8_t r = 1; r <= 5; r++) {
			switch (pick(3)) {
			case 0:
				code.push_back(I(0xb7, r, 0, 0, imm()));
				break;
			case 1:
				code.push_back(I(0xb4, r, 0, 0, imm()));
				break;
			default: {
				uint64_t v = rng();
				code.push_back(I(0x18, r, 0, 0, (int32_t)(uint32_t)v));
				code.push_back(I(0, 0, 0, 0, (int32_t)(uint32_t)(v >> 32)));
			}
			}
		}
	}
	void alu()
	{
		static const uint8_t ops[] = { 0x00, 0x10, 0x20, 0x40, 0x50,
					       0x60, 0x70, 0xa0, 0xb0 };
		const bool is64 = pick(2);
		const uint8_t cls = is64 ? 0x07 : 0x04;
		const uint8_t op = ops[pick(sizeof ops)];
		const uint8_t dst = vreg();
		if (pick(20) == 0) {
			// Unmodeled ops for coverage: neg, arsh, bswap, div, mod.
			switch (pick(5)) {
			case 0:
				code.push_back(I(cls | 0x80, dst, 0, 0, 0));
				return;
			case 1:
				code.push_back(I(cls | 0xc0, dst, 0, 0,
						 (int32_t)pick(is64 ? 64 : 32)));
				return;
			case 2:
				code.push_back(I(0x04 | 0xd0 | (pick(2) ? 0x08 : 0), dst, 0, 0,
						 16 << pick(3)));
				return;
			case 3:
				code.push_back(I(cls | 0x30, dst, 0, 0, 1 + (int32_t)pick(1000)));
				return;
			default:
				code.push_back(I(cls | 0x90, dst, 0, 0, 1 + (int32_t)pick(1000)));
				return;
			}
		}
		const bool x = pick(2);
		if (op == 0x60 || op == 0x70) {
			if (!x) {
				code.push_back(I(cls | op, dst, 0, 0,
						 (int32_t)pick(is64 ? 64 : 32)));
			} else {
				const uint8_t s = vreg();
				code.push_back(I(0xbf, 5, s, 0, 0)); // r5 = rs
				code.push_back(I(0x57, 5, 0, 0, is64 ? 63 : 31));
				code.push_back(I(cls | op | 0x08, dst, 5, 0, 0));
			}
			return;
		}
		if (x)
			code.push_back(I(cls | op | 0x08, dst, vreg(), 0, 0));
		else
			code.push_back(I(cls | op, dst, 0, 0, imm()));
	}
	void branch()
	{
		static const uint8_t ops[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60,
					       0x70, 0xa0, 0xb0, 0xc0, 0xd0 };
		const bool is64 = pick(2);
		const uint8_t cls = is64 ? 0x05 : 0x06;
		const uint8_t op = ops[pick(sizeof ops)];
		const bool x = pick(2);
		const int k = markers.size();
		if (x)
			code.push_back(I(cls | op | 0x08, vreg(), vreg(), 3, 0));
		else
			code.push_back(I(cls | op, vreg(), 0, 3, imm()));
		marker m;
		m.fall = code.size();
		code.push_back(I(0x47, 0, 0, 0, 1 << (2 * k)));
		m.fall_nop = code.size();
		code.push_back(I(0xbf, 9, 9, 0, 0)); // nop: r9 = r9
		code.push_back(I(0x05, 0, 0, 2, 0)); // ja +2
		m.taken = code.size();
		code.push_back(I(0x47, 0, 0, 0, 1 << (2 * k + 1)));
		m.taken_nop = code.size();
		code.push_back(I(0xbf, 9, 9, 0, 0));
		markers.push_back(m);
	}
	void rodata_ptr()
	{
		ptr_off = pick(value_size - 8 + 1);
		code.push_back(I(0x18, 6, 2, 0, rodata_fd));
		code.push_back(I(0, 0, 0, 0, (int32_t)ptr_off));
		have_ptr = true;
	}
	void rodata_load()
	{
		if (!have_ptr)
			return rodata_ptr();
		static const uint8_t sizes[] = { 0x71, 0x69, 0x61, 0x79 };
		static const int bytes[] = { 1, 2, 4, 8 };
		const int s = pick(4);
		// r7 = r6 (+k) sometimes, load through r7
		uint8_t base = 6;
		int64_t base_off = ptr_off;
		if (pick(3) == 0) {
			code.push_back(I(0xbf, 7, 6, 0, 0));
			const int64_t k = (int64_t)pick(17) - 8;
			code.push_back(I(0x07, 7, 0, 0, (int32_t)k));
			base = 7;
			base_off += k;
		}
		// offset such that [base_off+off, +bytes) stays inside the value
		const int64_t lo = -base_off, hi = value_size - bytes[s] - base_off;
		int64_t off;
		if (hi < lo || pick(10) == 0)
			off = (int64_t)pick(2 * value_size) - value_size; // may be out of range
		else
			off = lo + (int64_t)pick(hi - lo + 1);
		code.push_back(I(sizes[s], vreg(), base, (int16_t)off, 0));
	}
	void rodata_move_ptr()
	{
		if (!have_ptr)
			return rodata_ptr();
		const int64_t k = (int64_t)pick(17) - 8;
		code.push_back(I(0x07, 6, 0, 0, (int32_t)k));
		ptr_off += k;
	}
	void build(bool with_rodata, int steps)
	{
		init_regs();
		if (with_rodata)
			rodata_ptr();
		for (int i = 0; i < steps; i++) {
			const auto r = pick(100);
			if (with_rodata && r < 30)
				rodata_load();
			else if (with_rodata && r < 40)
				rodata_move_ptr();
			else if (with_rodata && r < 43)
				rodata_ptr();
			else if (r < 72 || markers.size() >= 15)
				alu();
			else
				branch();
		}
		code.push_back(I(0x95, 0, 0, 0, 0));
	}
};

static int run(bpftime_prog &prog, uint64_t &ret, bool jit = true)
{
	int res = prog.bpftime_prog_load(jit);
	if (res < 0)
		return res;
	uint64_t memory[8] = {};
	return prog.bpftime_prog_exec(memory, sizeof(memory), &ret);
}

int main(int argc, char **argv)
{
	const uint64_t seed0 = argc > 1 ? strtoull(argv[1], nullptr, 0) : 1;
	const int count = argc > 2 ? atoi(argv[2]) : 200;
	const bool with_rodata = argc > 3 ? atoi(argv[3]) != 0 : false;
	const int steps = argc > 4 ? atoi(argv[4]) : 24;

	const std::string shm_name = "ReachFuzzShm-" + std::to_string(getpid());
	setenv("BPFTIME_GLOBAL_SHM_NAME", shm_name.c_str(), 1);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	int rodata_fd = -1;
	const int value_size = 64;
	std::vector<uint8_t> value(value_size);
	if (with_rodata) {
		rodata_fd = bpftime_maps_create(-1, "rodata",
						{ .type = BPF_MAP_TYPE_ARRAY,
						  .key_size = 4,
						  .value_size = (uint32_t)value_size,
						  .max_ents = 1,
						  .flags = BPF_F_RDONLY_PROG });
		if (rodata_fd < 0) {
			fprintf(stderr, "map create failed\n");
			return 2;
		}
	}
	int mismatches = 0, decided = 0, branches = 0, load_fail = 0,
	    variant_fail = 0, progs = 0;
	for (int n = 0; n < count; n++) {
		const uint64_t seed = seed0 * 1000003ULL + n;
		generator g(seed);
		g.rodata_fd = rodata_fd;
		g.value_size = value_size;
		if (with_rodata) {
			// New random bytes for every program: the map is not
			// frozen until after the update (fresh map each time
			// would exhaust fds; instead reuse the map and freeze
			// only once; bytes vary across runs by seed).
			static bool frozen = false;
			if (!frozen) {
				std::mt19937_64 r(seed0);
				for (auto &b : value)
					b = (uint8_t)r();
				// Make some bytes "interesting"
				value[0] = 0;
				value[1] = 1;
				value[2] = 0xff;
				value[8] = 0x80;
				const uint32_t key = 0;
				if (bpftime_map_update_elem(rodata_fd, &key, value.data(), 0) != 0) {
					fprintf(stderr, "update failed\n");
					return 2;
				}
				if (bpftime_map_freeze(rodata_fd) != 0) {
					fprintf(stderr, "freeze failed\n");
					return 2;
				}
				frozen = true;
			}
		}
		g.build(with_rodata, steps);
		auto constant = [&](int fd) -> std::optional<std::span<const uint8_t> > {
			if (!with_rodata || fd != rodata_fd)
				return std::nullopt;
			return std::span<const uint8_t>(value);
		};
		const auto reach = reachable_instructions(g.code, constant);
		bpftime_prog prog(g.code.data(), g.code.size(), "fuzz",
				  runtime_config{});
		uint64_t ret = 0;
		int res = run(prog, ret);
		progs++;
		if (res != 0) {
			load_fail++;
			if (load_fail <= 3)
				fprintf(stderr, "seed %llu: load/exec failed %d\n",
					(unsigned long long)seed, res);
			continue;
		}
		bool bad = false;
		for (size_t k = 0; k < g.markers.size(); k++) {
			const auto &m = g.markers[k];
			branches++;
			const bool fall_bit = ret & (1ULL << (2 * k));
			const bool taken_bit = ret & (1ULL << (2 * k + 1));
			if (!reach[m.fall] || !reach[m.taken])
				decided++;
			if ((!reach[m.fall] && fall_bit) ||
			    (!reach[m.taken] && taken_bit)) {
				bad = true;
				fprintf(stderr,
					"MISMATCH seed %llu branch %zu at pc %zu: analysis fall=%d taken=%d, jit fall=%d taken=%d\n",
					(unsigned long long)seed, k, m.fall - 1,
					(int)reach[m.fall], (int)reach[m.taken],
					(int)fall_bit, (int)taken_bit);
			}
			if (!fall_bit && !taken_bit && reach[m.fall - 1]) {
				// The branch never ran at runtime although the
				// analysis entered it: cannot happen in straight
				// line code.
				fprintf(stderr, "seed %llu: branch %zu not executed?\n",
					(unsigned long long)seed, k);
			}
		}
		if (bad) {
			mismatches++;
			fprintf(stderr, "program (seed %llu):\n", (unsigned long long)seed);
			for (size_t pc = 0; pc < g.code.size(); pc++) {
				const auto &in = g.code[pc];
				fprintf(stderr, "  %3zu: %02x dst=%d src=%d off=%d imm=%d (0x%x)%s\n",
					pc, in.code, in.dst_reg, in.src_reg, in.off,
					in.imm, (unsigned)in.imm,
					reach[pc] ? "" : "   [unreachable]");
			}
		}
		// Variant through the real load path: call 158 in every arm the
		// analysis found unreachable.
		bool any_dead = false;
		auto variant = g.code;
		for (const auto &m : g.markers) {
			if (!reach[m.fall]) {
				variant[m.fall_nop] = I(0x85, 0, 0, 0, 158);
				any_dead = true;
			}
			if (!reach[m.taken]) {
				variant[m.taken_nop] = I(0x85, 0, 0, 0, 158);
				any_dead = true;
			}
		}
		if (any_dead) {
			bpftime_prog vprog(variant.data(), variant.size(),
					   "fuzz_variant", runtime_config{});
			uint64_t vret = 0;
			int vres = run(vprog, vret);
			if (vres != 0 || vret != ret) {
				variant_fail++;
				fprintf(stderr,
					"VARIANT seed %llu: res=%d ret=%llx (plain %llx)\n",
					(unsigned long long)seed, vres,
					(unsigned long long)vret,
					(unsigned long long)ret);
			}
		}
	}
	printf("programs=%d load_failures=%d branches=%d decided=%d mismatches=%d variant_failures=%d\n",
	       progs, load_fail, branches, decided, mismatches, variant_fail);
	bpftime_destroy_global_shm();
	boost::interprocess::shared_memory_object::remove(shm_name.c_str());
	return (mismatches || variant_fail) ? 1 : 0;
}
