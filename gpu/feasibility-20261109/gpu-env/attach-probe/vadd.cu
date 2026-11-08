// Minimal CUDA app in the shape of bpftime's example/gpu/*/vec_add.cu:
// one __global__ kernel, one __device__ global, <<<>>> launch,
// cudaMemcpyToSymbol / cudaMemcpyFromSymbol.
#include <cstdio>
#include <cuda_runtime.h>

__device__ int d_counter;
__constant__ int d_N;

__global__ void vectorAdd(const float *A, const float *B, float *C)
{
	int i = blockDim.x * blockIdx.x + threadIdx.x;
	if (i < d_N) {
		C[i] = A[i] + B[i];
		atomicAdd(&d_counter, 1);
	}
}

int main()
{
	const int n = 1024;
	float *a, *b, *c;
	if (cudaMalloc(&a, n * sizeof(float)) != cudaSuccess ||
	    cudaMalloc(&b, n * sizeof(float)) != cudaSuccess ||
	    cudaMalloc(&c, n * sizeof(float)) != cudaSuccess) {
		printf("cudaMalloc failed\n");
		return 1;
	}
	cudaMemset(a, 0, n * sizeof(float));
	cudaMemset(b, 0, n * sizeof(float));
	int zero = 0;
	cudaMemcpyToSymbol(d_N, &n, sizeof(n));
	cudaMemcpyToSymbol(d_counter, &zero, sizeof(zero));
	vectorAdd<<<(n + 255) / 256, 256>>>(a, b, c);
	cudaError_t e = cudaDeviceSynchronize();
	int cnt = -1;
	cudaMemcpyFromSymbol(&cnt, d_counter, sizeof(cnt));
	printf("sync=%s counter=%d\n", cudaGetErrorString(e), cnt);
	return 0;
}
