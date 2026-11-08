#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
__global__ void k(int *p) { *p = 1; }
int main() {
  int *d; cudaError_t e = cudaMalloc(&d, 4); k<<<1,1>>>(d); cudaError_t s = cudaDeviceSynchronize();
  printf("cudaMalloc=%s sync=%s\n", cudaGetErrorString(e), cudaGetErrorString(s));
  FILE *f = fopen("/proc/self/maps", "r"); char line[1024], last[1024] = "";
  while (fgets(line, sizeof line, f)) {
    char *path = strchr(line, '/'); if (!path) continue;
    if (!strstr(path, "cuda") && !strstr(path, "nvidia") && !strstr(path, "libnv") && !strstr(path, "nvgpu") && !strstr(path, "/dev/nv")) continue;
    if (!strcmp(path, last)) continue; strcpy(last, path); printf("  map: %s", path);
  }
  return 0;
}
