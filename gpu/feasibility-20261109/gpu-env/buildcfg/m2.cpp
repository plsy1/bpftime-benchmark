#include <cuda.h>
#include <cuda_runtime_api.h>
#include <nvrtc.h>
#include <nvPTXCompiler.h>
int main(){unsigned a,b; nvPTXCompilerGetVersion(&a,&b); int v; cudaRuntimeGetVersion(&v); int x,y; nvrtcVersion(&x,&y); return (int)cuInit(0)*0 + (int)(a*0);}
