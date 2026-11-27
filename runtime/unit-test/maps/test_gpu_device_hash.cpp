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
#include <cstring>
#include <map>
#include <mutex>
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
	~segment()
	{
		shared_memory_object::remove(name.c_str());
	}
	nv_gpu_device_hash_map_impl *make(uint64_t max_entries)
	{
		return m.construct<nv_gpu_device_hash_map_impl>(
			anonymous_instance)(m, max_entries);
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

const uint64_t SELF = devhash_owner_record(0x5151, 4242);
bool nobody_alive(uint32_t, uint64_t)
{
	return false;
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
				      return map->make_image(SELF, false, i, p,
							     nobody_alive,
							     200 * MS) ?
						     3 :
						     0;
			      },
			      std::chrono::seconds(5)) == 0);
		CHECK(map->is_seal_dead());
		CHECK(map->get_phase() == DEVHASH_PHASE_CPU);
		CHECK(map->get_owner() == 0); // the claim was given back
		// another process tries at once, and gives up at once
		const auto t0 = clk::now();
		CHECK_FALSE(map->make_image(devhash_owner_record(0x5151, 9),
					    false, img, path, nobody_alive,
					    200 * MS));
		CHECK(clk::now() - t0 < std::chrono::milliseconds(50));
		CHECK(map->get_owner() == 0);
		// the writer resumes: it read phase 0 (the seal had given up)
		CHECK(w.resume_and_wait() == 0);
		CHECK(map->make_image(SELF, false, img, path, nobody_alive,
				      200 * MS));
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

