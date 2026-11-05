// Definitions the header needs from platform_utils.cpp (not linking libruntime.a).
#include <platform_utils.hpp>
#include <sched.h>
#include <thread>
int my_sched_getcpu() { return sched_getcpu(); }
int bpftime_cpu_index_limit() { return (int)std::thread::hardware_concurrency(); }
