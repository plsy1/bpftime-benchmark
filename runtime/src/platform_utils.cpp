#include "platform_utils.hpp"
#include "spdlog/spdlog.h"

#if __linux__
#include <sched.h>

int my_sched_getcpu() {
    return ::sched_getcpu();
}

#elif __APPLE__
#include <sys/sysctl.h>
#include <pthread.h>

int my_sched_getcpu() {
    int cpu = -1;
    size_t len = sizeof(cpu);

    if (sysctlbyname("hw.cpulocation", &cpu, &len, NULL, 0) == -1) {
        SPDLOG_ERROR("Couldn't get cpu location for the system");
        return -1;  
    }
    return cpu;
}

int sched_getaffinity([[maybe_unused]] pid_t pid, [[maybe_unused]]size_t cpusetsize, cpu_set_t *mask) {
    CPU_ZERO(mask);
    CPU_SET(my_sched_getcpu(), mask);
    return 0;
}

int sched_setaffinity([[maybe_unused]]pid_t pid, [[maybe_unused]]size_t cpusetsize, [[maybe_unused]]const cpu_set_t *mask) {
    return 0;
}

#endif

#include <unistd.h>
#if __linux__
#include <cstdio>
#endif

bool bpftime_parse_cpu_list(const char *text, std::vector<int> &ids)
{
    ids.clear();
    const char *p = text;
    while (*p != 0 && *p != '\n') {
        char *end;
        long first = strtol(p, &end, 10), last = first;
        if (end == p || first < 0)
            return false;
        p = end;
        if (*p == '-') {
            last = strtol(p + 1, &end, 10);
            if (end == p + 1 || last < first)
                return false;
            p = end;
        }
        if (!ids.empty() && first <= ids.back())
            return false;
        for (long cpu = first; cpu <= last; cpu++)
            ids.push_back((int)cpu);
        if (*p == ',')
            p++;
        else if (*p != 0 && *p != '\n')
            return false;
    }
    return !ids.empty();
}

namespace {
std::vector<int> &possible_cpu_ids()
{
    static std::vector<int> ids = [] {
        std::vector<int> result;
#if __linux__
        if (FILE *f = fopen("/sys/devices/system/cpu/possible", "re")) {
            char line[4096];
            bool ok = fgets(line, sizeof(line), f) != nullptr &&
                      bpftime_parse_cpu_list(line, result);
            fclose(f);
            if (ok)
                return result;
        }
#endif
        const long conf = sysconf(_SC_NPROCESSORS_CONF);
        result.clear();
        for (long cpu = 0; cpu < (conf > 0 ? conf : 1); cpu++)
            result.push_back((int)cpu);
        return result;
    }();
    return ids;
}
} // namespace

const std::vector<int> &bpftime_possible_cpu_ids()
{
    return possible_cpu_ids();
}

int bpftime_num_possible_cpus()
{
    return (int)possible_cpu_ids().size();
}

int bpftime_cpu_index_limit()
{
    return possible_cpu_ids().back() + 1;
}

void bpftime_set_possible_cpus_for_test(const std::vector<int> &ids)
{
    possible_cpu_ids() = ids;
}
