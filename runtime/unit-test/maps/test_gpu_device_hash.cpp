// The device hash map's CPU side (1533; plan results
// gpu/hash-build-plan-20261127, step S2): phase-0 bpf hash semantics, the
// seal that keeps CPU writers out of the device copy, the image the copy
// starts from, the delivered mirror the CPU reads afterwards, the takeover
// image and the owner's path. No GPU: deliveries are synthetic stagings
// made from a host model of the device directory. Checks that could hang
// under a fault run in forked children with a deadline.
#if defined(BPFTIME_ENABLE_CUDA_ATTACH)
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/gpu/nv_gpu_device_hash_map.hpp>
#include <bpf_map/array_map_binding.hpp>
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <producer_identity.hpp>
#include <handler/handler_manager.hpp>
#include <catch2/catch_test_macros.hpp>
#include <linux/bpf.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <poll.h>
#include <pthread.h>
#include <set>
#include <string>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;
using namespace bpftime_devhash;

namespace
{
using clk = std::chrono::steady_clock;
constexpr uint64_t MS = 1000000ull;

struct segment {
	std::string name;
	managed_shared_memory m;
	explicit segment(const char *tag, size_t bytes = 64 << 20)
		: name(std::string("bpftime-devhash-") + tag + "-" +
		       std::to_string(getpid())),
		  m((shared_memory_object::remove(name.c_str()), create_only),
		    name.c_str(), bytes)
	{
	}
	std::vector<nv_gpu_device_hash_map_impl *> made;
	~segment()
	{
		// the maps' destructors close this process's descriptions of
		// the segment's file, which would keep it alive
		for (auto *map : made)
			m.destroy_ptr(map);
		shared_memory_object::remove(name.c_str());
	}
	nv_gpu_device_hash_map_impl *make(uint64_t max_entries)
	{
		auto *map = m.construct<nv_gpu_device_hash_map_impl>(
			anonymous_instance)(m, max_entries);
		made.push_back(map);
		return map;
	}
};

long put(nv_gpu_device_hash_map_impl *map, uint32_t k, uint64_t v,
	 uint64_t flags = BPF_ANY)
{
	return map->elem_update(&k, &v, flags);
}

const uint64_t *get(nv_gpu_device_hash_map_impl *map, uint32_t k)
{
	return (const uint64_t *)map->elem_lookup(&k);
}

// A key whose home slot is `home` in a directory of 2^log_slots slots.
uint32_t key_with_home(uint32_t home, uint32_t log_slots, uint32_t from = 1)
{
	for (uint32_t k = from;; k++)
		if (home_slot(k, log_slots) == home)
			return k;
}

// Host walk of an image (the device's: state first).
int64_t image_find(const devhash_image &img, uint32_t key, uint32_t log_slots)
{
	const uint64_t slots = img.headers.size();
	uint64_t i = home_slot(key, log_slots);
	for (uint64_t n = 0; n < slots; n++) {
		const uint64_t h = img.headers[i];
		if (state_of(h) == ST_EMPTY)
			return -1;
		if (holds_key(h, key))
			return (int64_t)i;
		i = (i + 1) & (slots - 1);
	}
	return -1;
}

// ------------------------------------------------------------ hooks
// One parking point at a time, armed by the test.
std::mutex park_mu;
std::condition_variable park_cv;
bool park_armed = false;
devhash_hook_point park_point;
uint64_t park_arg = 0; // slot for after_pass_c_slot
bool parked = false;
bool released = false;
thread_local bool park_this_thread = false;

void park_hook(devhash_hook_point p, uint64_t arg)
{
	std::unique_lock<std::mutex> g(park_mu);
	if (!park_armed || p != park_point)
		return;
	if (p == devhash_hook_point::delivery_after_pass_c_slot &&
	    arg != park_arg)
		return;
	if ((p == devhash_hook_point::writer_after_phase_load ||
	     p == devhash_hook_point::writer_after_inflight_inc) &&
	    !park_this_thread)
		return;
	park_armed = false;
	parked = true;
	park_cv.notify_all();
	park_cv.wait(g, [] { return released; });
}

void arm(devhash_hook_point p, uint64_t arg = 0)
{
	std::lock_guard<std::mutex> g(park_mu);
	park_armed = true;
	park_point = p;
	park_arg = arg;
	parked = false;
	released = false;
	set_devhash_hook_for_test(park_hook);
}

bool wait_parked(std::chrono::milliseconds limit = std::chrono::seconds(5))
{
	std::unique_lock<std::mutex> g(park_mu);
	return park_cv.wait_for(g, limit, [] { return parked; });
}

void release()
{
	std::lock_guard<std::mutex> g(park_mu);
	released = true;
	park_cv.notify_all();
}

void disarm()
{
	set_devhash_hook_for_test(nullptr);
	std::lock_guard<std::mutex> g(park_mu);
	park_armed = false;
	released = true;
	park_cv.notify_all();
}

// Releases and joins threads when a check fails and unwinds.
struct joiner {
	std::vector<std::thread *> ts;
	~joiner()
	{
		disarm();
		for (auto *t : ts)
			if (t->joinable())
				t->join();
	}
};

// Runs fn in a forked child (CPU only) with a deadline; the child's exit
// status, or -1 when it was killed at the deadline.
template <class Fn>
int in_child(Fn &&fn, std::chrono::milliseconds deadline)
{
	pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		int rc = 1;
		try {
			rc = fn();
		} catch (...) {
			rc = 2;
		}
		_exit(rc);
	}
	const auto end = clk::now() + deadline;
	int st = 0;
	while (clk::now() < end) {
		if (waitpid(pid, &st, WNOHANG) == pid)
			return WIFEXITED(st) ? WEXITSTATUS(st) : -2;
		usleep(2000);
	}
	kill(pid, SIGKILL);
	waitpid(pid, &st, 0);
	return -1;
}

// A forked writer that stops (SIGSTOP) or blocks for ever at a hook point;
// the parent learns it got there through a pipe.
struct hooked_writer {
	pid_t pid = -1;
	int fds[2] = { -1, -1 };
	static inline int wfd = -1;
	static inline devhash_hook_point where;
	static inline bool stop = false;
	static void hook(devhash_hook_point p, uint64_t)
	{
		if (p != where)
			return;
		char c = 'x';
		(void)!write(wfd, &c, 1);
		if (stop)
			raise(SIGSTOP);
		else
			for (;;)
				pause();
	}
	// the child's exit status: 0 when its update returned want_rc
	hooked_writer(nv_gpu_device_hash_map_impl *map, uint32_t key,
		      devhash_hook_point at, bool stop_there, long want_rc)
	{
		REQUIRE(pipe(fds) == 0);
		pid = fork();
		REQUIRE(pid >= 0);
		if (pid == 0) {
			wfd = fds[1];
			where = at;
			stop = stop_there;
			set_devhash_hook_for_test(hook);
			_exit(put(map, key, key) == want_rc ? 0 : 4);
		}
		close(fds[1]); // EOF if the child dies before the hook
		fds[1] = -1;
		char c;
		REQUIRE(read(fds[0], &c, 1) == 1);
	}
	~hooked_writer()
	{
		if (pid > 0) {
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
		}
		close(fds[0]);
	}
	void kill_now()
	{
		kill(pid, SIGKILL);
		waitpid(pid, nullptr, 0); // reaped: gone
		pid = -1;
	}
	int resume_and_wait()
	{
		int st = 0;
		kill(pid, SIGCONT);
		waitpid(pid, &st, 0);
		pid = -1;
		return WIFEXITED(st) ? WEXITSTATUS(st) : -2;
	}
};

// ------------------------------------------------- device model, staging
// The device directory as the GPU changes it: slots only go empty ->
// claiming -> live or refused, keys never change, sums only grow.
struct device_model {
	uint64_t max_entries, slots;
	uint32_t log_slots;
	std::vector<uint64_t> hdr, sum;
	uint64_t count = 0;
	explicit device_model(uint64_t m)
		: max_entries(m), slots(slots_for(m)),
		  log_slots(log_slots_for(m)), hdr(slots_for(m), 0),
		  sum(slots_for(m), 0)
	{
	}
	// claim a slot for k (walk from home, state first); -1 if none
	int64_t claim(uint32_t k)
	{
		uint64_t i = home_slot(k, log_slots);
		for (uint64_t n = 0; n < slots; n++) {
			if (state_of(hdr[i]) == ST_EMPTY) {
				hdr[i] = make_header(k, 0, ST_CLAIMING);
				return (int64_t)i;
			}
			if (holds_key(hdr[i], k))
				return -1;
			i = (i + 1) & (slots - 1);
		}
		return -1;
	}
	// the winner's reservation: live with the next row, or refused
	void resolve(int64_t slot, uint64_t value)
	{
		const uint32_t k = key_of(hdr[slot]);
		const uint64_t row = count++;
		if (row < max_entries) {
			hdr[slot] = make_header(k, row, ST_LIVE);
			sum[slot] = value;
		} else {
			hdr[slot] = make_header(k, 0, ST_REFUSED);
		}
	}
	int64_t insert(uint32_t k, uint64_t value)
	{
		const int64_t s = claim(k);
		if (s >= 0)
			resolve(s, value);
		return s;
	}
	int64_t find(uint32_t k) const
	{
		uint64_t i = home_slot(k, log_slots);
		for (uint64_t n = 0; n < slots; n++) {
			if (state_of(hdr[i]) == ST_EMPTY)
				return -1;
			if (holds_key(hdr[i], k))
				return (int64_t)i;
			i = (i + 1) & (slots - 1);
		}
		return -1;
	}
	// A snapshot: every slot as the device has it, except those in
	// `shown`, given the header they show (read before they were filled:
	// 0; or a defensive staging).
	std::vector<uint8_t>
	staging(const std::map<uint64_t, uint64_t> &shown = {}) const
	{
		const staging_layout st = staging_for(max_entries);
		std::vector<uint8_t> buf(st.total, 0);
		for (uint64_t s = 0; s < slots; s++) {
			uint64_t h = hdr[s], v = sum[s];
			if (auto it = shown.find(s); it != shown.end()) {
				h = it->second;
				v = state_of(h) == ST_LIVE ? sum[s] : 0;
			}
			std::memcpy(&buf[staging_header_offset(s)], &h, 8);
			std::memcpy(&buf[staging_value_offset(s)], &v, 8);
		}
		std::memcpy(&buf[st.count], &count, 8);
		return buf;
	}
};

