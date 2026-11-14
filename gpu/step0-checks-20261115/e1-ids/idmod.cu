// idmod.cu: the same recording kernel as ids.cu's k_plain, built into separate modules
// (build/idmod.ptx, loaded with cuModuleLoadData = driver JIT; build/idmod.cubin, loaded with
// cuLibraryLoadData). Case d of ids.cu launches it with cuLaunchKernel.
#include "rec.cuh"

extern "C" __global__ void idmod_plain(IdRec *rec, uint32_t tag, uint32_t kind)
{
	record_plain(rec, tag, kind);
}
