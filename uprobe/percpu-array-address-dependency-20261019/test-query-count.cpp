#include "bpf_map/userspace/per_cpu_array_map.hpp"
#include "catch2/catch_test_macros.hpp"
#include <boost/interprocess/shared_memory_object.hpp>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <linux/bpf.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
int selected_cpu = 0;
int query_count = 0;
bool change_query_errno = false;
struct remove_test_shm {
    std::string name;
    explicit remove_test_shm(std::string value) : name(std::move(value))
    {
        boost::interprocess::shared_memory_object::remove(name.c_str());
    }
    ~remove_test_shm()
    {
        boost::interprocess::shared_memory_object::remove(name.c_str());
    }
};
}

// Test-only definition. Do not link platform_utils.cpp or production runtime
// into this standalone executable, and never preload it in a benchmark.
int my_sched_getcpu()
{
    ++query_count;
    if (change_query_errno)
        errno = EAGAIN;
    return selected_cpu;
}

TEST_CASE("Both per-CPU lookup paths query CPU once and preserve slot/errno semantics")
{
    const std::string name = "bpftime-query-count-test-" + std::to_string(getpid());
    remove_test_shm cleanup(name);
    boost::interprocess::managed_shared_memory memory(
        boost::interprocess::create_only, name.c_str(), 4 << 20);
    for (uint32_t width : {1U, 8U, 24U, 257U}) {
        CAPTURE(width);
        bpftime::per_cpu_array_map_impl map(memory, width, 4, 2);
        for (uint32_t key : {0U, 3U}) {
            std::vector<uint8_t> values(width * 2);
            for (int cpu=0; cpu<2; ++cpu)
                for (uint32_t byte=0; byte<width; ++byte)
                    values[cpu*width+byte] = static_cast<uint8_t>(key*17+cpu*29+byte);
            REQUIRE(map.elem_update_userspace(&key, values.data(), BPF_ANY)==0);
        }
        for (bool production : {true, false}) {
            auto lookup = [&](const void *key) {
                return production ? map.elem_lookup_base(key) : map.elem_lookup(key);
            };
            for (bool inject_errno : {false, true}) {
                change_query_errno = inject_errno;
                for (int cpu : {0, 1, 0, 1}) {
                    selected_cpu = cpu;
                    for (uint32_t key : {0U, 3U}) {
                        query_count=0;
                        errno=EDOM;
                        auto *value=static_cast<uint8_t *>(lookup(&key));
                        REQUIRE(query_count==1);
                        REQUIRE(value!=nullptr);
                        REQUIRE(errno==(inject_errno ? EAGAIN : EDOM));
                        for (uint32_t byte=0; byte<width; ++byte)
                            REQUIRE(value[byte]==static_cast<uint8_t>(key*17+cpu*29+byte));
                    }
                    for (uint32_t key : {4U, std::numeric_limits<uint32_t>::max()}) {
                        query_count=0;
                        errno=EDOM;
                        REQUIRE(lookup(&key)==nullptr);
                        REQUIRE(query_count==1);
                        REQUIRE(errno==ENOENT);
                    }
                    query_count=0;
                    errno=EDOM;
                    REQUIRE(lookup(nullptr)==nullptr);
                    REQUIRE(query_count==1);
                    REQUIRE(errno==ENOENT);
                }
            }
        }
    }
}
