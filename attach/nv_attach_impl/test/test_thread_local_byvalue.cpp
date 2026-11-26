#include "catch2/catch_test_macros.hpp"
#include "nv_thread_local_routing.hpp"
#include <string>
#include <vector>

using namespace bpftime::attach;

// The thread-local area by value (2026-11-26): a kernel whose probe
// functions reach the area only through constant-key lookups and 32-bit
// accesses with 64-bit registers passes it by value; any other form keeps
// the area in memory for the whole kernel.
namespace
{
const char *K = "kern";
const std::string ENTRY = "__probe_func__kern";
const std::string EXIT = "__retprobe_func__kern";

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

thread_local_map_desc tl(int fd = 4, uint64_t offset = 0, uint64_t entries = 1)
{
	thread_local_map_desc d;
	d.fd = fd;
	d.offset = offset;
	d.value_size = 8;
	d.max_entries = entries;
	return d;
}

std::string call_block(int seq, const std::string &helper,
		       const std::string &p0, const std::string &p1,
		       const std::string &ret)
{
	return "\t{ // callseq " + std::to_string(seq) +
	       ", 0\n"
	       "\t.reg .b32 temp_param_reg;\n"
	       "\t.param .b64 param0;\n"
	       "\tst.param.b64 \t[param0+0], " +
	       p0 +
	       ";\n"
	       "\t.param .b64 param1;\n"
	       "\tst.param.b64 \t[param1+0], " +
	       p1 +
	       ";\n"
	       "\t.param .b64 param2;\n"
	       "\tst.param.b64 \t[param2+0], %rd11;\n"
	       "\t.param .b64 param3;\n"
	       "\tst.param.b64 \t[param3+0], %rd12;\n"
	       "\t.param .b64 param4;\n"
	       "\tst.param.b64 \t[param4+0], %rd13;\n"
	       "\t.param .b64 retval0;\n"
	       "\tcall.uni (retval0), \n\t" +
	       helper +
	       ", \n\t(\n"
	       "\tparam0, \n\tparam1, \n\tparam2, \n\tparam3, \n\tparam4\n\t);\n"
	       "\tld.param.b64 \t" +
	       ret +
	       ", [retval0+0];\n"
	       "\t} // callseq " +
	       std::to_string(seq) + "\n";
}

// the gate every probe function starts with, and llvmbpf's frame
std::string start(const std::string &name)
{
	return ".visible .func " + name +
	       "()\n{\n"
	       "\t{\n"
	       "\t.reg .pred \t%bpftime_off;\n"
	       "\t.reg .b32 \t%bpftime_on;\n"
	       "\tld.global.u32 \t%bpftime_on, [__bpftime_probes_enabled];\n"
	       "\tsetp.eq.s32 \t%bpftime_off, %bpftime_on, 0;\n"
	       "\t@%bpftime_off ret;\n"
	       "\t}\n"
	       "\t.local .align 8 .b8 \t__local_depot0[592];\n"
	       "\t.reg .b64 \t%SP;\n"
	       "\t.reg .b64 \t%SPL;\n"
	       "\t.reg .pred \t%p<11>;\n"
	       "\t.reg .b32 \t%r<8>;\n"
	       "\t.reg .b64 \t%rd<58>;\n\n"
	       "\tmov.u64 \t%SPL, __local_depot0;\n"
	       "\tcvta.local.u64 \t%SP, %SPL;\n"
	       "\tadd.u64 \t%rd3, %SP, 0;\n"
	       "\tadd.u64 \t%rd4, %SPL, 0;\n";
}

// a constant-key lookup of map `fd` into `ret`, the key stored just before
std::string lookup(const std::string &key_def, const std::string &ret,
		   const std::string &fd = "4", int seq = 9)
{
	return key_def + "\tst.local.u32 \t[%rd4+508], %r1;\n"
			 "\tadd.s64 \t%rd1, %rd3, 508;\n"
			 "\tmov.u64 \t%rd5, " +
	       fd + ";\n" +
	       call_block(seq, "_bpf_helper_ext_0001", "%rd5", "%rd1", ret);
}

const std::string KEY0 = "\tmov.u32 \t%r1, 0;\n";

// lat's entry: the time into the 8 bytes, as two 32-bit stores
std::string entry(const std::string &key_def = KEY0,
		  const std::string &uses = "", const std::string &fd = "4")
{
	return start(ENTRY) + lookup(key_def, "%rd9", fd) +
	       "\tsetp.eq.s64 \t%p1, %rd9, 0;\n"
	       "\t@%p1 bra \t$L__BB0_2;\n" +
	       call_block(10, "_bpf_helper_ext_0502", "%rd5", "%rd1", "%rd14") +
	       uses +
	       "\tst.u32 \t[%rd9], %rd14;\n"
	       "\tshr.u64 \t%rd16, %rd14, 32;\n"
	       "\tst.u32 \t[%rd9+4], %rd16;\n"
	       "$L__BB0_2:\n"
	       "\tret;\n\n}\n";
}

// lat's exit: the start read back as two 32-bit loads, twice
std::string exit_fn(const std::string &uses = "")
{
	return start(EXIT) + lookup(KEY0, "%rd15") +
	       "\tsetp.eq.s64 \t%p1, %rd15, 0;\n"
	       "\t@%p1 bra \t$L__BB0_3;\n"
	       "\tld.u32 \t%rd17, [%rd15];\n"
	       "\tld.u32 \t%rd18, [%rd15+4];\n"
	       "\tshl.b64 \t%rd19, %rd18, 32;\n"
	       "\tor.b64 \t%rd40, %rd19, %rd17;\n" +
	       uses +
	       "\tld.u32 \t%rd26, [%rd15];\n"
	       "\tld.u32 \t%rd27, [%rd15+4];\n"
	       "$L__BB0_3:\n"
	       "\tret;\n\n}\n";
}

std::string kernel(const std::string &entry_call = "    call __probe_func__kern;\n",
		   const std::string &exit_call = "$L__BB1_9:\ncall __retprobe_func__kern;\n")
{
	return ".visible .entry kern()\n{\n" + entry_call +
	       "\t.reg .pred %p<3>;\n\tmov.u32 \t%r1, 1;\n" + exit_call +
	       "\tret;\n}\n";
}

const std::string HEAD = ".version 8.1\n.target sm_87\n.address_size 64\n";

std::string route(const std::string &ptx, thread_local_routing_stats &st,
		  std::vector<thread_local_map_desc> maps = { tl() },
		  uint64_t area = 8)
{
	return route_thread_local_maps(ptx, { K }, maps, area, &st, {});
}

std::string why(const thread_local_routing_stats &st)
{
	return st.byvalue_fallbacks.empty() ? "" : st.byvalue_fallbacks[0];
}

// the memory form: the area's address passed to every probe function
bool memory_form(const std::string &out)
{
	return has(out, "\t.local .align 16 .b8 \t__bpftime_tl[") &&
	       has(out, ", (%bpftime_tl);") && !has(out, "__bpftime_tv");
}
} // namespace

