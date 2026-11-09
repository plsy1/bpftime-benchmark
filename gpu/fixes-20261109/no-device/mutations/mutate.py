#!/usr/bin/env python3
"""Mutations of the no-GPU fix (dev, runtime/src/attach/bpf_attach_ctx_cuda.cpp).
Usage: mutate.py NAME SOURCE_TREE. Each must make [cuda-no-device] fail."""
import sys
from pathlib import Path

F = "runtime/src/attach/bpf_attach_ctx_cuda.cpp"
CHECK = """		SPDLOG_WARN(
			"CommSharedMem has no device address (cuMemHostGetDevicePointer: {})",
			(int)err);
		return std::nullopt;"""
MUTATIONS = {
    # as before the fix: no device address throws out of bpf_attach_ctx's constructor
    "throw-again": (CHECK, CHECK.replace(
        "		return std::nullopt;",
        '		throw std::runtime_error("Unable to get device pointer for CommSharedMem");')),
    # the request slot cleared before knowing this process can serve it
    "memset-first": ("	CUdeviceptr device_addr = 0;\n	if (auto err = cuMemHostGetDevicePointer(",
                     "	memset(cuda_shared_mem, 0, sizeof(*cuda_shared_mem));\n"
                     "	CUdeviceptr device_addr = 0;\n	if (auto err = cuMemHostGetDevicePointer("),
}

name, tree = sys.argv[1], Path(sys.argv[2])
old, new = MUTATIONS[name]
path = tree / F
text = path.read_text()
assert text.count(old) == 1, (name, text.count(old))
path.write_text(text.replace(old, new))
print(f"applied {name}")
