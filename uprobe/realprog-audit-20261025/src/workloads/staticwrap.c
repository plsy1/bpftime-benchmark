// Static exec wrapper for memleak -c: a static binary ignores the inherited LD_PRELOAD, so the
// tool's syscall-server is not loaded here. Execs $SW_CMD with the words of $SW_ARGS, with
// LD_PRELOAD set to $SW_PRELOAD (empty for the kernel backend).
// Build: gcc -static -O2 -o staticwrap staticwrap.c
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    char *cmd = getenv("SW_CMD"), *args = getenv("SW_ARGS"), *pre = getenv("SW_PRELOAD");
    if (!cmd) return 127;
    char *argv[16] = {cmd};
    int n = 1;
    for (char *w = strtok(args ? strdup(args) : NULL, " "); w && n < 15; w = strtok(NULL, " ")) argv[n++] = w;
    argv[n] = NULL;
    setenv("LD_PRELOAD", pre ? pre : "", 1);
    execv(cmd, argv);
    return 127;
}