TEST_CASE("lat's probes take the thread-local area by value", "[tl-byvalue]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + exit_fn() + entry() + kernel(), st);
	INFO(why(st));
	REQUIRE(st.byvalue_kernels == 1);
	REQUIRE(st.byvalue_functions == 2);
	REQUIRE(st.byvalue_lookups == 2);
	REQUIRE(st.byvalue_accesses == 6);
	REQUIRE(st.byvalue_fallbacks.empty());
	REQUIRE(st.routed_functions == 0);
	// the kernel: a .local slot, zeroed, the word around each call
	REQUIRE(has(out, ".visible .entry kern()\n{\n"
			 "\t.local .align 8 .b8 \t__bpftime_tv[8];\n"
			 "\t.reg .b64 \t%bpftime_tv_a<1>, %bpftime_tv_r<1>;\n"
			 "\tst.local.u64 \t[__bpftime_tv+0], 0;\n\n"
			 "    ld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n"
			 "\tcall (%bpftime_tv_r0), __probe_func__kern, (%bpftime_tv_a0);\n"
			 "\tst.local.u64 \t[__bpftime_tv+0], %bpftime_tv_r0;\n"));
	REQUIRE(has(out, "$L__BB1_9:\n"
			 "ld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n"
			 "\tcall __retprobe_func__kern, (%bpftime_tv_a0);\n"
			 "\tret;\n"));
	REQUIRE_FALSE(has(out, "__bpftime_tl"));
	// the entry writes: the word in and out; the halves given back before
	// both rets
	const std::string give_back =
		"\tmov.b64 \t%bptv_x, {%bptv_h0, %bptv_h1};\n"
		"\tst.param.b64 \t[__probe_func__kern_bpftime_tv_r0], %bptv_x;\n";
	REQUIRE(has(out, ".visible .func (.param .b64 __probe_func__kern_bpftime_tv_r0) "
			 "__probe_func__kern(\n"
			 "\t.param .b64 __probe_func__kern_bpftime_tv_a0\n)\n{\n"
			 "\t.reg .b32 \t%bptv_h<2>;\n"
			 "\t.reg .b64 \t%bptv_x;\n"
			 "\tld.param.b64 \t%bptv_x, [__probe_func__kern_bpftime_tv_a0];\n"
			 "\tmov.b64 \t{%bptv_h0, %bptv_h1}, %bptv_x;\n"
			 "\t{\n"));
	REQUIRE(has(out, give_back + "\t@%bpftime_off ret;\n"));
	REQUIRE(has(out, "$L__BB0_2:\n" + give_back + "\tret;\n"));
	REQUIRE(has(out, "\tmov.u64 \t%rd9, 1; // bpftime thread-local lookup by value, map 4\n"));
	REQUIRE(has(out, "\tcvt.u32.u64 \t%bptv_h0, %rd14;\n"
			 "\tshr.u64 \t%rd16, %rd14, 32;\n"
			 "\tcvt.u32.u64 \t%bptv_h1, %rd16;\n"));
	// the exit only reads: the word in, nothing back
	REQUIRE(has(out, ".visible .func __retprobe_func__kern(\n"
			 "\t.param .b64 __retprobe_func__kern_bpftime_tv_a0\n)\n{\n"));
	REQUIRE_FALSE(has(out, "__retprobe_func__kern_bpftime_tv_r0"));
	REQUIRE(has(out, "\tcvt.u64.u32 \t%rd17, %bptv_h0;\n"
			 "\tcvt.u64.u32 \t%rd18, %bptv_h1;\n"));
	REQUIRE(has(out, "\tcvt.u64.u32 \t%rd26, %bptv_h0;\n"
			 "\tcvt.u64.u32 \t%rd27, %bptv_h1;\n"));
	REQUIRE(count(out, "st.param.b64 \t[__probe_func__kern_bpftime_tv_r0]") == 2);
	// nothing reaches memory through the constant any more
	REQUIRE_FALSE(has(out, "[%rd9"));
	REQUIRE_FALSE(has(out, "[%rd15"));
	REQUIRE_FALSE(has(out, "%bpftime_tl_in"));
	REQUIRE_FALSE(has(out, "_bpf_helper_ext_0001"));
}

