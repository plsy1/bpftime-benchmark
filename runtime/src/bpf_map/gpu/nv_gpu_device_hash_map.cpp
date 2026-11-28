#include "nv_gpu_device_hash_map.hpp"
#include "fork_handlers.hpp"
#include "producer_identity.hpp"
#include "spdlog/spdlog.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sched.h>
#include <stdexcept>
#include <sys/stat.h>
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

// ------------------------------------------ this process's locks (D8, D11b)
//
// A writer slot and the owner's claim are open file description locks on
// bytes of the segment's file. The kernel releases them when the process
// exits, is killed or execs (the description is O_CLOEXEC), in any pid
// namespace; a fork child closes its inherited copy (fork handler below), or
// it would keep a dead parent's locks held.
//
// Everything here runs inside the traced process, possibly in a probe
// (a BPF program's map update) that fired inside libc: system calls are
// made raw (no interposed or hooked libc function: the syscall server
// interposes close, and a probe on open or malloc would re-enter), nothing
// allocates, and a thread already inside these paths takes the overflow
// slot instead of waiting for local_mutex again.

long raw(long n, long a = 0, long b = 0, long c = 0, long d = 0)
{
	return syscall(n, a, b, c, d);
}
int raw_close(int fd)
{
	return (int)raw(SYS_close, fd);
}
pid_t raw_getpid()
{
	return (pid_t)raw(SYS_getpid);
}
bool raw_fstat(int fd, struct stat *st)
{
	return raw(SYS_fstat, fd, (long)st) == 0;
}
int raw_open(const char *path, int flags)
{
	const long fd = raw(SYS_openat, AT_FDCWD, (long)path, flags);
	return fd < 0 ? -1 : (int)fd;
}

// Calls fn(line) for each line of a /proc file (lines up to 4 KiB; longer
// ones are skipped), read with raw system calls into a stack buffer; stops
// when fn returns true. False when the file cannot be read.
template <class Fn> bool for_each_proc_line(const char *path, Fn &&fn)
{
	const int fd = raw_open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	char buf[8192];
	size_t have = 0;
	bool skipping = false, done = false;
	while (!done) {
		const long n =
			raw(SYS_read, fd, (long)(buf + have), sizeof(buf) - 1 - have);
		if (n <= 0)
			break;
		have += (size_t)n;
		size_t start = 0;
		for (size_t i = 0; i < have && !done; i++) {
			if (buf[i] != '\n')
				continue;
			buf[i] = 0;
			if (!skipping)
				done = fn(buf + start);
			skipping = false;
			start = i + 1;
		}
		std::memmove(buf, buf + start, have - start);
		have -= start;
		if (have == sizeof(buf) - 1) {
			have = 0; // a line too long: skip to its end
			skipping = true;
		}
	}
	raw_close(fd);
	return true;
}

// The segment's file, found from the mapping that holds addr and opened
// O_RDWR | O_CLOEXEC at a number of 3 or more (a host started with its
// standard streams closed would otherwise write its output into the
// segment); -1 without one (no /proc, an anonymous mapping, or a file
// removed meanwhile). Every process locks the same file this way: a guessed
// path could be another file, whose locks nobody else sees.
int open_segment_file(const void *addr, dev_t &dev, ino_t &ino)
{
	const unsigned long a = (unsigned long)(uintptr_t)addr;
	int fd = -1;
	for_each_proc_line("/proc/self/maps", [&](char *line) {
		unsigned long lo, hi, off, inode;
		unsigned major, minor;
		char perms[8];
		int pos = 0;
		if (sscanf(line, "%lx-%lx %7s %lx %x:%x %lu %n", &lo, &hi, perms,
			   &off, &major, &minor, &inode, &pos) < 7)
			return false;
		if (a < lo || a >= hi)
			return false;
		const char *path = line + pos;
		if (path[0] != '/' || inode == 0 ||
		    strstr(path, " (deleted)") != nullptr)
			return true;
		fd = raw_open(path, O_RDWR | O_CLOEXEC);
		if (fd >= 0 && fd < 3) {
			const long high = raw(SYS_fcntl, fd, F_DUPFD_CLOEXEC, 3);
			raw_close(fd);
			fd = high < 0 ? -1 : (int)high;
		}
		struct stat st;
		if (fd >= 0 &&
		    (!raw_fstat(fd, &st) || st.st_ino != (ino_t)inode)) {
			raw_close(fd);
			fd = -1;
		}
		if (fd >= 0) {
			dev = st.st_dev;
			ino = st.st_ino;
		}
		return true;
	});
	return fd;
}