void deliver(nv_gpu_device_hash_map_impl *map, const std::vector<uint8_t> &st)
{
	map->apply_delivery(st.data());
	map->finish_delivery();
}

// A map in phase 2 with an empty mirror (as after a first copy of an empty
// table), for delivery tests.
void to_device_phase(nv_gpu_device_hash_map_impl *map)
{
	devhash_image img;
	REQUIRE(map->seal_and_build(img));
	map->seed_mirror(img);
	map->publish_device_phase();
}

} // namespace

// ------------------------------------------------------------- S2.1

TEST_CASE("Device hash map creation checks", "[gpu-devhash-cpu]")
{
	segment seg("create");
	auto *manager = seg.m.construct<handler_manager>("handler_manager")(
		seg.m, MIN_MAX_FD_COUNT);
	(void)map_binding_mutex(seg.m);
	const int type = (int)bpftime::bpf_map_type::BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP;
	struct {
		const char *what;
		uint32_t key, value, entries;
		int error;
	} bad[] = {
		{ "key of 8 bytes", 8, 8, 16, EINVAL },
		{ "value of 16 bytes", 4, 16, 16, EINVAL },
		{ "value of 4 bytes", 4, 4, 16, EINVAL },
		{ "no entries", 4, 8, 0, EINVAL },
		{ "more than 2^22 entries", 4, 8, (1u << 22) + 1, E2BIG },
	};
	for (auto &c : bad) {
		INFO(c.what);
		errno = 0;
		CHECK(manager->set_handler(
			      3,
			      bpf_map_handler(3, "dh", seg.m,
					      { .type = type,
						.key_size = c.key,
						.value_size = c.value,
						.max_ents = c.entries }),
			      seg.m) == -1);
		CHECK(errno == c.error);
		CHECK_FALSE(manager->is_allocated(3));
	}
	REQUIRE(manager->set_handler(3,
				     bpf_map_handler(3, "dh", seg.m,
						     { .type = type,
						       .key_size = 4,
						       .value_size = 8,
						       .max_ents = 1024 }),
				     seg.m) == 3);
	manager->clear_id_at(3, seg.m);
}

// ------------------------------------------------------------- S2.2

TEST_CASE("Device hash map phase 0 behaves as a hash map", "[gpu-devhash-cpu]")
{
	segment seg("phase0");
	auto *map = seg.make(8);
	auto *ref = seg.m.construct<fix_size_hash_map_impl>(anonymous_instance)(
		seg.m, 8, 4, 8);
	struct op {
		char kind; // u update, d delete
		uint32_t key;
		uint64_t value, flags;
	};
	std::vector<op> ops = {
		{ 'u', 1, 10, BPF_NOEXIST }, { 'u', 1, 11, BPF_NOEXIST },
		{ 'u', 2, 20, BPF_EXIST },   { 'u', 1, 12, BPF_EXIST },
		{ 'u', 2, 21, BPF_ANY },     { 'd', 3, 0, 0 },
		{ 'd', 2, 0, 0 },	     { 'u', 9, 1, 4 /* > EXIST */ },
	};
	for (uint32_t k = 10; k < 20; k++)
		ops.push_back({ 'u', k, k, BPF_NOEXIST }); // fills: E2BIG
	for (auto &o : ops) {
		INFO("op " << o.kind << " key " << o.key);
		errno = 0;
		long a = o.kind == 'u' ? map->elem_update(&o.key, &o.value,
							  o.flags) :
					 map->elem_delete(&o.key);
		const int ea = errno;
		errno = 0;
		long b = o.kind == 'u' ? ref->elem_update(&o.key, &o.value,
							  o.flags) :
					 ref->elem_delete(&o.key);
		const int eb = errno;
		CHECK(a == b);
		if (a != 0)
			CHECK(ea == eb);
	}
	for (uint32_t k = 0; k < 32; k++) {
		const void *va = map->elem_lookup(&k), *vb = ref->elem_lookup(&k);
		CHECK((va == nullptr) == (vb == nullptr));
		if (va && vb)
			CHECK(*(const uint64_t *)va == *(const uint64_t *)vb);
	}
	std::set<uint32_t> ka, kb;
	uint32_t cur, next;
	for (int r = map->map_get_next_key(nullptr, &next); r == 0;
	     cur = next, r = map->map_get_next_key(&cur, &next))
		ka.insert(next);
	for (int r = ref->map_get_next_key(nullptr, &next); r == 0;
	     cur = next, r = ref->map_get_next_key(&cur, &next))
		kb.insert(next);
	CHECK(ka == kb);
	CHECK(ka.size() == 8);
	// a null next_key is EINVAL in both phases (it was a SIGSEGV in 2)
	errno = 0;
	CHECK(map->map_get_next_key(nullptr, nullptr) == -1);
	CHECK(errno == EINVAL);
}

// ------------------------------------------------------------- S2.3

TEST_CASE("The seal waits for a writer in flight", "[gpu-devhash-cpu]")
{
	SECTION("a writer that read phase 0 completes, and its key is in the image")
	{
		segment seg("sealrace");
		auto *map = seg.make(4096);
		arm(devhash_hook_point::writer_after_phase_load);
		std::atomic<long> wrc{ 99 };
		devhash_image img;
		std::atomic<bool> done{ false }, ok{ false };
		std::thread writer([&] {
			park_this_thread = true;
			wrc = put(map, 777, 7, BPF_NOEXIST);
		});
		std::thread sealer;
		joiner j{ { &writer, &sealer } };
		REQUIRE(wait_parked());
		sealer = std::thread([&] {
			ok = map->seal_and_build(img, 3000 * MS);
			done = true;
		});
		// release when the sealer has finished, or after 50 ms
		const auto end = clk::now() + std::chrono::milliseconds(50);
		while (!done && clk::now() < end)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		const bool done_before_release = done;
		release();
		writer.join();
		sealer.join();
		CHECK_FALSE(done_before_release);
		REQUIRE(ok);
		REQUIRE(wrc == 0);
		CHECK(image_find(img, 777, log_slots_for(4096)) >= 0);
	}
	SECTION("a writer between its increment and its phase load backs out")
	{
		segment seg("sealmid");
		auto *map = seg.make(4096);
		arm(devhash_hook_point::writer_after_inflight_inc);
		std::atomic<long> wrc{ 99 };
		std::atomic<int> werr{ 0 };
		devhash_image img;
		std::atomic<bool> done{ false }, ok{ false };
		std::thread writer([&] {
			park_this_thread = true;
			wrc = put(map, 778, 7, BPF_NOEXIST);
			werr = errno;
		});
		std::thread sealer;
		joiner j{ { &writer, &sealer } };
		REQUIRE(wait_parked());
		sealer = std::thread([&] {
			ok = map->seal_and_build(img, 3000 * MS);
			done = true;
		});
		const auto end = clk::now() + std::chrono::milliseconds(50);
		while (!done && clk::now() < end)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		const bool done_before_release = done;
		release();
		writer.join();
		sealer.join();
		CHECK_FALSE(done_before_release); // the seal waited for it
		REQUIRE(ok);
		CHECK(wrc == -1);
		CHECK(werr == EPERM); // it read phase 1
		CHECK(image_find(img, 778, log_slots_for(4096)) < 0);
	}
	SECTION("free-running writers: every key is in the image or got EPERM")
	{
		segment seg("sealrace2");
		auto *map = seg.make(4096);
		std::vector<std::vector<std::pair<uint32_t, long>>> done(8);
		std::atomic<bool> go{ true };
		std::vector<std::thread> ws;
		for (int t = 0; t < 8; t++)
			ws.emplace_back([&, t] {
				for (uint32_t i = 0; i < 400 && go; i++) {
					const uint32_t k = t * 1000 + i + 1;
					done[t].push_back(
						{ k, put(map, k, k, BPF_NOEXIST) });
				}
			});
		std::this_thread::sleep_for(std::chrono::microseconds(300));
		devhash_image img;
		const bool ok = map->seal_and_build(img);
		go = false;
		for (auto &w : ws)
			w.join();
		REQUIRE(ok);
		size_t in = 0;
		for (auto &d : done)
			for (auto &[k, rc] : d) {
				const bool found =
					image_find(img, k, log_slots_for(4096)) >=
					0;
				CHECK(found == (rc == 0));
				in += rc == 0;
			}
		CHECK(img.count == in);
	}
}

// ------------------------------------------------------------- S2.4

TEST_CASE("Writers of dead processes are discounted, a stopped one only delays the seal",
	  "[gpu-devhash-cpu]")
{
	SECTION("killed in flight: the seal discounts it")
	{
		segment seg("deadwriter");
		auto *map = seg.make(256);
		for (uint32_t k = 1; k <= 64; k++)
			REQUIRE(put(map, k, k) == 0);
		{
			hooked_writer w(map, 1000,
					devhash_hook_point::writer_after_inflight_inc,
					false, 0);
			CHECK(map->get_inflight() == 1);
			w.kill_now();
		}
		const int rc = in_child(
			[&]() -> int {
				devhash_image img;
				if (!map->seal_and_build(img, 2000 * MS))
					return 3;
				return img.count == 64 &&
					       image_find(img, 1000,
							  log_slots_for(256)) <
						       0 ?
					       0 :
					       4;
			},
			std::chrono::seconds(10));
		CHECK(rc == 0);
		CHECK(map->get_inflight() == 0);
		CHECK_FALSE(map->is_seal_dead());
	}
	SECTION("killed by an ordinary exit of a multi-threaded process")
	{
		segment seg("exitwriter");
		auto *map = seg.make(4096);
		// exits until one has left writers in flight (2-23 of 40 did
		// in the review's measurement)
		int leaked = 0;
		for (int round = 0; round < 200 && leaked == 0; round++) {
			const int rc = in_child(
				[&]() -> int {
					for (int t = 0; t < 4; t++)
						std::thread([&, t] {
							for (uint32_t i = 0;;
							     i++)
								put(map,
								    t * 100000 +
									    i %
										    500,
								    i);
						}).detach();
					usleep(5000);
					_exit(0); // kills the writers
				},
				std::chrono::seconds(10));
			REQUIRE(rc == 0);
			leaked += map->get_inflight() != 0;
		}
		REQUIRE(leaked > 0);
		const int rc = in_child(
			[&]() -> int {
				devhash_image img;
				return map->seal_and_build(img, 2000 * MS) ? 0 :
									     3;
			},
			std::chrono::seconds(10));
		CHECK(rc == 0);
		CHECK(map->get_inflight() == 0);
	}
	SECTION("stopped in flight: the seal fails, and seals once it has finished")
	{
		segment seg("stoppedwriter");
		auto *map = seg.make(256);
		hooked_writer w(map, 2000,
				devhash_hook_point::writer_after_inflight_inc, true,
				0);
		devhash_image img;
		devhash_path path;
		// the first attempt times out (in a child: it could hang)
		CHECK(in_child(
			      [&]() -> int {
				      devhash_image i;
				      devhash_path p;
				      return map->make_image(false, i, p,
							     200 * MS) ?
						     3 :
						     0;
			      },
			      std::chrono::seconds(5)) == 0);
		CHECK(map->is_seal_dead());
		CHECK(map->get_phase() == DEVHASH_PHASE_CPU);
		CHECK(map->get_owner_pid() == 0); // the claim was given back
		// a later attempt gives up at once
		const auto t0 = clk::now();
		CHECK_FALSE(map->make_image(false, img, path, 200 * MS));
		CHECK(clk::now() - t0 < std::chrono::milliseconds(50));
		CHECK(map->get_owner_pid() == 0);
		// the writer resumes: it read phase 0 (the seal had given up)
		CHECK(w.resume_and_wait() == 0);
		CHECK(map->make_image(false, img, path, 200 * MS));
		CHECK(path == devhash_path::first_copy);
		CHECK_FALSE(map->is_seal_dead());
		CHECK(image_find(img, 2000, log_slots_for(256)) >= 0);
	}
}