TEST_CASE("The owner's path and its liveness", "[gpu-devhash-cpu]")
{
	const uint64_t self = devhash_owner_record(0x1111, 100);
	const uint64_t other = devhash_owner_record(0x1111, 200);
	const uint64_t foreign = devhash_owner_record(0x2222, 200);
	using p = devhash_path;
	CHECK(devhash_choose_path(0, 0, self, false, false) == p::first_copy);
	CHECK(devhash_choose_path(1, 0, self, false, false) ==
	      p::recover_sealing);
	CHECK(devhash_choose_path(2, 0, self, false, false) == p::takeover);
	CHECK(devhash_choose_path(2, other, self, false, true) == p::not_owner);
	CHECK(devhash_choose_path(2, other, self, false, false) == p::takeover);
	CHECK(devhash_choose_path(0, foreign, self, false, false) ==
	      p::not_owner);
	CHECK(devhash_choose_path(2, self, self, true, false) == p::have_copy);
	CHECK(devhash_choose_path(2, self, self, false, false) == p::takeover);

	SECTION("liveness through stubbed probes")
	{
		static int kill_err;
		static int tasks;
		static uint64_t start;
		static const devhash_liveness_probes stub = {
			[](uint32_t) { return kill_err; },
			[](uint32_t) { return tasks; },
			[](uint32_t) { return start; },
		};
		set_devhash_liveness_probes_for_test(&stub);
		struct {
			int kill_err, tasks;
			uint64_t now_start, recorded;
			bool alive;
		} cases[] = {
			{ 0, 1, 7, 7, true }, // running
			{ EPERM, 1, 7, 7, true }, // another user's
			{ ESRCH, -1, 0, 7, false }, // gone
			{ 0, 0, 7, 7, false }, // all tasks Z/X
			{ 0, -1, 7, 7, true }, // /proc unreadable
			{ 0, 1, 9, 7, false }, // pid reused
			{ 0, 1, 9, 0, true }, // start unknown
		};
		for (auto &c : cases) {
			kill_err = c.kill_err;
			tasks = c.tasks;
			start = c.now_start;
			INFO("kill " << c.kill_err << " tasks " << c.tasks
				     << " start " << c.now_start << "/"
				     << c.recorded);
			CHECK(devhash_process_alive(200, c.recorded) == c.alive);
		}
		set_devhash_liveness_probes_for_test(nullptr);
	}
	SECTION("a process whose main thread exited while another runs is alive")
	{
		int fds[2];
		REQUIRE(pipe(fds) == 0);
		pid_t pid = fork();
		REQUIRE(pid >= 0);
		if (pid == 0) {
			static int wfd;
			wfd = fds[1];
			pthread_t t;
			pthread_create(
				&t, nullptr,
				[](void *) -> void * {
					usleep(20000);
					char c = 'x';
					(void)!write(wfd, &c, 1);
					for (;;)
						pause();
				},
				nullptr);
			// the main thread only, without unwinding through the
			// test framework (pthread_exit would)
			syscall(SYS_exit, 0);
		}
		close(fds[1]);
		fds[1] = -1;
		char c;
		REQUIRE(read(fds[0], &c, 1) == 1);
		const uint64_t st = devhash_process_start_time(pid);
		CHECK(st != 0);
		CHECK(devhash_process_alive(pid, st));
		kill(pid, SIGKILL);
		// gone even before it is reaped (a zombie)
		for (int i = 0; i < 200 && devhash_process_alive(pid, st); i++)
			usleep(5000);
		CHECK_FALSE(devhash_process_alive(pid, st));
		waitpid(pid, nullptr, 0);
		close(fds[0]);
	}
	SECTION("a claimant between its CAS and its start time is not a reused pid")
	{
		segment seg("claimrace");
		auto *map = seg.make(8);
		const uint64_t ns = producer_pid_namespace();
		// a previous owner that is gone, with its start time recorded
		int fds[2];
		REQUIRE(pipe(fds) == 0);
		pid_t dead = fork();
		REQUIRE(dead >= 0);
		if (dead == 0) {
			for (;;)
				pause();
		}
		CHECK(map->claim(devhash_owner_record(ns, dead), false,
				 nobody_alive) == p::first_copy);
		kill(dead, SIGKILL);
		waitpid(dead, nullptr, 0);
		close(fds[0]);
		close(fds[1]);
		// the first claimant stops between its CAS and its start time
		arm(devhash_hook_point::claim_after_cas);
		std::atomic<int> first_path{ -1 };
		std::thread a([&] {
			first_path = (int)map->claim(
				devhash_owner_record(ns, getpid()), false);
		});
		joiner j{ { &a } };
		REQUIRE(wait_parked());
		// a second claimant (another live process's record) sees a live
		// owner: the dead owner's start time is not taken for this one's
		CHECK(map->claim(devhash_owner_record(ns, getppid()), false) ==
		      p::not_owner);
		CHECK(map->get_owner() == devhash_owner_record(ns, getpid()));
		release();
		a.join();
		CHECK(first_path == (int)p::first_copy);
	}
	SECTION("claim asks about liveness, and gives the claim back")
	{
		segment seg("claim");
		auto *map = seg.make(8);
		CHECK(map->claim(other, false,
				 [](uint32_t, uint64_t) { return true; }) ==
		      p::first_copy);
		CHECK(map->get_owner() == other);
		CHECK(map->claim(self, false,
				 [](uint32_t, uint64_t) { return true; }) ==
		      p::not_owner);
		CHECK(map->claim(self, false, nobody_alive) == p::first_copy);
		CHECK(map->get_owner() == self);
		CHECK(map->claim(self, true, nobody_alive) == p::have_copy);
		map->release_claim(self);
		CHECK(map->get_owner() == 0);
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
			ok = map->make_image(SELF, false, img, path,
					     nobody_alive, 3000 * MS) &&
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
				      return map->make_image(SELF, false, img, p,
							     nobody_alive,
							     200 * MS) ?
						     3 :
						     0;
			      },
			      std::chrono::seconds(5)) == 0);
		CHECK(map->get_phase() == DEVHASH_PHASE_CPU);
		CHECK(map->is_seal_dead());
		CHECK(map->get_owner() == 0);
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
