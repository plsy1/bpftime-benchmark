// Per-process cost of preparing a program: JIT compile (what every traced process does now)
// against linking an AOT object compiled once (what a shared code cache would do).
//   bench_aot jit   <bpf.o> <p1,p2,...>          -> "jit_ms X" (all programs, one process)
//   bench_aot build <bpf.o> <p1,...> <dir>       -> writes dir/<p>.o, "aot_compile_ms X"
//   bench_aot load  <bpf.o> <p1,...> <dir>       -> "load_ms X"
// One measurement per process: the first LLJIT of a process pays LLVM's one-time set-up, as a
// traced process does. Helpers: the AOT tool's 1000 placeholders (same for every mode).
#include "bpftime_helper_group.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_config.hpp"
#include "ebpf-vm.h"
#include <bpf/libbpf.h>
#include <bpftime_vm_compat.hpp>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

static int empty_helper()
{
	return 0;
}

static double ms_since(std::chrono::steady_clock::time_point t0)
{
	return std::chrono::duration<double, std::milli>(
		       std::chrono::steady_clock::now() - t0)
		.count();
}

static int quiet(libbpf_print_level, const char *, va_list)
{
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s jit|build|load bpf.o prog [obj]\n",
			argv[0]);
		return 2;
	}
	std::string mode = argv[1];
	// No shared segment here: the process-local config (VM "llvm", as the agent default).
	bpftime_set_runtime_config(bpftime::runtime_config{});
	libbpf_set_print(quiet);
	bpf_object *obj = bpf_object__open(argv[2]);
	if (!obj) {
		fprintf(stderr, "open %s failed\n", argv[2]);
		return 1;
	}
	// argv[3]: comma-separated program names; argv[4]: directory of the AOT objects.
	std::vector<std::string> names;
	{
		std::string list = argv[3], cur;
		for (char c : list + ",") {
			if (c == ',') {
				if (!cur.empty())
					names.push_back(cur);
				cur.clear();
			} else
				cur += c;
		}
	}
	std::vector<std::unique_ptr<bpftime::bpftime_prog> > progs;
	double total = 0, extra = 0;
	size_t insns = 0;
	for (auto &name : names) {
		bpf_program *prog =
			bpf_object__find_program_by_name(obj, name.c_str());
		if (!prog) {
			fprintf(stderr, "no program %s; programs:", name.c_str());
			bpf_program *p;
			bpf_object__for_each_program(p, obj)
				fprintf(stderr, " %s", bpf_program__name(p));
			fprintf(stderr, "\n");
			return 1;
		}
		insns += bpf_program__insn_cnt(prog);
		auto bp = std::make_unique<bpftime::bpftime_prog>(
			(const ebpf_inst *)bpf_program__insns(prog),
			bpf_program__insn_cnt(prog), name.c_str());
		bpftime::bpftime_helper_group helpers;
		for (int i = 0; i < 1000; i++) {
			bpftime::bpftime_helper_info info;
			info.index = i;
			info.name = "empty_helper";
			info.fn = (void *)empty_helper;
			helpers.register_helper(info);
		}
		helpers.add_helper_group_to_prog(bp.get());
		std::string objpath =
			argc > 4 ? std::string(argv[4]) + "/" + name + ".o" : "";
		if (mode == "jit" || mode == "build") {
			auto t0 = std::chrono::steady_clock::now();
			int r = bp->bpftime_prog_load(true);
			total += ms_since(t0);
			if (r < 0) {
				fprintf(stderr, "bpftime_prog_load %s failed %d\n",
					name.c_str(), r);
				return 1;
			}
			if (mode == "build") {
				auto vm = bp->get_vm();
				auto t1 = std::chrono::steady_clock::now();
				auto out = vm->vm_instance->do_aot_compile(false);
				extra += ms_since(t1);
				std::ofstream ofs(objpath, std::ios::binary);
				ofs.write((const char *)out.data(), out.size());
				printf("object %s %zu bytes\n", name.c_str(),
				       out.size());
			}
		} else if (mode == "load") {
			std::ifstream ifs(objpath, std::ios::binary);
			std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)),
						 std::istreambuf_iterator<char>());
			auto t0 = std::chrono::steady_clock::now();
			int r = bp->load_aot_object(buf);
			total += ms_since(t0);
			if (r < 0) {
				fprintf(stderr, "load_aot_object %s failed %d\n",
					name.c_str(), r);
				return 1;
			}
		} else {
			fprintf(stderr, "unknown mode\n");
			return 2;
		}
		progs.push_back(std::move(bp));
	}
	printf("programs %zu insns %zu %s_ms %.3f", names.size(), insns,
	       mode == "load" ? "load" : "jit", total);
	if (mode == "build")
		printf(" aot_compile_ms %.3f", extra);
	printf("\n");
	bpf_object__close(obj);
	return 0;
}
