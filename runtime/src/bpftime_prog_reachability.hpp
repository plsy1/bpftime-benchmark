#ifndef _BPFTIME_PROG_REACHABILITY_HPP
#define _BPFTIME_PROG_REACHABILITY_HPP
#include <cstdint>
#include <ebpf-vm.h>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace bpftime
{
// The value of a map no program and no syscall can change any more: frozen,
// and created read-only to programs (libbpf's .rodata). Empty for other maps.
using constant_map_value =
	std::function<std::optional<std::span<const uint8_t> >(int fd)>;

// Which instructions can run, pruned as the kernel's verifier prunes them: a
// branch whose condition is decided by values loaded from constant maps is
// followed one way only (libbpf-tools keep their options in .rodata, and the
// code of an option that is off may call helpers bpftime does not have).
// Conservative: an instruction it does not model leaves the registers it may
// write unknown, and an undecided branch is followed both ways, so a mistake
// can keep dead code, not drop live code.
std::vector<bool>
reachable_instructions(const std::vector<ebpf_inst> &insns,
		       const constant_map_value &constant);
} // namespace bpftime
#endif
