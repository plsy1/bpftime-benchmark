#include "catch2/catch_test_macros.hpp"
#include "nv_attach_impl.hpp"
#include "nv_thread_local_routing.hpp"
#include "trampoline/gpu_devhash_layout.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

using namespace bpftime::attach;

// Device hash maps (1533) in the routing pass (plan results
// gpu/hash-build-plan-20261127, S6): a constant-fd lookup becomes the inline
// hit path with the walk's call behind it, an update the insert's call, a
// delete -22; the devhash functions go in once at the start of the module
// body. Without a device copy, or with a non-constant fd, the trampoline
// keeps the call.
namespace
{
const char *K = "kern";
const std::string HEAD = ".version 8.1\n.target sm_87\n.address_size 64\n";
const uint64_t BASE = 0x203400000ull;

bool has(const std::string &s, const std::string &x)
{
	return s.find(x) != std::string::npos;
}

size_t count(const std::string &s, const std::string &x)
{
	size_t n = 0;
	for (size_t p = s.find(x); p != std::string::npos; p = s.find(x, p + 1))
		n++;
	return n;
}

hash_map_desc h14(uint64_t base = BASE, uint64_t entries = 260,
		  uint64_t shards = 256)
{
	hash_map_desc d;
	d.fd = 14;
	d.base = base;
	d.max_entries = entries;
	d.shards = shards;
	return d;
}

// one helper call as llvmbpf emits it: map in param0, key pointer in
// param1, then param2 (value), param3 (flags), param4
std::string call(int seq, int helper, const std::string &fd,
		 const std::string &key, const std::string &ret,
		 const std::string &value = "%rd57",
		 const std::string &flags = "%rd11")
{
	char h[32];
	snprintf(h, sizeof(h), "_bpf_helper_ext_%04d", helper);
	return "\t{ // callseq " + std::to_string(seq) +
	       ", 0\n"
	       "\t.reg .b32 temp_param_reg;\n"
	       "\t.param .b64 param0;\n"
	       "\tst.param.b64 \t[param0+0], " +
	       fd +
	       ";\n"
	       "\t.param .b64 param1;\n"
	       "\tst.param.b64 \t[param1+0], " +
	       key +
	       ";\n"
	       "\t.param .b64 param2;\n"
	       "\tst.param.b64 \t[param2+0], " +
	       value +
	       ";\n"
	       "\t.param .b64 param3;\n"
	       "\tst.param.b64 \t[param3+0], " +
	       flags +
	       ";\n"
	       "\t.param .b64 param4;\n"
	       "\tst.param.b64 \t[param4+0], %rd12;\n"
	       "\t.param .b64 retval0;\n"
	       "\tcall.uni (retval0), \n\t" +
	       h +
	       ", \n\t(\n"
	       "\tparam0, \n\tparam1, \n\tparam2, \n\tparam3, \n\tparam4\n\t);\n"
	       "\tld.param.b64 \t" +
	       ret +
	       ", [retval0+0];\n"
	       "\t} // callseq " +
	       std::to_string(seq) + "\n";
}

const std::string FRAME = "\t.local .align 8 .b8 \t__local_depot0[592];\n"
			  "\t.reg .b64 \t%SP;\n\t.reg .b64 \t%SPL;\n"
			  "\t.reg .pred \t%p<20>;\n\t.reg .b32 \t%r<20>;\n"
			  "\t.reg .b64 \t%rd<80>;\n"
			  "\tmov.u64 \t%SPL, __local_depot0;\n"
			  "\tcvta.local.u64 \t%SP, %SPL;\n"
			  "\tadd.u64 \t%rd9, %SP, 0;\n"
			  "\tadd.u64 \t%rd1, %SPL, 0;\n"
			  "\tadd.s64 \t%rd2, %rd9, 512;\n"
			  "\tadd.s64 \t%rd51, %rd2, -8;\n";

// a probe function: its body between FRAME and a final ret
std::string func(const std::string &name, const std::string &body)
{
	return ".visible .func " + name + "()\n{\n" + FRAME + body +
	       "\tret;\n\n}\n";
}

// lookup into %rd55, null check, add 1 at the result
std::string lookup_and_add(const std::string &fd_def = "\tmov.u64 \t%rd52, 14;\n",
			   const std::string &fd = "%rd52", int seq = 20,
			   const std::string &ret = "%rd55",
			   const std::string &label = "$L__BB0_5")
{
	return fd_def + call(seq, 1, fd, "%rd51", ret) +
	       "\tsetp.eq.s64 \t%p10, " + ret + ", 0;\n\t@%p10 bra \t" +
	       label + ";\n\tatom.add.u64 \t%rd56, [" + ret + "], 1;\n" +
	       label + ":\n";
}

std::string route(const std::string &ptx,
		  const std::vector<hash_map_desc> &h,
		  thread_local_routing_stats &st)
{
	return route_thread_local_maps(ptx, { K }, {}, 0, &st, {}, h);
}

std::string emitted_lookup(const std::string &out, int n = 0)
{
	const std::string open =
		"{ // bpftime hash-map lookup, map 14\n";
	size_t at = 0;
	for (int i = 0; i <= n; i++) {
		at = out.find(open, i == 0 ? 0 : at + 1);
		if (at == std::string::npos)
			return "";
	}
	size_t end = out.find("} // bpftime hash-map lookup\n", at);
	return end == std::string::npos ? "" : out.substr(at, end - at);
}
} // namespace

