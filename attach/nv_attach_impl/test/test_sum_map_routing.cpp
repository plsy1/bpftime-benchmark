#include "catch2/catch_test_macros.hpp"
#include "nv_thread_local_routing.hpp"
#include <string>

using namespace bpftime::attach;

// Sum maps (1531) in the routing pass: a constant-fd lookup becomes the
// shard address inline, and an atomic on that address takes the global
// state space (results gpu/probe-cost-20261117, section 7). Atomics whose
// address the pass cannot attribute keep the generic form behind
// isspacep tests, now with a global branch.
namespace
{
const char *K = "kern";

// a probe function as llvmbpf emits it: the lookup of map `fd_arg` (an
// immediate or a register) and an add of 1 at the result, null-checked
std::string probe(const std::string &fd_def, const std::string &fd_arg,
		  const std::string &extra = "",
		  const std::string &atom = "atom.add.u64 \t%rd56, [%rd55], 1;")
{
	return std::string(".version 8.1\n.target sm_87\n.address_size 64\n"
			   ".visible .func __retprobe_func__kern()\n{\n"
			   "\t.reg .pred \t%p<20>;\n\t.reg .b64 \t%rd<80>;\n") +
	       fd_def + "\tadd.s64 \t%rd51, %rd2, -8;\n" +
	       "\t{ // callseq 20, 0\n"
	       "\t.reg .b32 temp_param_reg;\n"
	       "\t.param .b64 param0;\n"
	       "\tst.param.b64 \t[param0+0], " +
	       fd_arg +
	       ";\n"
	       "\t.param .b64 param1;\n"
	       "\tst.param.b64 \t[param1+0], %rd51;\n"
	       "\t.param .b64 param2;\n"
	       "\tst.param.b64 \t[param2+0], %rd57;\n"
	       "\t.param .b64 param3;\n"
	       "\tst.param.b64 \t[param3+0], %rd11;\n"
	       "\t.param .b64 param4;\n"
	       "\tst.param.b64 \t[param4+0], %rd12;\n"
	       "\t.param .b64 retval0;\n"
	       "\tcall.uni (retval0), \n\t_bpf_helper_ext_0001, \n\t(\n"
	       "\tparam0, \n\tparam1, \n\tparam2, \n\tparam3, \n\tparam4\n\t);\n"
	       "\tld.param.b64 \t%rd55, [retval0+0];\n"
	       "\t} // callseq 20\n" +
	       extra +
	       "\tsetp.eq.s64 \t%p10, %rd55, 0;\n"
	       "\t@%p10 bra \t$L__BB0_5;\n\t" +
	       atom + "\n$L__BB0_5:\n\tret;\n\n}\n";
}

sum_map_desc sum14(uint64_t base = 0x203400000ull, uint64_t entries = 260,
		   uint64_t shards = 256)
{
	sum_map_desc d;
	d.fd = 14;
	d.base = base;
	d.value_size = 8;
	d.max_entries = entries;
	d.shards = shards;
	return d;
}

bool has(const std::string &s, const std::string &x)
{
	return s.find(x) != std::string::npos;
}

std::string route(const std::string &ptx, const std::vector<sum_map_desc> &s,
		  thread_local_routing_stats &st)
{
	return route_thread_local_maps(ptx, { K }, {}, 0, &st, s);
}
} // namespace

TEST_CASE("A constant-fd sum-map lookup becomes the shard address, its add a global atomic",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(probe("\tmov.u64 \t%rd52, 14;\n", "%rd52"), { sum14() },
			 st);
	REQUIRE(st.sum_lookups == 1);
	REQUIRE(st.global_atomics == 1);
	REQUIRE(st.guarded_atomics == 0);
	REQUIRE_FALSE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE(has(out, "ld.u32 \t%bpsm_k0, [%rd51];"));
	REQUIRE(has(out, "setp.lt.u32 \t%bpsm_p0, %bpsm_k0, 260;"));
	REQUIRE(has(out, "mov.u32 \t%bpsm_s0, %smid;"));
	REQUIRE(has(out, "and.b32 \t%bpsm_s0, %bpsm_s0, 7;"));
	REQUIRE(has(out, "mov.u32 \t%bpsm_l0, %laneid;"));
	REQUIRE(has(out, "mad.lo.u32 \t%bpsm_k0, %bpsm_k0, 256, %bpsm_s0;"));
	REQUIRE(has(out, "mul.wide.u32 \t%bpsm_a0, %bpsm_k0, 8;"));
	REQUIRE(has(out, "add.u64 \t%bpsm_a0, %bpsm_a0, " +
				 std::to_string(0x203400000ull) + ";"));
	REQUIRE(has(out, "selp.b64 \t%rd55, %bpsm_a0, 0, %bpsm_p0;"));
	REQUIRE(has(out, "cvta.to.global.u64 %bpag_a0, %rd55;"));
	REQUIRE(has(out, "atom.global.add.u64 %rd56, [%bpag_a0+0], 1;"));
	REQUIRE_FALSE(has(out, "isspacep"));
	// the program's null check stays
	REQUIRE(has(out, "setp.eq.s64 \t%p10, %rd55, 0;"));
}

TEST_CASE("An immediate map fd, offsets and reductions keep their form on the global address",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(probe("", "14", "",
			       "red.add.u64 \t[%rd55+8], %rd9;"),
			 { sum14() }, st);
	REQUIRE(st.sum_lookups == 1);
	REQUIRE(st.global_atomics == 1);
	REQUIRE(has(out, "red.global.add.u64 [%bpag_a0+8], %rd9;"));
	REQUIRE_FALSE(has(out, "isspacep"));
}

