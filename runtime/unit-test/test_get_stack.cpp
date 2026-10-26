#include "bpf_attach_ctx.hpp"
#include "bpftime_shm.hpp"
#include "common_def.hpp"
#include "linux/bpf.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <boost/scope_exit.hpp>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

std::vector<uint64_t> mock_stack;

class mock_stack_attach_impl : public bpftime::attach::base_attach_impl {
    public:
	int detach_by_id(int) override
	{
		return 0;
	}

	int create_attach_with_ebpf_callback(
		bpftime::attach::ebpf_run_callback &&,
		const bpftime::attach::attach_private_data &, int) override
	{
		return 0;
	}

	void *call_attach_specific_function(const std::string &name,
					    void *) override
	{
		if (name != "generate_stack")
			return nullptr;
		return new std::vector<uint64_t>(mock_stack);
	}
};

bpftime::bpf_attach_ctx mock_attach_ctx;

} // namespace

bpftime::bpf_attach_ctx &get_global_attach_ctx()
{
	static const bool initialized = [] {
		mock_attach_ctx.register_attach_impl(
			{ 6 }, std::make_unique<mock_stack_attach_impl>(),
			[](const std::string_view &, int &) {
				return std::unique_ptr<
					bpftime::attach::attach_private_data>();
			});
		return true;
	}();
	(void)initialized;
	return mock_attach_ctx;
}

extern "C" int64_t bpftime_get_stack(uint64_t, uint64_t, uint64_t, uint64_t,
				     uint64_t);
extern "C" int64_t bpftime_get_stackid(uint64_t, uint64_t, uint64_t, uint64_t,
				       uint64_t);

TEST_CASE("get_stack rejects build IDs")
{
	REQUIRE(bpftime_get_stack(0, 0, 0,
				  BPF_F_USER_STACK | BPF_F_USER_BUILD_ID,
				  0) == -ENOTSUP);
}

TEST_CASE("get_stack uses byte-sized buffers and returns copied bytes")
{
	mock_stack = { 0x1111111111111111, 0x2222222222222222,
		       0x3333333333333333 };
	std::array<uint64_t, 4> buffer;
	buffer.fill(UINT64_MAX);

	auto copied =
		bpftime_get_stack(0, reinterpret_cast<uint64_t>(buffer.data()),
				  sizeof(buffer), BPF_F_USER_STACK, 0);

	REQUIRE(copied == 3 * sizeof(uint64_t));
	REQUIRE(buffer[0] == mock_stack[0]);
	REQUIRE(buffer[1] == mock_stack[1]);
	REQUIRE(buffer[2] == mock_stack[2]);
	REQUIRE(buffer[3] == 0);
}

TEST_CASE("get_stack truncates only at complete frame boundaries")
{
	mock_stack = { 0x1111111111111111, 0x2222222222222222,
		       0x3333333333333333 };
	std::array<uint64_t, 3> buffer;
	buffer.fill(UINT64_MAX);

	auto copied =
		bpftime_get_stack(0, reinterpret_cast<uint64_t>(buffer.data()),
				  2 * sizeof(uint64_t), BPF_F_USER_STACK, 0);

	REQUIRE(copied == 2 * sizeof(uint64_t));
	REQUIRE(buffer[0] == mock_stack[0]);
	REQUIRE(buffer[1] == mock_stack[1]);
	REQUIRE(buffer[2] == UINT64_MAX);
}

TEST_CASE("get_stack rejects a partial frame buffer")
{
	mock_stack = { 0x1111111111111111, 0x2222222222222222 };
	std::array<uint8_t, 15> buffer;
	buffer.fill(0xff);

	REQUIRE(bpftime_get_stack(0, reinterpret_cast<uint64_t>(buffer.data()),
				  buffer.size(), BPF_F_USER_STACK,
				  0) == -EINVAL);
	REQUIRE(std::all_of(buffer.begin(), buffer.end(),
			    [](uint8_t value) { return value == 0; }));
}

TEST_CASE("get_stack applies frame skips before the byte limit")
{
	mock_stack = { 0x1111111111111111, 0x2222222222222222,
		       0x3333333333333333 };
	std::array<uint64_t, 3> buffer;
	buffer.fill(UINT64_MAX);

	auto copied =
		bpftime_get_stack(0, reinterpret_cast<uint64_t>(buffer.data()),
				  sizeof(buffer), BPF_F_USER_STACK | 1, 0);

	REQUIRE(copied == 2 * sizeof(uint64_t));
	REQUIRE(buffer[0] == mock_stack[1]);
	REQUIRE(buffer[1] == mock_stack[2]);
	REQUIRE(buffer[2] == 0);
}

TEST_CASE("get_stack rejects skips beyond the available trace")
{
	mock_stack = { 0x1111111111111111, 0x2222222222222222 };
	std::array<uint64_t, 2> buffer;
	buffer.fill(UINT64_MAX);

	REQUIRE(bpftime_get_stack(0, reinterpret_cast<uint64_t>(buffer.data()),
				  sizeof(buffer), BPF_F_USER_STACK | 3,
				  0) == -EFAULT);
	REQUIRE(std::all_of(buffer.begin(), buffer.end(),
			    [](uint64_t value) { return value == 0; }));
}

// The helper returns a 64-bit value: an error must be negative there too
// (kernel: -EEXIST is 0xffffffffffffffef). A program that keeps the result in
// a 64-bit variable and tests < 0 took 0x00000000ffffffef for a valid id.
TEST_CASE("get_stackid returns errors as negative 64-bit values")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name = "GetStackidShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	bool initialized = false;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (initialized) {
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
		}
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	initialized = true;

	// One bucket: a second, different trace collides with the first.
	const int map_fd =
		bpftime_maps_create(-1, "stackid_err",
				    { .type = BPF_MAP_TYPE_STACK_TRACE,
				      .key_size = sizeof(uint32_t),
				      .value_size = 4 * sizeof(uint64_t),
				      .max_ents = 1 });
	REQUIRE(map_fd >= 0);
	mock_stack = { 0x1111111111111111, 0x2222222222222222 };
	REQUIRE(bpftime_get_stackid(0, map_fd, BPF_F_USER_STACK, 0, 0) == 0);
	mock_stack = { 0x3333333333333333, 0x4444444444444444 };
	REQUIRE(bpftime_get_stackid(0, map_fd, BPF_F_USER_STACK, 0, 0) ==
		-(int64_t)EEXIST);
}