// ------------------------------------------------------------- S2.5

TEST_CASE("A stripe held by a dead reader does not stop the seal",
	  "[gpu-devhash-cpu]")
{
	segment seg("stripe");
	auto *map = seg.make(256);
	for (uint32_t k = 1; k <= 100; k++)
		REQUIRE(put(map, k, k * 3) == 0);
	auto &table = map->table_for_test().table_for_test();
	const uint32_t k = 42; // a prefilled key: its stripe has a key
	const size_t stripe = table.stripe_of_key(&k);
	table.lock_stripe_for_test(stripe);
	const int rc = in_child(
		[&]() -> int {
			devhash_image img;
			if (!map->seal_and_build(img))
				return 3;
			const int64_t j =
				image_find(img, 42, log_slots_for(256));
			return j >= 0 && img.values[j] == 126 &&
					       img.count == 100 ?
				       0 :
				       4;
		},
		std::chrono::seconds(10));
	table.unlock_stripe_for_test(stripe);
	CHECK(rc == 0);
}

// ------------------------------------------------------------- S2.6

TEST_CASE("The image holds exactly the live keys, updated ones included",
	  "[gpu-devhash-cpu]")
{
	segment seg("image");
	auto *map = seg.make(256);
	std::map<uint32_t, uint64_t> live;
	for (uint32_t k = 1; k <= 200; k++) {
		REQUIRE(put(map, k, k, BPF_NOEXIST) == 0);
		live[k] = k;
	}
	for (uint32_t k = 1; k <= 100; k++) {
		REQUIRE(put(map, k, k + 1000, BPF_ANY) == 0);
		live[k] = k + 1000;
	}
	for (uint32_t k = 50; k < 100; k++) {
		REQUIRE(put(map, k, k + 5000, BPF_EXIST) == 0);
		live[k] = k + 5000;
	}
	for (uint32_t k = 150; k < 180; k++) {
		REQUIRE(map->elem_delete(&k) == 0);
		live.erase(k);
	}
	devhash_image img;
	REQUIRE(map->seal_and_build(img));
	CHECK(img.count == live.size());
	std::set<uint64_t> rows;
	uint64_t keyed = 0;
	for (uint64_t s = 0; s < img.headers.size(); s++) {
		const uint64_t h = img.headers[s];
		if (state_of(h) == ST_EMPTY)
			continue;
		REQUIRE(state_of(h) == ST_LIVE);
		keyed++;
		rows.insert(row_of(h));
		auto it = live.find(key_of(h));
		REQUIRE(it != live.end());
		CHECK(img.values[s] == it->second);
		CHECK(image_find(img, key_of(h), log_slots_for(256)) ==
		      (int64_t)s);
	}
	CHECK(keyed == live.size());
	CHECK(rows.size() == live.size());
	CHECK(*rows.rbegin() == live.size() - 1);
}

// ------------------------------------------------------------- S2.7

TEST_CASE("The delivered mirror stays walkable", "[gpu-devhash-cpu]")
{
	segment seg("mirror");
	const uint64_t M = 32;
	auto *map = seg.make(M);
	to_device_phase(map);
	device_model dev(M);
	const uint32_t L = dev.log_slots;
	const uint64_t S = dev.slots;

	SECTION("A live key behind a slot read before it was filled")
	{
		const uint32_t h = 10;
		const uint32_t k1 = key_with_home(h, L);
		const uint32_t k2 = key_with_home(h, L, k1 + 1);
		dev.insert(k1, 5);
		dev.insert(k2, 7);
		REQUIRE(dev.find(k2) == h + 1);
		deliver(map, dev.staging({ { h, 0 } }));
		// rule (c): every key live in the staging is found
		REQUIRE(get(map, k2) != nullptr);
		CHECK(*get(map, k2) == 7);
		deliver(map, dev.staging());
		CHECK(get(map, k1) != nullptr);
		CHECK(get(map, k2) != nullptr);
	}

	SECTION("A key across the end of the directory, parked in pass C")
	{
		const uint32_t k2 = key_with_home(S - 1, L);
		const uint32_t k = key_with_home(S - 1, L, k2 + 1);
		dev.insert(k2, 3);
		dev.insert(k, 4);
		REQUIRE(dev.find(k) == 0);
		arm(devhash_hook_point::delivery_after_pass_c_slot, 0);
		auto st = dev.staging();
		std::thread w([&] { deliver(map, st); });
		joiner j{ { &w } };
		REQUIRE(wait_parked());
		// rule (d): what get_next_key returns, lookup finds
		uint32_t next = 0;
		REQUIRE(map->map_get_next_key(nullptr, &next) == 0);
		CHECK(next == k);
		CHECK(get(map, next) != nullptr);
		release();
	}

	SECTION("Values before headers, parked before pass A")
	{
		const uint32_t k = key_with_home(20, L);
		dev.insert(k, 99);
		arm(devhash_hook_point::delivery_before_pass_a);
		auto st = dev.staging();
		std::thread w([&] { deliver(map, st); });
		{
			joiner j{ { &w } };
			REQUIRE(wait_parked());
			// rule (b): a found value is never below the staged one
			if (auto *v = get(map, k))
				CHECK(*v >= 99);
			release();
		}
		REQUIRE(get(map, k) != nullptr);
		CHECK(*get(map, k) == 99);
	}

	SECTION("A later delivery's value reaches the mirror")
	{
		const uint32_t k = key_with_home(30, L);
		const int64_t s = dev.insert(k, 5);
		deliver(map, dev.staging());
		REQUIRE(get(map, k) != nullptr);
		CHECK(*get(map, k) == 5);
		dev.sum[s] = 9;
		deliver(map, dev.staging());
		CHECK(*get(map, k) == 9);
	}

	SECTION("Live slots shown empty again keep their keys")
	{
		const uint32_t h = 3;
		const uint32_t k1 = key_with_home(h, L);
		const uint32_t k2 = key_with_home(h, L, k1 + 1);
		const uint32_t k3 = key_with_home(40, L);
		dev.insert(k1, 1);
		dev.insert(k2, 2);
		dev.insert(k3, 3);
		deliver(map, dev.staging());
		REQUIRE(get(map, k1) != nullptr);
		REQUIRE(get(map, k3) != nullptr);
		// k1's slot lies on k2's path, k3's on no path
		deliver(map, dev.staging({ { h, 0 }, { 40, 0 } }));
		CHECK(get(map, k1) != nullptr);
		CHECK(get(map, k2) != nullptr);
		CHECK(get(map, k3) != nullptr);
	}

	SECTION("Key 0 behind an opaque slot, a claim that turns live")
	{
		const uint32_t h0 = home_slot(0, L);
		const uint32_t ka = key_with_home(h0, L);
		dev.insert(ka, 11);
		const int64_t s0 = dev.claim(0); // claiming, behind ka
		REQUIRE(s0 >= 0);
		deliver(map, dev.staging({ { (uint64_t)dev.find(ka), 0 } }));
		CHECK(get(map, 0) == nullptr); // claiming: not found
		dev.resolve(s0, 12);
		deliver(map, dev.staging({ { (uint64_t)dev.find(ka), 0 } }));
		REQUIRE(get(map, 0) != nullptr);
		CHECK(*get(map, 0) == 12);
		deliver(map, dev.staging());
		CHECK(get(map, ka) != nullptr);
	}
}

