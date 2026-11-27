// The device hash map's CPU side (1533; plan results
// gpu/hash-build-plan-20261127, step S2): phase-0 bpf hash semantics, the
// seal that keeps CPU writers out of the device copy, the image the copy
// starts from, the delivered mirror the CPU reads afterwards, the takeover
// image and the owner's path. No GPU: deliveries are synthetic stagings
// made from a host model of the device directory.
#if defined(BPFTIME_ENABLE_CUDA_ATTACH)
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/gpu/nv_gpu_device_hash_map.hpp>
#include <bpf_map/array_map_binding.hpp>
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
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
#include <set>
#include <string>
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
uint64_t park_arg = 0; // slot for after_pass_c_slot; phase for writers
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
	// claim a slot for k (walk from home); returns it, or -1
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
	// A snapshot: every slot as the device has it, except the slots in
	// `stale`, read before they were filled (or before they changed):
	// shown with their header from `old` (a header the slot had earlier).
	std::vector<uint8_t> staging(const std::map<uint64_t, uint64_t> &old = {})
		const
	{
		const staging_layout st = staging_for(max_entries);
		std::vector<uint8_t> buf(st.total, 0);
		for (uint64_t s = 0; s < slots; s++) {
			uint64_t h = hdr[s], v = sum[s];
			if (auto it = old.find(s); it != old.end()) {
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
	std::set<uint32_t> ka, kb;
	for (uint32_t k = 0; k < 32; k++) {
		const void *va = map->elem_lookup(&k), *vb = ref->elem_lookup(&k);
		CHECK((va == nullptr) == (vb == nullptr));
		if (va && vb)
			CHECK(*(const uint64_t *)va == *(const uint64_t *)vb);
	}
	uint32_t cur, next;
	for (int r = map->map_get_next_key(nullptr, &next); r == 0;
	     cur = next, r = map->map_get_next_key(&cur, &next))
		ka.insert(next);
	for (int r = ref->map_get_next_key(nullptr, &next); r == 0;
	     cur = next, r = ref->map_get_next_key(&cur, &next))
		kb.insert(next);
	CHECK(ka == kb);
	CHECK(ka.size() == 8);
}

// ------------------------------------------------------------- S2.3

TEST_CASE("The seal waits for a writer that read phase 0", "[gpu-devhash-cpu]")
{
	segment seg("sealrace");
	auto *map = seg.make(4096);
	arm(devhash_hook_point::writer_after_phase_load);
	std::atomic<long> wrc{ 99 };
	std::thread writer([&] {
		park_this_thread = true;
		wrc = put(map, 777, 7, BPF_NOEXIST);
	});
	REQUIRE(wait_parked());
	devhash_image img;
	std::atomic<bool> sealed_done{ false }, sealed_ok{ false };
	std::thread sealer([&] {
		sealed_ok = map->seal_and_build(img, 3000000000ull);
		sealed_done = true;
	});
	// release the writer when the sealer has finished, or after 50 ms
	const auto end = clk::now() + std::chrono::milliseconds(50);
	while (!sealed_done && clk::now() < end)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const bool done_before_release = sealed_done;
	release();
	writer.join();
	sealer.join();
	disarm();
	CHECK_FALSE(done_before_release);
	REQUIRE(sealed_ok);
	REQUIRE(wrc == 0);
	CHECK(image_find(img, 777, log_slots_for(4096)) >= 0);

	// free-running writers: every key is in the image, or got EPERM
	segment seg2("sealrace2");
	auto *map2 = seg2.make(4096);
	std::vector<std::vector<std::pair<uint32_t, long>>> done(8);
	std::atomic<bool> go{ true };
	std::vector<std::thread> ws;
	for (int t = 0; t < 8; t++)
		ws.emplace_back([&, t] {
			for (uint32_t i = 0; i < 400 && go; i++) {
				const uint32_t k = t * 1000 + i + 1;
				done[t].push_back(
					{ k, put(map2, k, k, BPF_NOEXIST) });
			}
		});
	std::this_thread::sleep_for(std::chrono::microseconds(300));
	devhash_image img2;
	REQUIRE(map2->seal_and_build(img2));
	go = false;
	for (auto &w : ws)
		w.join();
	size_t in = 0, refused = 0;
	for (auto &d : done)
		for (auto &[k, rc] : d) {
			const bool found =
				image_find(img2, k, log_slots_for(4096)) >= 0;
			if (rc == 0) {
				CHECK(found);
				in++;
			} else {
				CHECK_FALSE(found);
				refused++;
			}
		}
	CHECK(img2.count == in);
	INFO("inserted " << in << ", refused " << refused);
}

// ------------------------------------------------------------- S2.4

TEST_CASE("A writer that dies in flight stops the seal, a stopped one only delays it",
	  "[gpu-devhash-cpu]")
{
	segment seg("deadwriter");
	auto *map = seg.make(256);
	for (uint32_t k = 1; k <= 64; k++)
		REQUIRE(put(map, k, k) == 0);
	int pipefd[2];
	REQUIRE(pipe(pipefd) == 0);
	pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		static int wfd;
		wfd = pipefd[1];
		set_devhash_hook_for_test([](devhash_hook_point p, uint64_t) {
			if (p != devhash_hook_point::writer_after_inflight_inc)
				return;
			char c = 'x';
			(void)!write(wfd, &c, 1);
			for (;;)
				pause();
		});
		put(map, 1000, 1);
		_exit(0);
	}
	char c;
	REQUIRE(read(pipefd[0], &c, 1) == 1);
	kill(child, SIGKILL);
	waitpid(child, nullptr, 0);
	CHECK(map->get_inflight() == 1);
	devhash_image img;
	const auto t0 = clk::now();
	CHECK_FALSE(map->seal_and_build(img, 200000000ull));
	CHECK(clk::now() - t0 >= std::chrono::milliseconds(200));
	CHECK(map->get_phase() == DEVHASH_PHASE_CPU);
	CHECK(map->is_seal_dead());
	// later CPU updates work, on every stripe
	for (uint32_t k = 1; k <= 64; k++)
		CHECK(put(map, k, k + 100, BPF_EXIST) == 0);
	// a second attempt, from another process, returns at once
	const int rc = in_child(
		[&]() -> int {
			devhash_image i2;
			const auto t = clk::now();
			const bool ok = map->seal_and_build(i2, 200000000ull);
			return !ok && clk::now() - t <
						std::chrono::milliseconds(50) ?
				       0 :
				       3;
		},
		std::chrono::seconds(5));
	CHECK(rc == 0);
	close(pipefd[0]);
	close(pipefd[1]);

	// A stopped writer: the seal times out, the writer finishes later,
	// and the next attempt seals.
	segment seg2("stoppedwriter");
	auto *map2 = seg2.make(256);
	int p2[2];
	REQUIRE(pipe(p2) == 0);
	pid_t c2 = fork();
	REQUIRE(c2 >= 0);
	if (c2 == 0) {
		static int wfd2;
		wfd2 = p2[1];
		set_devhash_hook_for_test([](devhash_hook_point p, uint64_t) {
			if (p != devhash_hook_point::writer_after_inflight_inc)
				return;
			char x = 'x';
			(void)!write(wfd2, &x, 1);
			raise(SIGSTOP);
		});
		// it read phase 0 before it stopped: the update goes through
		_exit(put(map2, 2000, 2) == 0 ? 0 : 4);
	}
	REQUIRE(read(p2[0], &c, 1) == 1);
	int st = 0;
	REQUIRE(waitpid(c2, &st, WUNTRACED) == c2);
	REQUIRE(WIFSTOPPED(st));
	devhash_image img2;
	CHECK_FALSE(map2->seal_and_build(img2, 200000000ull));
	CHECK(map2->is_seal_dead());
	kill(c2, SIGCONT);
	REQUIRE(waitpid(c2, &st, 0) == c2);
	CHECK(WIFEXITED(st));
	CHECK(map2->get_inflight() == 0);
	CHECK(map2->seal_and_build(img2, 200000000ull));
	CHECK_FALSE(map2->is_seal_dead());
	close(p2[0]);
	close(p2[1]);
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
	const uint32_t k = 42;
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
	std::map<uint32_t, uint64_t> first_value;

	SECTION("A live key behind a slot read before it was filled")
	{
		// K1 at its home h, K2 (same home) at h + 1; the snapshot read
		// h before K1 claimed it
		const uint32_t h = 10;
		const uint32_t k1 = key_with_home(h, L);
		const uint32_t k2 = key_with_home(h, L, k1 + 1);
		dev.insert(k1, 5);
		dev.insert(k2, 7);
		REQUIRE(dev.find(k2) == h + 1);
		deliver(map, dev.staging({ { h, 0 } }));
		// rule (c): every key live in the staging is found
		CHECK(get(map, k2) != nullptr);
		if (auto *v = get(map, k2))
			CHECK(*v == 7);
		// K1 shows up with the next delivery
		deliver(map, dev.staging());
		CHECK(get(map, k1) != nullptr);
		CHECK(get(map, k2) != nullptr);
	}

	SECTION("A key across the end of the directory, parked in pass C")
	{
		// K2 at S-1 (its home), K at slot 0 (home S-1): both new in one
		// delivery; pass C stores slot 0 first
		const uint32_t k2 = key_with_home(S - 1, L);
		const uint32_t k = key_with_home(S - 1, L, k2 + 1);
		dev.insert(k2, 3);
		dev.insert(k, 4);
		REQUIRE(dev.find(k) == 0);
		arm(devhash_hook_point::delivery_after_pass_c_slot, 0);
		auto st = dev.staging();
		std::thread w([&] { deliver(map, st); });
		REQUIRE(wait_parked());
		// rule (d): what get_next_key returns, lookup finds
		uint32_t next = 0;
		REQUIRE(map->map_get_next_key(nullptr, &next) == 0);
		CHECK(next == k);
		CHECK(get(map, next) != nullptr);
		release();
		w.join();
		disarm();
	}

	SECTION("Values before headers, parked before pass A")
	{
		const uint32_t k = key_with_home(20, L);
		dev.insert(k, 99);
		arm(devhash_hook_point::delivery_before_pass_a);
		auto st = dev.staging();
		std::thread w([&] { deliver(map, st); });
		REQUIRE(wait_parked());
		// rule (b): a found value is never below the one staged with it
		if (auto *v = get(map, k))
			CHECK(*v >= 99);
		release();
		w.join();
		disarm();
		REQUIRE(get(map, k) != nullptr);
		CHECK(*get(map, k) == 99);
	}

	SECTION("A live slot shown empty again keeps its key")
	{
		// K1 live at its home h; K2 (home h) at h + 1. A later staging
		// shows K1's slot empty: on K2's path (repair would make it
		// opaque) and off any path.
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
		deliver(map, dev.staging({ { h, 0 }, { 40, 0 } }));
		// rule (a): found once, found again
		CHECK(get(map, k1) != nullptr);
		CHECK(get(map, k2) != nullptr);
		CHECK(get(map, k3) != nullptr);
	}

	SECTION("Claiming keys, repairs that last, key 0, and a random run with readers")
	{
		// key 0 with an opaque slot on its path
		const uint32_t h0 = home_slot(0, L);
		const uint32_t ka = key_with_home(h0, L);
		dev.insert(ka, 11);
		const int64_t s0 = dev.claim(0); // claiming, behind ka
		REQUIRE(s0 >= 0);
		// the snapshot read ka's slot before it was filled
		deliver(map, dev.staging({ { (uint64_t)dev.find(ka), 0 } }));
		CHECK(get(map, 0) == nullptr); // claiming: not found
		dev.resolve(s0, 12);
		deliver(map, dev.staging({ { (uint64_t)dev.find(ka), 0 } }));
		REQUIRE(get(map, 0) != nullptr);
		CHECK(*get(map, 0) == 12);
		deliver(map, dev.staging());
		CHECK(get(map, ka) != nullptr);

		// random: a writer delivers, readers check (a) (b) (c) (d)
		std::mutex mu;
		std::map<uint32_t, uint64_t> first; // value first staged live
		std::vector<std::set<uint32_t>> live_at; // per delivery
		std::atomic<bool> stop{ false };
		std::atomic<uint64_t> failures{ 0 };
		const uint64_t base =
			*map->status_word(DEVHASH_STATUS_DELIVERIES);
		auto reader = [&] {
			std::set<uint32_t> seen;
			while (!stop) {
				const uint64_t n = __atomic_load_n(
					map->status_word(
						DEVHASH_STATUS_DELIVERIES),
					__ATOMIC_ACQUIRE);
				std::set<uint32_t> must;
				std::map<uint32_t, uint64_t> mins;
				{
					std::lock_guard<std::mutex> g(mu);
					if (n > base && n - base <= live_at.size())
						must = live_at[n - base - 1];
					mins = first;
				}
				for (uint32_t k : must)
					if (!get(map, k))
						failures++;
				for (uint32_t k : seen)
					if (!get(map, k))
						failures++;
				for (auto &[k, v0] : mins) {
					if (auto *v = get(map, k)) {
						seen.insert(k);
						if (*v < v0)
							failures++;
					}
				}
				// a dump visits each slot at most once
				uint32_t cur, nx;
				uint64_t steps = 0;
				for (int r = map->map_get_next_key(nullptr, &nx);
				     r == 0; cur = nx,
					 r = map->map_get_next_key(&cur, &nx)) {
					if (!get(map, nx))
						failures++;
					if (++steps > S) {
						failures++;
						break;
					}
				}
			}
		};
		std::thread r1(reader), r2(reader);
		uint32_t seed = 12345;
		auto rnd = [&] {
			seed = seed * 1103515245 + 12345;
			return seed >> 8;
		};
		std::vector<int64_t> claiming;
		for (int round = 0; round < 400; round++) {
			for (int i = 0; i < 2; i++) {
				const uint32_t k = 100 + rnd() % 200;
				if (dev.find(k) >= 0)
					continue;
				const int64_t s = dev.claim(k);
				if (s < 0)
					continue;
				if (rnd() % 3 == 0)
					claiming.push_back(s);
				else
					dev.resolve(s, 1 + rnd() % 50);
			}
			if (!claiming.empty() && rnd() % 2 == 0) {
				dev.resolve(claiming.back(), 1 + rnd() % 50);
				claiming.pop_back();
			}
			for (uint64_t s = 0; s < S; s++)
				if (state_of(dev.hdr[s]) == ST_LIVE)
					dev.sum[s] += rnd() % 3;
			// stale slots: newly filled ones read before filling
			std::map<uint64_t, uint64_t> old;
			for (uint64_t s = 0; s < S; s++)
				if (state_of(dev.hdr[s]) != ST_EMPTY &&
				    state_of(map->mirror_header(s)) ==
					    ST_EMPTY &&
				    rnd() % 3 == 0)
					old[s] = 0;
			auto st = dev.staging(old);
			std::set<uint32_t> live_now;
			{
				std::lock_guard<std::mutex> g(mu);
				for (uint64_t s = 0; s < S; s++) {
					uint64_t hh;
					std::memcpy(&hh,
						    &st[staging_header_offset(s)],
						    8);
					if (state_of(hh) == ST_LIVE) {
						live_now.insert(key_of(hh));
						first.emplace(key_of(hh),
							      dev.sum[s]);
					}
				}
				live_at.push_back(live_now);
			}
			deliver(map, st);
		}
		stop = true;
		r1.join();
		r2.join();
		CHECK(failures == 0);

		// the same rules from a reader in another process
		const int rc = in_child(
			[&]() -> int {
				for (auto &[k, v0] : first) {
					auto *v = get(map, k);
					if (!v || *v < v0)
						return 3;
				}
				return 0;
			},
			std::chrono::seconds(10));
		CHECK(rc == 0);
	}
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
	// past capacity: refused slots; then claiming
	for (uint32_t k = 100; k < 104; k++)
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
	uint32_t refused = 100;
	CHECK(get(map, refused) == nullptr);
	// a full directory: lookups of absent keys end
	for (uint32_t k = 300; k < 340; k++)
		dev.claim(k);
	deliver(map, dev.staging());
	uint32_t absent = 77777;
	CHECK(get(map, absent) == nullptr);
	CHECK(errno == ENOENT);
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
		const int64_t dead = dev.claim(50); // a claim that died
		dev.count++; // ... after reserving a row
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
	SECTION("the count when few slots are empty")
	{
		for (uint64_t empty : { uint64_t(0), uint64_t(1) }) {
			INFO("empty slots " << empty);
			segment seg("takeovercount");
			auto *map = seg.make(M);
			to_device_phase(map);
			device_model dev(M);
			// L = M - 2 live, the rest refused, `empty` left empty
			uint32_t k = 1;
			while (dev.count < M - 2)
				dev.insert(k++, 1);
			dev.count = M; // two rows held by dead claims
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
			CHECK(img.count == (empty == 0 ? M : M - 1));
		}
	}
}

// ------------------------------------------------------------ S2.10

TEST_CASE("The owner's path", "[gpu-devhash-cpu]")
{
	const uint64_t self = devhash_owner_record(0x1111, 100);
	const uint64_t other = devhash_owner_record(0x1111, 200);
	const uint64_t foreign = devhash_owner_record(0x2222, 200);
	using p = devhash_path;
	auto path = [&](uint64_t phase, uint64_t owner, bool alive,
			bool cached = false, bool dead_seal = false,
			uint64_t inflight = 0) {
		return devhash_choose_path(phase, dead_seal, inflight, owner,
					   self, cached, alive);
	};
	CHECK(path(0, 0, false) == p::first_copy);
	CHECK(path(1, 0, false) == p::recover_sealing);
	CHECK(path(2, 0, false) == p::takeover);
	CHECK(path(2, other, true) == p::not_owner); // alive (EPERM too)
	CHECK(path(2, other, false) == p::takeover); // dead or zombie
	CHECK(path(1, other, false) == p::recover_sealing);
	CHECK(path(0, foreign, false) == p::not_owner); // other namespace
	CHECK(path(2, foreign, true) == p::not_owner);
	CHECK(path(2, self, false, true) == p::have_copy);
	CHECK(path(2, self, false, false) == p::takeover); // pid reuse
	CHECK(path(0, 0, false, false, true, 1) == p::seal_dead);
	CHECK(path(0, 0, false, false, true, 0) == p::first_copy);

	// claim() asks about liveness only for a same-namespace owner
	segment seg("claim");
	auto *map = seg.make(8);
	CHECK(map->claim(other, false, [](uint32_t) { return true; }) ==
	      p::first_copy);
	CHECK(map->get_owner() == other);
	CHECK(map->claim(self, false, [](uint32_t) { return true; }) ==
	      p::not_owner);
	CHECK(map->claim(self, false, [](uint32_t) { return false; }) ==
	      p::first_copy);
	CHECK(map->get_owner() == self);
	map->release_claim(self);
	CHECK(map->get_owner() == 0);
}

// ------------------------------------------------------------ S2.11

TEST_CASE("Recovery from a dead sealer waits for a writer that read phase 0",
	  "[gpu-devhash-cpu]")
{
	segment seg("recover");
	auto *map = seg.make(256);
	REQUIRE(put(map, 5, 50) == 0);
	arm(devhash_hook_point::writer_after_phase_load);
	std::atomic<long> wrc{ 99 };
	std::thread writer([&] {
		park_this_thread = true;
		wrc = put(map, 6, 60, BPF_NOEXIST);
	});
	REQUIRE(wait_parked());
	map->force_phase_for_test(DEVHASH_PHASE_SEALING); // the sealer died
	devhash_image img;
	std::atomic<bool> done{ false }, ok{ false };
	std::thread t([&] {
		ok = map->recover_sealing(img, 3000000000ull);
		done = true;
	});
	const auto end = clk::now() + std::chrono::milliseconds(50);
	while (!done && clk::now() < end)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const bool done_before_release = done;
	release();
	writer.join();
	t.join();
	disarm();
	CHECK_FALSE(done_before_release);
	REQUIRE(ok);
	CHECK(wrc == 0);
	CHECK(image_find(img, 6, log_slots_for(256)) >= 0);
	CHECK(image_find(img, 5, log_slots_for(256)) >= 0);

	// a writer killed in flight: the recovery times out to phase 0
	segment seg2("recover2");
	auto *map2 = seg2.make(256);
	int pfd[2];
	REQUIRE(pipe(pfd) == 0);
	pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		static int wfd;
		wfd = pfd[1];
		set_devhash_hook_for_test([](devhash_hook_point p, uint64_t) {
			if (p != devhash_hook_point::writer_after_inflight_inc)
				return;
			char x = 'x';
			(void)!write(wfd, &x, 1);
			for (;;)
				pause();
		});
		put(map2, 9, 9);
		_exit(0);
	}
	char c;
	REQUIRE(read(pfd[0], &c, 1) == 1);
	kill(child, SIGKILL);
	waitpid(child, nullptr, 0);
	map2->force_phase_for_test(DEVHASH_PHASE_SEALING);
	devhash_image img2;
	CHECK_FALSE(map2->recover_sealing(img2, 200000000ull));
	CHECK(map2->get_phase() == DEVHASH_PHASE_CPU);
	CHECK(map2->is_seal_dead());
	CHECK(put(map2, 10, 10) == 0);
	close(pfd[0]);
	close(pfd[1]);
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
#endif
