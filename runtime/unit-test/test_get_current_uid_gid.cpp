/* SPDX-License-Identifier: MIT */
#include <catch2/catch_test_macros.hpp>
#include <boost/scope_exit.hpp>
#include "bpftime_credential_cache.hpp"
#include <cerrno>
#include <cstdint>
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
struct credentials {
	uid_t uid;
	gid_t gid;
};

thread_local const credentials *current_credentials = nullptr;
} // namespace

extern "C" {
uint64_t bpf_get_current_uid_gid(uint64_t, uint64_t, uint64_t, uint64_t,
				 uint64_t);
uid_t __real_getuid() noexcept;
gid_t __real_getgid() noexcept;
bool bpftime_agent_setup_credential_cache();
int bpftime_setuid_hook(uid_t);

// Change query results without changing the test process's real credentials.
// Calls outside the scoped override, including other tests, use libc.
uid_t __wrap_getuid() noexcept
{
	return current_credentials ? current_credentials->uid : __real_getuid();
}

gid_t __wrap_getgid() noexcept
{
	return current_credentials ? current_credentials->gid : __real_getgid();
}
}

TEST_CASE("bpf_get_current_uid_gid reports current unsigned credentials",
	  "[uid_gid]")
{
	{
		const auto *previous = current_credentials;
		BOOST_SCOPE_EXIT_ALL(=)
		{
			current_credentials = previous;
		};
		const struct {
			credentials ids;
			uint64_t expected;
		} cases[] = {
			{ { 1001, 2002 }, 0x000007d2000003e9ULL },
			{ { 3003, 2002 }, 0x000007d200000bbbULL },
			{ { 3003, 4004 }, 0x00000fa400000bbbULL },
			{ { 0, 0 }, 0 },
			{ { 0x80000001U, 42 }, 0x0000002a80000001ULL },
			{ { 42, 0x80000002U }, 0x800000020000002aULL },
		};
		// Keep calls in one process to detect cached UID or GID values.
		for (const auto &test : cases) {
			current_credentials = &test.ids;
			CAPTURE(test.ids.uid, test.ids.gid);
			CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
			      test.expected);
		}
	}
	const auto actual = bpf_get_current_uid_gid(0, 0, 0, 0, 0);
	CHECK(static_cast<uint32_t>(actual) == __real_getuid());
	CHECK(static_cast<uint32_t>(actual >> 32) == __real_getgid());
}

namespace
{
uint64_t expected_ids(const credentials &ids)
{
	return (static_cast<uint64_t>(ids.gid) << 32) | ids.uid;
}

struct scoped_credential_cache {
	const credentials *previous = current_credentials;
	scoped_credential_cache()
	{
		bpftime_set_credential_cache_enabled(true);
	}
	~scoped_credential_cache()
	{
		bpftime_set_credential_cache_enabled(false);
		current_credentials = previous;
	}
};
} // namespace

TEST_CASE("bpf_get_current_uid_gid cache is dropped on invalidation and fork",
	  "[uid_gid]")
{
	const credentials first{ 1001, 2002 }, second{ 3003, 4004 },
		in_child{ 5005, 6006 };
	{
		scoped_credential_cache cache;
		current_credentials = &first;
		REQUIRE(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
			expected_ids(first));
		current_credentials = &second;
		// Cached: an unannounced change is not seen.
		CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
		      expected_ids(first));
		bpftime_invalidate_credential_cache();
		CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
		      expected_ids(second));

		pid_t child = fork();
		REQUIRE(child >= 0);
		if (child == 0) {
			current_credentials = &in_child;
			_exit(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
					      expected_ids(in_child) ?
				      0 :
				      1);
		}
		int status = -1;
		REQUIRE(waitpid(child, &status, 0) == child);
		REQUIRE(WIFEXITED(status));
		CHECK(WEXITSTATUS(status) == 0);
		CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
		      expected_ids(second));
	}
	// Disabled again: every call queries.
	current_credentials = &first;
	CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) == expected_ids(first));
	current_credentials = nullptr;
}

TEST_CASE("Credential setter interposers invalidate and keep results",
	  "[uid_gid]")
{
	const credentials first{ 1001, 2002 }, second{ 3003, 4004 };
	scoped_credential_cache cache;
	current_credentials = &first;
	REQUIRE(bpf_get_current_uid_gid(0, 0, 0, 0, 0) == expected_ids(first));
	current_credentials = &second;
	// No-op change, permitted for every user; this binary links the hooks.
	errno = 0;
	REQUIRE(setresuid((uid_t)-1, (uid_t)-1, (uid_t)-1) == 0);
	CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) == expected_ids(second));
	// A failing call keeps the real return value and errno.
	errno = 0;
	CHECK(setuid((uid_t)-1) == -1);
	CHECK(errno == EINVAL);
	errno = 0;
	CHECK(bpftime_setuid_hook((uid_t)-1) == -1);
	CHECK(errno == EINVAL);
}

TEST_CASE("Credential cache stays off unless the interposers are bound",
	  "[uid_gid]")
{
	const credentials first{ 1001, 2002 }, second{ 3003, 4004 };
	const auto *previous = current_credentials;
	const bool bound = bpftime_agent_setup_credential_cache();
	CHECK(bound == (dlsym(RTLD_DEFAULT, "setuid") ==
			(void *)bpftime_setuid_hook));
	current_credentials = &first;
	REQUIRE(bpf_get_current_uid_gid(0, 0, 0, 0, 0) == expected_ids(first));
	current_credentials = &second;
	CHECK(bpf_get_current_uid_gid(0, 0, 0, 0, 0) ==
	      expected_ids(bound ? first : second));
	bpftime_set_credential_cache_enabled(false);
	current_credentials = previous;
}