// 0, or the errno of F_OFD_SETLK (EAGAIN: held by another description)
int ofd_lock(int fd, uint64_t byte, short type)
{
	struct flock fl;
	std::memset(&fl, 0, sizeof(fl));
	fl.l_type = type;
	fl.l_whence = SEEK_SET;
	fl.l_start = (off_t)byte;
	fl.l_len = 1;
	const long r = raw(SYS_fcntl, fd, F_OFD_SETLK, (long)&fl);
	return r == 0 ? 0 : (r == -1 ? errno : (int)-r);
}

// Whether the description fd holds a write lock on byte of file ino: the
// lock lines of /proc/self/fdinfo/<fd> are its own description's (another
// description of the same file, or another file at the same number, shows
// none of them). The kernel merges one description's adjacent locks of one
// type into one range (writer slot 255 and the claim's byte 256 show as one
// line from slot 255), so the byte must lie in a line's range, not start it.
// A number that names another file, or none: false. fdinfo unreadable while
// the number names the segment's file (a host at its descriptor limit, a
// seccomp filter): taken as held, as an unreadable maps file keeps a holder
// alive.
bool description_holds(int fd, ino_t ino, uint64_t byte)
{
	struct stat st {};
	if (!raw_fstat(fd, &st) || st.st_ino != ino)
		return false;
	char path[64];
	snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", fd);
	bool held = false, lines = false;
	const bool opened = for_each_proc_line(path, [&](char *line) {
		lines = true;
		if (strncmp(line, "lock:", 5) != 0 ||
		    strstr(line, "OFDLCK") == nullptr)
			return false;
		const char *w = strstr(line, "WRITE ");
		if (w == nullptr)
			return false;
		int pid;
		unsigned major, minor;
		unsigned long i;
		unsigned long long start;
		char end_text[32];
		if (sscanf(w + 6, "%d %x:%x:%lu %llu %31s", &pid, &major, &minor,
			   &i, &start, end_text) != 6 ||
		    (ino_t)i != ino)
			return false;
		const unsigned long long end =
			strcmp(end_text, "EOF") == 0 ?
				~0ull :
				strtoull(end_text, nullptr, 10);
		if (start <= byte && byte <= end)
			held = true;
		return held;
	});
	if (!opened || !lines)
		return true;
	return held;
}

// This image of the process: drawn when the runtime loads (an exec loads a
// new image with the same pid) and in fork children.
std::atomic<uint64_t> image_nonce{ random_generation() };

// This process as a holder hint: pid namespace (low 32 bits of its inode,
// as the perf shards record it) << 32 | pid.
uint64_t self_hint()
{
	return ((producer_pid_namespace() & 0xffffffffull) << 32) |
	       (uint32_t)raw_getpid();
}

// What a maps file says about the segment's file (inode ino): 1 mapped, 0
// not mapped (the file has lines), 2 empty (a zombie, or a thread-group
// leader that exited while other threads run), -1 unreadable (errno set).
int maps_state(const char *path, uint64_t ino)
{
	bool lines = false, mapped = false;
	if (!for_each_proc_line(path, [&](char *line) {
		    lines = true;
		    unsigned long lo, hi, off, inode;
		    unsigned major, minor;
		    char perms[8];
		    if (sscanf(line, "%lx-%lx %7s %lx %x:%x %lu", &lo, &hi,
			       perms, &off, &major, &minor, &inode) == 7 &&
			inode == ino)
			    mapped = true;
		    return mapped;
	    }))
		return -1;
	return mapped ? 1 : lines ? 0 : 2;
}

// Whether the process a holder hint names still runs and maps the
// segment's file (inode ino): then its lock was released by a host that
// closed its description, not by its death or an exec into an image
// without the segment. A process of another pid namespace (its pid means
// nothing here) counts as gone; one whose maps cannot be read, as alive.
// A process whose main thread exited shows empty maps for its pid: its
// other threads' maps decide.
bool holder_alive(uint64_t hint, uint64_t ino)
{
	if (hint == 0 ||
	    (hint >> 32) != (producer_pid_namespace() & 0xffffffffull))
		return false;
	const long pid = (long)(uint32_t)hint;
	if (raw(SYS_kill, pid, 0) != 0 && errno == ESRCH)
		return false;
	char path[96];
	snprintf(path, sizeof(path), "/proc/%ld/maps", pid);
	const int st = maps_state(path, ino);
	if (st == 1 || st == 0)
		return st == 1;
	if (st == -1)
		return errno != ENOENT && errno != ESRCH; // gone meanwhile
	// empty: the threads that still run
	snprintf(path, sizeof(path), "/proc/%ld/task", pid);
	const int dir = raw_open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dir < 0)
		return errno != ENOENT && errno != ESRCH;
	int found = 0; // 1 a thread maps it, -1 a thread's maps do not
	alignas(8) char buf[4096];
	for (;;) {
		const long n = raw(SYS_getdents64, dir, (long)buf, sizeof(buf));
		if (n <= 0)
			break;
		for (long at = 0; at < n && found == 0;) {
			struct ent {
				uint64_t ino;
				int64_t off;
				unsigned short reclen;
				unsigned char type;
				char name[1];
			};
			const ent *d = (const ent *)(buf + at);
			at += d->reclen;
			if (d->name[0] < '0' || d->name[0] > '9')
				continue;
			char tpath[160];
			snprintf(tpath, sizeof(tpath), "/proc/%ld/task/%s/maps",
				 pid, d->name);
			const int t = maps_state(tpath, ino);
			if (t == 1)
				found = 1;
			else if (t == 0)
				found = -1;
			else if (t == -1 && errno != ENOENT && errno != ESRCH)
				found = 1; // unreadable: alive
		}
		if (found != 0)
			break;
	}
	raw_close(dir);
	return found == 1; // every thread empty: a zombie
}