TEST_CASE("A constant-fd hash-map lookup is inline with the walk behind it",
	  "[hash-map-routing]")
{
	namespace dh = bpftime_devhash;
	const dh::layout l = dh::layout_for(260, 256);
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern", lookup_and_add()),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 1);
	REQUIRE(st.global_atomics == 1);
	REQUIRE(st.guarded_atomics == 0);
	REQUIRE(st.devhash_text_inserted == 1);
	REQUIRE_FALSE(has(out, "_bpf_helper_ext_0001"));
	const std::string b = emitted_lookup(out);
	REQUIRE_FALSE(b.empty());
	// the home slot: (key ^ SEED) * HASH_MUL >> (64 - logS), one strong
	// header load at base + dir
	REQUIRE(has(b, "\tld.u32 \t%bph_k0, [%rd51];\n"));
	REQUIRE(has(b, "xor.b64 \t%bph_x0, %bph_kk0, " +
			       std::to_string((long long)dh::SEED) + ";\n"));
	REQUIRE(has(b, "mul.lo.u64 \t%bph_x0, %bph_x0, " +
			       std::to_string((long long)dh::HASH_MUL) + ";\n"));
	REQUIRE(has(b, "shr.u64 \t%bph_x0, %bph_x0, 54;\n"));
	REQUIRE(has(b, "add.u64 \t%bph_a0, %bph_x0, " +
			       std::to_string(BASE + l.dir) + ";\n"));
	REQUIRE(has(b, "\tld.relaxed.gpu.global.b64 \t%bph_h0, [%bph_a0];\n"));
	// memory loads: the key and the header (and the call's result)
	REQUIRE(count(b, "ld.") - count(b, "ld.param.") == 2);
	REQUIRE_FALSE(has(b, "ld.global"));
	// live K: the calling thread's shard of the row
	REQUIRE(has(b, "setp.eq.u64 \t%bph_p0, %bph_x0, 2;\n"
		       "\tsetp.eq.and.u32 \t%bph_p0, %bph_hk0, %bph_k0, %bph_p0;\n"
		       "\t@%bph_p0 bra \t$Lbph_hit_0;\n"));
	REQUIRE(has(b, "and.b32 \t%bph_row0, %bph_row0, 536870911;\n"));
	REQUIRE(has(b, "mad.lo.u32 \t%bph_s0, %bph_row0, 256, %bph_s0;\n"));
	REQUIRE(has(b, "add.u64 \t%bph_v0, %bph_x0, " +
			       std::to_string(BASE + l.rows) + ";\n"));
	REQUIRE_FALSE(has(b, "selp")); // r = max_entries: no single rows
	// empty: NULL without a call; otherwise the walk
	REQUIRE(has(b, "\tmov.u64 \t%bph_v0, 0;\n"
		       "\tsetp.eq.u64 \t%bph_q0, %bph_x0, 0;\n"
		       "\t@%bph_q0 bra \t$Lbph_join_0;\n"));
	REQUIRE(has(b, "\tcall (__bph_c0_r), __bpftime_devhash_lookup, (__bph_c0_a0, __bph_c0_a1, __bph_c0_a2, __bph_c0_a3);\n"
		       "\tld.param.b64 \t%bph_v0, [__bph_c0_r];\n"));
	REQUIRE(has(b, "$Lbph_join_0:\n\tmov.u64 \t%rd55, %bph_v0;\n"));
	// the program's register: one definition, its add global
	// (in the probe function: the devhash text has registers of its own)
	const std::string probe =
		out.substr(out.find(".visible .func __retprobe_func__kern"));
	REQUIRE(count(probe, "\t%rd55, ") == 1);
	REQUIRE_FALSE(has(probe, "[retval0+0];"));
	REQUIRE(has(out, "atom.global.add.u64 %rd56, [%bpag_a0], 1;"));
	REQUIRE(has(out, "setp.eq.s64 \t%p10, %rd55, 0;"));
}

