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
	// the whole lookup, in order: key, bound, shard, index, address
	const std::string lookup =
		"{ // bpftime sum-map lookup, map 14\n"
		"\t.reg .pred \t%bpsm_p0;\n"
		"\t.reg .b32 \t%bpsm_k0, %bpsm_s0, %bpsm_l0;\n"
		"\t.reg .b64 \t%bpsm_a0, %bpsm_w0;\n"
		"\tld.u32 \t%bpsm_k0, [%rd51];\n"
		"\tsetp.lt.u32 \t%bpsm_p0, %bpsm_k0, 260;\n"
		"\tmov.u32 \t%bpsm_s0, %smid;\n"
		"\tand.b32 \t%bpsm_s0, %bpsm_s0, 7;\n"
		"\tshl.b32 \t%bpsm_s0, %bpsm_s0, 5;\n"
		"\tmov.u32 \t%bpsm_l0, %laneid;\n"
		"\tor.b32 \t%bpsm_s0, %bpsm_s0, %bpsm_l0;\n"
		"\tmad.lo.u32 \t%bpsm_k0, %bpsm_k0, 256, %bpsm_s0;\n"
		"\tmul.wide.u32 \t%bpsm_a0, %bpsm_k0, 8;\n"
		"\tadd.u64 \t%bpsm_a0, %bpsm_a0, " +
		std::to_string(0x203400000ull) +
		";\n"
		"\tselp.b64 \t%rd55, %bpsm_a0, 0, %bpsm_p0;\n"
		"} // bpftime sum-map lookup\n";
	REQUIRE(has(out, lookup));
	const std::string add = "{ // bpftime: atomic on a sum map's value\n"
				"\t.reg .b64 %bpag_a0;\n"
				"\tadd.u64 %bpag_a0, %rd55, 0;\n"
				"\tcvta.to.global.u64 %bpag_a0, %bpag_a0;\n"
				"\tatom.global.add.u64 %rd56, [%bpag_a0], 1;\n"
				"\t}";
	REQUIRE(has(out, add));
	REQUIRE(out.find(lookup) < out.find(add));
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
	REQUIRE(has(out, "add.u64 %bpag_a0, %rd55, 8;\n"
			 "\tcvta.to.global.u64 %bpag_a0, %bpag_a0;\n"
			 "\tred.global.add.u64 [%bpag_a0], %rd9;"));
	REQUIRE_FALSE(has(out, "isspacep"));
}

