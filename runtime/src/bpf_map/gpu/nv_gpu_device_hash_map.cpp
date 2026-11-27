#include "nv_gpu_device_hash_map.hpp"
#include "producer_identity.hpp"
#include "spdlog/spdlog.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sched.h>
#include <signal.h>
#include <stdexcept>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

using namespace bpftime;
using namespace bpftime_devhash;

namespace
{
std::atomic<devhash_hook_fn> devhash_hook{ nullptr };

inline void call_hook(devhash_hook_point point, uint64_t arg)
{
	if (auto fn = devhash_hook.load(std::memory_order_acquire); fn)
		fn(point, arg);
}

uint64_t monotonic_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t random_generation()
{
	uint64_t g = 0;
	if (syscall(SYS_getrandom, &g, sizeof(g), 0x1 /* GRND_NONBLOCK */) !=
	    (long)sizeof(g))
		g = monotonic_ns() ^ ((uint64_t)getpid() << 32) ^
		    (uint64_t)(uintptr_t)&g;
	return g | 1;
}

inline void cpu_relax()
{
#if defined(__aarch64__)
	asm volatile("yield" ::: "memory");
#elif defined(__x86_64__)
	asm volatile("pause" ::: "memory");
#endif
}

// "pid (comm) S ..." -> the text after the comm's closing parenthesis
bool read_stat_tail(const char *path, char *buf, size_t size,
		    const char **tail)
{
	FILE *f = fopen(path, "re");
	if (f == nullptr)
		return false;
	const size_t n = fread(buf, 1, size - 1, f);
	fclose(f);
	buf[n] = 0;
	const char *end = strrchr(buf, ')');
	if (end == nullptr || end[1] != ' ')
		return false;
	*tail = end + 2;
	return true;
}

int real_kill_errno(uint32_t pid)
{
	return kill((pid_t)pid, 0) == 0 ? 0 : errno;
}

// 1 if a task of the process is neither Z nor X, 0 if all are, -1 unknown
int real_tasks_running(uint32_t pid)
{
	char dir[64];
	snprintf(dir, sizeof(dir), "/proc/%u/task", pid);
	DIR *d = opendir(dir);
	if (d == nullptr)
		return -1;
	int result = 0;
	while (struct dirent *e = readdir(d)) {
		if (e->d_name[0] < '0' || e->d_name[0] > '9')
			continue;
		char path[320], buf[512];
		const char *tail;
		snprintf(path, sizeof(path), "/proc/%u/task/%s/stat", pid,
			 e->d_name);
		if (!read_stat_tail(path, buf, sizeof(buf), &tail)) {
			result = result == 1 ? 1 : -1;
			continue;
		}
		if (tail[0] != 'Z' && tail[0] != 'X') {
			result = 1;
			break;
		}
	}
	closedir(d);
	return result;
}

uint64_t real_start_time(uint32_t pid)
{
	char path[64], buf[1024];
	const char *tail;
	snprintf(path, sizeof(path), "/proc/%u/stat", pid);
	if (!read_stat_tail(path, buf, sizeof(buf), &tail))
		return 0;
	// tail starts at field 3 (state); starttime is field 22
	const char *p = tail;
	for (int field = 3; field < 22; field++) {
		p = strchr(p, ' ');
		if (p == nullptr)
			return 0;
		p++;
	}
	return strtoull(p, nullptr, 10);
}

const devhash_liveness_probes real_probes = { real_kill_errno,
					      real_tasks_running,
					      real_start_time };
std::atomic<const devhash_liveness_probes *> probes{ &real_probes };
} // namespace

void bpftime::set_devhash_hook_for_test(devhash_hook_fn fn)
{
	devhash_hook.store(fn, std::memory_order_release);
}

void bpftime::set_devhash_liveness_probes_for_test(
	const devhash_liveness_probes *p)
{
	probes.store(p ? p : &real_probes, std::memory_order_release);
}

uint64_t bpftime::devhash_process_start_time(uint32_t pid)
{
	return probes.load(std::memory_order_acquire)->start_time(pid);
}

bool bpftime::devhash_process_alive(uint32_t pid, uint64_t start)
{
	const devhash_liveness_probes *p =
		probes.load(std::memory_order_acquire);
	if (p->kill_errno(pid) == ESRCH)
		return false;
	// EPERM: a process of another user, alive
	if (p->tasks_running(pid) == 0)
		return false; // exited, not yet reaped
	if (start != 0) {
		const uint64_t now = p->start_time(pid);
		if (now != 0 && now != start)
			return false; // the pid belongs to another process now
	}
	return true;
}

