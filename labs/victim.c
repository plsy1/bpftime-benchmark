#include <stdio.h>
#include <unistd.h>
__attribute__((noinline)) int target_func(int x) { asm volatile(""); return x + 1; }
int main() { int s = 0; for (;;) { s = target_func(s); usleep(100000); } }
