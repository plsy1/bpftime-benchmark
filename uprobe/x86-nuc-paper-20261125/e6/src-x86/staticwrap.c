// Static exec wrapper for memleak -c: a static binary ignores the inherited LD_PRELOAD, so the
// tool's syscall-server is not loaded here. Execs $SW_CMD with the words of $SW_ARGS, with
// LD_PRELOAD set to $SW_PRELOAD (empty for the kernel backend).
// x86 E6 change (2026-11-26): if $SW_CPUS is set (comma-separated CPU list), set this process's CPU
// affinity to it before exec, so the workload runs on the measured CPU and not on the tool's CPU
// (memleak -c forks the command from the tool, so it would inherit the tool's affinity).
// Build: gcc -static -O2 -o staticwrap staticwrap.c
#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    char *cmd = getenv("SW_CMD"), *args = getenv("SW_ARGS"), *pre = getenv("SW_PRELOAD"), *cpus = getenv("SW_CPUS");
    if (!cmd) return 127;
    if (cpus && *cpus) {
        cpu_set_t set; CPU_ZERO(&set);
        for (char *c = strtok(strdup(cpus), ","); c; c = strtok(NULL, ",")) CPU_SET(atoi(c), &set);
        if (sched_setaffinity(0, sizeof set, &set)) return 126;
    }
    char *argv[16] = {cmd};
    int n = 1;
    for (char *w = strtok(args ? strdup(args) : NULL, " "); w && n < 15; w = strtok(NULL, " ")) argv[n++] = w;
    argv[n] = NULL;
    setenv("LD_PRELOAD", pre ? pre : "", 1);
    execv(cmd, argv);
    return 127;
}