TEST_CASE("The mirror under deliveries, with readers in threads and processes",
	  "[gpu-devhash-cpu]")
{
	segment seg("mirrorrun");
	const uint64_t M = 128; // 256 slots
	auto *map = seg.make(M);
	to_device_phase(map);
	device_model dev(M);
	const uint64_t S = dev.slots;
	// Precomputed: the stagings, and per delivery n the keys live in any
	// staging up to n with the largest value staged for them by then.
	std::vector<std::vector<uint8_t>> stagings;
	std::vector<std::map<uint32_t, uint64_t>> must;
	std::map<uint32_t, uint64_t> acc;
	uint32_t seed = 12345;
	auto rnd = [&] {
		seed = seed * 1103515245 + 12345;
		return seed >> 8;
	};
	std::vector<int64_t> claiming;
	for (int round = 0; round < 300; round++) {
		std::vector<uint64_t> before = dev.hdr;
		const uint32_t k = 1 + rnd() % 2000;
		if (dev.find(k) < 0) {
			const int64_t s = dev.claim(k);
			if (s >= 0) {
				if (rnd() % 3 == 0)
					claiming.push_back(s);
				else
					dev.resolve(s, 1 + rnd() % 50);
			}
		}
		if (!claiming.empty() && rnd() % 2 == 0) {
			dev.resolve(claiming.back(), 1 + rnd() % 50);
			claiming.pop_back();
		}
		for (uint64_t s = 0; s < S; s++)
			if (state_of(dev.hdr[s]) == ST_LIVE)
				dev.sum[s] += rnd() % 3;
		// slots filled this round may be read before they were filled;
		// every 25 rounds, keyed slots are shown empty again
		std::map<uint64_t, uint64_t> shown;
		for (uint64_t s = 0; s < S; s++)
			if (before[s] == 0 && dev.hdr[s] != 0 && rnd() % 2 == 0)
				shown[s] = 0;
		if (round % 25 == 24)
			for (int i = 0; i < 3; i++) {
				const uint64_t s = rnd() % S;
				if (is_keyed(dev.hdr[s]))
					shown[s] = 0;
			}
		stagings.push_back(dev.staging(shown));
		for (uint64_t s = 0; s < S; s++) {
			uint64_t hh;
			std::memcpy(&hh, &stagings.back()[staging_header_offset(s)],
				    8);
			if (state_of(hh) == ST_LIVE)
				acc[key_of(hh)] =
					std::max(acc[key_of(hh)], dev.sum[s]);
		}
		must.push_back(acc);
	}
	const uint64_t base = *map->status_word(DEVHASH_STATUS_DELIVERIES);
	auto *stop = seg.m.construct<uint64_t>(anonymous_instance)(0);
	// one reader's loop; returns its failures
	auto reader = [&]() -> uint64_t {
		uint64_t failures = 0;
		std::set<uint32_t> seen;
		while (!__atomic_load_n(stop, __ATOMIC_ACQUIRE)) {
			const uint64_t n = __atomic_load_n(
				map->status_word(DEVHASH_STATUS_DELIVERIES),
				__ATOMIC_ACQUIRE);
			// (c) and (b): the keys live by delivery n, with at least
			// the value staged for them by then
			if (n > base)
				for (auto &[k, v] : must[n - base - 1]) {
					auto *p = get(map, k);
					if (!p || *p < v)
						failures++;
				}
			// (a): found once, found again
			for (uint32_t k : seen)
				if (!get(map, k))
					failures++;
			// (d): what get_next_key returns, lookup finds
			uint32_t cur, nx;
			uint64_t steps = 0;
			for (int r = map->map_get_next_key(nullptr, &nx); r == 0;
			     cur = nx, r = map->map_get_next_key(&cur, &nx)) {
				if (!get(map, nx))
					failures++;
				seen.insert(nx);
				if (++steps > S) {
					failures++;
					break;
				}
			}
		}
		return failures;
	};
	// readers in other processes, forked before the deliveries
	std::vector<pid_t> kids;
	for (int i = 0; i < 2; i++) {
		pid_t pid = fork();
		REQUIRE(pid >= 0);
		if (pid == 0)
			_exit(reader() == 0 ? 0 : 3);
		kids.push_back(pid);
	}
	std::atomic<uint64_t> thread_failures{ 0 };
	std::thread r1([&] { thread_failures += reader(); });
	std::thread r2([&] { thread_failures += reader(); });
	for (auto &st : stagings) {
		deliver(map, st);
		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}
	__atomic_store_n(stop, 1, __ATOMIC_RELEASE);
	r1.join();
	r2.join();
	CHECK(thread_failures == 0);
	for (pid_t pid : kids) {
		int st = 0;
		const auto end = clk::now() + std::chrono::seconds(10);
		while (waitpid(pid, &st, WNOHANG) != pid) {
			if (clk::now() > end) {
				kill(pid, SIGKILL);
				waitpid(pid, &st, 0);
				break;
			}
			usleep(2000);
		}
		CHECK((WIFEXITED(st) && WEXITSTATUS(st) == 0));
	}
	// the run kept filling slots: the directory is not full early
	uint64_t filled = 0;
	for (uint64_t s = 0; s < S; s++)
		filled += dev.hdr[s] != 0;
	INFO("slots filled: " << filled);
	CHECK(filled > S / 2);
}

// ------------------------------------------------------------- S2.8

TEST_CASE("get_next_key in phase 2: live keys in slot order", "[gpu-devhash-cpu]")
{
	segment seg("nextkey");
	const uint64_t M = 8; // 16 slots
	auto *map = seg.make(M);
	to_device_phase(map);
	device_model dev(M);
	for (uint32_t k = 1; dev.count < M; k++)
		dev.insert(k, k);
	for (uint32_t k = 100; k < 104; k++) // refused
		dev.insert(k, 1);
	dev.claim(200);
	deliver(map, dev.staging());
	std::vector<uint32_t> expect;
	for (uint64_t s = 0; s < dev.slots; s++)
		if (state_of(dev.hdr[s]) == ST_LIVE)
			expect.push_back(key_of(dev.hdr[s]));
	std::vector<uint32_t> got;
	uint32_t cur, next;
	for (int r = map->map_get_next_key(nullptr, &next); r == 0;
	     cur = next, r = map->map_get_next_key(&cur, &next)) {
		got.push_back(next);
		REQUIRE(got.size() <= dev.slots);
	}
	CHECK(got == expect);
	uint32_t unknown = 999;
	REQUIRE(map->map_get_next_key(&unknown, &next) == 0);
	CHECK(next == expect.front());
	CHECK(get(map, 100) == nullptr);
	errno = 0;
	CHECK(map->map_get_next_key(nullptr, nullptr) == -1);
	CHECK(errno == EINVAL);
	// a full directory: lookups of absent keys end (in a child: a walk
	// without its bound would hang)
	for (uint32_t k = 300; k < 340; k++)
		dev.claim(k);
	deliver(map, dev.staging());
	CHECK(in_child(
		      [&]() -> int {
			      errno = 0;
			      return get(map, 77777) == nullptr &&
					     errno == ENOENT ?
				             0 :
				             3;
		      },
		      std::chrono::seconds(5)) == 0);
}

// ------------------------------------------------------------- S2.9

TEST_CASE("The takeover image keeps the slots and renumbers the rows",
	  "[gpu-devhash-cpu]")
{
	const uint64_t M = 16; // 32 slots
	SECTION("rows, opaque slots and counters")
	{
		segment seg("takeover");
		auto *map = seg.make(M);
		to_device_phase(map);
		device_model dev(M);
		for (uint32_t k = 1; k <= 3; k++)
			dev.insert(k, k * 10);
		const int64_t dead = dev.claim(50); // a claim that died...
		dev.count++; // ... after reserving a row: a gap in the rows
		for (uint32_t k = 4; k <= 6; k++)
			dev.insert(k, k * 10);
		REQUIRE(dead >= 0);
		auto st = dev.staging();
		const uint64_t c = 42;
		std::memcpy(&st[staging_for(M).counters + 8], &c, 8);
		deliver(map, st);
		const devhash_image img = map->takeover_image();
		uint64_t rank = 0;
		for (uint64_t s = 0; s < dev.slots; s++) {
			if (state_of(dev.hdr[s]) == ST_LIVE) {
				CHECK(img.headers[s] ==
				      make_header(key_of(dev.hdr[s]), rank,
						  ST_LIVE));
				CHECK(img.values[s] == dev.sum[s]);
				rank++;
			} else if (state_of(dev.hdr[s]) != ST_EMPTY) {
				CHECK(img.headers[s] == OPAQUE_WORD);
			} else {
				CHECK(img.headers[s] == 0);
			}
		}
		CHECK(rank == 6);
		CHECK(img.count == 6); // E = 25 > M: count = L
		CHECK(img.counters[1] == 42);
	}
	SECTION("the count when few slots are empty, and inserts until full")
	{
		for (uint64_t empty : { uint64_t(0), uint64_t(1), uint64_t(3) }) {
			INFO("empty slots " << empty);
			segment seg("takeovercount");
			auto *map = seg.make(M);
			to_device_phase(map);
			device_model dev(M);
			// L = M - 4 live, rows of dead claims, then refused
			uint32_t k = 1;
			while (dev.count < M - 4)
				dev.insert(k++, 1);
			dev.count = M;
			uint64_t filled = 0;
			for (uint64_t s = 0; s < dev.slots; s++)
				filled += state_of(dev.hdr[s]) != ST_EMPTY;
			while (filled < dev.slots - empty) {
				const int64_t s = dev.claim(k++);
				if (s >= 0) {
					dev.resolve(s, 0); // refused
					filled++;
				}
			}
			deliver(map, dev.staging());
			const devhash_image img = map->takeover_image();
			CHECK(img.count == std::max(M - 4, M - empty));
			// the seeded device: inserting new keys never fills the
			// directory while the count is below max_entries
			device_model seeded(M);
			seeded.hdr = img.headers;
			seeded.count = img.count;
			for (uint32_t n = 0; n < 4 * M; n++) {
				bool full = true;
				for (uint64_t s = 0; s < seeded.slots; s++)
					full &= seeded.hdr[s] != 0;
				if (full) {
					CHECK(seeded.count >= M);
					break;
				}
				seeded.insert(100000 + n, 1);
			}
		}
	}
}

// ------------------------------------------------------------ S2.10

TEST_CASE("The owner's path", "[gpu-devhash-cpu]")
{
	using p = devhash_path;
	CHECK(devhash_choose_path(0, true, false) == p::first_copy);
	CHECK(devhash_choose_path(1, true, false) == p::recover_sealing);
	CHECK(devhash_choose_path(2, true, false) == p::takeover);
	for (uint64_t phase = 0; phase < 3; phase++) {
		CHECK(devhash_choose_path(phase, false, false) == p::not_owner);
		CHECK(devhash_choose_path(phase, false, true) == p::not_owner);
		CHECK(devhash_choose_path(phase, true, true) == p::have_copy);
	}
	SECTION("claim, the cached copy and giving the claim back")
	{
		segment seg("claim");
		auto *map = seg.make(8);
		CHECK(map->claim(false) == p::first_copy);
		CHECK(map->get_owner_pid() == (uint64_t)getpid());
		CHECK(map->still_holds_claim());
		// this process's own description takes its lock again
		CHECK(map->claim(true) == p::have_copy);
		// another process cannot
		CHECK(in_child([&]() -> int {
			      return map->claim(false) == p::not_owner ? 0 : 3;
		      },
			       std::chrono::seconds(5)) == 0);
		map->release_claim();
		CHECK(map->get_owner_pid() == 0);
		CHECK_FALSE(map->still_holds_claim());
		CHECK(in_child([&]() -> int {
			      return map->claim(false) == p::first_copy ? 0 : 3;
		      },
			       std::chrono::seconds(5)) == 0);
	}
}