// ------------------------------------------------- orders of the seal

extern "C" __attribute__((noinline, used)) uint64_t
bpftime_devhash_writer_enter(uint64_t *count, uint64_t *phase)
{
	// seq_cst both: with the sealer's below, one side sees the other (an
	// acquire load here becomes LDAPR on an RCpc build, which may pass the
	// increment)
	__atomic_fetch_add(count, 1, __ATOMIC_SEQ_CST);
	call_hook(devhash_hook_point::writer_after_inflight_inc, 0);
	return __atomic_load_n(phase, __ATOMIC_SEQ_CST);
}

extern "C" __attribute__((noinline, used)) uint64_t
bpftime_devhash_sealer_enter(uint64_t *phase, uint64_t *counts, uint64_t n,
			     uint64_t stride)
{
	__atomic_store_n(phase, DEVHASH_PHASE_SEALING, __ATOMIC_SEQ_CST);
	uint64_t sum = 0;
	for (uint64_t i = 0; i < n; i++)
		sum += __atomic_load_n(counts + i * stride, __ATOMIC_SEQ_CST);
	return sum;
}

extern "C" __attribute__((noinline, used)) uint64_t
bpftime_devhash_sealer_poll(uint64_t *counts, uint64_t n, uint64_t stride)
{
	// A poll that reads 0 must synchronize with the writers' release
	// decrements, so that the image's scan sees their table writes.
	uint64_t sum = 0;
	for (uint64_t i = 0; i < n; i++)
		sum += __atomic_load_n(counts + i * stride, __ATOMIC_SEQ_CST);
	return sum;
}

extern "C" __attribute__((noinline, used)) uint64_t
bpftime_devhash_reader_phase(uint64_t *phase)
{
	// pairs with the release store of PHASE 2 after the mirror is seeded
	return __atomic_load_n(phase, __ATOMIC_ACQUIRE);
}

// ---------------------------------------------------- owner and paths

uint64_t bpftime::devhash_owner_record(uint64_t pid_namespace, uint32_t pid)
{
	return ((pid_namespace & 0xffffffffull) << 32) | pid;
}

uint64_t bpftime::devhash_self_record()
{
	return devhash_owner_record(producer_pid_namespace(), (uint32_t)getpid());
}

devhash_path bpftime::devhash_choose_path(uint64_t phase, uint64_t owner,
					  uint64_t self, bool has_cached_copy,
					  bool owner_alive)
{
	if (owner == self) {
		if (has_cached_copy)
			return devhash_path::have_copy;
		// the same (namespace, pid) without a copy here: the pid was
		// reused after the owner died
	} else if (owner != 0) {
		// an owner in another pid namespace is never taken over: its
		// pid means nothing here
		if ((owner >> 32) != (self >> 32))
			return devhash_path::not_owner;
		if (owner_alive)
			return devhash_path::not_owner;
	}
	switch (phase) {
	case DEVHASH_PHASE_CPU:
		return devhash_path::first_copy;
	case DEVHASH_PHASE_SEALING:
		return devhash_path::recover_sealing;
	default:
		return devhash_path::takeover;
	}
}

// ------------------------------------------------------------ the map

nv_gpu_device_hash_map_impl::nv_gpu_device_hash_map_impl(
	boost::interprocess::managed_shared_memory &memory,
	uint64_t max_entries)
	: table(memory, max_entries, 4, 8),
	  mirror_headers(memory.get_segment_manager()),
	  mirror_values(memory.get_segment_manager()),
	  status_buffer(memory.get_segment_manager()),
	  max_entries(max_entries), slots(slots_for(max_entries)),
	  log_slots(log_slots_for(max_entries)),
	  generation(random_generation())
{
	if (max_entries == 0 || max_entries > MAX_ENTRIES_LIMIT)
		throw std::runtime_error(
			"device hash map: max_entries must be 1 to 2^22");
	mirror_headers.resize(slots * 8);
	mirror_values.resize(slots * 8);
	status_buffer.resize(DEVHASH_STATUS_WORDS * 8);
	std::memset(mirror_headers.data(), 0, mirror_headers.size());
	std::memset(mirror_values.data(), 0, mirror_values.size());
	std::memset(status_buffer.data(), 0, status_buffer.size());
	SPDLOG_INFO(
		"GPU device hash map (1533): max_entries={}, slots={}, mirror {} bytes",
		max_entries, slots, 2 * slots * 8);
}

