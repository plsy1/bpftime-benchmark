#include <cstdio>
__global__ void k(float *a, int n) { int i = blockIdx.x * blockDim.x + threadIdx.x; if (i < n) a[i] += 1.0f; }
int main() { float *a; int n = 1 << 20; cudaMalloc(&a, n * 4);
  for (int r = 0; r < 10; r++) k<<<(n + 255) / 256, 256>>>(a, n);
  cudaDeviceSynchronize(); cudaFree(a); printf("done\n"); return 0; }