// One entry per map object this process uses. The writers' fast path reads
// obj, gen and slot without a lock; everything else changes under
// local_mutex.
struct local_map {
	std::atomic<const void *> obj{ nullptr };
	std::atomic<uint64_t> gen{ 0 };
	// -1 none yet, 0..WRITER_SLOTS-1 held, WRITER_SLOTS the overflow slot
	std::atomic<int64_t> slot{ -1 };
	// in the overflow slot: scan again after this time
	std::atomic<uint64_t> retry_ns{ 0 };
	int fd = -1;
	dev_t dev = 0;
	ino_t ino = 0;
	bool owner_locked = false;
	bool overflow_logged = false;
};
// Open addressing on the object's address; entries are never removed (a
// new map at a destroyed one's address takes its entry over).
constexpr uint32_t LOCAL_MAPS = 1024;
constexpr uint64_t OVERFLOW_RETRY_NS = 100000000ull;
constexpr uint64_t NO_FILE_RETRY_NS = 10000000000ull;
local_map local_maps[LOCAL_MAPS];
std::atomic<uint32_t> local_map_count{ 0 };
std::mutex local_mutex;
// The process the table belongs to: a vfork-style child (CLONE_VM, no
// fork handler) shares the table and the parent's memory but has an fd
// table of its own, so it must not record descriptions there.
// Set when the runtime loads (a vfork child runs no initializer), and
// in fork children by the handler.
std::atomic<pid_t> table_pid{ (pid_t)syscall(SYS_getpid) };
// The sealer's probe description while a seal runs (a fork child closes
// its copy: it could hold a slot's lock during a discount).
std::atomic<int> sealer_probe_fd{ -1 };
// a thread inside these paths (a probe firing in a libc call made here)
thread_local bool in_local_paths = false;
std::atomic<uint32_t> usable_slots{ 0 }; // tests; 0: all
std::atomic<uint64_t> slot_scans{ 0 };

inline uint32_t local_home(const void *obj)
{
	return (uint32_t)((((uint64_t)(uintptr_t)obj >> 4) *
			   0x9e3779b97f4a7c15ull) >>
			  54); // 10 bits: LOCAL_MAPS
}

// The entry of obj, or null (lock-free; the writers' fast path).
inline local_map *find_local(const void *obj)
{
	uint32_t i = local_home(obj);
	for (uint32_t n = 0; n < LOCAL_MAPS; n++) {
		const void *o = local_maps[i].obj.load(std::memory_order_acquire);
		if (o == obj)
			return &local_maps[i];
		if (o == nullptr)
			return nullptr;
		i = (i + 1) & (LOCAL_MAPS - 1);
	}
	return nullptr;
}

void forget_locks(local_map &e)
{
	e.fd = -1;
	e.owner_locked = false;
	e.slot.store(-1, std::memory_order_relaxed);
	e.retry_ns.store(0, std::memory_order_relaxed);
}

// Closes the entry's description if its number still names a description
// of the segment's file (a number the host reused for a file of its own is
// left alone), then forgets it.
void close_description(local_map &e)
{
	struct stat st;
	if (e.fd >= 0 && raw_fstat(e.fd, &st) && st.st_dev == e.dev &&
	    st.st_ino == e.ino)
		raw_close(e.fd);
	forget_locks(e);
}

