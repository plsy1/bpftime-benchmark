#include <llvmbpf.hpp>
#include <bpftime_array_cpu_query.hpp>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>

extern "C" bool bpftime_resolve_array_cpu_query_v1(void *, bpftime_array_cpu_query_hooks *) __attribute__((weak));
static unsigned cpu_calls, lookup_calls;
static uint64_t selected_cpu=1;
static uint64_t cpu_query(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t)
{
    ++cpu_calls;
    return selected_cpu;
}
static uint64_t lookup(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t)
{
    ++lookup_calls;
    errno=ENOENT;
    return 0;
}
int main()
{
    assert(!bpftime_resolve_array_cpu_query_v1);
    std::array<uint64_t,12> values{};
    bpftime::llvmbpf_vm vm;
    assert(vm.register_external_function(1,"bpftime_bound_percpu_array_lookup_v1",(void *)lookup)==0);
    assert(vm.register_external_function(8,"bpf_get_smp_processor_id",(void *)cpu_query)==0);
    struct instruction {uint8_t op,regs; int16_t off; int32_t imm;};
    static_assert(sizeof(instruction)==8);
    instruction code[]={{0xbf,0x12,0,0},{0xb7,0x01,0,7},{0x85,0,0,1},{0x95,0,0,0}};
    assert(vm.load_code(code,sizeof(code))==0);
    assert(vm.register_array_map({7,(uint64_t)values.data(),4,8,24,4})==0);
    auto compiled=vm.compile();
    assert(compiled);
    for(uint32_t key : {0U,1U,3U,4U,UINT32_MAX}) {
        for(uint64_t cpu : {uint64_t(0),uint64_t(2),uint64_t(3),UINT64_MAX}) {
            selected_cpu=cpu; cpu_calls=lookup_calls=0; errno=EDOM;
            uint64_t result=(*compiled)(&key,4);
            assert(cpu_calls==(key<4 ? 1U:0U));
            if(key<4 && cpu<3) {
                assert(result==(uint64_t)&values[key*3+cpu]);
                assert(lookup_calls==0 && errno==EDOM);
            } else assert(result==0 && lookup_calls==1 && errno==ENOENT);
        }
    }
    cpu_calls=lookup_calls=0; errno=EDOM;
    assert((*compiled)(nullptr,0)==0);
    assert(cpu_calls==0 && lookup_calls==1 && errno==ENOENT);
    puts("Standalone VM without runtime provider: query, bounds and errno checks passed.");
}
