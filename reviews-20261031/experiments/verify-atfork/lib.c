#include <pthread.h>
#include <unistd.h>
static int cached = -1;
static void child(void) { cached = -1; }
__attribute__((constructor)) static void init(void) { pthread_atfork(0, 0, child); }
int lib_pid(void) { if (cached == -1) cached = getpid(); return cached; }