// ------------------------------------------------------------ S2.14
namespace
{
// a read of n bytes within ms, or false
bool read_within(int fd, void *buf, size_t n, int ms = 10000)
{
	size_t got = 0;
	while (got < n) {
		struct pollfd p = { fd, POLLIN, 0 };
		if (poll(&p, 1, ms) != 1)
			return false;
		const ssize_t r = read(fd, (char *)buf + got, n - got);
		if (r <= 0)
			return false;
		got += (size_t)r;
	}
	return true;
}

// A forked process that runs setup() and then waits until it is killed;
// the parent learns that setup() returned (with its result) through a
// pipe.
struct held_process {
	pid_t pid = -1;
	int fds[2] = { -1, -1 };
	int result = -1;
	template <class Fn> explicit held_process(Fn &&setup)
	{
		REQUIRE(pipe(fds) == 0);
		pid = fork();
		REQUIRE(pid >= 0);
		if (pid == 0) {
			close(fds[0]);
			char r = (char)setup();
			(void)!write(fds[1], &r, 1);
			for (;;)
				pause();
		}
		close(fds[1]);
		fds[1] = -1;
		char r = -1;
		REQUIRE(read_within(fds[0], &r, 1));
		result = r;
	}
	~held_process()
	{
		kill_now();
		close(fds[0]);
	}
	void kill_now()
	{
		if (pid > 0) {
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
			pid = -1;
		}
	}
};

bool comm_is(pid_t pid, const char *name)
{
	char path[64], buf[64] = {};
	snprintf(path, sizeof(path), "/proc/%d/comm", pid);
	FILE *f = fopen(path, "re");
	if (f == nullptr)
		return false;
	if (fgets(buf, sizeof(buf), f) == nullptr)
		buf[0] = 0;
	fclose(f);
	buf[strcspn(buf, "\n")] = 0;
	return strcmp(buf, name) == 0;
}

bool wait_comm(pid_t pid, const char *name)
{
	for (int i = 0; i < 2000; i++) {
		if (comm_is(pid, name))
			return true;
		usleep(1000);
	}
	return false;
}

// the claim, tried for up to 2 s (the kernel releases a dead or exec'd
// holder's lock as it tears the process down)
bool claim_eventually(nv_gpu_device_hash_map_impl *map)
{
	for (int i = 0; i < 400; i++) {
		if (map->claim(false) == devhash_path::first_copy)
			return true;
		usleep(5000);
	}
	return false;
}

// a seal in a child with a deadline: 0 when it sealed and want_key is (or,
// with want_key 0, nothing is required) in the image
int seal_in_child(nv_gpu_device_hash_map_impl *map, uint32_t want_key,
		  uint64_t bound_ns = 2000 * MS)
{
	return in_child(
		[&]() -> int {
			devhash_image img;
			if (!map->seal_and_build(img, bound_ns))
				return 3;
			if (want_key != 0 &&
			    image_find(img, want_key,
				       log_slots_for(map->get_max_entries())) <
				    0)
				return 4;
			return 0;
		},
		std::chrono::seconds(10));
}

struct usable_slots_guard {
	explicit usable_slots_guard(uint32_t n)
	{
		set_devhash_usable_writer_slots_for_test(n);
	}
	~usable_slots_guard()
	{
		set_devhash_usable_writer_slots_for_test(0);
	}
};

// blocks the calling writer thread for ever after its increment (and
// tells wfd), in a child that will exec or be killed
int block_wfd = -1;
std::atomic<bool> block_armed{ false };
void block_hook(devhash_hook_point p, uint64_t)
{
	if (p != devhash_hook_point::writer_after_inflight_inc ||
	    !block_armed.exchange(false))
		return;
	char c = 'b';
	(void)!write(block_wfd, &c, 1);
	for (;;)
		pause();
}
} // namespace

TEST_CASE("Writer slots and the owner's claim follow the processes",
	  "[gpu-devhash-cpu]")
{
	using p = devhash_path;
	SECTION("exec in flight: the seal discounts the slot while the new image runs")
	{
		segment seg("execwriter");
		auto *map = seg.make(256);
		int fds[2];
		REQUIRE(pipe(fds) == 0);
		pid_t pid = fork();
		REQUIRE(pid >= 0);
		if (pid == 0) {
			for (int t = 0; t < 4; t++)
				std::thread([&, t] {
					for (uint32_t i = 0;; i++)
						put(map, 1 + t * 1000 + i % 500,
						    i);
				}).detach();
			int inner[2];
			if (pipe(inner) != 0)
				_exit(5);
			block_wfd = inner[1];
			set_devhash_hook_for_test(block_hook);
			std::thread([&] {
				block_armed = true;
				put(map, 99999, 1);
			}).detach();
			char c;
			if (read(inner[0], &c, 1) != 1)
				_exit(6);
			(void)!write(fds[1], &c, 1);
			execl("/bin/sleep", "sleep", "20", (char *)nullptr);
			_exit(7);
		}
		close(fds[1]);
		char c;
		REQUIRE(read_within(fds[0], &c, 1));
		close(fds[0]);
		REQUIRE(wait_comm(pid, "sleep"));
		// what the threads exec killed left in flight (the blocked
		// writer at least)
		CHECK(map->get_inflight() >= 1);
		CHECK(seal_in_child(map, 0) == 0);
		CHECK(comm_is(pid, "sleep")); // the new image still runs
		CHECK(map->get_inflight() == 0);
		kill(pid, SIGKILL);
		waitpid(pid, nullptr, 0);
	}
	SECTION("a fork child does not keep its dead parent's slot")
	{
		segment seg("forkwriter");
		auto *map = seg.make(256);
		int fds[2];
		REQUIRE(pipe(fds) == 0);
		pid_t parent = fork();
		REQUIRE(parent >= 0);
		if (parent == 0) {
			if (put(map, 1, 1) != 0) // takes a slot
				_exit(5);
			pid_t c = fork(); // an idle child
			if (c < 0)
				_exit(8);
			if (c == 0) {
				for (;;)
					pause();
			}
			if (write(fds[1], &c, sizeof(c)) != sizeof(c))
				_exit(6);
			block_wfd = fds[1];
			set_devhash_hook_for_test(block_hook);
			block_armed = true;
			put(map, 2, 2); // blocks in flight
			_exit(7);
		}
		close(fds[1]);
		pid_t child = -1;
		REQUIRE(read_within(fds[0], &child, sizeof(child)));
		char c;
		REQUIRE(read_within(fds[0], &c, 1));
		close(fds[0]);
		CHECK(map->get_inflight() == 1);
		kill(parent, SIGKILL);
		waitpid(parent, nullptr, 0);
		REQUIRE(child > 0);
		CHECK(kill(child, 0) == 0); // the child lives on
		CHECK(seal_in_child(map, 1) == 0);
		CHECK(map->get_inflight() == 0);
		kill(child, SIGKILL);
	}
	SECTION("a slot left with a count is taken and cleared by the next writer")
	{
		segment seg("slotreuse");
		auto *map = seg.make(256);
		usable_slots_guard one(1);
		{
			hooked_writer w(map, 1000,
					devhash_hook_point::writer_after_inflight_inc,
					false, 0);
			CHECK(map->get_inflight() == 1);
			w.kill_now();
		}
		// the next writer takes the only slot and stays alive, idle
		held_process b([&] { return put(map, 7, 70) == 0 ? 0 : 1; });
		REQUIRE(b.result == 0);
		CHECK(map->get_inflight() == 0);
		// with b alive the slot cannot be discounted: the seal succeeds
		// only because b cleared what the dead writer left
		CHECK(seal_in_child(map, 7, 300 * MS) == 0);
	}
	SECTION("slots exhausted: the overflow slot, and one scan per 100 ms")
	{
		segment seg("exhaust");
		auto *map = seg.make(256);
		usable_slots_guard two(2);
		held_process a([&] { return put(map, 1, 1) == 0 ? 0 : 1; });
		held_process b([&] { return put(map, 2, 2) == 0 ? 0 : 1; });
		REQUIRE(a.result == 0);
		REQUIRE(b.result == 0);
		const int rc = in_child(
			[&]() -> int {
				const uint64_t s0 = devhash_slot_scans_for_test();
				const auto t0 = clk::now();
				int n = 0;
				while (clk::now() - t0 <
				       std::chrono::milliseconds(50)) {
					if (put(map, 3 + n % 100, n) != 0)
						return 3;
					n++;
				}
				const uint64_t s1 = devhash_slot_scans_for_test();
				if (n < 100 || s1 - s0 != 1)
					return 4;
				usleep(120000);
				put(map, 3, 3);
				if (devhash_slot_scans_for_test() - s1 != 1)
					return 5;
				// and its writes went to the overflow slot
				static nv_gpu_device_hash_map_impl *m;
				static uint64_t seen;
				m = map;
				set_devhash_hook_for_test(
					[](devhash_hook_point p, uint64_t) {
						if (p ==
						    devhash_hook_point::
							    writer_after_inflight_inc)
							seen = m->writer_slots_for_test()
								       [nv_gpu_device_hash_map_impl::
										WRITER_SLOTS]
								       .count;
					});
				put(map, 4, 4);
				set_devhash_hook_for_test(nullptr);
				return seen == 1 ? 0 : 6;
			},
			std::chrono::seconds(10));
		CHECK(rc == 0);
	}
	SECTION("one owner at a time: two processes claim at once")
	{
		segment seg("claimrace");
		auto *map = seg.make(8);
		int both_first = 0, none = 0;
		for (int round = 0; round < 200; round++) {
			int go[2], res[2], done[2];
			REQUIRE(pipe(go) == 0);
			REQUIRE(pipe(res) == 0);
			REQUIRE(pipe(done) == 0);
			pid_t kids[2];
			for (int i = 0; i < 2; i++) {
				kids[i] = fork();
				REQUIRE(kids[i] >= 0);
				if (kids[i] == 0) {
					char c;
					if (read(go[0], &c, 1) != 1)
						_exit(5);
					const char r =
						(char)map->claim(false);
					(void)!write(res[1], &r, 1);
					(void)!read(done[0], &c, 1);
					_exit(0);
				}
			}
			const char g[2] = { 'g', 'g' };
			REQUIRE(write(go[1], g, 2) == 2);
			int firsts = 0;
			for (int i = 0; i < 2; i++) {
				char r;
				REQUIRE(read_within(res[0], &r, 1));
				firsts += r == (char)p::first_copy;
			}
			both_first += firsts == 2;
			none += firsts == 0;
			REQUIRE(write(done[1], g, 2) == 2);
			for (pid_t k : kids)
				waitpid(k, nullptr, 0);
			for (int *f : { go, res, done }) {
				close(f[0]);
				close(f[1]);
			}
		}
		CHECK(both_first == 0);
		CHECK(none == 0);
	}
	SECTION("the claim is free once its owner is killed or execs, and a fork child does not hold it")
	{
		segment seg("ownerlife");
		auto *map = seg.make(8);
		{
			held_process a([&] {
				return map->claim(false) == p::first_copy ? 0 : 1;
			});
			REQUIRE(a.result == 0);
			CHECK(map->get_owner_pid() == (uint64_t)a.pid);
			CHECK(map->claim(false) == p::not_owner);
			a.kill_now();
			CHECK(claim_eventually(map));
			map->release_claim();
		}
		{
			int fds[2];
			REQUIRE(pipe(fds) == 0);
			pid_t a = fork();
			REQUIRE(a >= 0);
			if (a == 0) {
				const char r = map->claim(false) == p::first_copy;
				(void)!write(fds[1], &r, 1);
				execl("/bin/sleep", "sleep", "20",
				      (char *)nullptr);
				_exit(7);
			}
			close(fds[1]);
			char r = 0;
			REQUIRE(read_within(fds[0], &r, 1));
			close(fds[0]);
			REQUIRE(r == 1);
			REQUIRE(wait_comm(a, "sleep"));
			CHECK(claim_eventually(map));
			map->release_claim();
			kill(a, SIGKILL);
			waitpid(a, nullptr, 0);
		}
		{
			int fds[2];
			REQUIRE(pipe(fds) == 0);
			pid_t a = fork();
			REQUIRE(a >= 0);
			if (a == 0) {
				if (map->claim(false) != p::first_copy)
					_exit(5);
				pid_t c = fork();
				if (c < 0)
					_exit(8);
				if (c == 0) {
					for (;;)
						pause();
				}
				(void)!write(fds[1], &c, sizeof(c));
				for (;;)
					pause();
			}
			close(fds[1]);
			pid_t c = -1;
			REQUIRE(read_within(fds[0], &c, sizeof(c)));
			close(fds[0]);
			kill(a, SIGKILL);
			waitpid(a, nullptr, 0);
			REQUIRE(c > 0);
			CHECK(kill(c, 0) == 0);
			CHECK(claim_eventually(map));
			map->release_claim();
			kill(c, SIGKILL);
		}
	}
	SECTION("an exception between the seal and the copy leaves phase 0 and the claim free")
	{
		segment seg("throwing");
		auto *map = seg.make(64);
		REQUIRE(put(map, 1, 1) == 0);
		set_devhash_hook_for_test([](devhash_hook_point pt, uint64_t) {
			if (pt == devhash_hook_point::build_image_start)
				throw std::bad_alloc();
		});
		devhash_image img;
		devhash_path path;
		CHECK_THROWS_AS(map->make_image(false, img, path),
				std::bad_alloc);
		set_devhash_hook_for_test(nullptr);
		CHECK(map->get_phase() == DEVHASH_PHASE_CPU);
		CHECK(map->get_owner_pid() == 0);
		CHECK(put(map, 2, 2) == 0);
		CHECK(in_child([&]() -> int {
			      return map->claim(false) == p::first_copy ? 0 : 3;
		      },
			       std::chrono::seconds(5)) == 0);
	}
}

