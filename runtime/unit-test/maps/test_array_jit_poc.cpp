#include <catch2/catch_test_macros.hpp>
#include <ebpf-vm.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace {
uint8_t *storage;
uint32_t width;
uint32_t capacity;
unsigned helper_calls;
uint64_t last_helper_map;
uint64_t counted_lookup(uint64_t map, uint64_t key, uint64_t, uint64_t, uint64_t)
{
	++helper_calls;
	last_helper_map = map;
	uint32_t index;
	std::memcpy(&index, reinterpret_cast<void *>(key), sizeof(index));
	if (map != 7 || index >= capacity) {
		errno = ENOENT;
		return 0;
	}
	return reinterpret_cast<uint64_t>(storage + index * width);
}
using vm_ptr = std::unique_ptr<ebpf_vm, decltype(&ebpf_destroy)>;
vm_ptr make_vm(bool dynamic_handle, uint32_t handle = 7)
{
	vm_ptr vm(ebpf_create("llvm"), ebpf_destroy);
	REQUIRE(vm != nullptr);
	REQUIRE(ebpf_register(vm.get(), 1, "bpf_map_lookup_elem", (void *)counted_lookup) == 0);
	ebpf_inst code[] = {
		{0xbf, 2, 1, 0, 0}, // key = context address
		{0xb7, 1, 0, 0, static_cast<int32_t>(handle)},
		{0x85, 0, 0, 0, 1},
		{0x95, 0, 0, 0, 0},
	};
	if (dynamic_handle)
		code[1] = {0x79, 1, 1, 8, 0}; // runtime map handle in context
	char *error = nullptr;
	REQUIRE(ebpf_load(vm.get(), code, sizeof(code), &error) == 0);
	return vm;
}
}

TEST_CASE("Array JIT bridge preserves success, miss errno and live storage",
          "[array-jit-poc]")
{
	capacity = 4;
	for (auto bytes : {1U,3U,8U,24U,257U}) {
		width = bytes;
		std::vector<uint8_t> backing(width * capacity);
		storage = backing.data();
		auto vm = make_vm(false);
		REQUIRE(ebpf_register_array_map(vm.get(), 7, (uint64_t)storage,
			4, width, width, capacity) == 0);
		char *error = nullptr;
		auto fn = ebpf_compile(vm.get(), &error);
		REQUIRE(fn != nullptr);
		REQUIRE(ebpf_register_array_map(vm.get(), 8, (uint64_t)storage,
			4, width, width, capacity) == -EBUSY);
		for (uint32_t key : {0U,1U,3U,4U,UINT32_MAX}) {
			helper_calls = 0;
			errno = EDOM;
			uint8_t key_bytes[8] = {};
			std::memcpy(key_bytes + 1, &key, sizeof(key));
			auto actual = fn(key_bytes + 1, sizeof(key));
			if (key < capacity) {
				REQUIRE(actual == (uint64_t)(storage + key * width));
				REQUIRE(helper_calls == 0);
				REQUIRE(errno == EDOM);
				storage[key * width] = uint8_t(key + 91);
				REQUIRE(*(uint8_t *)actual == uint8_t(key + 91));
			} else {
				REQUIRE(actual == 0);
				REQUIRE(helper_calls == 1);
				REQUIRE(last_helper_map == 7);
				REQUIRE(errno == ENOENT);
			}
		}
	}
}

TEST_CASE("Array JIT canonicalizes only the pinned standard helper's miss",
          "[array-jit-poc]")
{
	std::vector<uint64_t> backing(4, 9);
	storage = (uint8_t *)backing.data(); width = 8; capacity = 4;
	vm_ptr vm(ebpf_create("llvm"), ebpf_destroy);
	REQUIRE(vm != nullptr);
	REQUIRE(ebpf_register(vm.get(),1,"bpftime_bound_array_lookup_v1",(void *)counted_lookup)==0);
	ebpf_inst code[]={{0xbf,2,1,0,0},{0xb7,1,0,0,7},
		{0x85,0,0,0,1},{0x95,0,0,0,0}};
	char *error = nullptr;
	REQUIRE(ebpf_load(vm.get(),code,sizeof(code),&error)==0);
	REQUIRE(ebpf_register_array_map(vm.get(),7,(uint64_t)storage,4,8,8,4)==0);
	auto fn=ebpf_compile(vm.get(),&error);
	REQUIRE(fn!=nullptr);
	for (uint32_t key : {0U,3U,4U,UINT32_MAX}) {
		helper_calls=0; errno=EDOM;
		auto result=fn(&key,sizeof(key));
		if (key<4) {
			REQUIRE(result==(uint64_t)&backing[key]);
			REQUIRE(helper_calls==0);
			REQUIRE(errno==EDOM);
		} else {
			REQUIRE(result==0);
			REQUIRE(helper_calls==1);
			REQUIRE(last_helper_map==UINT64_MAX);
			REQUIRE(errno==ENOENT);
		}
	}
}

TEST_CASE("Array JIT leaves dynamic and unregistered handles on helper path",
          "[array-jit-poc]")
{
	std::vector<uint64_t> backing(4, 99);
	storage = (uint8_t *)backing.data(); width = 8; capacity = 4;
	for (bool dynamic : {false,true}) {
		auto vm = make_vm(dynamic, 8);
		REQUIRE(ebpf_register_array_map(vm.get(), 7, (uint64_t)storage,4,8,8,4)==0);
		char *error = nullptr;
		auto fn = ebpf_compile(vm.get(), &error);
		REQUIRE(fn != nullptr);
		struct {uint32_t key; uint32_t pad; uint64_t handle;} input{2,0,7};
		helper_calls = 0;
		auto actual = fn(&input,sizeof(input));
		REQUIRE(helper_calls == 1);
		REQUIRE(actual == (dynamic ? (uint64_t)&backing[2] : 0));
	}
	vm_ptr ubpf(ebpf_create("ubpf"), ebpf_destroy);
	REQUIRE(ebpf_register_array_map(ubpf.get(),7,(uint64_t)storage,4,8,8,4)==-ENOTSUP);
	auto llvm = make_vm(false);
	REQUIRE(ebpf_register_array_map(llvm.get(),7,0,4,8,8,4)==-EINVAL);
	REQUIRE(ebpf_register_array_map(llvm.get(),7,(uint64_t)storage,8,8,8,4)==-EINVAL);
	REQUIRE(ebpf_register_array_map(llvm.get(),7,(uint64_t)storage,4,8,8,0)==-EINVAL);
}
