// attrs.cu - dump device attributes relevant to bpftime GPU support
#include <cstdio>
#include <cuda.h>
#include <cuda_runtime.h>

#define A(x) {x, #x}
int main()
{
	int drv = 0, rt = 0, cudrv = 0;
	cudaDriverGetVersion(&drv);
	cudaRuntimeGetVersion(&rt);
	cuInit(0);
	cuDriverGetVersion(&cudrv);
	printf("cudaDriverGetVersion=%d cudaRuntimeGetVersion=%d cuDriverGetVersion=%d\n", drv, rt, cudrv);
	int ndev = 0;
	cudaError_t e = cudaGetDeviceCount(&ndev);
	printf("cudaGetDeviceCount=%d (%s)\n", ndev, cudaGetErrorString(e));
	if (ndev < 1)
		return 1;
	cudaDeviceProp p;
	e = cudaGetDeviceProperties(&p, 0);
	printf("cudaGetDeviceProperties: %s\n", cudaGetErrorString(e));
	printf("name=%s cc=%d.%d totalGlobalMem=%zu (%.2f GiB) sharedMemPerBlock=%zu sharedMemPerMultiprocessor=%zu "
	       "regsPerBlock=%d regsPerMultiprocessor=%d warpSize=%d maxThreadsPerBlock=%d "
	       "maxThreadsDim=[%d,%d,%d] maxGridSize=[%d,%d,%d] multiProcessorCount=%d l2CacheSize=%d "
	       "persistingL2CacheMaxSize=%d maxThreadsPerMultiProcessor=%d maxBlocksPerMultiProcessor=%d "
	       "integrated=%d canMapHostMemory=%d unifiedAddressing=%d managedMemory=%d concurrentManagedAccess=%d "
	       "pageableMemoryAccess=%d pageableMemoryAccessUsesHostPageTables=%d hostNativeAtomicSupported=%d "
	       "directManagedMemAccessFromHost=%d computePreemptionSupported=%d cooperativeLaunch=%d "
	       "concurrentKernels=%d asyncEngineCount=%d pciBusID=%d pciDeviceID=%d pciDomainID=%d\n",
	       p.name, p.major, p.minor, p.totalGlobalMem, p.totalGlobalMem / 1073741824.0, p.sharedMemPerBlock,
	       p.sharedMemPerMultiprocessor, p.regsPerBlock, p.regsPerMultiprocessor, p.warpSize, p.maxThreadsPerBlock,
	       p.maxThreadsDim[0], p.maxThreadsDim[1], p.maxThreadsDim[2], p.maxGridSize[0], p.maxGridSize[1],
	       p.maxGridSize[2], p.multiProcessorCount, p.l2CacheSize, p.persistingL2CacheMaxSize,
	       p.maxThreadsPerMultiProcessor, p.maxBlocksPerMultiProcessor, p.integrated, p.canMapHostMemory,
	       p.unifiedAddressing, p.managedMemory, p.concurrentManagedAccess, p.pageableMemoryAccess,
	       p.pageableMemoryAccessUsesHostPageTables, p.hostNativeAtomicSupported, p.directManagedMemAccessFromHost,
	       p.computePreemptionSupported, p.cooperativeLaunch, p.concurrentKernels, p.asyncEngineCount, p.pciBusID,
	       p.pciDeviceID, p.pciDomainID);

	struct {
		cudaDeviceAttr a;
		const char *n;
	} at[] = {
		// requested
		A(cudaDevAttrIntegrated),
		A(cudaDevAttrUnifiedAddressing),
		A(cudaDevAttrCanMapHostMemory),
		A(cudaDevAttrPageableMemoryAccess),
		A(cudaDevAttrPageableMemoryAccessUsesHostPageTables),
		A(cudaDevAttrConcurrentManagedAccess),
		A(cudaDevAttrHostRegisterSupported),
		A(cudaDevAttrCanUseHostPointerForRegisteredMem),
		A(cudaDevAttrManagedMemory),
		A(cudaDevAttrComputePreemptionSupported),
		A(cudaDevAttrCooperativeLaunch),
		A(cudaDevAttrL2CacheSize),
		A(cudaDevAttrMultiProcessorCount),
		A(cudaDevAttrMaxThreadsPerBlock),
		A(cudaDevAttrMaxThreadsPerMultiProcessor),
		// extras
		A(cudaDevAttrMaxBlocksPerMultiprocessor),
		A(cudaDevAttrHostNativeAtomicSupported),
		A(cudaDevAttrDirectManagedMemAccessFromHost),
		A(cudaDevAttrConcurrentKernels),
		A(cudaDevAttrAsyncEngineCount),
		A(cudaDevAttrKernelExecTimeout),
		A(cudaDevAttrComputeMode),
		A(cudaDevAttrHostRegisterReadOnlySupported),
		A(cudaDevAttrMemoryPoolsSupported),
		A(cudaDevAttrGPUDirectRDMASupported),
		A(cudaDevAttrCanFlushRemoteWrites),
		A(cudaDevAttrMaxSharedMemoryPerBlockOptin),
		A(cudaDevAttrMaxPersistingL2CacheSize),
		A(cudaDevAttrGlobalMemoryBusWidth),
		A(cudaDevAttrMemoryClockRate),
		A(cudaDevAttrClockRate),
		A(cudaDevAttrEccEnabled),
		A(cudaDevAttrWarpSize),
		A(cudaDevAttrComputeCapabilityMajor),
		A(cudaDevAttrComputeCapabilityMinor),
	};
	for (auto &x : at) {
		int v = -12345;
		cudaError_t r = cudaDeviceGetAttribute(&v, x.a, 0);
		printf("  %-52s = %d%s%s\n", x.n, v, r == cudaSuccess ? "" : "  ERR:", r == cudaSuccess ? "" : cudaGetErrorString(r));
	}
	// driver API cross-check of a few
	CUdevice d;
	cuDeviceGet(&d, 0);
	struct {
		CUdevice_attribute a;
		const char *n;
	} dat[] = {
		A(CU_DEVICE_ATTRIBUTE_INTEGRATED),
		A(CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY),
		A(CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING),
		A(CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS),
		A(CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS_USES_HOST_PAGE_TABLES),
		A(CU_DEVICE_ATTRIBUTE_HOST_REGISTER_SUPPORTED),
		A(CU_DEVICE_ATTRIBUTE_CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM),
		A(CU_DEVICE_ATTRIBUTE_HOST_NATIVE_ATOMIC_SUPPORTED),
		A(CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS),
		A(CU_DEVICE_ATTRIBUTE_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED),
		A(CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR_SUPPORTED),
		A(CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_FLUSH_WRITES_OPTIONS),
		A(CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_WRITES_ORDERING),
		A(CU_DEVICE_ATTRIBUTE_CAN_USE_64_BIT_STREAM_MEM_OPS),
		A(CU_DEVICE_ATTRIBUTE_CAN_USE_STREAM_WAIT_VALUE_NOR),
	};
	for (auto &x : dat) {
		int v = -12345;
		CUresult r = cuDeviceGetAttribute(&v, x.a, d);
		const char *es = nullptr;
		if (r != CUDA_SUCCESS)
			cuGetErrorString(r, &es);
		printf("  %-64s = %d%s%s\n", x.n, v, r == CUDA_SUCCESS ? "" : "  ERR:", r == CUDA_SUCCESS ? "" : es);
	}
	size_t fr = 0, tot = 0;
	cudaMemGetInfo(&fr, &tot);
	printf("cudaMemGetInfo free=%zu total=%zu\n", fr, tot);
	return 0;
}
