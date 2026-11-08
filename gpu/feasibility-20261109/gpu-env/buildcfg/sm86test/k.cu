#include <cstdio>
__global__ void k(int *a){ a[threadIdx.x] = threadIdx.x + 1; }
int main(){ int *d; int h[64] = {0}; cudaMalloc(&d, sizeof(h)); cudaMemset(d, 0, sizeof(h)); k<<<1,64>>>(d);
  cudaError_t le = cudaGetLastError(); cudaError_t se = cudaDeviceSynchronize();
  cudaMemcpy(h, d, sizeof(h), cudaMemcpyDeviceToHost);
  printf("launch=%s sync=%s h[63]=%d\n", cudaGetErrorString(le), cudaGetErrorString(se), h[63]); return 0; }
