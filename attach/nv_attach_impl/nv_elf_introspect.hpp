#pragma once

#include "nv_attach_utils.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpftime::attach::elf_introspect {

struct loaded_module {
	std::string path;
	std::uintptr_t base_addr = 0;
};

struct symbol_range {
	std::uintptr_t start = 0;
	std::uintptr_t end = 0; // inclusive-exclusive, may equal start when unknown
	std::string name;
};

std::vector<loaded_module> list_loaded_modules();

std::optional<std::pair<const void *, std::size_t>>
find_section_in_memory(const loaded_module &mod, std::string_view section_name);

std::vector<const __fatBinC_Wrapper_t *>
scan_fatbin_wrappers(const void *section_addr, std::size_t section_size);

// Function and object symbols of .symtab and .dynsym, sorted by address (a
// sized symbol after an unsized one at the same address). AArch64 mapping
// symbols ($x, $d) are left out: they share addresses with real symbols.
std::vector<symbol_range> read_function_symbols(const loaded_module &mod);

// The loaded module whose segments contain addr.
std::optional<loaded_module> module_containing(std::uintptr_t addr);

// Names host addresses (kernel stubs, __constant__ shadows) from the symbol
// tables of the one loaded module that contains the address, read the first
// time an address in it is asked for. An address outside every module (a
// heap pointer, such as a CUDA 13 kernel handle) has no name.
class host_symbol_resolver {
    public:
	std::optional<std::string> resolve(std::uintptr_t addr);
	// modules whose symbol tables were read
	std::size_t modules_read() const;

    private:
	mutable std::mutex mutex;
	std::map<std::pair<std::string, std::uintptr_t>,
		 std::vector<symbol_range>>
		symbols_by_module;
};

} // namespace bpftime::attach::elf_introspect