// The entry of (obj, gen), made when missing; null when the table is full
// or this is not the table's process (a vfork-style child). Under
// local_mutex.
local_map *local_entry(const void *obj, uint64_t gen)
{
	if (table_pid.load(std::memory_order_relaxed) != raw_getpid())
		return nullptr;
	if (local_map *e = find_local(obj); e != nullptr) {
		if (e->gen.load(std::memory_order_relaxed) != gen) {
			// a new map at a destroyed one's address: closing the
			// description drops the old map's locks
			close_description(*e);
			e->overflow_logged = false;
			e->gen.store(gen, std::memory_order_release);
		}
		return e;
	}
	// keep one entry empty, so that every probe ends
	if (local_map_count.load(std::memory_order_relaxed) + 1 >= LOCAL_MAPS)
		return nullptr;
	uint32_t i = local_home(obj);
	while (local_maps[i].obj.load(std::memory_order_relaxed) != nullptr)
		i = (i + 1) & (LOCAL_MAPS - 1);
	local_map &e = local_maps[i];
	forget_locks(e);
	e.overflow_logged = false;
	e.gen.store(gen, std::memory_order_relaxed);
	e.obj.store(obj, std::memory_order_release);
	local_map_count.fetch_add(1, std::memory_order_relaxed);
	return &e;
}

// The entry's description, opened when missing. A description the host
// closed (its number may name another file now, or another description of
// the segment's file) has lost its locks: it is forgotten, not closed. The
// writer slot is then taken again at the same index when its lock is still
// free (writers in flight keep counting in it); the claim is not (another
// process may have taken it over). Under local_mutex.
bool ensure_description(local_map &e, const void *obj, uint64_t base)
{
	if (e.fd >= 0) {
		struct stat st {};
		const int64_t s = e.slot.load(std::memory_order_relaxed);
		const bool same_file = raw_fstat(e.fd, &st) &&
				       st.st_dev == e.dev && st.st_ino == e.ino;
		// the slot's or the claim's lock shows that the number still
		// names our description
		const uint64_t probe =
			e.owner_locked ?
				base + nv_gpu_device_hash_map_impl::OWNER_LOCK_BYTE :
			(s >= 0 && s < (int64_t)nv_gpu_device_hash_map_impl::
						WRITER_SLOTS) ?
				base + (uint64_t)s :
				~0ull;
		if (same_file &&
		    (probe == ~0ull || description_holds(e.fd, e.ino, probe)))
			return true;
		{
			// what the check saw, for the log (rare path)
			std::string locks;
			char path[64];
			snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", e.fd);
			for_each_proc_line(path, [&](char *line) {
				if (strncmp(line, "lock:", 5) == 0 &&
				    locks.size() < 512)
					locks += std::string(line) + "; ";
				return false;
			});
			SPDLOG_WARN(
				"device hash map: this process's description of the segment was closed; its writer slot and claim are gone (fd {}, same file {}, dev {} ino {}, want dev {} ino {}, probe {}, owner {}, slot {}, locks: {})",
				e.fd, same_file, (uint64_t)st.st_dev,
				(uint64_t)st.st_ino, (uint64_t)e.dev,
				(uint64_t)e.ino, probe, e.owner_locked, s, locks);
		}
		const int64_t old_slot = s;
		const bool was_owner = e.owner_locked;
		forget_locks(e);
		e.fd = open_segment_file(obj, e.dev, e.ino);
		// no other process takes a slot or the claim from a holder that
		// still runs (holder_alive): take them again
		if (e.fd >= 0 && old_slot >= 0 &&
		    old_slot < (int64_t)nv_gpu_device_hash_map_impl::WRITER_SLOTS &&
		    ofd_lock(e.fd, base + (uint64_t)old_slot, F_WRLCK) == 0)
			e.slot.store(old_slot, std::memory_order_release);
		if (e.fd >= 0 && was_owner &&
		    ofd_lock(e.fd,
			     base + nv_gpu_device_hash_map_impl::OWNER_LOCK_BYTE,
			     F_WRLCK) == 0)
			e.owner_locked = true;
		return e.fd >= 0;
	}
	e.fd = open_segment_file(obj, e.dev, e.ino);
	return e.fd >= 0;
}

// a fork from a thread already inside these paths (a signal handler that
// forks) does not wait for the mutex its own thread holds
thread_local bool fork_took_mutex = false;
void devhash_fork_prepare()
{
	fork_took_mutex = !in_local_paths;
	if (fork_took_mutex)
		local_mutex.lock();
}
void devhash_fork_parent()
{
	if (fork_took_mutex)
		local_mutex.unlock();
}
// The child's copies share the parent's descriptions and so its locks: a
// dead parent's slot and claim would stay held while the child runs. Raw
// close: the syscall server's interposed close takes a shared mutex.
void devhash_fork_child()
{
	for (uint32_t i = 0; i < LOCAL_MAPS; i++)
		if (local_maps[i].obj.load(std::memory_order_relaxed) != nullptr)
			close_description(local_maps[i]);
	const int probe = sealer_probe_fd.exchange(-1);
	if (probe >= 0)
		raw_close(probe);
	table_pid.store(raw_getpid(), std::memory_order_relaxed);
	image_nonce.store(random_generation(), std::memory_order_relaxed);
	if (fork_took_mutex)
		local_mutex.unlock();
}
[[maybe_unused]] const int devhash_fork_status =
	bpftime::register_fork_handlers(devhash_fork_prepare,
					devhash_fork_parent, devhash_fork_child);

