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
uint64_t selected_cpu;
unsigned cpu_calls;
bool advance_cpu;
uint64_t counted_cpu(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	++cpu_calls;
	return advance_cpu ? selected_cpu++ : selected_cpu;
}
uint64_t last_helper_map;
uint64_t counted_lookup(uint64_t map, uint64_t key, uint64_t, uint64_t, uint64_t)
{
	++helper_calls;
	last_helper_map = map;
	if (map != 7) {
		errno = ENOENT;
		return 0;
	}
	uint32_t index;
	std::memcpy(&index, reinterpret_cast<void *>(key), sizeof(index));
	if (index >= capacity) {
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

TEST_CASE("Per-CPU array JIT selects CPU per lookup with bounds and arbitrary widths",
          "[array-jit-poc][percpu-array-jit]")
{
	capacity = 4;
	advance_cpu = false;
	for (unsigned cpus : {1U,3U,12U}) {
		for (unsigned bytes : {1U,3U,8U,24U,257U}) {
			width = bytes;
			std::vector<uint8_t> backing(capacity * cpus * width);
			storage = backing.data();
			vm_ptr vm(ebpf_create("llvm"), ebpf_destroy);
			REQUIRE(ebpf_register(vm.get(),1,"bpftime_bound_percpu_array_lookup_v1",(void *)counted_lookup)==0);
			REQUIRE(ebpf_register(vm.get(),8,"bpf_get_smp_processor_id",(void *)counted_cpu)==0);
			ebpf_inst code[]={{0xbf,2,1,0,0},{0xb7,1,0,0,7},
				{0x85,0,0,0,1},{0x95,0,0,0,0}};
			char *error = nullptr;
			REQUIRE(ebpf_load(vm.get(),code,sizeof(code),&error)==0);
			REQUIRE(ebpf_register_array_map(vm.get(),7,(uint64_t)storage,4,width,width*cpus,capacity)==0);
			auto fn=ebpf_compile(vm.get(),&error);
			REQUIRE(fn!=nullptr);
			for (uint64_t cpu : {uint64_t(0),uint64_t(cpus-1),uint64_t(cpus),UINT64_MAX}) {
				selected_cpu=cpu;
				for (uint32_t key : {0U,3U,4U,UINT32_MAX}) {
					helper_calls=cpu_calls=0; errno=EDOM;
					uint8_t input[8]{};
					std::memcpy(input+1,&key,4);
					auto result=fn(input+1,4);
					REQUIRE(cpu_calls==(key<capacity ? 1U : 0U));
					if (key<capacity && cpu<cpus) {
						REQUIRE(result==(uint64_t)(storage+(key*cpus+cpu)*width));
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
			helper_calls=cpu_calls=0; errno=EDOM;
			REQUIRE(fn(nullptr,0)==0);
			REQUIRE(helper_calls==1);
			REQUIRE(last_helper_map==UINT64_MAX);
			REQUIRE(cpu_calls==0);
			REQUIRE(errno==ENOENT);
		}
	}
}

TEST_CASE("Per-CPU array JIT does not hoist CPU selection within an invocation",
          "[array-jit-poc][percpu-array-jit]")
{
	std::vector<uint64_t> backing{11,22,33};
	storage=(uint8_t *)backing.data(); width=8; capacity=1;
	vm_ptr vm(ebpf_create("llvm"),ebpf_destroy);
	REQUIRE(ebpf_register(vm.get(),1,"bpftime_bound_percpu_array_lookup_v1",(void *)counted_lookup)==0);
	REQUIRE(ebpf_register(vm.get(),8,"bpf_get_smp_processor_id",(void *)counted_cpu)==0);
	// r6 keeps key, r7 keeps first value; the two calls must select different CPUs.
	ebpf_inst code[]={{0xbf,6,1,0,0},{0xbf,2,6,0,0},{0xb7,1,0,0,7},
		{0x85,0,0,0,1},{0x79,7,0,0,0},{0xbf,2,6,0,0},{0xb7,1,0,0,7},
		{0x85,0,0,0,1},{0x79,0,0,0,0},{0x0f,0,7,0,0},{0x95,0,0,0,0}};
	char *error=nullptr;
	REQUIRE(ebpf_load(vm.get(),code,sizeof(code),&error)==0);
	REQUIRE(ebpf_register_array_map(vm.get(),7,(uint64_t)storage,4,8,24,1)==0);
	auto fn=ebpf_compile(vm.get(),&error);
	REQUIRE(fn!=nullptr);
	uint32_t key=0; selected_cpu=0; advance_cpu=true; cpu_calls=helper_calls=0;
	auto result=fn(&key,4);
	advance_cpu=false;
	REQUIRE(result==33);
	REQUIRE(cpu_calls==2);
	REQUIRE(helper_calls==0);
}

TEST_CASE("Per-CPU array JIT requires the marked CPU helper contract",
          "[array-jit-poc][percpu-array-jit]")
{
	std::vector<uint64_t> backing(12,42);
	storage=(uint8_t *)backing.data(); width=8; capacity=4;
	for (bool missing : {true,false}) {
		vm_ptr vm(ebpf_create("llvm"),ebpf_destroy);
		REQUIRE(ebpf_register(vm.get(),1,"bpftime_bound_percpu_array_lookup_v1",(void *)counted_lookup)==0);
		if (!missing)
			REQUIRE(ebpf_register(vm.get(),8,"custom_cpu",(void *)counted_cpu)==0);
		ebpf_inst code[]={{0xbf,2,1,0,0},{0xb7,1,0,0,7},{0x85,0,0,0,1},{0x95,0,0,0,0}};
		char *error=nullptr;
		REQUIRE(ebpf_load(vm.get(),code,sizeof(code),&error)==0);
		REQUIRE(ebpf_register_array_map(vm.get(),7,(uint64_t)storage,4,8,24,4)==0);
		auto fn=ebpf_compile(vm.get(),&error);
		REQUIRE(fn!=nullptr);
		uint32_t key=1; helper_calls=cpu_calls=0;
		REQUIRE(fn(&key,4)==(uint64_t)&backing[1]);
		REQUIRE(helper_calls==1);
		REQUIRE(cpu_calls==0);
	}
}
