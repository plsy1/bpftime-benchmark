#include <cuda_runtime.h>
#include <stdio.h>
int main(){cudaDeviceProp p; cudaGetDeviceProperties(&p,0); int v=0; cudaDriverGetVersion(&v); int rv=0; cudaRuntimeGetVersion(&rv);
printf("name=%s cc=%d.%d SMs=%d warpSize=%d maxThreadsPerBlock=%d integrated=%d unifiedAddressing=%d concurrentManagedAccess=%d totalGlobalMem=%zuMB driver=%d runtime=%d\n",
p.name,p.major,p.minor,p.multiProcessorCount,p.warpSize,p.maxThreadsPerBlock,p.integrated,p.unifiedAddressing,p.concurrentManagedAccess,p.totalGlobalMem>>20,v,rv);}