int64_t nv_gpu_device_hash_map_impl::mirror_find(uint32_t key)
{
	uint64_t i = home_slot(key, log_slots);
	for (uint64_t n = 0; n < slots; n++) {
		const uint64_t h = __atomic_load_n(mhdr() + i, __ATOMIC_ACQUIRE);
		if (state_of(h) == ST_EMPTY)
			return -1;
		// past everything but live K: claiming, refused and opaque
		// slots never end a walk
		if (state_of(h) == ST_LIVE && key_of(h) == key)
			return (int64_t)i;
		i = (i + 1) & (slots - 1);
	}
	return -1;
}

nv_gpu_device_hash_map_impl::writer_slot &
nv_gpu_device_hash_map_impl::my_writer_slot()
{
	int pid;
	int64_t tid;
	current_producer_identity(pid, tid);
	const uint64_t self =
		devhash_owner_record(producer_pid_namespace(), (uint32_t)pid);
	// this process's start time, cached per pid (fork children differ)
	static std::atomic<uint64_t> cached_pid{ 0 }, cached_start{ 0 };
	uint64_t my_start;
	if (cached_pid.load(std::memory_order_acquire) == (uint64_t)pid) {
		my_start = cached_start.load(std::memory_order_relaxed);
	} else {
		my_start = devhash_process_start_time((uint32_t)pid);
		cached_start.store(my_start, std::memory_order_relaxed);
		cached_pid.store((uint64_t)pid, std::memory_order_release);
	}
	// A slot with this record but another start time belongs to a dead
	// process that had this pid: not ours (its count may be a leftover
	// the sealer will discount).
	for (uint32_t i = 0; i < WRITER_SLOTS; i++) {
		if (__atomic_load_n(&writers[i].owner, __ATOMIC_ACQUIRE) != self)
			continue;
		const uint64_t st =
			__atomic_load_n(&writers[i].start, __ATOMIC_ACQUIRE);
		if (st == 0 || my_start == 0 || st == my_start)
			return writers[i];
	}
	for (int pass = 0; pass < 2; pass++) {
		for (uint32_t i = 0; i < WRITER_SLOTS; i++) {
			uint64_t cur =
				__atomic_load_n(&writers[i].owner, __ATOMIC_ACQUIRE);
			// second pass: a slot of a process that is gone, with
			// nothing in flight, is free too
			if (cur != 0 &&
			    !(pass == 1 && (cur >> 32) == (self >> 32) &&
			      __atomic_load_n(&writers[i].count,
					      __ATOMIC_ACQUIRE) == 0 &&
			      !devhash_process_alive(
				      (uint32_t)cur,
				      __atomic_load_n(&writers[i].start,
						      __ATOMIC_ACQUIRE))))
				continue;
			if (__atomic_compare_exchange_n(&writers[i].owner, &cur,
							self, false,
							__ATOMIC_ACQ_REL,
							__ATOMIC_ACQUIRE)) {
				__atomic_store_n(&writers[i].start, my_start,
						 __ATOMIC_RELEASE);
				return writers[i];
			}
		}
	}
	return writers[WRITER_SLOTS]; // shared, never discounted
}

uint64_t nv_gpu_device_hash_map_impl::inflight_sum()
{
	return bpftime_devhash_sealer_poll(&writers[0].count, WRITER_SLOTS + 1,
					   sizeof(writer_slot) / 8);
}

uint64_t nv_gpu_device_hash_map_impl::get_inflight()
{
	return inflight_sum();
}