TEST_CASE("The second word of a 16-byte area is two more halves", "[tl-byvalue]")
{
	thread_local_routing_stats st;
	// map 4's value at bytes 8-15
	auto out = route(HEAD + exit_fn() + entry() + kernel(), st, { tl(4, 8) },
			 16);
	INFO(why(st));
	REQUIRE(st.byvalue_kernels == 1);
	REQUIRE(has(out, "\t.local .align 8 .b8 \t__bpftime_tv[16];\n"
			 "\t.reg .b64 \t%bpftime_tv_a<2>, %bpftime_tv_r<2>;\n"
			 "\tst.local.u64 \t[__bpftime_tv+0], 0;\n"
			 "\tst.local.u64 \t[__bpftime_tv+8], 0;\n"));
	REQUIRE(has(out, "    ld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n"
			 "\tld.local.u64 \t%bpftime_tv_a1, [__bpftime_tv+8];\n"
			 "\tcall (%bpftime_tv_r0, %bpftime_tv_r1), __probe_func__kern, "
			 "(%bpftime_tv_a0, %bpftime_tv_a1);\n"
			 "\tst.local.u64 \t[__bpftime_tv+0], %bpftime_tv_r0;\n"
			 "\tst.local.u64 \t[__bpftime_tv+8], %bpftime_tv_r1;\n"));
	REQUIRE(has(out, ".visible .func (.param .b64 __probe_func__kern_bpftime_tv_r0, "
			 ".param .b64 __probe_func__kern_bpftime_tv_r1) __probe_func__kern(\n"
			 "\t.param .b64 __probe_func__kern_bpftime_tv_a0,\n"
			 "\t.param .b64 __probe_func__kern_bpftime_tv_a1\n)"));
	REQUIRE(has(out, "\t.reg .b32 \t%bptv_h<4>;\n"));
	REQUIRE(has(out, "\tmov.b64 \t{%bptv_h2, %bptv_h3}, %bptv_x;\n"));
	REQUIRE(has(out, "\tcvt.u32.u64 \t%bptv_h2, %rd14;\n"));
	REQUIRE(has(out, "\tcvt.u32.u64 \t%bptv_h3, %rd16;\n"));
	REQUIRE(has(out, "\tcvt.u64.u32 \t%rd17, %bptv_h2;\n"));
	REQUIRE(has(out, "\tst.param.b64 \t[__probe_func__kern_bpftime_tv_r1], %bptv_x;\n"));
}

