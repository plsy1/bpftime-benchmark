// Prints CLOCK_MONOTONIC (ns) to $STAMP_FILE on entering main, calls malloc/free once, exits.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    FILE *f = fopen(getenv("STAMP_FILE"), "a");
    if (f) { fprintf(f, "main %lld\n", (long long)t.tv_sec * 1000000000LL + t.tv_nsec); fclose(f); }
    free(malloc(64));
    return 0;
}