TEST_CASE("A row at or above r is the row's single word", "[hash-map-routing]")
{
	namespace dh = bpftime_devhash;
	const dh::layout l = dh::layout_for(600, 256);
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern", lookup_and_add()),
			 { h14(BASE, 600) }, st);
	REQUIRE(st.hash_lookups == 1);
	const std::string b = emitted_lookup(out);
	REQUIRE(has(b, "setp.lt.u32 \t%bph_q0, %bph_row0, 512;\n"
		       "\tsub.u32 \t%bph_l0, %bph_row0, 512;\n"
		       "\tmul.wide.u32 \t%bph_a0, %bph_l0, 8;\n"
		       "\tadd.u64 \t%bph_a0, %bph_a0, " +
			       std::to_string(BASE + l.single) + ";\n"));
	REQUIRE(has(b, "selp.b64 \t%bph_v0, %bph_v0, %bph_a0, %bph_q0;\n"));
	REQUIRE(has(b, "shr.u64 \t%bph_x0, %bph_x0, 53;\n")); // 2048 slots
}

TEST_CASE("A hash map without a device copy keeps the trampoline's calls",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     lookup_and_add() +
					     call(21, 2, "%rd52", "%rd51",
						  "%rd60")),
			 { h14(0) }, st);
	REQUIRE(st.hash_lookups == 0);
	REQUIRE(st.hash_inserts == 0);
	REQUIRE(st.devhash_text_inserted == 0);
	REQUIRE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE(has(out, "_bpf_helper_ext_0002"));
	REQUIRE_FALSE(has(out, "%bph"));
	REQUIRE_FALSE(has(out, "__bpftime_devhash"));
}

TEST_CASE("A map fd register written twice is not a constant fd for a hash map",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     lookup_and_add(
					     "\tmov.u64 \t%rd52, 14;\n"
					     "\tmov.u64 \t%rd52, 15;\n")),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 0);
	REQUIRE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE(st.devhash_text_inserted == 0);
}

TEST_CASE("An update becomes the insert's call, a delete -22",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     "\tmov.u64 \t%rd52, 14;\n" +
					     call(30, 2, "%rd52", "%rd51",
						  "%rd60", "%rd58", "1") +
					     call(31, 3, "%rd52", "%rd51",
						  "%rd61")),
			 { h14() }, st);
	REQUIRE(st.hash_inserts == 1);
	REQUIRE(st.hash_deletes == 1);
	REQUIRE(st.devhash_text_inserted == 1);
	REQUIRE_FALSE(has(out, "_bpf_helper_ext_0002"));
	REQUIRE_FALSE(has(out, "_bpf_helper_ext_0003"));
	REQUIRE(has(out, "\tmov.u64 \t%bphu_b0, " + std::to_string(BASE) +
				 ";\n\tmov.u64 \t%bphu_m0, 260;\n"
				 "\tmov.u64 \t%bphu_n0, 256;\n"
				 "\tmov.u64 \t%bphu_v0, %rd58;\n"
				 "\tmov.u64 \t%bphu_f0, 1;\n"));
	REQUIRE(has(out, "\tcall (__bphu_c0_r), __bpftime_devhash_insert, (__bphu_c0_a0, __bphu_c0_a1, __bphu_c0_a2, __bphu_c0_a3, __bphu_c0_a4, __bphu_c0_a5);\n"
			 "\tld.param.b64 \t%rd60, [__bphu_c0_r];\n"));
	REQUIRE(has(out, "mov.u64 \t%rd61, -22; // bpftime hash-map delete, map 14"));
}

