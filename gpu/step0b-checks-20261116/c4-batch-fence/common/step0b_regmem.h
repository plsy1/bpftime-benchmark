// step0b_regmem.h: copy of step0/common/step0_regmem.h for step 0b (names step0b_*).
// Registered host memory made the way bpftime makes it
// (runtime/src/bpftime_shm_internal.cpp:1414-1469 in dev 52126f33).
//
//   shm_open(step0b_...) -> ftruncate -> mmap MAP_SHARED -> read-prefault every
//   page (as bpftime's prefault_range) -> cuMemHostRegister(DEVICEMAP |
//   PORTABLE) -> cuMemHostGetDevicePointer.
//
// Differences from bpftime, on purpose:
//  - The segment is one process's, not a Boost.Interprocess segment that the
//    syscall server created; the pages are the same kind (POSIX shm, tmpfs).
//  - An optional second MAP_SHARED mapping of the same object, never
//    registered, stands for the loader process's view (the CPU reader).
//  - The name is unlinked as soon as both mappings exist, so a killed run
//    leaves nothing in /dev/shm.
#pragma once

#include <cuda.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define STEP0B_CU(call)                                                         \
	do {                                                                   \
		CUresult step0b_rc_ = (call);                                   \
		if (step0b_rc_ != CUDA_SUCCESS) {                               \
			const char *step0b_s_ = nullptr;                        \
			cuGetErrorString(step0b_rc_, &step0b_s_);                \
			fprintf(stderr, "%s:%d: %s -> %d (%s)\n", __FILE__,   \
				__LINE__, #call, (int)step0b_rc_,               \
				step0b_s_ ? step0b_s_ : "?");                    \
			std::exit(10);                                         \
		}                                                              \
	} while (0)

struct step0b_regmem {
	std::string name;
	size_t size = 0;
	void *host = nullptr; // registered mapping (the agent's view)
	void *host2 = nullptr; // second, unregistered mapping (the loader's view)
	CUdeviceptr dev = 0; // cuMemHostGetDevicePointer
	bool registered = false;
};

// The device's primary context made current on this thread, as the agent
// does before it registers the segment.
static inline CUcontext step0b_primary_ctx(int ordinal = 0)
{
	STEP0B_CU(cuInit(0));
	CUdevice dev = 0;
	STEP0B_CU(cuDeviceGet(&dev, ordinal));
	CUcontext ctx = nullptr;
	STEP0B_CU(cuDevicePrimaryCtxRetain(&ctx, dev));
	STEP0B_CU(cuCtxSetCurrent(ctx));
	return ctx;
}

// name must start with "step0b_". Returns false (with a message) on failure;
// nothing is left in /dev/shm in either case.
static inline bool step0b_regmem_create(step0b_regmem &m, const std::string &name,
				       size_t size, bool second_mapping)
{
	if (name.rfind("step0b_", 0) != 0) {
		fprintf(stderr, "shm name must start with step0b_: %s\n",
			name.c_str());
		return false;
	}
	m.name = "/" + name;
	m.size = size;
	int fd = shm_open(m.name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0) {
		fprintf(stderr, "shm_open(%s): %s\n", m.name.c_str(),
			strerror(errno));
		return false;
	}
	bool ok = ftruncate(fd, (off_t)size) == 0;
	if (ok) {
		m.host = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			      fd, 0);
		ok = m.host != MAP_FAILED;
		if (!ok)
			m.host = nullptr;
	}
	if (ok && second_mapping) {
		m.host2 = mmap(nullptr, size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, fd, 0);
		ok = m.host2 != MAP_FAILED;
		if (!ok)
			m.host2 = nullptr;
	}
	if (!ok)
		fprintf(stderr, "ftruncate/mmap(%s, %zu): %s\n", m.name.c_str(),
			size, strerror(errno));
	// The mappings keep the object alive; the name is not needed any more.
	shm_unlink(m.name.c_str());
	close(fd);
	if (!ok)
		return false;
	// bpftime's prefault_range: one read per 4 KiB page.
	{
		volatile unsigned char sink = 0;
		auto *p = static_cast<volatile unsigned char *>(m.host);
		for (size_t i = 0; i < size; i += 4096)
			sink ^= p[i];
		(void)sink;
	}
	CUresult rc = cuMemHostRegister(m.host, size,
					CU_MEMHOSTREGISTER_DEVICEMAP |
						CU_MEMHOSTREGISTER_PORTABLE);
	if (rc != CUDA_SUCCESS) {
		fprintf(stderr, "cuMemHostRegister: %d\n", (int)rc);
		return false;
	}
	m.registered = true;
	rc = cuMemHostGetDevicePointer(&m.dev, m.host, 0);
	if (rc != CUDA_SUCCESS) {
		fprintf(stderr, "cuMemHostGetDevicePointer: %d\n", (int)rc);
		return false;
	}
	return true;
}

static inline void step0b_regmem_destroy(step0b_regmem &m)
{
	if (m.registered)
		cuMemHostUnregister(m.host);
	if (m.host)
		munmap(m.host, m.size);
	if (m.host2)
		munmap(m.host2, m.size);
	m = step0b_regmem();
}
