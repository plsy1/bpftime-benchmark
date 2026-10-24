/* SPDX-License-Identifier: MIT */
// The agent interposes libc's uid/gid setters so that bpf_get_current_uid_gid
// can cache the credentials instead of paying two system calls per call. Each
// interposer calls the real function and then invalidates the cache, keeping
// its return value and errno. The cache is enabled only when every interposer
// is the definition the process binds (the agent was LD_PRELOADed); a late
// dlopen or another interposer keeps the helper querying on every call.
// Credentials changed by raw system calls, bypassing libc, are not observed.
#include "bpftime_credential_cache.hpp"
#include <atomic>
#include <cerrno>
#include <dlfcn.h>
#include <sys/types.h>

namespace
{
template <class F> F real_function(std::atomic<void *> &slot, const char *name)
{
	auto fn = slot.load(std::memory_order_acquire);
	if (!fn) {
		fn = dlsym(RTLD_NEXT, name);
		slot.store(fn, std::memory_order_release);
	}
	return reinterpret_cast<F>(fn);
}
} // namespace

#define BPFTIME_CREDENTIAL_HOOK(name, params, args)                            \
	namespace                                                              \
	{                                                                      \
	std::atomic<void *> real_##name{ nullptr };                            \
	}                                                                      \
	extern "C" __attribute__((visibility("hidden"))) int                   \
	bpftime_##name##_hook params                                           \
	{                                                                      \
		auto fn = real_function<int(*) params>(real_##name, #name);    \
		int ret = -1;                                                  \
		if (fn)                                                        \
			ret = fn args;                                         \
		else                                                           \
			errno = ENOSYS;                                        \
		bpftime_invalidate_credential_cache();                         \
		return ret;                                                    \
	}                                                                      \
	extern "C" __attribute__((visibility("default"),                       \
				  alias("bpftime_" #name "_hook"))) int name params;

BPFTIME_CREDENTIAL_HOOK(setuid, (uid_t uid), (uid))
BPFTIME_CREDENTIAL_HOOK(setgid, (gid_t gid), (gid))
BPFTIME_CREDENTIAL_HOOK(seteuid, (uid_t euid), (euid))
BPFTIME_CREDENTIAL_HOOK(setegid, (gid_t egid), (egid))
BPFTIME_CREDENTIAL_HOOK(setreuid, (uid_t ruid, uid_t euid), (ruid, euid))
BPFTIME_CREDENTIAL_HOOK(setregid, (gid_t rgid, gid_t egid), (rgid, egid))
BPFTIME_CREDENTIAL_HOOK(setresuid, (uid_t ruid, uid_t euid, uid_t suid),
			(ruid, euid, suid))
BPFTIME_CREDENTIAL_HOOK(setresgid, (gid_t rgid, gid_t egid, gid_t sgid),
			(rgid, egid, sgid))

// Resolves the real functions (so the interposers never call dlsym later) and
// enables the cache only if the process binds all eight interposers. Returns
// whether the cache was enabled.
extern "C" bool bpftime_agent_setup_credential_cache()
{
	struct entry {
		const char *name;
		std::atomic<void *> &real;
		void *hook;
	} entries[] = {
		{ "setuid", real_setuid, (void *)bpftime_setuid_hook },
		{ "setgid", real_setgid, (void *)bpftime_setgid_hook },
		{ "seteuid", real_seteuid, (void *)bpftime_seteuid_hook },
		{ "setegid", real_setegid, (void *)bpftime_setegid_hook },
		{ "setreuid", real_setreuid, (void *)bpftime_setreuid_hook },
		{ "setregid", real_setregid, (void *)bpftime_setregid_hook },
		{ "setresuid", real_setresuid, (void *)bpftime_setresuid_hook },
		{ "setresgid", real_setresgid, (void *)bpftime_setresgid_hook },
	};
	bool bound = true;
	for (auto &e : entries) {
		if (!e.real.load(std::memory_order_acquire))
			e.real.store(dlsym(RTLD_NEXT, e.name),
				     std::memory_order_release);
		bound = bound && e.real.load(std::memory_order_acquire) &&
			dlsym(RTLD_DEFAULT, e.name) == e.hook;
	}
	bpftime_set_credential_cache_enabled(bound);
	return bound;
}
