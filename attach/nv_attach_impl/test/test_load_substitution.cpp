#include "catch2/catch_test_macros.hpp"
#include "nv_attach_impl.hpp"
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <tuple>

using namespace bpftime::attach;

// What cudart passes to cuLibraryLoadData is its fatbin wrapper; anything that
// is not a fatbin (a cubin, PTX text) is loaded as it is.
TEST_CASE("Images passed to cuLibraryLoadData are recognized as fatbins",
	  "[load-substitution]")
{
	alignas(8) unsigned char fatbin[64] = {};
	const uint32_t fatbin_magic = 0xBA55ED50;
	memcpy(fatbin, &fatbin_magic, sizeof(fatbin_magic));
	REQUIRE(nv_attach_impl::fatbin_data_of_image(fatbin) == fatbin);

	__fatBinC_Wrapper_t wrapper{};
	wrapper.magic = 0x466243b1;
	wrapper.version = 1;
	wrapper.data = reinterpret_cast<const unsigned long long *>(fatbin);
	REQUIRE(nv_attach_impl::fatbin_data_of_image(&wrapper) == fatbin);

	alignas(8) unsigned char cubin[64] = { 0x7f, 'E', 'L', 'F' };
	__fatBinC_Wrapper_t not_fatbin = wrapper;
	not_fatbin.data = reinterpret_cast<const unsigned long long *>(cubin);
	REQUIRE(nv_attach_impl::fatbin_data_of_image(&not_fatbin) == nullptr);
	REQUIRE(nv_attach_impl::fatbin_data_of_image(cubin) == nullptr);
	REQUIRE(nv_attach_impl::fatbin_data_of_image(".version 8.1\n") ==
		nullptr);
	REQUIRE(nv_attach_impl::fatbin_data_of_image(nullptr) == nullptr);
}

// One image replaces the fatbin: the probed PTX for the highest virtual
// architecture the device runs (cuobjdump: <file>.<n>.sm_<arch>.ptx).
TEST_CASE("The patched PTX for the device's architecture is chosen",
	  "[load-substitution]")
{
	const std::map<std::string, std::tuple<std::string, bool>> patched{
		{ "patched.temp.1.sm_75.ptx", { "a", true } },
		{ "patched.temp.2.sm_86.ptx", { "b", true } },
		{ "patched.temp.3.sm_90.ptx", { "c", true } },
		// no probed kernel in it
		{ "patched.temp.4.sm_87.ptx", { "d", false } },
	};
	REQUIRE(nv_attach_impl::pick_patched_ptx(patched, 87) ==
		"patched.temp.2.sm_86.ptx");
	REQUIRE(nv_attach_impl::pick_patched_ptx(patched, 75) ==
		"patched.temp.1.sm_75.ptx");
	REQUIRE_FALSE(nv_attach_impl::pick_patched_ptx(patched, 70).has_value());
	// device architecture unknown: the newest
	REQUIRE(nv_attach_impl::pick_patched_ptx(patched, 0) ==
		"patched.temp.3.sm_90.ptx");
	const std::map<std::string, std::tuple<std::string, bool>> unprobed{
		{ "patched.temp.1.sm_75.ptx", { "a", false } },
	};
	REQUIRE_FALSE(
		nv_attach_impl::pick_patched_ptx(unprobed, 87).has_value());
}