TEST_CASE("Deletes alone do not put the devhash functions in",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     "\tmov.u64 \t%rd52, 14;\n" +
					     call(31, 3, "%rd52", "%rd51",
						  "%rd61")),
			 { h14() }, st);
	REQUIRE(st.hash_deletes == 1);
	REQUIRE(st.devhash_text_inserted == 0);
	REQUIRE_FALSE(has(out, "__bpftime_devhash"));
}

TEST_CASE("Two inline lookups in one function get their own labels",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(
		HEAD + func("__retprobe_func__kern",
			    lookup_and_add() +
				    lookup_and_add("", "14", 22, "%rd65",
						   "$L__BB0_7")),
		{ h14() }, st);
	REQUIRE(st.hash_lookups == 2);
	REQUIRE(count(out, "$Lbph_hit_0:") == 1);
	REQUIRE(count(out, "$Lbph_hit_1:") == 1);
	REQUIRE(count(out, "$Lbph_join_1:") == 1);
	REQUIRE(has(emitted_lookup(out, 1), "%bph_k1"));
	REQUIRE(st.global_atomics == 2);
}

TEST_CASE("The devhash functions go in once, before every function",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__probe_func__kern", lookup_and_add()) +
				 func("__retprobe_func__kern", lookup_and_add()),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 2);
	const std::string def = "__bpftime_devhash_lookup(\n";
	REQUIRE(count(out, ".func  (.param .b64 func_retval0) __bpftime_devhash_lookup(") == 1);
	REQUIRE(count(out, ".func  (.param .b64 func_retval0) __bpftime_devhash_insert(") == 1);
	const size_t text = out.find("__bpftime_devhash_lookup(");
	REQUIRE(text < out.find(".visible .func __probe_func__kern"));
	REQUIRE(text > out.find(".address_size 64"));
	REQUIRE_FALSE(has(out, ".visible .func  (.param .b64 func_retval0) __bpftime_devhash"));
}

TEST_CASE("A hash-map lookup takes its key from the store into its stack slot",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     "\tst.local.u32 \t[%rd1+504], %r7;\n" +
					     lookup_and_add("", "14")),
			 { h14() }, st);
	REQUIRE(st.keys_forwarded == 1);
	REQUIRE(has(out, "\tmov.u32 \t%bph_k0, %r7;\n"));
	REQUIRE_FALSE(has(out, "ld.u32 \t%bph_k0"));
}

TEST_CASE("ptxas accepts a routed module with an inline lookup and an update",
	  "[hash-map-routing]")
{
	const char *ptxas = "/usr/local/cuda/bin/ptxas";
	if (access(ptxas, X_OK) != 0)
		SKIP("no ptxas");
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     lookup_and_add() +
					     call(30, 2, "%rd52", "%rd51",
						  "%rd60", "%rd51", "1")),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 1);
	REQUIRE(st.hash_inserts == 1);
	char path[] = "/tmp/bpftime-hash-routing-XXXXXX";
	const int fd = mkstemp(path);
	REQUIRE(fd >= 0);
	REQUIRE(write(fd, out.data(), out.size()) == (ssize_t)out.size());
	close(fd);
	const std::string cmd = std::string(ptxas) + " -arch=sm_87 -o /dev/null " +
				path + " > " + path + ".log 2>&1";
	const int rc = std::system(cmd.c_str());
	std::string log;
	if (FILE *f = fopen((std::string(path) + ".log").c_str(), "r")) {
		char buf[4096];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		buf[n] = 0;
		log = buf;
		fclose(f);
	}
	unlink(path);
	unlink((std::string(path) + ".log").c_str());
	INFO(log);
	CHECK(rc == 0);
}

TEST_CASE("A module whose only device-model map is a hash map is routed",
	  "[hash-map-routing]")
{
	bpftime::attach::nv_attach_impl impl;
	std::vector<MapBasicInfo> info(15);
	for (auto &m : info)
		m = MapBasicInfo{ false, 0, 0, 0, 0, nullptr, 0 };
	info[14] = MapBasicInfo{ true, 4, 8, 260, 1533, (void *)(uintptr_t)BASE,
				 256 };
	impl.map_basic_info = info;
	const std::string ptx =
		HEAD + func("__retprobe_func__kern", lookup_and_add());
	const std::string out =
		impl.route_thread_local_maps_of_module(ptx, "gate.ptx");
	REQUIRE(has(out, "{ // bpftime hash-map lookup, map 14"));
	REQUIRE(has(out, "__bpftime_devhash_lookup("));
	// without a copy (base 0) the module keeps the trampoline's call
	info[14].extra_buffer = nullptr;
	impl.map_basic_info = info;
	const std::string out0 =
		impl.route_thread_local_maps_of_module(ptx, "gate.ptx");
	REQUIRE(has(out0, "_bpf_helper_ext_0001"));
	REQUIRE_FALSE(has(out0, "__bpftime_devhash"));
}

