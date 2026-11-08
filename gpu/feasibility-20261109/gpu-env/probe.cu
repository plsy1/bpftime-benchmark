// Minimal read-only probe: device attributes, CUDA-13 launch handle vs host stub,
// CUDA IPC handle creation, and cudaHostRegister on a MAP_SHARED /dev/shm mapping.
#include <cuda.h>
#include <cuda_runtime.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
__global__ void bump(volatile int *p) { p[0] = p[0] + 1; }
static const char *nm(CUresult r){ const char *s=0; cuGetErrorName(r,&s); return s?s:"?"; }
static void who(const char *tag, const void *a){ Dl_info i; memset(&i,0,sizeof i);
  int ok = dladdr(a,&i); printf("%s %p dladdr=%d obj=%s sym=%s\n", tag, a, ok,
  ok&&i.dli_fname?i.dli_fname:"-", ok&&i.dli_sname?i.dli_sname:"(null)"); }
int main(){
  cudaFree(0);
  CUdevice d; cuCtxGetDevice(&d);
  struct { CUdevice_attribute a; const char *n; } at[] = {
    {CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR,"cc_major"},
    {CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR,"cc_minor"},
    {CU_DEVICE_ATTRIBUTE_INTEGRATED,"integrated"},
    {CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY,"canMapHostMemory"},
    {CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING,"unifiedAddressing"},
    {CU_DEVICE_ATTRIBUTE_HOST_REGISTER_SUPPORTED,"hostRegisterSupported"},
    {CU_DEVICE_ATTRIBUTE_CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM,"canUseHostPtrForRegisteredMem"},
    {CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS,"pageableMemoryAccess"},
    {CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS_USES_HOST_PAGE_TABLES,"pageableUsesHostPageTables"},
    {CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS,"concurrentManagedAccess"},
    {CU_DEVICE_ATTRIBUTE_MANAGED_MEMORY,"managedMemory"},
    {CU_DEVICE_ATTRIBUTE_IPC_EVENT_SUPPORTED,"ipcEventSupported"},
    {CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR_SUPPORTED,"posixFdHandleSupported"},
    {CU_DEVICE_ATTRIBUTE_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED,"vmmSupported"},
  };
  for (auto &x: at){ int v=-1; CUresult r=cuDeviceGetAttribute(&v,x.a,d); printf("attr %s=%d (%s)\n",x.n,v,nm(r)); }
  int drv=0, rt=0; cudaDriverGetVersion(&drv); cudaRuntimeGetVersion(&rt); printf("driverVersion=%d runtimeVersion=%d\n",drv,rt);
  // CUDA 13 launch ABI: handle from cudaGetKernel vs host stub address
  cudaKernel_t k = 0; cudaError_t ge = cudaGetKernel(&k, (const void*)bump);
  printf("cudaGetKernel=%s\n", cudaGetErrorName(ge));
  who("host_stub", (const void*)bump); who("kernel_handle", (const void*)k);
  // CUDA IPC (memory) handle creation
  CUdeviceptr dp=0; CUresult r=cuMemAlloc(&dp, 1<<20); printf("cuMemAlloc=%s\n", nm(r));
  CUipcMemHandle h; r=cuIpcGetMemHandle(&h, dp); printf("cuIpcGetMemHandle=%s\n", nm(r));
  if (dp) cuMemFree(dp);
  // cudaHostRegister on MAP_SHARED /dev/shm mapping (like a boost managed_shared_memory segment)
  char name[64]; snprintf(name,sizeof name,"/gpuenv_probe_%d",(int)getpid());
  int fd=shm_open(name,O_CREAT|O_RDWR,0600); size_t sz=2u<<20; if(fd<0||ftruncate(fd,sz)){perror("shm");return 1;}
  int *hp=(int*)mmap(0,sz,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0); memset(hp,0,sz); hp[0]=41;
  cudaError_t e=cudaHostRegister(hp,sz,cudaHostRegisterMapped); printf("cudaHostRegister(MAP_SHARED shm)=%s\n",cudaGetErrorName(e));
  if(e==cudaSuccess){ void *dv=0; e=cudaHostGetDevicePointer(&dv,hp,0); printf("cudaHostGetDevicePointer=%s host=%p dev=%p same=%d\n",cudaGetErrorName(e),(void*)hp,dv,dv==(void*)hp);
    bump<<<1,1>>>((volatile int*)dv); e=cudaDeviceSynchronize(); printf("kernel on registered shm=%s value=%d (expect 42)\n",cudaGetErrorName(e),hp[0]);
    cudaHostUnregister(hp); }
  // malloc'd (pageable) memory passed directly to the GPU
  int *pm=(int*)malloc(4096); pm[0]=7; bump<<<1,1>>>((volatile int*)pm); e=cudaDeviceSynchronize();
  printf("kernel on pageable malloc ptr=%s value=%d\n", cudaGetErrorName(e), pm[0]);
  munmap(hp,sz); close(fd); shm_unlink(name);
  return 0;
}
