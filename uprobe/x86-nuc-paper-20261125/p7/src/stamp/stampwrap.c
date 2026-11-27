// staticwrap.c with a time stamp: prints CLOCK_MONOTONIC (ns) to $STAMP_FILE just before execv.
// Static, so the tool's inherited LD_PRELOAD is ignored here. Build: gcc -static -O2.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
int main(void) {
    char *cmd = getenv("SW_CMD"), *args = getenv("SW_ARGS"), *pre = getenv("SW_PRELOAD");
    if (!cmd) return 127;
    char *argv[16] = {cmd};
    int n = 1;
    for (char *w = strtok(args ? strdup(args) : NULL, " "); w && n < 15; w = strtok(NULL, " ")) argv[n++] = w;
    argv[n] = NULL;
    setenv("LD_PRELOAD", pre ? pre : "", 1);
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    FILE *f = fopen(getenv("STAMP_FILE"), "a");
    if (f) { fprintf(f, "exec %lld\n", (long long)t.tv_sec * 1000000000LL + t.tv_nsec); fclose(f); }
    execv(cmd, argv);
    return 127;
}
