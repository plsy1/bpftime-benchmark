#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wcast-qual"
#define __NV_CUBIN_HANDLE_STORAGE__ static
#if !defined(__CUDA_INCLUDE_COMPILER_INTERNAL_HEADERS__)
#define __CUDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#endif
#include "crt/host_runtime.h"
#include "e6_gpubw.fatbin.c"
extern __attribute__((visibility("hidden"))) void __device_stub__Z7e6_copyPK6float4PS_m(const struct float4 *__restrict__, struct float4 *__restrict__, size_t);
static void __nv_cudaEntityRegisterCallback(void **);
static void __sti____cudaRegisterAll(void) __attribute__((__constructor__));
__attribute__((visibility("hidden"))) void __device_stub__Z7e6_copyPK6float4PS_m(const struct float4 *__restrict__ __par0, struct float4 *__restrict__ __par1, size_t __par2){ const struct float4 *__T4;
 struct float4 *__T5;
__cudaLaunchPrologue(3);__T4 = __par0;__cudaSetupArgSimple(__T4, 0UL);__T5 = __par1;__cudaSetupArgSimple(__T5, 8UL);__cudaSetupArgSimple(__par2, 16UL);__cudaLaunch(((char *)((void ( *)(const struct float4 *__restrict__, struct float4 *__restrict__, size_t))e6_copy)));}
# 15 "e6_gpubw.cu"
void e6_copy( const struct ::float4 *__restrict__ __cuda_0,struct ::float4 *__restrict__ __cuda_1,::size_t __cuda_2)
# 16 "e6_gpubw.cu"
{__device_stub__Z7e6_copyPK6float4PS_m( __cuda_0,__cuda_1,__cuda_2);



}
# 1 ".keep/e6_gpubw.cudafe1.stub.c"
static void __nv_cudaEntityRegisterCallback( void **__T11) {  __nv_dummy_param_ref(__T11); __nv_save_fatbinhandle_for_managed_rt(__T11); __cudaRegisterEntry(__T11, ((void ( *)(const struct float4 *__restrict__, struct float4 *__restrict__, size_t))e6_copy), _Z7e6_copyPK6float4PS_m, (-1)); }
static void __sti____cudaRegisterAll(void) {  __cudaRegisterBinary(__nv_cudaEntityRegisterCallback);  }

#pragma GCC diagnostic pop
