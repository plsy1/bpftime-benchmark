#include <cstdio>
#include <cuda_runtime.h>
__global__ void bump(int *p) { p[0] += 1; }
int main(){
  int *d; cudaMalloc(&d, 4); cudaMemset(d, 0, 4);
  cudaKernel_t k = 0; cudaGetKernel(&k, (const void*)bump);
  printf("host_stub=%p cudaGetKernel_handle=%p\n", (void*)bump, (void*)k); fflush(stdout);
  bump<<<1,1>>>(d); cudaError_t e = cudaDeviceSynchronize();
  printf("launch=%s\n", cudaGetErrorName(e)); return 0;
}