TEST_CASE("A call site keeps its label and predicate", "[tl-byvalue]")
{
	thread_local_routing_stats st;
	auto out = route(HEAD + exit_fn() + entry() +
				 kernel("$L__BB1_1: @%p2 call __probe_func__kern;\n",
					"\t@!%p1 call __retprobe_func__kern;\n"),
			 st);
	INFO(why(st));
	REQUIRE(st.byvalue_kernels == 1);
	// the load unguarded after the label, call and write-back guarded
	REQUIRE(has(out, "$L__BB1_1: ld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n"
			 "\t@%p2 call (%bpftime_tv_r0), __probe_func__kern, (%bpftime_tv_a0);\n"
			 "\t@%p2 st.local.u64 \t[__bpftime_tv+0], %bpftime_tv_r0;\n"));
	REQUIRE(has(out, "\tld.local.u64 \t%bpftime_tv_a0, [__bpftime_tv+0];\n"
			 "\t@!%p1 call __retprobe_func__kern, (%bpftime_tv_a0);\n"));
}

TEST_CASE("A probe function that never touches the area takes nothing", "[tl-byvalue]")
{
	thread_local_routing_stats st;
	// the exit only tests its lookup for NULL
	std::string ex = start(EXIT) + lookup(KEY0, "%rd15") +
			 "\tsetp.ne.s64 \t%p1, %rd15, 0;\n"
			 "\tret;\n\n}\n";
	auto out = route(HEAD + ex + entry() + kernel(), st);
	INFO(why(st));
	REQUIRE(st.byvalue_kernels == 1);
	REQUIRE(has(out, "$L__BB1_9:\ncall __retprobe_func__kern;\n"));
	REQUIRE(has(out, ".visible .func __retprobe_func__kern()\n{"));
	REQUIRE(has(out, "\tmov.u64 \t%rd15, 1; // bpftime thread-local lookup by value, map 4\n"));
	REQUIRE(has(out, "\tsetp.ne.s64 \t%p1, %rd15, 0;\n"));
}

TEST_CASE("Calls on other maps get 0 for the area", "[tl-byvalue]")
{
	thread_local_routing_stats st;
	// the entry also looks up map 7 (not thread-local) with a constant fd
	std::string uses = "\tmov.u64 \t%rd20, 7;\n" +
			   call_block(11, "_bpf_helper_ext_0001", "%rd20", "%rd1",
				      "%rd21");
	auto out = route(HEAD + exit_fn() + entry(KEY0, uses) + kernel(), st);
	INFO(why(st));
	REQUIRE(st.byvalue_kernels == 1);
	REQUIRE(has(out, "\tst.param.b64 \t[param4+0], 0;\n"));
	REQUIRE(has(out, "_bpf_helper_ext_0001"));
}

