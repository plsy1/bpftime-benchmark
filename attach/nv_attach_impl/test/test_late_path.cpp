// The late path (probes attached after cudart loaded the fatbins): naming host
// addresses and reading the loaded fatbins.
#include "catch2/catch_test_macros.hpp"
#include "nv_attach_impl.hpp"
#include "nv_elf_introspect.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace bpftime::attach;

namespace
{
// Internal linkage: only in .symtab, so dladdr cannot name them.
__attribute__((noinline)) int late_path_symbol_target(int x)
{
	return x * 3 + 1;
}
int late_path_symbol_data[16];

bool contains(const std::optional<std::string> &name, const char *part)
{
	return name && name->find(part) != std::string::npos;
}
} // namespace

// The late path used to read the .symtab and .dynsym of every loaded module
// before main, one 24-byte read per symbol: 556,752 symbols, 1.39 s, 97% of
// them in two bpftime libraries without GPU code. It named a heap address
// (a CUDA 13 kernel handle) after the nearest lower symbol of size 0 (`__end__`),
// and on aarch64 a `__constant__` shadow after a mapping symbol (`$d`) at the
// same address, so the copy to it was skipped.
TEST_CASE("Host addresses are named from the module that contains them",
	  "[late-path]")
{
	elf_introspect::host_symbol_resolver resolver;

	const auto function = (std::uintptr_t)&late_path_symbol_target;
	CHECK(contains(resolver.resolve(function), "late_path_symbol_target"));
	// inside the function
	CHECK(contains(resolver.resolve(function + 4),
		       "late_path_symbol_target"));
	const auto data = (std::uintptr_t)&late_path_symbol_data[5];
	CHECK(contains(resolver.resolve(data), "late_path_symbol_data"));
	CHECK(resolver.modules_read() == 1);

	auto heap = std::make_unique<char[]>(4096);
	CHECK_FALSE(resolver.resolve((std::uintptr_t)heap.get() + 100));
	CHECK(resolver.modules_read() == 1);

	auto module = elf_introspect::module_containing(function);
	REQUIRE(module);
	std::size_t mapping_symbols = 0;
	for (const auto &symbol : elf_introspect::read_function_symbols(*module))
		if (symbol.name[0] == '$')
			mapping_symbols++;
	CHECK(mapping_symbols == 0);
}

namespace
{
void put_fatbin_header(std::vector<uint8_t> &out, uint16_t header_size,
		       uint64_t size)
{
	const uint32_t magic = 0xBA55ED50;
	const uint16_t version = 1;
	const auto at = out.size();
	out.resize(at + header_size);
	memcpy(out.data() + at, &magic, 4);
	memcpy(out.data() + at + 4, &version, 2);
	memcpy(out.data() + at + 6, &header_size, 2);
	memcpy(out.data() + at + 8, &size, 8);
}
void put_entry(std::vector<uint8_t> &out, uint16_t kind, uint64_t payload)
{
	const uint16_t version = 1;
	const uint32_t header_size = 16;
	const auto at = out.size();
	out.resize(at + header_size + payload, 0x5a);
	memcpy(out.data() + at, &kind, 2);
	memcpy(out.data() + at + 2, &version, 2);
	memcpy(out.data() + at + 4, &header_size, 4);
	memcpy(out.data() + at + 8, &payload, 8);
}
} // namespace

// .nv_fatbin holds the device-link fatbin (a cubin only) and right after it
// the program's (cubin and PTX); the late bootstrap read the section and
// then each wrapper, which point at the same bytes, and ran cuobjdump 2-3
// times on one fatbin.
TEST_CASE("The late bootstrap reads each fatbin once", "[late-path]")
{
	constexpr uint16_t ELF = 2, PTX = 1;
	std::vector<uint8_t> link_only, program;
	put_fatbin_header(link_only, 16, 32);
	put_entry(link_only, ELF, 16);
	put_fatbin_header(program, 16, 64);
	put_entry(program, ELF, 16);
	put_entry(program, PTX, 16);

	// the section: both fatbins back to back, then zeros
	std::vector<uint64_t> storage(64, 0);
	auto *section = reinterpret_cast<uint8_t *>(storage.data());
	memcpy(section, link_only.data(), link_only.size());
	memcpy(section + link_only.size(), program.data(), program.size());
	const void *first = section;
	const void *second = section + link_only.size();
	const void *zeros = section + link_only.size() + program.size();

	using v = std::vector<const void *>;
	CHECK(nv_attach_impl::fatbins_to_read({ first, second, first }) ==
	      v{ first });
	CHECK(nv_attach_impl::fatbins_to_read({ second, first }) == v{ first });
	CHECK(nv_attach_impl::fatbins_to_read({ second }) == v{ second });
	CHECK(nv_attach_impl::fatbins_to_read({ nullptr, zeros }).empty());

	CHECK_FALSE(nv_attach_impl::fatbin_has_ptx(link_only));
	CHECK(nv_attach_impl::fatbin_has_ptx(program));
	std::vector<uint8_t> both(section,
				  section + link_only.size() + program.size());
	CHECK(nv_attach_impl::fatbin_has_ptx(both));
	// a layout it does not understand goes to cuobjdump
	CHECK(nv_attach_impl::fatbin_has_ptx(std::vector<uint8_t>(32, 0x11)));
}