void nv_gpu_device_hash_map_impl::discount_dead_writers()
{
	const uint64_t self = devhash_self_record();
	for (uint32_t i = 0; i < WRITER_SLOTS; i++) {
		uint64_t cur = __atomic_load_n(&writers[i].owner, __ATOMIC_ACQUIRE);
		if (cur == 0 || cur == self || (cur >> 32) != (self >> 32) ||
		    __atomic_load_n(&writers[i].count, __ATOMIC_SEQ_CST) == 0)
			continue;
		if (devhash_process_alive(
			    (uint32_t)cur,
			    __atomic_load_n(&writers[i].start, __ATOMIC_ACQUIRE)))
			continue;
		// Gone: nothing of it runs any more, so its count is what its
		// dead threads left. Take the slot (so no new process with a
		// reused record inherits the count), then clear it.
		if (!__atomic_compare_exchange_n(&writers[i].owner, &cur,
						 ~0ull, false, __ATOMIC_ACQ_REL,
						 __ATOMIC_ACQUIRE))
			continue;
		SPDLOG_INFO(
			"device hash map: discounting {} writer(s) of pid {}, which is gone",
			__atomic_load_n(&writers[i].count, __ATOMIC_RELAXED),
			(uint32_t)cur);
		__atomic_store_n(&writers[i].count, 0, __ATOMIC_SEQ_CST);
		__atomic_store_n(&writers[i].start, 0, __ATOMIC_RELAXED);
		__atomic_store_n(&writers[i].owner, 0, __ATOMIC_RELEASE);
	}
}

void *nv_gpu_device_hash_map_impl::elem_lookup(const void *key)
{
	if (bpftime_devhash_reader_phase(&phase) != DEVHASH_PHASE_DEVICE)
		return table.elem_lookup(key);
	const int64_t j = mirror_find(*(const uint32_t *)key);
	if (j < 0) {
		errno = ENOENT;
		return nullptr;
	}
	return mval() + j;
}

long nv_gpu_device_hash_map_impl::elem_update(const void *key,
					      const void *value,
					      uint64_t flags)
{
	writer_slot &w = my_writer_slot();
	const uint64_t ph = bpftime_devhash_writer_enter(&w.count, &phase);
	call_hook(devhash_hook_point::writer_after_phase_load, ph);
	if (ph != DEVHASH_PHASE_CPU) {
		__atomic_fetch_sub(&w.count, 1, __ATOMIC_RELEASE);
		// the device copy is the map from the seal on
		errno = EPERM;
		return -1;
	}
	const long r = table.elem_update(key, value, flags);
	const int err = errno;
	__atomic_fetch_sub(&w.count, 1, __ATOMIC_RELEASE);
	errno = err;
	return r;
}

long nv_gpu_device_hash_map_impl::elem_delete(const void *key)
{
	writer_slot &w = my_writer_slot();
	const uint64_t ph = bpftime_devhash_writer_enter(&w.count, &phase);
	call_hook(devhash_hook_point::writer_after_phase_load, ph);
	if (ph != DEVHASH_PHASE_CPU) {
		__atomic_fetch_sub(&w.count, 1, __ATOMIC_RELEASE);
		errno = EPERM;
		return -1;
	}
	const long r = table.elem_delete(key);
	const int err = errno;
	__atomic_fetch_sub(&w.count, 1, __ATOMIC_RELEASE);
	errno = err;
	return r;
}

int nv_gpu_device_hash_map_impl::map_get_next_key(const void *key,
						  void *next_key)
{
	if (next_key == nullptr) {
		errno = EINVAL; // as the CPU table answers
		return -1;
	}
	if (bpftime_devhash_reader_phase(&phase) != DEVHASH_PHASE_DEVICE)
		return table.map_get_next_key(key, next_key);
	// slot order over live slots; a missing or unknown key starts over
	uint64_t start = 0;
	if (key != nullptr) {
		const int64_t j = mirror_find(*(const uint32_t *)key);
		if (j >= 0)
			start = (uint64_t)j + 1;
	}
	for (uint64_t i = start; i < slots; i++) {
		const uint64_t h = __atomic_load_n(mhdr() + i, __ATOMIC_ACQUIRE);
		if (state_of(h) == ST_LIVE) {
			*(uint32_t *)next_key = key_of(h);
			return 0;
		}
	}
	errno = ENOENT;
	return -1;
}

// ------------------------------------------------------- the agent side

