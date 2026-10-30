#include "agent_namespace.hpp"
#include "spdlog/spdlog.h"
#include <cstring>
#include <dlfcn.h>
#include <gnu/lib-names.h>
#include <link.h>
#include <pthread.h>
#include <vector>

namespace bpftime
{
namespace
{
// Keys below 32 are stored in the thread descriptor itself. Higher keys use
// second-level arrays that the libc of the first setter allocates and the
// thread's own libc frees at thread exit: an array of the agent's libc would
// be freed by the process's.
constexpr pthread_key_t INLINE_KEYS = 32;
// The agent (GLib/Frida, spdlog) used 5 keys when measured.
constexpr size_t FEW_AGENT_KEYS = 8;
using key_create_t = int (*)(pthread_key_t *, void (*)(void *));
using key_delete_t = int (*)(pthread_key_t);
} // namespace

void *load_agent_in_new_namespace(const char *path)
{
	// The process's libc: take every free key below INLINE_KEYS and never
	// release them. Its later keys are INLINE_KEYS and above.
	std::vector<pthread_key_t> agent_keys;
	for (;;) {
		pthread_key_t key;
		if (pthread_key_create(&key, nullptr) != 0)
			break;
		if (key >= INLINE_KEYS) {
			pthread_key_delete(key);
			break;
		}
		agent_keys.push_back(key);
	}
	void *libc = dlmopen(LM_ID_NEWLM, LIBC_SO, RTLD_NOW | RTLD_LOCAL);
	if (libc == nullptr)
		return nullptr;
	Lmid_t lmid;
	if (dlinfo(libc, RTLD_DI_LMID, &lmid) != 0)
		return nullptr;
	auto create = (key_create_t)dlsym(libc, "pthread_key_create");
	auto remove = (key_delete_t)dlsym(libc, "pthread_key_delete");
	if (create == nullptr || remove == nullptr)
		return nullptr;
	// The agent's libc: take every key, then free the reserved ones, so that
	// it allocates only those. Beyond them its pthread_key_create fails
	// rather than sharing a slot with the process.
	pthread_key_t key;
	[[maybe_unused]] size_t taken = 0;
	while (create(&key, nullptr) == 0)
		taken++;
	for (auto k : agent_keys)
		remove(k);
	if (agent_keys.size() < FEW_AGENT_KEYS)
		SPDLOG_WARN(
			"Only {} pthread keys left for the agent (the process holds the others below {})",
			agent_keys.size(), INLINE_KEYS);
	SPDLOG_DEBUG("Reserved {} pthread keys for the agent's namespace ({} taken there)",
		     agent_keys.size(), taken);
	return dlmopen(lmid, path, RTLD_NOW | RTLD_LOCAL);
}
// The executable segments of every object in the namespace of `handle`.
namespace
{
using agent_fork_hook_t = void (*)(int);
agent_fork_hook_t agent_fork_hook;
} // namespace

bool install_agent_fork_hook(void *handle)
{
	auto hook = (agent_fork_hook_t)dlsym(handle, "bpftime_agent_fork_hook");
	if (hook == nullptr)
		return false;
	// Once per process: the agent is loaded once.
	if (agent_fork_hook != nullptr)
		return true;
	agent_fork_hook = hook;
	// The phases are bpftime::fork_phase: prepare 0, parent 1, child 2.
	return pthread_atfork([] { agent_fork_hook(0); },
			      [] { agent_fork_hook(1); },
			      [] { agent_fork_hook(2); }) == 0;
}

std::vector<std::pair<uint64_t, uint64_t> > namespace_code_ranges(void *handle)
{
	struct link_map *lm = nullptr;
	if (dlinfo(handle, RTLD_DI_LINKMAP, &lm) != 0 || lm == nullptr)
		return {};
	while (lm->l_prev != nullptr)
		lm = lm->l_prev;
	struct search {
		std::vector<struct link_map *> objects;
		std::vector<std::pair<uint64_t, uint64_t> > ranges;
	} ctx;
	for (; lm != nullptr; lm = lm->l_next)
		ctx.objects.push_back(lm);
	// dl_iterate_phdr visits every namespace; an object is ours when its
	// load address and name match (ld.so is shared by the namespaces).
	dl_iterate_phdr(
		[](struct dl_phdr_info *info, size_t, void *data) {
			auto *ctx = (search *)data;
			for (auto *obj : ctx->objects) {
				if (obj->l_addr != info->dlpi_addr ||
				    strcmp(obj->l_name ? obj->l_name : "",
					   info->dlpi_name ? info->dlpi_name : "") != 0)
					continue;
				for (int i = 0; i < info->dlpi_phnum; i++) {
					const auto &ph = info->dlpi_phdr[i];
					if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X))
						ctx->ranges.push_back(
							{ info->dlpi_addr + ph.p_vaddr,
							  info->dlpi_addr + ph.p_vaddr +
								  ph.p_memsz });
				}
				break;
			}
			return 0;
		},
		&ctx);
	return ctx.ranges;
}
} // namespace bpftime