// A task that shares this process's memory but did not run the fork
// handler (a vfork-style child): it must not take local_mutex (killed
// while holding it, the parent's would stay held) nor touch the table.
bool foreign_task()
{
	return table_pid.load(std::memory_order_relaxed) != raw_getpid();
}

// Holds local_mutex unless this thread is already inside these paths.
// The writers' slow path only tries it: a writer never waits on it (a probe
// or a signal handler on a thread that holds it, a fork child's handler).
struct local_scope {
	bool entered = false;
	explicit local_scope(bool try_only = false)
	{
		if (in_local_paths)
			return;
		if (try_only) {
			if (!local_mutex.try_lock())
				return;
		} else {
			local_mutex.lock();
		}
		in_local_paths = true;
		entered = true;
	}
	~local_scope()
	{
		if (!entered)
			return;
		local_mutex.unlock();
		in_local_paths = false;
	}
};
} // namespace

void bpftime::set_devhash_hook_for_test(devhash_hook_fn fn)
{
	devhash_hook.store(fn, std::memory_order_release);
}

void bpftime::set_devhash_usable_writer_slots_for_test(uint32_t n)
{
	usable_slots.store(n, std::memory_order_relaxed);
}

uint64_t bpftime::devhash_slot_scans_for_test()
{
	return slot_scans.load(std::memory_order_relaxed);
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
	// any slot not zero (an or, not a sum: a count wrapped below zero
	// cannot cancel another slot's writers)
	uint64_t any = 0;
	for (uint64_t i = 0; i < n; i++)
		any |= __atomic_load_n(counts + i * stride, __ATOMIC_SEQ_CST);
	return any;
}

extern "C" __attribute__((noinline, used)) uint64_t
bpftime_devhash_sealer_poll(uint64_t *counts, uint64_t n, uint64_t stride)
{
	// A poll that reads 0 must synchronize with the writers' release
	// decrements, so that the image's scan sees their table writes.
	uint64_t any = 0;
	for (uint64_t i = 0; i < n; i++)
		any |= __atomic_load_n(counts + i * stride, __ATOMIC_SEQ_CST);
	return any;
}

extern "C" __attribute__((noinline, used)) uint64_t
bpftime_devhash_reader_phase(uint64_t *phase)
{
	// pairs with the release store of PHASE 2 after the mirror is seeded
	return __atomic_load_n(phase, __ATOMIC_ACQUIRE);
}

// ---------------------------------------------------- owner and paths

