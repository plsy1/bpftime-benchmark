// Values of the shared hash map were placed right after the key in elements
// rounded to 4 bytes: with an 8-byte key and value, a quarter of the values
// crossed a 16-byte boundary, and a BPF atomic add on one (the JIT emits
// ldaddal) killed the traced process with SIGBUS on aarch64.
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/bpftime_hash_map.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

TEST_CASE("Hash map values are aligned for atomics",
	  "[bpftime_hash_map][alignment]")
{
	const std::string name =
		"bpftime_hash_align_test_" + std::to_string(getpid());
	constexpr size_t entries = 1024;
	for (size_t key_size : { 1, 3, 4, 8, 16, 64 }) {
		for (size_t value_size :
		     { 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24 }) {
			INFO("key " << key_size << " value " << value_size);
			shared_memory_object::remove(name.c_str());
			managed_shared_memory segment(create_only, name.c_str(),
						      8 << 20);
			bpftime_hash_map map(segment, entries, key_size,
					     value_size);
			const size_t align = value_size >= 8 ? 8 : 4;
			std::vector<uint8_t> key(key_size), value(value_size);
			size_t inserted = 0;
			for (size_t i = 0; i < entries; i++) {
				// Distinct keys for every key size (1 byte: 256).
				std::memset(key.data(), 0, key_size);
				std::memcpy(key.data(), &i,
					    std::min(key_size, sizeof(i)));
				if (key_size == 1 && i >= 256)
					break;
				REQUIRE(map.elem_update(key.data(), value.data()));
				inserted++;
				auto *p = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				REQUIRE(p != nullptr);
				REQUIRE(reinterpret_cast<uintptr_t>(p) % align ==
					0);
				if (value_size >= 8)
					__atomic_fetch_add(
						reinterpret_cast<uint64_t *>(p), 1,
						__ATOMIC_SEQ_CST);
				else if (value_size >= 4)
					__atomic_fetch_add(
						reinterpret_cast<uint32_t *>(p), 1,
						__ATOMIC_SEQ_CST);
				// The whole value, to its last byte.
				for (size_t b = 1; b < value_size; b++)
					p[b] = 0xff;
			}
			REQUIRE(map.get_elem_count() == inserted);
			// No value write reached a neighbour's key or value.
			for (size_t i = 0; i < inserted; i++) {
				std::memset(key.data(), 0, key_size);
				std::memcpy(key.data(), &i,
					    std::min(key_size, sizeof(i)));
				auto *p = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				REQUIRE(p != nullptr);
				CHECK(p[0] == (value_size >= 4 ? 1 : 0));
				for (size_t b = 1; b < value_size; b++)
					REQUIRE(p[b] == 0xff);
			}
			shared_memory_object::remove(name.c_str());
		}
	}
}
