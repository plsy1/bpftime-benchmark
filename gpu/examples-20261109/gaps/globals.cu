// Device globals outside cudaMemcpyToSymbol, next to a probed kernel.
// vectorAdd keeps the benchmark's signature (_Z9vectorAddPKfS0_Pf) so the kt3 probes hook it;
// inc() is never probed. Correct output: C[1] = 3.00 and g/g/m = 5 5 5.
#include <cstdio>
#include <cuda_runtime.h>
__constant__ int d_N;
__device__ int g;
__managed__ int m;
__global__ void vectorAdd(const float *A, const float *B, float *C)
{
	int i = blockDim.x * blockIdx.x + threadIdx.x;
	if (i < d_N)
		C[i] = A[i] + B[i];
}
__global__ void inc()
{
	g++;
	m++;
}
#define CK(x) do { cudaError_t e_ = (x); if (e_ != cudaSuccess) printf("%s: %s\n", #x, cudaGetErrorString(e_)); } while (0)
int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	int n = 5;
	CK(cudaMemcpyToSymbol(d_N, &n, sizeof n));
	float hA[5], hB[5], hC[5];
	for (int i = 0; i < 5; i++) { hA[i] = i; hB[i] = 2 * i; hC[i] = -1; }
	float *A, *B, *C;
	CK(cudaMalloc(&A, sizeof hA)); CK(cudaMalloc(&B, sizeof hB)); CK(cudaMalloc(&C, sizeof hC));
	CK(cudaMemcpy(A, hA, sizeof hA, cudaMemcpyHostToDevice));
	CK(cudaMemcpy(B, hB, sizeof hB, cudaMemcpyHostToDevice));
	CK(cudaMemset(C, 0, sizeof hC));
	vectorAdd<<<1, 5>>>(A, B, C);
	CK(cudaDeviceSynchronize());
	CK(cudaMemcpy(hC, C, sizeof hC, cudaMemcpyDeviceToHost));
	printf("vectorAdd: C[0] = %.2f, C[1] = %.2f (expected 0.00, 3.00)\n", hC[0], hC[1]);
	int *pg = nullptr;
	CK(cudaGetSymbolAddress((void **)&pg, g));
	CK(cudaMemset(pg, 0, sizeof(int)));
	m = 0;
	CK(cudaDeviceSynchronize());
	for (int k = 0; k < 5; k++) {
		inc<<<1, 1>>>();
		CK(cudaDeviceSynchronize());
	}
	int gp = -1, gs = -1;
	CK(cudaMemcpy(&gp, pg, sizeof(int), cudaMemcpyDeviceToHost));
	CK(cudaMemcpyFromSymbol(&gs, g, sizeof(int)));
	printf("globals: g via pointer = %d, g via symbol = %d, managed m = %d (expected 5 5 5)\n", gp, gs, m);
	printf("last error: %s\n", cudaGetErrorString(cudaGetLastError()));
	return 0;
}