// ------------------------------------------------------------ S2.11

TEST_CASE("Recovery from a dead sealer waits for a writer that read phase 0",
	  "[gpu-devhash-cpu]")
{
	SECTION("the writer completes and its key is in the image")
	{
		segment seg("recover");
		auto *map = seg.make(256);
		REQUIRE(put(map, 5, 50) == 0);
		arm(devhash_hook_point::writer_after_phase_load);
		std::atomic<long> wrc{ 99 };
		devhash_image img;
		std::atomic<bool> done{ false }, ok{ false };
		std::thread writer([&] {
			park_this_thread = true;
			wrc = put(map, 6, 60, BPF_NOEXIST);
		});
		std::thread t;
		joiner j{ { &writer, &t } };
		REQUIRE(wait_parked());
		map->force_phase_for_test(DEVHASH_PHASE_SEALING); // died
		t = std::thread([&] {
			devhash_path path;
			ok = map->make_image(false, img, path, 3000 * MS) &&
			     path == devhash_path::recover_sealing;
			done = true;
		});
		const auto end = clk::now() + std::chrono::milliseconds(50);
		while (!done && clk::now() < end)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		const bool done_before_release = done;
		release();
		writer.join();
		t.join();
		CHECK_FALSE(done_before_release);
		REQUIRE(ok);
		CHECK(wrc == 0);
		CHECK(image_find(img, 6, log_slots_for(256)) >= 0);
		CHECK(image_find(img, 5, log_slots_for(256)) >= 0);
	}
	SECTION("a writer stopped in flight: the recovery times out to phase 0")
	{
		segment seg("recover2");
		auto *map = seg.make(256);
		hooked_writer w(map, 9,
				devhash_hook_point::writer_after_inflight_inc, true,
				0);
		map->force_phase_for_test(DEVHASH_PHASE_SEALING);
		CHECK(in_child(
			      [&]() -> int {
				      devhash_image img;
				      devhash_path p;
				      return map->make_image(false, img, p,
							     200 * MS) ?
						     3 :
						     0;
			      },
			      std::chrono::seconds(5)) == 0);
		CHECK(map->get_phase() == DEVHASH_PHASE_CPU);
		CHECK(map->is_seal_dead());
		CHECK(map->get_owner_pid() == 0);
		CHECK(w.resume_and_wait() == 0);
		CHECK(put(map, 10, 10) == 0);
	}
}

// ------------------------------------------------------------ S2.13

TEST_CASE("Lookups read the CPU table until phase 2", "[gpu-devhash-cpu]")
{
	segment seg("phase1");
	auto *map = seg.make(64);
	REQUIRE(put(map, 3, 33) == 0);
	map->force_phase_for_test(DEVHASH_PHASE_SEALING);
	REQUIRE(get(map, 3) != nullptr);
	CHECK(*get(map, 3) == 33);
	errno = 0;
	CHECK(put(map, 4, 44) == -1);
	CHECK(errno == EPERM);
	uint32_t k = 3;
	CHECK(map->elem_delete(&k) == -1);
	CHECK(errno == EPERM);
}

// ----------------------------------------------- the handler's dispatch

TEST_CASE("The map handler dispatches 1533", "[gpu-devhash-cpu]")
{
	segment seg("handler");
	auto *manager = seg.m.construct<handler_manager>("handler_manager")(
		seg.m, MIN_MAX_FD_COUNT);
	(void)map_binding_mutex(seg.m);
	const int type = (int)bpftime::bpf_map_type::BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP;
	REQUIRE(manager->set_handler(5,
				     bpf_map_handler(5, "dh", seg.m,
						     { .type = type,
						       .key_size = 4,
						       .value_size = 8,
						       .max_ents = 16 }),
				     seg.m) == 5);
	auto &h = std::get<bpf_map_handler>((*manager)[5]);
	CHECK(h.get_userspace_value_size() == 8);
	uint32_t k = 7, next = 0;
	uint64_t v = 70;
	CHECK(h.map_update_elem(&k, &v, BPF_NOEXIST, true) == 0);
	errno = 0;
	CHECK(h.map_update_elem(&k, &v, BPF_NOEXIST, true) == -1);
	CHECK(errno == EEXIST);
	REQUIRE(h.map_lookup_elem(&k, true) != nullptr);
	CHECK(*(const uint64_t *)h.map_lookup_elem(&k, true) == 70);
	CHECK(h.bpf_map_get_next_key(nullptr, &next, true) == 0);
	CHECK(next == 7);
	uint64_t out = 0;
	errno = 0;
	CHECK(h.map_lookup_and_delete_elem(&k, &out, true) == -1);
	CHECK(errno == 524); // ENOTSUPP, as the kernel for such types
	CHECK(h.map_delete_elem(&k, true) == 0);
	errno = 0;
	CHECK(h.map_lookup_elem(&k, true) == nullptr);
	// phase 2: the handler reads the mirror
	auto impl_opt = h.try_get_device_hash_map_impl();
	REQUIRE(impl_opt.has_value());
	auto *impl = *impl_opt;
	to_device_phase(impl);
	device_model dev(16);
	dev.insert(9, 90);
	deliver(impl, dev.staging());
	k = 9;
	REQUIRE(h.map_lookup_elem(&k, true) != nullptr);
	CHECK(*(const uint64_t *)h.map_lookup_elem(&k, true) == 90);
	errno = 0;
	CHECK(h.map_update_elem(&k, &v, BPF_ANY, true) == -1);
	CHECK(errno == EPERM);
	// after freeze, syscall writes are EPERM in any phase
	CHECK(h.freeze() == 0);
	errno = 0;
	CHECK(h.map_delete_elem(&k, true) == -1);
	CHECK(errno == EPERM);
	manager->clear_id_at(5, seg.m);
}
#endif

// ------------------------------------------------- S2.14, second review
namespace
{
// open descriptions of this process
int open_fds()
{
	int n = 0;
	if (DIR *d = opendir("/proc/self/fd")) {
		while (readdir(d) != nullptr)
			n++;
		closedir(d);
	}
	return n;
}
} // namespace

