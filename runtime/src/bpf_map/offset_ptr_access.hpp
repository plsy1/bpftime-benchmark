/* SPDX-License-Identifier: MIT */
#ifndef BPFTIME_OFFSET_PTR_ACCESS_HPP
#define BPFTIME_OFFSET_PTR_ACCESS_HPP

#include <boost/interprocess/offset_ptr.hpp>
#include <cstddef>

namespace bpftime::detail
{
// Keep Boost's null and rebasing semantics, but expose the uncommon null
// case as control flow so a caller's dependent loads need not await a mask.
// This is a local accessor: it changes neither Boost nor the stored pointer.
template <typename T, typename Difference, typename Offset, std::size_t Alignment>
inline T *offset_ptr_get_branch(
	const boost::interprocess::offset_ptr<T, Difference, Offset, Alignment> &ptr)
	noexcept
{
	if (!ptr) [[unlikely]]
		return nullptr;
	return ptr.get();
}
} // namespace bpftime::detail

#endif