// ---- mirrors of test_sum_map_routing.cpp's cases for 1533 (plan S6)
namespace
{
// a lookup of map 14 into %rd55, `extra` after it, then the null check
// and `atom` at the result
std::string lookup_extra(const std::string &extra,
			 const std::string &atom = "atom.add.u64 \t%rd56, [%rd55], 1;",
			 const std::string &fd = "14")
{
	return call(20, 1, fd, "%rd51", "%rd55") + extra +
	       "\tsetp.eq.s64 \t%p10, %rd55, 0;\n\t@%p10 bra \t$L__BB0_5;\n\t" +
	       atom + "\n$L__BB0_5:\n";
}
} // namespace

TEST_CASE("A hash-map address register written elsewhere keeps the guarded atomic",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     lookup_extra("\tmov.b64 \t%rd55, %rd9;\n")),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 1);
	REQUIRE(st.global_atomics == 0);
	REQUIRE(st.guarded_atomics == 1);
	REQUIRE(has(out, "isspacep.local"));
}

TEST_CASE("Hash-map lookups joined into one register keep the global atomic",
	  "[hash-map-routing]")
{
	// a second lookup (another branch) writes the same register
	const std::string second = call(21, 1, "14", "%rd60", "%rd55");
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     lookup_extra(second)),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 2);
	REQUIRE(st.global_atomics == 1);
	REQUIRE(st.guarded_atomics == 0);
	// one of them from a map without a device copy: not only device
	// addresses any more
	hash_map_desc other = h14(0);
	other.fd = 16;
	thread_local_routing_stats st2;
	auto out2 = route(HEAD + func("__retprobe_func__kern",
				      lookup_extra(call(21, 1, "16", "%rd60",
							"%rd55"))),
			  { h14(), other }, st2);
	REQUIRE(st2.hash_lookups == 1);
	REQUIRE(st2.global_atomics == 0);
	REQUIRE(st2.guarded_atomics == 1);
}

TEST_CASE("A lookup of a map that is not a device hash map keeps the call and the guard",
	  "[hash-map-routing]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     lookup_extra("", "atom.add.u64 \t%rd56, [%rd55], 1;",
						  "15")),
			 { h14() }, st);
	REQUIRE(st.hash_lookups == 0);
	REQUIRE(has(out, "_bpf_helper_ext_0001"));
	REQUIRE(st.guarded_atomics == 1);
	REQUIRE(st.devhash_text_inserted == 0);
}

TEST_CASE("Only 64-bit adds on a hash map's value take the global fast path",
	  "[hash-map-routing]")
{
	for (const char *atom : { "atom.add.u32 \t%r16, [%rd55], 1;",
				  "atom.exch.b64 \t%rd56, [%rd55], %rd9;",
				  "atom.or.b64 \t%rd56, [%rd55], %rd9;" }) {
		thread_local_routing_stats st;
		auto out = route(HEAD + func("__retprobe_func__kern",
					     lookup_extra("", atom)),
				 { h14() }, st);
		INFO(atom);
		REQUIRE(st.hash_lookups == 1);
		REQUIRE(st.global_atomics == 0);
		REQUIRE(st.guarded_atomics == 1);
	}
}

TEST_CASE("A hash-map call block with more than the call is refused, a commented marker ignored",
	  "[hash-map-routing]")
{
	std::string ptx = HEAD + func("__retprobe_func__kern", lookup_extra(""));
	const std::string at = "\t.param .b64 retval0;\n";
	ptx.insert(ptx.find(at), "\tst.local.u32 \t[%rd1+504], %r9;\n");
	thread_local_routing_stats st;
	REQUIRE_THROWS_AS(route(ptx, { h14() }, st), std::runtime_error);
	thread_local_routing_stats st2;
	auto out = route(HEAD + func("__retprobe_func__kern",
				     "\t// { // callseq 99, 0 }\n" +
					     lookup_extra("")),
			 { h14() }, st2);
	REQUIRE(st2.hash_lookups == 1);
	REQUIRE(has(out, "// { // callseq 99, 0 }"));
}