TEST_CASE("Writer slots: the cases the review of the lock rework found",
	  "[gpu-devhash-cpu]")
{
	SECTION("a writer in flight in the overflow slot keeps the seal waiting")
	{
		segment seg("overflowflight");
		auto *map = seg.make(256);
		usable_slots_guard one(1);
		held_process a([&] { return put(map, 1, 1) == 0 ? 0 : 1; });
		REQUIRE(a.result == 0);
		// b finds the only slot held: it writes in the overflow slot
		hooked_writer b(map, 2, devhash_hook_point::writer_after_inflight_inc,
				true, 0);
		CHECK(map->writer_slots_for_test()
			      [nv_gpu_device_hash_map_impl::WRITER_SLOTS]
				      .count == 1);
		CHECK(seal_in_child(map, 0, 300 * MS) == 3);
		CHECK(map->is_seal_dead());
		CHECK(b.resume_and_wait() == 0);
	}
	SECTION("a scan passes a slot held by a live process without clearing it")
	{
		segment seg("scanpast");
		auto *map = seg.make(256);
		usable_slots_guard two(2);
		hooked_writer a(map, 1, devhash_hook_point::writer_after_inflight_inc,
				true, 0);
		REQUIRE(map->get_inflight() == 1);
		held_process b([&] { return put(map, 2, 2) == 0 ? 0 : 1; });
		REQUIRE(b.result == 0);
		// a's writer is still counted, wherever b's scan began
		CHECK(map->get_inflight() == 1);
		CHECK(seal_in_child(map, 0, 300 * MS) == 3);
		CHECK(a.resume_and_wait() == 0);
	}
	SECTION("a new map at a destroyed map's address gets locks of its own")
	{
		segment seg("sameaddress");
		auto *map1 = seg.make(256);
		REQUIRE(put(map1, 1, 1) == 0); // this process holds a slot of map1
		// another process destroys map1 and makes a map at its address
		const int rc = in_child(
			[&]() -> int {
				seg.m.destroy_ptr(map1);
				auto *m2 = seg.m.construct<nv_gpu_device_hash_map_impl>(
					anonymous_instance)(seg.m, 256);
				return m2 == map1 ? 0 : 3;
			},
			std::chrono::seconds(10));
		REQUIRE(rc == 0);
		auto *map2 = map1; // the new map, at the same address
		// a writer of this process in flight on the new map: a seal in
		// another process must wait for it (the slot's lock is held at
		// the new map's bytes)
		arm(devhash_hook_point::writer_after_inflight_inc);
		std::atomic<long> wrc{ 99 };
		std::thread writer([&] {
			park_this_thread = true;
			wrc = put(map2, 5, 5);
		});
		joiner j{ { &writer } };
		REQUIRE(wait_parked());
		CHECK(map2->get_inflight() == 1);
		CHECK(seal_in_child(map2, 0, 300 * MS) == 3);
		release();
		writer.join();
		CHECK(wrc == 0);
	}
	SECTION("after SEAL_DEAD, a stalled writer that dies is discounted")
	{
		segment seg("sealdeaddies");
		auto *map = seg.make(256);
		hooked_writer w(map, 3, devhash_hook_point::writer_after_inflight_inc,
				true, 0);
		CHECK(seal_in_child(map, 0, 200 * MS) == 3);
		REQUIRE(map->is_seal_dead());
		w.kill_now(); // not resumed: its count stays raised
		CHECK(map->get_inflight() == 1);
		devhash_image img;
		devhash_path path;
		CHECK(map->make_image(false, img, path, 200 * MS));
		CHECK_FALSE(map->is_seal_dead());
		CHECK(map->get_inflight() == 0);
	}
	SECTION("a writer killed while the sealer polls is discounted before the deadline")
	{
		segment seg("periodic");
		auto *map = seg.make(256);
		hooked_writer w(map, 4, devhash_hook_point::writer_after_inflight_inc,
				false, 0);
		const int pid = w.pid;
		std::thread killer([&] {
			usleep(100000);
			kill(pid, SIGKILL);
		});
		const auto t0 = clk::now();
		CHECK(seal_in_child(map, 0, 5000 * MS) == 0);
		CHECK(clk::now() - t0 < std::chrono::milliseconds(3000));
		killer.join();
	}
	SECTION("the map's destructor closes this process's description")
	{
		CHECK(in_child(
			      [&]() -> int {
				      segment seg("dtor");
				      auto *map = seg.make(64);
				      const int before = open_fds();
				      if (put(map, 1, 1) != 0)
					      return 3;
				      const int with = open_fds();
				      seg.m.destroy_ptr(map);
				      seg.made.clear();
				      const int after = open_fds();
				      return with == before + 1 && after == before ?
						     0 :
						     4;
			      },
			      std::chrono::seconds(10)) == 0);
	}
}

TEST_CASE("An owner whose description the host closed takes its claim back",
	  "[gpu-devhash-cpu]")
{
	segment seg("ownerreopen");
	auto *map = seg.make(8);
	const std::string path = "/dev/shm/" + seg.name;
	int fds[2];
	REQUIRE(pipe(fds) == 0);
	const pid_t a = fork();
	REQUIRE(a >= 0);
	if (a == 0) {
		const int keep = dup2(fds[1], 900);
		if (keep != 900 || map->claim(false) != devhash_path::first_copy ||
		    !map->still_holds_claim())
			_exit(3);
		// the host closes every descriptor it does not know, then
		// opens the segment's file: the numbers come back as other
		// descriptions of the same file
		close_range(3, 899, 0);
		for (int i = 0; i < 16; i++)
			if (open(path.c_str(), O_RDWR) < 0)
				_exit(5);
		const char r = map->still_holds_claim() ? 1 : 0;
		(void)!write(keep, &r, 1);
		for (;;)
			pause();
	}
	close(fds[1]);
	char r = 0;
	const bool got = read_within(fds[0], &r, 1);
	close(fds[0]);
	struct killer {
		pid_t pid;
		~killer()
		{
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
		}
	} k{ a };
	REQUIRE(got);
	CHECK(r == 1);
	// the claim's lock is held again (by the owner's new description)
	const int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
	REQUIRE(fd >= 0);
	struct flock fl;
	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = (off_t)(map->lock_base() +
			     nv_gpu_device_hash_map_impl::OWNER_LOCK_BYTE);
	fl.l_len = 1;
	REQUIRE(fcntl(fd, F_OFD_GETLK, &fl) == 0);
	close(fd);
	CHECK(fl.l_type != F_UNLCK);
	// and no other process takes it over while the owner runs
	CHECK(map->claim(false) == devhash_path::not_owner);
}

TEST_CASE("The claim's lock alone keeps a second claimant out",
	  "[gpu-devhash-cpu]")
{
	// without the hint (as for an owner of another pid namespace, whose
	// hint counts as gone), the kernel's lock decides
	segment seg("lockalone");
	auto *map = seg.make(8);
	held_process a([&] {
		return map->claim(false) == devhash_path::first_copy ? 0 : 1;
	});
	REQUIRE(a.result == 0);
	map->clear_owner_hint_for_test();
	CHECK(map->claim(false) == devhash_path::not_owner);
	CHECK(in_child([&]() -> int {
		      return map->claim(false) == devhash_path::not_owner ? 0 : 3;
	      },
		       std::chrono::seconds(5)) == 0);
}

TEST_CASE("A free claim of an owner that runs is not taken over",
	  "[gpu-devhash-cpu]")
{
	// the owner's lock released while it runs (a host closed its
	// description) and not noticed yet: the hint keeps the claim its own
	segment seg("ownerhint");
	auto *map = seg.make(8);
	int fds[2];
	REQUIRE(pipe(fds) == 0);
	const pid_t a = fork();
	REQUIRE(a >= 0);
	if (a == 0) {
		const int keep = dup2(fds[1], 900);
		if (keep != 900 || map->claim(false) != devhash_path::first_copy)
			_exit(3);
		close_range(3, 899, 0);
		const char r = 1;
		(void)!write(keep, &r, 1);
		for (;;)
			pause();
	}
	close(fds[1]);
	char r = 0;
	const bool got = read_within(fds[0], &r, 1);
	close(fds[0]);
	struct killer {
		pid_t pid;
		~killer()
		{
			if (pid > 0) {
				kill(pid, SIGKILL);
				waitpid(pid, nullptr, 0);
			}
		}
	} k{ a };
	REQUIRE(got);
	CHECK(map->claim(false) == devhash_path::not_owner);
	kill(a, SIGKILL);
	waitpid(a, nullptr, 0);
	k.pid = -1;
	CHECK(claim_eventually(map));
	map->release_claim();
}