devhash_path nv_gpu_device_hash_map_impl::claim(uint64_t self,
						bool has_cached_copy,
						const alive_fn &alive)
{
	for (;;) {
		uint64_t cur = __atomic_load_n(&owner, __ATOMIC_ACQUIRE);
		const bool same_ns = (cur >> 32) == (self >> 32);
		const bool owner_alive =
			cur != 0 && cur != self && same_ns &&
			alive((uint32_t)cur,
			      __atomic_load_n(&owner_start, __ATOMIC_ACQUIRE));
		const devhash_path p = devhash_choose_path(
			get_phase(), cur, self, has_cached_copy, owner_alive);
		if (p == devhash_path::not_owner ||
		    p == devhash_path::have_copy)
			return p;
		if (cur != self &&
		    !__atomic_compare_exchange_n(&owner, &cur, self, false,
						 __ATOMIC_ACQ_REL,
						 __ATOMIC_ACQUIRE))
			continue;
		__atomic_store_n(&owner_start,
				 devhash_process_start_time((uint32_t)self),
				 __ATOMIC_RELEASE);
		__atomic_store_n(status_word(DEVHASH_STATUS_OWNER_PID),
				 (uint64_t)(uint32_t)self, __ATOMIC_RELAXED);
		// only the owner changes PHASE: read it again as the owner
		return devhash_choose_path(get_phase(), self, self, false,
					   false);
	}
}

