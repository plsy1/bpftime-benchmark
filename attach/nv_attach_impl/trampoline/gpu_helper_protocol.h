// Memory shared by the GPU trampoline (default_trampoline.cu, shipped as
// ../trampoline_ptx.h) and the host (the agent's CUDA watcher, the GPU ringbuf
// map). The PTX has every size and offset below compiled in: after changing
// this file, regenerate the PTX
// (make -C attach/nv_attach_impl/trampoline CLANG=clang++-16 ../trampoline_ptx.h).
#ifndef BPFTIME_GPU_HELPER_PROTOCOL_H
#define BPFTIME_GPU_HELPER_PROTOCOL_H

#include <cstddef>
#include <cstdint>

#ifndef BPFTIME_GPU_HELPER_MAX_BUF
// Upper bound for the key and value staging buffers of host calls. The old
// 1 << 30 made the shared segment too large for Boost IPC.
#define BPFTIME_GPU_HELPER_MAX_BUF (1 << 20)
#endif

namespace bpftime_gpu
{
static constexpr std::size_t GPU_HELPER_MAX_BUF = BPFTIME_GPU_HELPER_MAX_BUF;

enum class HelperOperation {
	MAP_LOOKUP = 1,
	MAP_UPDATE = 2,
	MAP_DELETE = 3,
	MAP_GET_NEXT_KEY = 4,
	TRACE_PRINTK = 6,
	GET_CURRENT_PID_TGID = 14,
	PUTS = 501
};

union HelperCallRequest {
	struct {
		char key[GPU_HELPER_MAX_BUF];
	} map_lookup;
	struct {
		char key[GPU_HELPER_MAX_BUF];
		char value[GPU_HELPER_MAX_BUF];
		uint64_t flags;
	} map_update;
	struct {
		char key[GPU_HELPER_MAX_BUF];
	} map_delete;

	struct {
		char fmt[1000];
		int fmt_size;
		unsigned long arg1, arg2, arg3;
	} trace_printk;
	struct {
		char data[10000];
	} puts;
	struct {
	} get_tid_pgid;
};

union HelperCallResponse {
	struct {
		int result;
	} map_update, map_delete, trace_printk, puts;
	struct {
		const void *value;
	} map_lookup;
	struct {
		uint64_t result;
	} get_tid_pgid;
};

// The one host-call slot. A GPU thread takes occupy_flag, writes request_id,
// map_id and req, fences (membar.sys) and sets flag1. The host takes the
// request by changing flag1 from 1 to 0 (acquire), writes resp and sets flag2
// (release). The GPU thread waits for flag2, clears it, reads resp and
// releases occupy_flag. occupy_flag lives here rather than in the module, so
// that every module of the process (one per fatbin) shares one lock.
struct CommSharedMem {
	int flag1;
	int flag2;
	int occupy_flag;
	int request_id;
	long map_id;
	HelperCallRequest req;
	HelperCallResponse resp;
	uint64_t time_sum[8];
};

// Each GPU thread's part of a GPU ringbuf map (1527): this header, then
// max_entries records of { uint64_t size; char data[value_size]; }.
// A GPU thread takes busy, writes the record at tail % max_entries, fences
// (membar.sys) and only then stores tail + 1. The host reads tail (acquire),
// reads every record from head to that tail and stores each new head
// (release) after it has read the record.
struct gpu_ringbuf_header {
	uint64_t head; // written by the host
	uint64_t tail; // written by the GPU thread holding busy
	int busy;
	// records the GPU could not write: the thread's records were full, the
	// record was larger than value_size, or another GPU thread was writing
	// to the same records
	uint32_t dropped;
};

static_assert(sizeof(gpu_ringbuf_header) == 24,
	      "the ringbuf header layout is compiled into the trampoline PTX");
static_assert(offsetof(CommSharedMem, req) == 24,
	      "the host-call layout is compiled into the trampoline PTX");
} // namespace bpftime_gpu

#endif