TEST_CASE("A sum map without a device copy keeps the trampoline's lookup",
	  "[sum-map-routing]")
{
	// a copy made later (after an owner exits) is then still found: the
	// trampoline reads the run-time map_info, a cached patched image would
	// keep an inlined NULL
	thread_local_routing_stats st;
	auto out = route(probe("\tmov.u64 \t%rd52, 14;\n", "%rd52"),
			 { sum14(0) }, st);
	REQUIRE(st.sum_lookups == 0);
	REQUIRE(st.global_atomics == 0);
	REQUIRE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE_FALSE(has(out, "%smid"));
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
	REQUIRE(st2.sum_lookups == 1);
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

TEST_CASE("Any other write of the lookup register keeps the guarded atomic",
	  "[sum-map-routing]")
{
	// forms a line-based definition test missed (review of 026f490f)
	const char *other[] = {
		"\tld.local.v2.u64 \t{%rd55, %rd70}, [%rd1+16];\n",
		"\tcall.uni (%rd55), f, (%rd9);\n",
		"\tmov.u64 \t%rd1, 0; mov.u64 \t%rd55, %rd9;\n",
		"$L__BB0_6: add.u64 \t%rd55, %SP, 8;\n",
		"\t@ %p1 add.u64 \t%rd55, %SP, 8;\n",
		"\t@!%p1 cvt.u64.u32 \t%rd55, %r9;\n",
		"\t.loc 1 5 3\n\tadd.u64 \t%rd55, %SP, 8;\n",
		"\t/* x, y */ add.u64 \t%rd55, %SP, 8;\n",
		"\tsetp.ne.s64 \t%p3|%p4, %rd9, %rd55;\n", // a read: still fine
	};
	for (size_t i = 0; i < sizeof(other) / sizeof(other[0]); i++) {
		thread_local_routing_stats st;
		auto out = route(probe("", "14", other[i]), { sum14() }, st);
		INFO(other[i]);
		REQUIRE(st.sum_lookups == 1);
		if (i + 1 == sizeof(other) / sizeof(other[0])) {
			REQUIRE(st.global_atomics == 1);
		} else {
			REQUIRE(st.global_atomics == 0);
			REQUIRE(st.guarded_atomics == 1);
		}
	}
}

TEST_CASE("A map fd register written twice is not a constant fd",
	  "[sum-map-routing]")
{
	for (const char *second : { "\tneg.s64 \t%rd52, %rd9;\n",
				    "\tnot.b64 \t%rd52, %rd9;\n",
				    "\trem.u64 \t%rd52, %rd9, 16;\n",
				    "\tld.local.v2.u64 \t{%rd52, %rd70}, [%rd1];\n" }) {
		thread_local_routing_stats st;
		auto out = route(probe(std::string("\tmov.u64 \t%rd52, 14;\n") +
					       second,
				       "%rd52"),
				 { sum14() }, st);
		INFO(second);
		REQUIRE(st.sum_lookups == 0);
		REQUIRE(has(out, "_bpf_helper_ext_0001"));
	}
}

TEST_CASE("Only 64-bit adds on a sum map's value take the global fast path",
	  "[sum-map-routing]")
{
	for (const char *atom : { "atom.add.u32 \t%r56, [%rd55], 1;",
				  "atom.exch.b64 \t%rd56, [%rd55], %rd9;",
				  "atom.or.b64 \t%rd56, [%rd55], %rd9;" }) {
		thread_local_routing_stats st;
		auto out = route(probe("", "14", "", atom), { sum14() }, st);
		INFO(atom);
		REQUIRE(st.sum_lookups == 1);
		REQUIRE(st.global_atomics == 0);
		REQUIRE(st.guarded_atomics == 1);
	}
}

TEST_CASE("Without a thread-local map kernels are not routed and helpers get 0",
	  "[sum-map-routing]")
{
	const std::string kernel = ".visible .entry kern()\n{\n"
				   "\tcall __retprobe_func__kern;\n\tret;\n}\n";
	thread_local_routing_stats st;
	auto out = route(probe("", "15") + kernel, { sum14() }, st);
	REQUIRE(has(out, kernel));
	REQUIRE(has(out, ".visible .func __retprobe_func__kern()"));
	REQUIRE(st.kernels == 0);
	REQUIRE(st.routed_functions == 0);
	REQUIRE(st.helper_calls_given_zero == 1);
	REQUIRE(has(out, "st.param.b64 \t[param4+0], 0;"));
	REQUIRE_FALSE(has(out, "[param4+0], %rd12;"));
	REQUIRE_FALSE(has(out, "__bpftime_tl"));
	// with one, the kernel gets the area and the probe its parameter
	thread_local_map_desc tl;
	tl.fd = 4;
	tl.offset = 0;
	tl.value_size = 8;
	tl.max_entries = 1;
	thread_local_routing_stats st2;
	auto out2 = route_thread_local_maps(probe("", "14") + kernel, { K },
					    { tl }, 8, &st2, { sum14() });
	REQUIRE(st2.kernels == 1);
	REQUIRE(st2.routed_functions == 1);
	REQUIRE(has(out2, "__bpftime_tl"));
	REQUIRE(st2.sum_lookups == 1);
	REQUIRE(st2.global_atomics == 1);
}

namespace
{
// llvmbpf's frame and the key stored into slot SP+504 just before the call
const std::string FRAME = "\tmov.u64 \t%SPL, __local_depot0;\n"
			  "\tcvta.local.u64 \t%SP, %SPL;\n"
			  "\tadd.u64 \t%rd9, %SP, 0;\n"
			  "\tadd.u64 \t%rd1, %SPL, 0;\n"
			  "\tadd.s64 \t%rd2, %rd9, 512;\n";
const std::string STORE = "\tst.local.u32 \t[%rd1+504], %r7;\n";
} // namespace

TEST_CASE("A lookup takes its key from the store into its stack slot",
	  "[sum-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(probe(FRAME + STORE, "14"), { sum14() }, st);
	REQUIRE(st.sum_lookups == 1);
	REQUIRE(st.keys_forwarded == 1);
	REQUIRE(has(out, "\tmov.u32 \t%bpsm_k0, %r7;\n"));
	REQUIRE_FALSE(has(out, "ld.u32 \t%bpsm_k0"));
	// a 64-bit register stored with st.u32 (the low half), generic store
	thread_local_routing_stats st2;
	auto out2 = route(probe(FRAME + "\tst.u32 \t[%rd2+-8], %rd30;\n", "14"),
			  { sum14() }, st2);
	REQUIRE(st2.keys_forwarded == 1);
	REQUIRE(has(out2, "\tcvt.u32.u64 \t%bpsm_k0, %rd30;\n"));
	// an immediate
	thread_local_routing_stats st3;
	auto out3 = route(probe(FRAME + "\tst.local.u32 \t[%rd1+504], 3;\n", "14"),
			  { sum14() }, st3);
	REQUIRE(st3.keys_forwarded == 1);
	REQUIRE(has(out3, "\tmov.u32 \t%bpsm_k0, 3;\n"));
}

TEST_CASE("A key that may have changed since its store is loaded again",
	  "[sum-map-routing]")
{
	const std::string cases[] = {
		// another path joins in between
		FRAME + STORE + "$L__BB0_4:\n",
		// a call in between (a helper may write the stack)
		FRAME + STORE +
			"\t{ // callseq 3, 0\n\tcall.uni f, ();\n\t} // callseq 3\n",
		// a store the pass cannot place in the frame
		FRAME + STORE + "\tst.u32 \t[%rd77], %r2;\n",
		// a store into part of the key
		FRAME + STORE + "\tst.local.u8 \t[%rd1+505], %rs1;\n",
		// a wider store over the key
		FRAME + STORE + "\tst.local.u64 \t[%rd1+504], %rd5;\n",
		// the stored register changes
		FRAME + STORE + "\tadd.s32 \t%r7, %r7, 1;\n",
		// an atomic in between
		FRAME + STORE + "\tatom.add.u32 \t%r9, [%rd77], 1;\n",
		// the store under a predicate (it may not have run)
		FRAME + "\t@%p3 st.local.u32 \t[%rd1+504], %r7;\n",
		FRAME + "\t@!%p3 st.local.u32 \t[%rd1+504], %r7;\n",
		// a guarded store after the key's store
		FRAME + STORE + "\t@%p3 st.local.u32 \t[%rd1+504], %r8;\n",
		// no store at all
		FRAME,
		// a frame that is not llvmbpf's
		"\tmov.u64 \t%SPL, __local_depot0;\n\tmov.u64 \t%SP, %rd77;\n"
		"\tadd.u64 \t%rd9, %SP, 0;\n\tadd.u64 \t%rd1, %SPL, 0;\n"
		"\tadd.s64 \t%rd2, %rd9, 512;\n" +
			STORE,
		// the slot register written twice
		FRAME + STORE + "\tmov.u64 \t%rd2, %rd77;\n",
	};
	for (const auto &c : cases) {
		thread_local_routing_stats st;
		auto out = route(probe(c, "14"), { sum14() }, st);
		INFO(c);
		REQUIRE(st.sum_lookups == 1);
		REQUIRE(st.keys_forwarded == 0);
		REQUIRE(has(out, "ld.u32 \t%bpsm_k0, [%rd51];"));
	}
	// a store to another slot in between does not matter
	thread_local_routing_stats st;
	auto out = route(probe(FRAME + STORE + "\tst.local.u32 \t[%rd1+500], %r2;\n",
			       "14"),
			 { sum14() }, st);
	REQUIRE(st.keys_forwarded == 1);
}