void nv_gpu_device_hash_map_impl::release_claim(uint64_t self)
{
	uint64_t cur = self;
	if (__atomic_compare_exchange_n(&owner, &cur, 0, false,
					__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
		__atomic_store_n(&owner_start, 0, __ATOMIC_RELEASE);
}

bool nv_gpu_device_hash_map_impl::make_image(uint64_t self,
					     bool has_cached_copy,
					     devhash_image &out,
					     devhash_path &path,
					     const alive_fn &alive,
					     uint64_t bound_ns)
{
	path = claim(self, has_cached_copy, alive);
	bool ok = false;
	switch (path) {
	case devhash_path::not_owner:
	case devhash_path::have_copy:
		return false;
	case devhash_path::first_copy:
		ok = seal_and_build(out, bound_ns);
		break;
	case devhash_path::recover_sealing:
		ok = recover_sealing(out, bound_ns);
		break;
	case devhash_path::takeover:
		out = takeover_image();
		ok = true;
		break;
	}
	if (!ok)
		release_claim(self);
	return ok;
}

bool nv_gpu_device_hash_map_impl::seal(uint64_t bound_ns)
{
	const uint64_t n = WRITER_SLOTS + 1, stride = sizeof(writer_slot) / 8;
	if (is_seal_dead()) {
		// a writer stayed in flight through an earlier seal; once it
		// has finished (stopped or slow) or its process is gone, seal
		// again
		discount_dead_writers();
		if (bpftime_devhash_sealer_poll(&writers[0].count, n, stride) !=
		    0)
			return false;
		__atomic_store_n(&seal_dead, 0, __ATOMIC_RELEASE);
	}
	if (bpftime_devhash_sealer_enter(&phase, &writers[0].count, n,
					 stride) == 0)
		return true;
	const uint64_t deadline = monotonic_ns() + bound_ns;
	for (uint64_t polls = 1;; polls++) {
		if (bpftime_devhash_sealer_poll(&writers[0].count, n, stride) ==
		    0)
			return true;
		if (polls % 64 != 0) {
			cpu_relax();
			continue;
		}
		// every 64 polls: processes that died with writers in flight
		if (polls % 4096 == 0) {
			discount_dead_writers();
			sched_yield();
		}
		if (monotonic_ns() > deadline) {
			// one last look after the deadline
			discount_dead_writers();
			if (bpftime_devhash_sealer_poll(&writers[0].count, n,
							stride) == 0)
				return true;
			__atomic_store_n(&phase, DEVHASH_PHASE_CPU,
					 __ATOMIC_SEQ_CST);
			__atomic_store_n(&seal_dead, 1, __ATOMIC_RELEASE);
			SPDLOG_WARN(
				"device hash map: a CPU writer of a running process stayed in an update for more than {} ms during the seal; no device copy until it has finished",
				bound_ns / 1000000);
			return false;
		}
	}
}

bool nv_gpu_device_hash_map_impl::build_image(devhash_image &out)
{
	// After a seal: a writer that increments now reads PHASE >= 1 and
	// leaves without touching the table, so the counts need not be 0.
	if (get_phase() == DEVHASH_PHASE_CPU)
		return false;
	out.headers.assign(slots, 0);
	out.values.assign(slots, 0);
	out.counters.fill(0);
	uint64_t n = 0;
	bool ok = true;
	table.for_each_in_use_unlocked([&](const void *k, const void *v) {
		if (!ok)
			return;
		uint32_t key;
		uint64_t value;
		std::memcpy(&key, k, sizeof(key));
		std::memcpy(&value, v, sizeof(value));
		if (n >= max_entries) {
			ok = false;
			return;
		}
		uint64_t i = home_slot(key, log_slots);
		for (uint64_t s = 0; s < slots; s++) {
			if (out.headers[i] == 0) {
				out.headers[i] = make_header(key, n, ST_LIVE);
				out.values[i] = value;
				n++;
				return;
			}
			if (holds_key(out.headers[i], key)) {
				ok = false; // a key twice: never from a frozen table
				return;
			}
			i = (i + 1) & (slots - 1);
		}
		ok = false;
	});
	out.count = n;
	if (!ok)
		SPDLOG_ERROR(
			"device hash map: the CPU table could not be placed in the directory");
	return ok;
}

bool nv_gpu_device_hash_map_impl::seal_and_build(devhash_image &out,
						 uint64_t bound_ns)
{
	if (!seal(bound_ns))
		return false;
	if (!build_image(out)) {
		unseal();
		return false;
	}
	return true;
}

bool nv_gpu_device_hash_map_impl::recover_sealing(devhash_image &out,
						  uint64_t bound_ns)
{
	reset_mirror();
	// seal() stores PHASE 1 again and polls: on a timeout PHASE 0 and
	// SEAL_DEAD, as for a first seal
	return seal_and_build(out, bound_ns);
}

devhash_image nv_gpu_device_hash_map_impl::takeover_image()
{
	devhash_image img;
	img.headers.assign(slots, 0);
	img.values.assign(slots, 0);
	uint64_t live = 0, empty = 0;
	for (uint64_t s = 0; s < slots; s++) {
		const uint64_t h = mirror_header(s);
		if (state_of(h) == ST_EMPTY) {
			empty++;
			continue;
		}
		if (state_of(h) == ST_LIVE) {
			// same slot, rows renumbered in slot order
			img.headers[s] = make_header(key_of(h), live, ST_LIVE);
			img.values[s] = mirror_value(s);
			live++;
		} else {
			// claiming, refused, opaque: occupied, no key
			img.headers[s] = OPAQUE_WORD;
		}
	}
	// Each new key takes one empty slot and adds 1 to the count: with
	// fewer than max_entries - L empty slots the directory would fill
	// below max_entries and every later insert of a new key would wait
	// out the full-directory bound.
	const uint64_t e = empty < max_entries ? empty : max_entries;
	img.count = live > max_entries - e ? live : max_entries - e;
	for (uint32_t c = 0; c < NUM_COUNTERS; c++)
		img.counters[c] = __atomic_load_n(
			status_word(DEVHASH_STATUS_COUNTERS + c),
			__ATOMIC_RELAXED);
	return img;
}

void nv_gpu_device_hash_map_impl::seed_mirror(const devhash_image &image)
{
	for (uint64_t s = 0; s < slots; s++)
		__atomic_store_n(mval() + s, image.values[s], __ATOMIC_RELAXED);
	std::atomic_thread_fence(std::memory_order_release);
	for (uint64_t s = 0; s < slots; s++)
		__atomic_store_n(mhdr() + s, image.headers[s],
				 __ATOMIC_RELEASE);
	__atomic_store_n(status_word(DEVHASH_STATUS_COUNT), image.count,
			 __ATOMIC_RELAXED);
}

void nv_gpu_device_hash_map_impl::reset_mirror()
{
	for (uint64_t s = 0; s < slots; s++) {
		__atomic_store_n(mhdr() + s, 0, __ATOMIC_RELAXED);
		__atomic_store_n(mval() + s, 0, __ATOMIC_RELAXED);
	}
}

void nv_gpu_device_hash_map_impl::publish_device_phase()
{
	__atomic_store_n(&phase, DEVHASH_PHASE_DEVICE, __ATOMIC_RELEASE);
}

void nv_gpu_device_hash_map_impl::unseal()
{
	__atomic_store_n(&phase, DEVHASH_PHASE_CPU, __ATOMIC_SEQ_CST);
}

void nv_gpu_device_hash_map_impl::apply_delivery(const uint8_t *staging)
{
	const staging_layout st = staging_for(max_entries);
	// the delivery thread's scratch, kept between deliveries
	static thread_local std::vector<uint64_t> hdr, val;
	hdr.resize(slots);
	val.resize(slots);
	for (uint64_t s = 0; s < slots; s++) {
		std::memcpy(&hdr[s], staging + staging_header_offset(s), 8);
		std::memcpy(&val[s], staging + staging_value_offset(s), 8);
	}
	// Guard first: a slot the mirror has non-empty keeps its header
	// where the snapshot read it empty, and a live header is replaced
	// only by a live header of the same key (a takeover renumbers rows).
	// Device slots never lose their key and live stays live; repairing
	// first would write opaque over a live key whose slot was read empty.
	// This thread is the mirror's only writer: relaxed reads of its own
	// stores.
	for (uint64_t s = 0; s < slots; s++) {
		const uint64_t m = __atomic_load_n(mhdr() + s, __ATOMIC_RELAXED);
		if (state_of(m) == ST_EMPTY)
			continue;
		const bool keep =
			state_of(hdr[s]) == ST_EMPTY ||
			(state_of(m) == ST_LIVE &&
			 !(state_of(hdr[s]) == ST_LIVE &&
			   key_of(hdr[s]) == key_of(m)));
		if (keep) {
			hdr[s] = m;
			val[s] = __atomic_load_n(mval() + s, __ATOMIC_RELAXED);
		}
	}
	// Repair: every empty slot between a key's home and its slot is
	// occupied on the device; the snapshot read it before it was filled.
	// Only keys the mirror does not hold at that slot yet: the path of a
	// key already there was made non-empty when it first came, and mirror
	// slots never go back to empty.
	for (uint64_t s = 0; s < slots; s++) {
		if (!is_keyed(hdr[s]) ||
		    holds_key(__atomic_load_n(mhdr() + s, __ATOMIC_RELAXED),
			      key_of(hdr[s])))
			continue;
		uint64_t i = home_slot(key_of(hdr[s]), log_slots);
		for (uint64_t n = 0; n < slots && i != s; n++) {
			if (state_of(hdr[i]) == ST_EMPTY)
				hdr[i] = OPAQUE_WORD;
			i = (i + 1) & (slots - 1);
		}
	}
	call_hook(devhash_hook_point::delivery_before_pass_a, 0);
	// Pass A: values
	for (uint64_t s = 0; s < slots; s++)
		if (state_of(hdr[s]) == ST_LIVE &&
		    __atomic_load_n(mval() + s, __ATOMIC_RELAXED) != val[s])
			__atomic_store_n(mval() + s, val[s], __ATOMIC_RELAXED);
	// Pass B: the occupancy of every slot this delivery fills
	for (uint64_t s = 0; s < slots; s++)
		if (__atomic_load_n(mhdr() + s, __ATOMIC_RELAXED) == 0 &&
		    state_of(hdr[s]) != ST_EMPTY)
			__atomic_store_n(mhdr() + s, OPAQUE_WORD,
					 __ATOMIC_RELAXED);
	std::atomic_thread_fence(std::memory_order_release);
	// Pass C: the changed headers, in ascending slot order
	for (uint64_t s = 0; s < slots; s++) {
		if (__atomic_load_n(mhdr() + s, __ATOMIC_RELAXED) == hdr[s])
			continue;
		__atomic_store_n(mhdr() + s, hdr[s], __ATOMIC_RELEASE);
		call_hook(devhash_hook_point::delivery_after_pass_c_slot, s);
	}
	uint64_t live = 0, refused = 0;
	for (uint64_t s = 0; s < slots; s++) {
		live += state_of(hdr[s]) == ST_LIVE;
		refused += state_of(hdr[s]) == ST_REFUSED;
	}
	for (uint32_t c = 0; c < NUM_COUNTERS; c++) {
		uint64_t v;
		std::memcpy(&v, staging + st.counters + 8ull * c, 8);
		__atomic_store_n(status_word(DEVHASH_STATUS_COUNTERS + c), v,
				 __ATOMIC_RELAXED);
	}
	uint64_t count;
	std::memcpy(&count, staging + st.count, 8);
	__atomic_store_n(status_word(DEVHASH_STATUS_COUNT), count,
			 __ATOMIC_RELAXED);
	__atomic_store_n(status_word(DEVHASH_STATUS_LIVE), live,
			 __ATOMIC_RELAXED);
	__atomic_store_n(status_word(DEVHASH_STATUS_REFUSED), refused,
			 __ATOMIC_RELAXED);
}

void nv_gpu_device_hash_map_impl::finish_delivery()
{
	__atomic_fetch_add(status_word(DEVHASH_STATUS_DELIVERIES), 1,
			   __ATOMIC_RELEASE);
}