devhash_path bpftime::devhash_choose_path(uint64_t phase, bool got_lock,
					  bool has_cached_copy)
{
	// the lock is held by another description: a live owner
	if (!got_lock)
		return devhash_path::not_owner;
	if (has_cached_copy)
		return devhash_path::have_copy;
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

namespace
{
std::atomic<uint64_t> devhash_destroyed{ 0 };
}
uint64_t bpftime::devhash_destroyed_for_test()
{
	return devhash_destroyed.load();
}

nv_gpu_device_hash_map_impl::~nv_gpu_device_hash_map_impl()
{
	devhash_destroyed.fetch_add(1);
	// this process's description for the map (its writer slot's and its
	// claim's locks) goes with the map; it would also keep a removed
	// segment's file alive
	if (foreign_task())
		return;
	local_scope scope;
	if (!scope.entered)
		return;
	local_map *e = find_local(this);
	if (e != nullptr &&
	    e->gen.load(std::memory_order_relaxed) == generation &&
	    table_pid.load(std::memory_order_relaxed) == raw_getpid())
		close_description(*e);
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

uint64_t nv_gpu_device_hash_map_impl::lock_base() const
{
	// beyond any segment's end, 4096 bytes per map, from the generation
	// (the same in every process). Two live maps share bytes with a chance
	// of about 2^-26 per pair; a shared byte only makes a slot or the claim
	// look held (the copy delayed, never wrong).
	return (1ull << 44) + ((generation >> 1) & ((1ull << 26) - 1)) * 4096;
}

uint64_t *nv_gpu_device_hash_map_impl::my_writer_count()
{
	if (local_map *e = find_local(this);
	    e != nullptr &&
	    e->gen.load(std::memory_order_acquire) == generation) {
		const int64_t s = e->slot.load(std::memory_order_acquire);
		if (s >= 0 && s < (int64_t)WRITER_SLOTS)
			return &writers[s].count;
		if (s == (int64_t)WRITER_SLOTS &&
		    monotonic_ns() <
			    e->retry_ns.load(std::memory_order_relaxed))
			return &writers[WRITER_SLOTS].count;
	}
	return slow_writer_count();
}

// A process's first write (and a retry from the overflow slot): the first
// slot from its pid's onwards whose lock it can take. The overflow slot,
// uncached, for a thread already inside these paths and for a vfork-style
// child.
uint64_t *nv_gpu_device_hash_map_impl::slow_writer_count()
{
	if (foreign_task())
		return &writers[WRITER_SLOTS].count;
	local_scope scope(true);
	if (!scope.entered)
		return &writers[WRITER_SLOTS].count;
	local_map *e = local_entry(this, generation);
	if (e == nullptr)
		return &writers[WRITER_SLOTS].count;
	const int64_t s = e->slot.load(std::memory_order_relaxed);
	if (s >= 0 && s < (int64_t)WRITER_SLOTS && e->fd >= 0)
		return &writers[s].count; // another thread took it meanwhile
	if (s == (int64_t)WRITER_SLOTS &&
	    monotonic_ns() < e->retry_ns.load(std::memory_order_relaxed))
		return &writers[WRITER_SLOTS].count;
	slot_scans.fetch_add(1, std::memory_order_relaxed);
	int64_t got = WRITER_SLOTS;
	const bool file = ensure_description(*e, this, lock_base());
	int lock_errno = 0;
	if (file && e->slot.load(std::memory_order_relaxed) >= 0 &&
	    e->slot.load(std::memory_order_relaxed) < (int64_t)WRITER_SLOTS)
		return &writers[e->slot.load(std::memory_order_relaxed)].count;
	if (file) {
		uint32_t n = usable_slots.load(std::memory_order_relaxed);
		if (n == 0 || n > WRITER_SLOTS)
			n = WRITER_SLOTS;
		const uint32_t first = (uint32_t)raw_getpid() % n;
		for (uint32_t k = 0; k < n; k++) {
			const uint32_t i = (first + k) % n;
			// the lock first: a slot is cleared only by its holder
			const int err = ofd_lock(e->fd, lock_base() + i, F_WRLCK);
			if (err == EAGAIN || err == EACCES)
				continue; // held
			if (err != 0) {
				// the system refuses the locks (a seccomp
				// filter): no slot of its own
				lock_errno = err;
				break;
			}
			const uint64_t was = __atomic_load_n(&writers[i].holder,
							     __ATOMIC_ACQUIRE);
			const uint64_t was_image = __atomic_load_n(
				&writers[i].holder_image, __ATOMIC_RELAXED);
			const uint64_t me = self_hint();
			const uint64_t mine =
				image_nonce.load(std::memory_order_relaxed);
			// this image's own slot (it lost its description):
			// its writers in flight count here
			const bool own = was == me && was_image == mine;
			if (was != me && holder_alive(was, e->ino)) {
				// its holder runs and lost its description:
				// its writers in flight count here
				ofd_lock(e->fd, lock_base() + i, F_UNLCK);
				continue;
			}
			__atomic_store_n(&writers[i].holder_image, mine,
					 __ATOMIC_RELAXED);
			__atomic_store_n(&writers[i].holder, me,
					 __ATOMIC_RELEASE);
			// its previous holder is gone (or an earlier image of
			// this pid, whose threads the exec killed)
			const uint64_t left =
				own ? 0 :
				      __atomic_exchange_n(&writers[i].count, 0,
							  __ATOMIC_SEQ_CST);
			if (left != 0)
				SPDLOG_INFO(
					"device hash map: writer slot {} taken; {} writer(s) left in flight there by a process that is gone",
					i, left);
			got = i;
			break;
		}
	}
	if (got == (int64_t)WRITER_SLOTS) {
		// no file or no locks: nothing will change soon, retry rarely
		const bool held = file && lock_errno == 0;
		e->retry_ns.store(monotonic_ns() + (held ? OVERFLOW_RETRY_NS :
							     NO_FILE_RETRY_NS),
				  std::memory_order_relaxed);
		if (!e->overflow_logged) {
			e->overflow_logged = true;
			SPDLOG_INFO(
				"device hash map: no writer slot of its own for pid {} ({}, errno {}); writing in the shared slot",
				raw_getpid(),
				held ? "all held" :
				file ? "locks refused" :
				       "no segment file to lock",
				lock_errno);
		}
	}
	e->slot.store(got, std::memory_order_release);
	return &writers[got].count;
}

uint64_t nv_gpu_device_hash_map_impl::inflight_sum()
{
	uint64_t sum = 0;
	for (uint32_t i = 0; i <= WRITER_SLOTS; i++)
		sum += __atomic_load_n(&writers[i].count, __ATOMIC_SEQ_CST);
	return sum;
}

uint64_t nv_gpu_device_hash_map_impl::get_inflight()
{
	return inflight_sum();
}

void nv_gpu_device_hash_map_impl::discount_dead_writers(int probe_fd,
							 uint64_t dev,
							 uint64_t ino)
{
	(void)dev;
	if (probe_fd < 0)
		return; // no file to probe: every count stands (alive)
	for (uint32_t i = 0; i < WRITER_SLOTS; i++) {
		if (__atomic_load_n(&writers[i].count, __ATOMIC_SEQ_CST) == 0)
			continue;
		// probe_fd is a description of the sealer's own: it conflicts
		// with every holder, this process's writers included
		if (ofd_lock(probe_fd, lock_base() + i, F_WRLCK) != 0)
			continue; // held: alive
		// Free: the holder exited, was killed or exec'd, or (still
		// running and mapping the segment) lost its description to a
		// host that closed it; no one takes the slot while the sealer
		// holds it.
		if (holder_alive(__atomic_load_n(&writers[i].holder,
						 __ATOMIC_ACQUIRE),
				 ino)) {
			ofd_lock(probe_fd, lock_base() + i, F_UNLCK);
			continue;
		}
		const uint64_t left = __atomic_exchange_n(&writers[i].count, 0,
							  __ATOMIC_SEQ_CST);
		ofd_lock(probe_fd, lock_base() + i, F_UNLCK);
		if (left != 0)
			SPDLOG_INFO(
				"device hash map: discounting {} writer(s) left in flight in slot {} by a process that is gone",
				left, i);
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
	uint64_t *w = my_writer_count();
	const uint64_t ph = bpftime_devhash_writer_enter(w, &phase);
	call_hook(devhash_hook_point::writer_after_phase_load, ph);
	if (ph != DEVHASH_PHASE_CPU) {
		__atomic_fetch_sub(w, 1, __ATOMIC_RELEASE);
		// the device copy is the map from the seal on
		errno = EPERM;
		return -1;
	}
	const long r = table.elem_update(key, value, flags);
	const int err = errno;
	__atomic_fetch_sub(w, 1, __ATOMIC_RELEASE);
	errno = err;
	return r;
}

long nv_gpu_device_hash_map_impl::elem_delete(const void *key)
{
	uint64_t *w = my_writer_count();
	const uint64_t ph = bpftime_devhash_writer_enter(w, &phase);
	call_hook(devhash_hook_point::writer_after_phase_load, ph);
	if (ph != DEVHASH_PHASE_CPU) {
		__atomic_fetch_sub(w, 1, __ATOMIC_RELEASE);
		errno = EPERM;
		return -1;
	}
	const long r = table.elem_delete(key);
	const int err = errno;
	__atomic_fetch_sub(w, 1, __ATOMIC_RELEASE);
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

devhash_path nv_gpu_device_hash_map_impl::claim(bool has_cached_copy)
{
	bool got = false, file = false, entry = false;
	if (!foreign_task()) {
		local_scope scope;
		local_map *e = scope.entered ? local_entry(this, generation) :
					       nullptr;
		entry = e != nullptr;
		if (e != nullptr && ensure_description(*e, this, lock_base())) {
			file = true;
			// the kernel decides: one description holds it at a time
			got = ofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE,
				       F_WRLCK) == 0;
			// a free claim whose holder still runs: it lost its
			// description, and still delivers
			const uint64_t was = __atomic_load_n(&owner_hint,
							     __ATOMIC_ACQUIRE);
			const uint64_t me = self_hint();
			if (got && was != 0 && was != me &&
			    holder_alive(was, e->ino)) {
				ofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE,
					 F_UNLCK);
				got = false;
				SPDLOG_WARN(
					"device hash map: the owner (pid {}) runs without its claim's description; not taken over",
					(uint32_t)was);
			}
			if (got)
				__atomic_store_n(&owner_hint, me,
						 __ATOMIC_RELEASE);
			e->owner_locked = got;
		}
	}
	if (!file)
		SPDLOG_WARN(
			"device hash map: pid {} cannot own the map ({})",
			raw_getpid(),
			entry ? "no segment file to lock" :
				"no entry: a vfork-style child, or 1023 maps used by this process");
	if (got) {
		__atomic_store_n(&owner_pid, (uint64_t)raw_getpid(),
				 __ATOMIC_RELEASE);
		__atomic_store_n(status_word(DEVHASH_STATUS_OWNER_PID),
				 (uint64_t)raw_getpid(), __ATOMIC_RELAXED);
	}
	// only the owner changes PHASE: read it as the owner
	return devhash_choose_path(get_phase(), got, has_cached_copy);
}

void nv_gpu_device_hash_map_impl::release_claim()
{
	if (foreign_task())
		return;
	local_scope scope;
	local_map *e = scope.entered ? local_entry(this, generation) : nullptr;
	if (e == nullptr || !e->owner_locked)
		return;
	// the record first: a next owner stores its own after taking the lock
	uint64_t me = (uint64_t)raw_getpid();
	__atomic_compare_exchange_n(&owner_pid, &me, 0, false, __ATOMIC_ACQ_REL,
				    __ATOMIC_ACQUIRE);
	uint64_t hint = self_hint();
	__atomic_compare_exchange_n(&owner_hint, &hint, 0, false,
				    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
	if (e->fd >= 0 && description_holds(e->fd, e->ino,
					    lock_base() + OWNER_LOCK_BYTE))
		ofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE, F_UNLCK);
	e->owner_locked = false;
}

bool nv_gpu_device_hash_map_impl::still_holds_claim()
{
	if (foreign_task())
		return false;
	local_scope scope;
	local_map *e = scope.entered ? local_entry(this, generation) : nullptr;
	if (e == nullptr || !e->owner_locked)
		return false;
	// our description must still be the one holding the claim's lock: a
	// description the host closed is forgotten with its locks (and the
	// same number may now be another description of the same file)
	return ensure_description(*e, this, lock_base()) && e->owner_locked &&
	       description_holds(e->fd, e->ino, lock_base() + OWNER_LOCK_BYTE);
}

bool nv_gpu_device_hash_map_impl::make_image(bool has_cached_copy,
					     devhash_image &out,
					     devhash_path &path,
					     uint64_t bound_ns)
{
	path = claim(has_cached_copy);
	if (path == devhash_path::not_owner || path == devhash_path::have_copy)
		return false;
	call_hook(devhash_hook_point::after_claim, (uint64_t)path);
	bool ok = false;
	try {
		switch (path) {
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
		default:
			break;
		}
	} catch (...) {
		// after a seal no CPU writer may wait on a copy that never comes
		if (get_phase() == DEVHASH_PHASE_SEALING)
			unseal();
		release_claim();
		throw;
	}
	if (!ok)
		release_claim();
	return ok;
}

bool nv_gpu_device_hash_map_impl::seal(uint64_t bound_ns)
{
	const uint64_t n = WRITER_SLOTS + 1, stride = sizeof(writer_slot) / 8;
	uint64_t *counts = &writers[0].count;
	// a description of the sealer's own, to probe the slots' locks (a fork
	// child closes its copy: it may hold a slot's lock for a moment)
	dev_t dev = 0;
	ino_t ino = 0;
	const int probe = open_segment_file(this, dev, ino);
	sealer_probe_fd.store(probe);
	struct closer {
		int fd;
		~closer()
		{
			int mine = fd;
			sealer_probe_fd.compare_exchange_strong(mine, -1);
			if (fd >= 0)
				raw_close(fd);
		}
	} probe_closer{ probe };
	if (is_seal_dead()) {
		// a writer stayed in flight through an earlier seal; once it
		// has finished (stopped or slow) or its process is gone, seal
		// again
		discount_dead_writers(probe, (uint64_t)dev, (uint64_t)ino);
		if (bpftime_devhash_sealer_poll(counts, n, stride) != 0)
			return false;
		__atomic_store_n(&seal_dead, 0, __ATOMIC_RELEASE);
	}
	if (bpftime_devhash_sealer_enter(&phase, counts, n, stride) == 0)
		return true;
	const uint64_t deadline = monotonic_ns() + bound_ns;
	for (uint64_t polls = 1;; polls++) {
		if (bpftime_devhash_sealer_poll(counts, n, stride) == 0)
			return true;
		if (polls % 64 != 0) {
			cpu_relax();
			continue;
		}
		if (polls == 64)
			call_hook(devhash_hook_point::seal_waiting, 0);
		// every 4096 polls: processes that died with writers in flight
		if (polls % 4096 == 0) {
			discount_dead_writers(probe, (uint64_t)dev, (uint64_t)ino);
			sched_yield();
		}
		if (monotonic_ns() > deadline) {
			// one last look after the deadline
			discount_dead_writers(probe, (uint64_t)dev, (uint64_t)ino);
			if (bpftime_devhash_sealer_poll(counts, n, stride) == 0)
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
	call_hook(devhash_hook_point::build_image_start, 0);
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
	uint64_t filled = 0;
	for (uint64_t s = 0; s < slots; s++)
		if (__atomic_load_n(mhdr() + s, __ATOMIC_RELAXED) == 0 &&
		    state_of(hdr[s]) != ST_EMPTY) {
			__atomic_store_n(mhdr() + s, OPAQUE_WORD,
					 __ATOMIC_RELAXED);
			filled++;
		}
	std::atomic_thread_fence(std::memory_order_release);
	call_hook(devhash_hook_point::delivery_between_b_c, filled);
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