TEST_CASE("Writer slots under a host closing descriptors and vfork children",
	  "[gpu-devhash-cpu]")
{
	SECTION("a writer whose description the host closed is still counted")
	{
		segment seg("hostclose");
		auto *map = seg.make(256);
		usable_slots_guard one(1);
		int fds[2];
		REQUIRE(pipe(fds) == 0);
		const pid_t a = fork();
		REQUIRE(a >= 0);
		if (a == 0) {
			const int keep = dup2(fds[1], 900);
			if (keep != 900 || put(map, 1, 1) != 0)
				_exit(3);
			close_range(3, 899, 0); // the slot's lock goes
			block_wfd = keep;
			set_devhash_hook_for_test(block_hook);
			block_armed = true;
			put(map, 2, 2); // in flight for ever
			_exit(4);
		}
		close(fds[1]);
		char c = 0;
		const bool got = read_within(fds[0], &c, 1);
		close(fds[0]);
		struct killer {
			pid_t pid;
			~killer()
			{
				kill(pid, SIGKILL);
				waitpid(pid, nullptr, 0);
			}
		} k{ a };
		REQUIRE(got);
		REQUIRE(map->get_inflight() == 1);
		// another writer finds the only slot's lock free, its holder
		// running: it does not take it (nor clear its count)
		held_process b([&] { return put(map, 3, 3) == 0 ? 0 : 1; });
		REQUIRE(b.result == 0);
		CHECK(map->get_inflight() == 1);
		// and the seal does not discount it
		CHECK(seal_in_child(map, 0, 300 * MS) == 3);
	}
	SECTION("a holder whose main thread exited is alive while a thread runs")
	{
		// its lock released by a host, its main thread gone: the
		// pid's maps read empty, the running thread's do not
		segment seg("leaderexit");
		auto *map = seg.make(256);
		int fds[2];
		REQUIRE(pipe(fds) == 0);
		const pid_t a = fork();
		REQUIRE(a >= 0);
		if (a == 0) {
			const int keep = dup2(fds[1], 900);
			if (keep != 900 || put(map, 1, 1) != 0)
				_exit(3);
			close_range(3, 899, 0);
			block_wfd = keep;
			set_devhash_hook_for_test(block_hook);
			block_armed = true;
			std::thread([&] { put(map, 2, 2); }).detach();
			// the main thread only (no unwinding through the test
			// framework)
			for (;;) {
				if (map->get_inflight() == 1)
					syscall(SYS_exit, 0);
				usleep(1000);
			}
		}
		close(fds[1]);
		char c = 0;
		const bool got = read_within(fds[0], &c, 1);
		close(fds[0]);
		struct killer {
			pid_t pid;
			~killer()
			{
				kill(pid, SIGKILL);
				waitpid(pid, nullptr, 0);
			}
		} k{ a };
		REQUIRE(got);
		usleep(50000); // the main thread has exited
		REQUIRE(map->get_inflight() == 1);
		CHECK(seal_in_child(map, 0, 300 * MS) == 3);
	}
	SECTION("the description never takes a standard stream's number")
	{
		segment seg("lowfds");
		auto *map = seg.make(64);
		const std::string path = "/dev/shm/" + seg.name;
		CHECK(in_child(
			      [&]() -> int {
				      close(0);
				      close(1);
				      close(2);
				      if (put(map, 1, 1) != 0)
					      return 3;
				      for (int fd = 0; fd < 3; fd++) {
					      char link[64], to[256] = {};
					      snprintf(link, sizeof(link),
						       "/proc/self/fd/%d", fd);
					      if (readlink(link, to,
							   sizeof(to) - 1) > 0 &&
						  path == to)
						      return 4;
				      }
				      return 0;
			      },
			      std::chrono::seconds(5)) == 0);
	}
	SECTION("a fork child leaves a host file at the description's old number")
	{
		segment seg("hostnumber");
		auto *map = seg.make(64);
		CHECK(in_child(
			      [&]() -> int {
				      if (put(map, 1, 1) != 0)
					      return 3;
				      // the host closes everything and opens a file
				      // of its own at the freed numbers
				      close_range(3, ~0U, 0);
				      int mine[8];
				      for (int &fd : mine)
					      if ((fd = open("/dev/null", O_RDONLY)) <
						  0)
						      return 4;
				      const pid_t c = fork();
				      if (c < 0)
					      return 5;
				      if (c == 0) {
					      for (int fd : mine)
						      if (fcntl(fd, F_GETFD) < 0)
							      _exit(6);
					      _exit(0);
				      }
				      int st = 0;
				      waitpid(c, &st, 0);
				      return WIFEXITED(st) ? WEXITSTATUS(st) : 7;
			      },
			      std::chrono::seconds(5)) == 0);
	}
	SECTION("a vfork child's first write leaves the parent's table alone")
	{
		segment seg("vforkwrite");
		auto *map = seg.make(256);
		const int rc = in_child(
			[&]() -> int {
				const pid_t v = vfork();
				if (v < 0)
					return 3;
				if (v == 0)
					_exit(put(map, 1, 1) == 0 ? 0 : 1);
				int st = 0;
				waitpid(v, &st, 0);
				if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
					return 4;
				// this process's own writer, in flight: a seal
				// elsewhere must wait for it
				int fds[2];
				if (pipe(fds) != 0)
					return 5;
				const pid_t sealer = fork();
				if (sealer < 0)
					return 6;
				if (sealer == 0) {
					char x;
					(void)!read(fds[0], &x, 1);
					devhash_image img;
					_exit(map->seal_and_build(img, 300 * MS) ? 1 :
										   0);
				}
				block_wfd = fds[1];
				set_devhash_hook_for_test(block_hook);
				block_armed = true;
				std::thread([&] { put(map, 2, 2); }).detach();
				waitpid(sealer, &st, 0);
				return WIFEXITED(st) ? WEXITSTATUS(st) * 10 : 8;
			},
			std::chrono::seconds(10));
		CHECK(rc == 0);
	}
}

// The image an exec test's child execs into: this binary, which opens the
// segment and writes the map again (an image that maps the segment, as a
// traced process with the agent preloaded again).
TEST_CASE("Device hash exec child", "[.devhash-exec-child]")
{
	const char *name = getenv("BPFTIME_DEVHASH_EXEC_SEG");
	const char *off = getenv("BPFTIME_DEVHASH_EXEC_OFF");
	if (name == nullptr || off == nullptr)
		return;
	managed_shared_memory m(open_only, name);
	auto *map = (nv_gpu_device_hash_map_impl *)((char *)m.get_address() +
						     strtoull(off, nullptr, 10));
	_exit(put(map, 7, 7) == 0 ? 0 : 1);
}

TEST_CASE("An exec'd image of a writer clears what its earlier image left",
	  "[gpu-devhash-cpu]")
{
	segment seg("execagain");
	auto *map = seg.make(256);
	const uint64_t off =
		(uint64_t)((char *)map - (char *)seg.m.get_address());
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		if (put(map, 1, 1) != 0) // takes its slot
			_exit(3);
		int inner[2];
		if (pipe(inner) != 0)
			_exit(4);
		block_wfd = inner[1];
		set_devhash_hook_for_test(block_hook);
		block_armed = true;
		std::thread([&] { put(map, 99, 1); }).detach();
		char c;
		if (read(inner[0], &c, 1) != 1)
			_exit(6);
		// a writer is in flight; the exec kills it
		setenv("BPFTIME_DEVHASH_EXEC_SEG", seg.name.c_str(), 1);
		setenv("BPFTIME_DEVHASH_EXEC_OFF", std::to_string(off).c_str(),
		       1);
		execl("/proc/self/exe", "bpftime_runtime_tests",
		      "[.devhash-exec-child]", (char *)nullptr);
		_exit(7);
	}
	int st = 0;
	for (int i = 0; i < 1000 && waitpid(pid, &st, WNOHANG) != pid; i++)
		usleep(10000);
	if (kill(pid, 0) == 0) {
		kill(pid, SIGKILL);
		waitpid(pid, &st, 0);
		FAIL("the exec'd image did not finish");
	}
	REQUIRE(WIFEXITED(st));
	REQUIRE(WEXITSTATUS(st) == 0);
	// the new image took its pid's slot again: the killed writer's count
	// is gone, its own write done
	CHECK(map->get_inflight() == 0);
	CHECK(seal_in_child(map, 0, 300 * MS) == 0);
}

// The kernel merges one description's adjacent write locks into one range:
// with this process's writer slot at 255 (byte lock_base + 255) the claim's
// byte (lock_base + 256) shows in fdinfo as one line from byte 255. The
// claim check read that as lost, and the owner stopped delivering (S5:
// found as a 1-in-10 failure of the GPU suite).
TEST_CASE("The claim stays held beside this process's writer slot 255",
	  "[gpu-devhash-cpu]")
{
	segment seg("slot255");
	auto *map = seg.make(64);
	// another description of the segment's file holds slots 0..254, so
	// this process's scan ends at 255
	const int other = open(("/dev/shm/" + seg.name).c_str(),
			       O_RDWR | O_CLOEXEC);
	REQUIRE(other >= 0);
	for (uint64_t i = 0; i < 255; i++) {
		struct flock fl {};
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = (off_t)(map->lock_base() + i);
		fl.l_len = 1;
		REQUIRE(fcntl(other, F_OFD_SETLK, &fl) == 0);
	}
	REQUIRE(put(map, 7, 1) == 0); // this process's writer slot: 255
	REQUIRE(map->claim(false) == devhash_path::first_copy);
	CHECK(map->still_holds_claim());
	map->release_claim();
	// given back: another description can take the claim's byte now
	struct flock fl {};
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = (off_t)(map->lock_base() +
			     nv_gpu_device_hash_map_impl::OWNER_LOCK_BYTE);
	fl.l_len = 1;
	CHECK(fcntl(other, F_OFD_SETLK, &fl) == 0);
	close(other);
}

namespace
{
// Makes every new descriptor fail with EMFILE (the lowest free number is
// the limit) until it goes out of scope.
struct no_new_fds {
	struct rlimit old {};
	bool ok = false;
	no_new_fds()
	{
		if (getrlimit(RLIMIT_NOFILE, &old) != 0)
			return;
		const int lowest = dup(0);
		if (lowest < 0)
			return;
		close(lowest);
		struct rlimit r = old;
		r.rlim_cur = (rlim_t)lowest;
		ok = setrlimit(RLIMIT_NOFILE, &r) == 0;
	}
	~no_new_fds()
	{
		if (ok)
			setrlimit(RLIMIT_NOFILE, &old);
	}
};

bool byte_free_for(int fd, uint64_t byte)
{
	struct flock fl {};
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = (off_t)byte;
	fl.l_len = 1;
	if (fcntl(fd, F_OFD_SETLK, &fl) != 0)
		return false;
	fl.l_type = F_UNLCK;
	fcntl(fd, F_OFD_SETLK, &fl);
	return true;
}
} // namespace

// A process at its descriptor limit (or under a seccomp filter) cannot open
// /proc/self/fdinfo: with the segment's file still at the description's
// number, the claim counts as held (the owner keeps delivering).
TEST_CASE("The claim counts as held when fdinfo cannot be read",
	  "[gpu-devhash-cpu]")
{
	segment seg("nofdinfo");
	auto *map = seg.make(64);
	REQUIRE(map->claim(false) == devhash_path::first_copy);
	REQUIRE(map->still_holds_claim());
	bool held = false;
	{
		no_new_fds limit;
		REQUIRE(limit.ok);
		REQUIRE(open("/proc/self/fdinfo/0", O_RDONLY) < 0);
		held = map->still_holds_claim();
	}
	CHECK(held);
	map->release_claim();
}

// Another file at the description's number (the host closed it and opened
// its own) is never taken for the description, even when fdinfo cannot be
// read: giving the claim back must not unlock the host's lock on its file.
TEST_CASE("Giving the claim back leaves another file at its number alone",
	  "[gpu-devhash-cpu]")
{
	segment seg("otherfile");
	auto *map = seg.make(64);
	REQUIRE(map->claim(false) == devhash_path::first_copy);
	const int ours = map->description_fd_for_test();
	REQUIRE(ours >= 3);
	const std::string host_path = "/dev/shm/" + seg.name + "-host";
	const int host = open(host_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	REQUIRE(host >= 0);
	const uint64_t byte =
		map->lock_base() + nv_gpu_device_hash_map_impl::OWNER_LOCK_BYTE;
	// the host's lock on its own file, at the claim's byte, through the
	// description that now has the agent's number
	REQUIRE(dup3(host, ours, O_CLOEXEC) == ours);
	struct flock fl {};
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = (off_t)byte;
	fl.l_len = 1;
	REQUIRE(fcntl(ours, F_OFD_SETLK, &fl) == 0);
	{
		no_new_fds limit;
		REQUIRE(limit.ok);
		map->release_claim();
	}
	// another description of the host's file still finds the byte held
	const int probe = open(host_path.c_str(), O_RDWR | O_CLOEXEC);
	REQUIRE(probe >= 0);
	CHECK_FALSE(byte_free_for(probe, byte));
	close(probe);
	close(host);
	unlink(host_path.c_str());
	// the number is the host's now: the map's destructor must leave it
	// (it checks the file before closing); the test closes it
	close(ours);
}