TEST_CASE("A sum map without a device copy gives NULL lookups",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(probe("\tmov.u64 \t%rd52, 14;\n", "%rd52"),
			 { sum14(0) }, st);
	REQUIRE(st.sum_lookups == 1);
	REQUIRE(st.global_atomics == 0);
	REQUIRE(has(out, "mov.b64 \t%rd55, 0;"));
	REQUIRE_FALSE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE_FALSE(has(out, "%smid"));
	// the (never reached) add keeps the guarded generic form
	REQUIRE(st.guarded_atomics == 1);
	REQUIRE(has(out, "isspacep.local"));
}

TEST_CASE("A large sum map's index is computed in 64 bits",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	// 2^25 entries x 256 shards: the index passes 2^32
	auto out = route(probe("", "14"), { sum14(0x203400000ull, 1ull << 25) },
			 st);
	REQUIRE(st.sum_lookups == 1);
	REQUIRE_FALSE(has(out, "mad.lo.u32"));
	REQUIRE(has(out, "mul.wide.u32 \t%bpsm_a0, %bpsm_k0, 256;"));
	REQUIRE(has(out, "cvt.u64.u32 \t%bpsm_w0, %bpsm_s0;"));
	REQUIRE(has(out, "mul.lo.u64 \t%bpsm_a0, %bpsm_a0, 8;"));
	// at exactly 2^32 it still fits
	thread_local_routing_stats st2;
	auto out2 = route(probe("", "14"), { sum14(0x203400000ull, 1ull << 24) },
			  st2);
	REQUIRE(has(out2, "mad.lo.u32"));
}

TEST_CASE("A sum-map address register written elsewhere keeps the guarded atomic",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	// %rd55 also comes from somewhere else (a join llvmbpf did not make
	// here, but the pass must not assume the lookup is its only source)
	auto out = route(probe("", "14", "\tmov.b64 \t%rd55, %rd9;\n"),
			 { sum14() }, st);
	REQUIRE(st.sum_lookups == 1);
	REQUIRE(st.global_atomics == 0);
	REQUIRE(st.guarded_atomics == 1);
	REQUIRE(has(out, "isspacep.local"));
}

TEST_CASE("Sum-map lookups joined into one register keep the global atomic",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	// a second lookup (another key, another branch) writes the same
	// register, as llvmbpf emits for count(k) called on several paths
	std::string second =
		"\t{ // callseq 21, 0\n"
		"\t.reg .b32 temp_param_reg;\n"
		"\t.param .b64 param0;\n"
		"\tst.param.b64 \t[param0+0], 14;\n"
		"\t.param .b64 param1;\n"
		"\tst.param.b64 \t[param1+0], %rd60;\n"
		"\t.param .b64 param4;\n"
		"\tst.param.b64 \t[param4+0], %rd12;\n"
		"\t.param .b64 retval0;\n"
		"\tcall.uni (retval0), \n\t_bpf_helper_ext_0001, \n\t(\n"
		"\tparam0, \n\tparam1, \n\tparam4\n\t);\n"
		"\tld.param.b64 \t%rd55, [retval0+0];\n"
		"\t} // callseq 21\n";
	auto out = route(probe("", "14", second), { sum14() }, st);
	REQUIRE(st.sum_lookups == 2);
	REQUIRE(st.global_atomics == 1);
	REQUIRE(st.guarded_atomics == 0);
	REQUIRE(has(out, "selp.b64 \t%rd55, %bpsm_a1, 0, %bpsm_p1;"));
	// one of them from a map without a device copy: no longer only
	// sum-map addresses
	thread_local_routing_stats st2;
	sum_map_desc other = sum14(0);
	other.fd = 16;
	std::string third = second;
	third.replace(third.find("[param0+0], 14;"), 15, "[param0+0], 16;");
	auto out2 = route(probe("", "14", third), { sum14(), other }, st2);
	REQUIRE(st2.sum_lookups == 2);
	REQUIRE(st2.global_atomics == 0);
	REQUIRE(st2.guarded_atomics == 1);
}

TEST_CASE("Atomics of unknown origin get a global branch before the generic form",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	// fd 15 is not a sum map: the trampoline answers the lookup
	auto out = route(probe("", "15"), { sum14() }, st);
	REQUIRE(st.sum_lookups == 0);
	REQUIRE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE(st.guarded_atomics == 1);
	REQUIRE(has(out, "isspacep.local %bpat_l0, %bpat_a0;"));
	REQUIRE(has(out, "isspacep.global %bpat_g0, %bpat_a0;"));
	REQUIRE(has(out, "or.pred %bpat_x0, %bpat_l0, %bpat_g0;"));
	REQUIRE(has(out, "@%bpat_g0 cvta.to.global.u64 %bpat_ga0, %bpat_a0;"));
	REQUIRE(has(out, "@%bpat_g0 atom.global.add.u64 %rd56, [%bpat_ga0], 1;"));
	REQUIRE(has(out, "@!%bpat_x0 atom.add.u64 %rd56, [%rd55], 1;"));
	REQUIRE(has(out, "@%bpat_l0 ld.b64 %bpat_o0, [%bpat_a0];"));
	REQUIRE(has(out, "@%bpat_l0 mov.b64 %rd56, %bpat_o0;"));
}