TEST_CASE("Any other form keeps the area in memory for the whole kernel",
	  "[tl-byvalue]")
{
	struct form {
		const char *what;
		std::string entry_fn, exit_fn;
		std::vector<thread_local_map_desc> maps;
		uint64_t area;
	};
	const std::string ok_exit = exit_fn();
	const std::vector<thread_local_map_desc> m = { tl() };
	const std::vector<form> forms = {
		{ "key loaded, not a constant",
		  entry("\tld.u32 \t%r1, [%rd3+16];\n"), ok_exit, m, 8 },
		{ "key out of range", entry("\tmov.u32 \t%r1, 1;\n"), ok_exit, m,
		  8 },
		{ "key in octal", entry("\tmov.u32 \t%r1, 00;\n"), ok_exit, m, 8 },
		{ "result copied", entry(KEY0, "\tmov.u64 \t%rd20, %rd9;\n"),
		  ok_exit, m, 8 },
		{ "result stored to memory",
		  entry(KEY0, "\tst.u64 \t[%rd3+16], %rd9;\n"), ok_exit, m, 8 },
		{ "atomic through the result",
		  entry(KEY0, "\tatom.add.u64 \t%rd20, [%rd9], 1;\n"), ok_exit, m,
		  8 },
		{ "64-bit access", entry(KEY0, "\tst.u64 \t[%rd9], %rd14;\n"),
		  ok_exit, m, 8 },
		{ "32-bit register", entry(KEY0, "\tst.u32 \t[%rd9], %r3;\n"),
		  ok_exit, m, 8 },
		{ "8-bit access", entry(KEY0, "\tst.u8 \t[%rd9], %rd14;\n"),
		  ok_exit, m, 8 },
		{ "signed load", entry(KEY0, "\tld.s32 \t%rd20, [%rd9];\n"),
		  ok_exit, m, 8 },
		{ "offset past the value",
		  entry(KEY0, "\tst.u32 \t[%rd9+8], %rd14;\n"), ok_exit, m, 8 },
		{ "offset not 4-aligned",
		  entry(KEY0, "\tst.u32 \t[%rd9+2], %rd14;\n"), ok_exit, m, 8 },
		{ "guarded access",
		  entry(KEY0, "\t@%p1 st.u32 \t[%rd9], %rd14;\n"), ok_exit, m, 8 },
		{ "two statements on a line",
		  entry(KEY0, "\tst.u32 \t[%rd9], %rd14; shr.u64 \t%rd20, %rd14, 1;\n"),
		  ok_exit, m, 8 },
		{ "labelled access",
		  entry(KEY0, "$L__BB0_7: st.u32 \t[%rd9], %rd14;\n"), ok_exit, m,
		  8 },
		{ "compare with another register",
		  entry(KEY0, "\tsetp.eq.s64 \t%p2, %rd9, %rd14;\n"), ok_exit, m,
		  8 },
		{ "a directive glued to an access",
		  entry(KEY0, "\t.loc 1 5 3\n\tst.u32 \t[%rd9], %rd14;\n"), ok_exit,
		  m, 8 },
		{ "update of the thread-local map",
		  entry(KEY0, call_block(12, "_bpf_helper_ext_0002", "%rd5",
					 "%rd1", "%rd22")),
		  ok_exit, m, 8 },
		{ "map fd not a constant",
		  entry(KEY0, call_block(12, "_bpf_helper_ext_0001", "%rd30",
					 "%rd1", "%rd22")),
		  ok_exit, m, 8 },
		{ "ret with a label on its line", entry(), [] {
			  std::string e = exit_fn();
			  e.replace(e.find("$L__BB0_3:\n\tret;"),
				    std::string("$L__BB0_3:\n\tret;").size(),
				    "$L__BB0_3: ret;");
			  return e;
		  }(),
		  m, 8 },
		{ "area larger than 16 bytes", entry(), ok_exit, { tl(4, 16) },
		  24 },
		{ "only the exit not eligible", entry(),
		  exit_fn("\tld.u32 \t%r3, [%rd15];\n"), m, 8 },
	};
	for (const auto &f : forms) {
		INFO(f.what);
		thread_local_routing_stats st;
		std::string ptx = HEAD + f.exit_fn + f.entry_fn + kernel();
		auto out = route(ptx, st, f.maps, f.area);
		REQUIRE(st.byvalue_kernels == 0);
		REQUIRE(st.byvalue_functions == 0);
		REQUIRE(st.byvalue_fallbacks.size() == 1);
		REQUIRE(st.routed_functions == 2);
		REQUIRE(memory_form(out));
		REQUIRE(has(out, "%bpftime_tl_in"));
		REQUIRE_FALSE(has(out, "%bptv_"));
	}
}

TEST_CASE("A by-value function called outside its kernel is refused",
	  "[tl-byvalue]")
{
	thread_local_routing_stats st;
	std::string other = ".visible .entry other()\n{\n"
			    "\tcall __probe_func__kern;\n\tret;\n}\n";
	REQUIRE_THROWS(route(HEAD + exit_fn() + entry() + kernel() + other, st));
}
