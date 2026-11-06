#include "catch2/catch_message.hpp"
#include "frida_attach_utils.hpp"
#include <catch2/catch_test_macros.hpp>
#include <frida_uprobe_attach_impl.hpp>
#include <cstdlib>
#if defined(__linux__)
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>
#endif
using namespace bpftime::attach;

extern "C" int __func_reolve_test(int a, int b)
{
	return a + b;
}

TEST_CASE("Test internal function resolve")
{
	frida_attach_impl man;
	void *addr = find_function_addr_by_name("__func_reolve_test");
	REQUIRE(addr != nullptr);
	REQUIRE(addr == (void *)&__func_reolve_test);
}

TEST_CASE("Test external function resolve")
{
	frida_attach_impl man;
	void *addr = find_function_addr_by_name("malloc");
	REQUIRE(addr != nullptr);
	INFO("malloc addr resolved: " << (uintptr_t)addr);
	REQUIRE(addr == (void *)&malloc);
}

TEST_CASE("Module offset resolution handles empty and missing paths")
{
	frida_attach_impl man;
	void *base = get_module_base_addr("");
	REQUIRE(base != nullptr);
	const auto offset = (uintptr_t)&__func_reolve_test - (uintptr_t)base;
	REQUIRE(resolve_function_addr_by_module_offset("", offset) ==
		(void *)&__func_reolve_test);
	REQUIRE(resolve_function_addr_by_module_offset(
		"/bpftime-nonexistent-module-for-resolution-test.so", 0) == nullptr);
}

#if defined(__linux__)
TEST_CASE("Mapped module paths survive VMA splitting")
{
	char path[] = "/tmp/bpftime-map-files-XXXXXX";
	const int fd = mkstemp(path);
	REQUIRE(fd >= 0);
	const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
	REQUIRE(page_size > 0);
	REQUIRE(ftruncate(fd, page_size * 2) == 0);
	void *mapping = mmap(nullptr, page_size * 2, PROT_READ, MAP_PRIVATE, fd, 0);
	REQUIRE(mapping != MAP_FAILED);

	// Split the original file mapping into two VMAs. Callers may still carry a
	// map_files range describing the pre-split mapping.
	REQUIRE(mprotect(static_cast<char *>(mapping) + page_size, page_size,
			 PROT_NONE) == 0);

	char module_name[128];
	snprintf(module_name, sizeof(module_name), "/proc/%d/map_files/%lx-%lx",
		 getpid(), reinterpret_cast<uintptr_t>(mapping),
		 reinterpret_cast<uintptr_t>(mapping) + page_size * 2);
	const auto resolved = resolve_mapped_module_path(module_name);

	REQUIRE(munmap(mapping, page_size * 2) == 0);
	REQUIRE(close(fd) == 0);
	REQUIRE(unlink(path) == 0);
	REQUIRE(resolved.has_value());
	REQUIRE(*resolved == path);
}
#endif

#if defined(__linux__)
#include <dlfcn.h>
// Module bases are kept once found (every probe looked its module up again:
// 0.42 ms per probe at process start, uprobe/more-dims-20261106); two
// modules resolved in turn keep their own.
TEST_CASE("Module offset resolution keeps each module's base")
{
	frida_attach_impl man;
	Dl_info libc_info{};
	REQUIRE(dladdr((void *)&malloc, &libc_info) != 0);
	REQUIRE(libc_info.dli_fname != nullptr);
	const std::string libc_path = libc_info.dli_fname;
	const auto malloc_offset =
		(uintptr_t)&malloc - (uintptr_t)libc_info.dli_fbase;
	void *exe_base = get_module_base_addr("");
	REQUIRE(exe_base != nullptr);
	const auto func_offset =
		(uintptr_t)&__func_reolve_test - (uintptr_t)exe_base;
	for (int i = 0; i < 3; i++) {
		CHECK(resolve_function_addr_by_module_offset("", func_offset) ==
		      (void *)&__func_reolve_test);
		CHECK(resolve_function_addr_by_module_offset(
			      libc_path, malloc_offset) == (void *)&malloc);
	}
}
#endif
