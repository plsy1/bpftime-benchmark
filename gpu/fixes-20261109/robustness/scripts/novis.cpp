#include <cuda.h>
#include <cstdio>
int main() { CUresult e = cuInit(0); int n = -1; CUresult c = cuDeviceGetCount(&n); CUdevice d; CUresult g = cuDeviceGet(&d, 0); printf("cuInit=%d count=%d(%d) cuDeviceGet=%d\n", (int)e, n, (int)c, (int)g); }
