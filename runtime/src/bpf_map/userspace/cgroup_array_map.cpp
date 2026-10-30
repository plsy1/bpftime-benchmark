/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#include "spdlog/spdlog.h"
#include "fork_handlers.hpp"
#include <bpf_map/userspace/cgroup_array_map.hpp>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/bpf.h>
#include <optional>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#ifndef CGROUP2_SUPER_MAGIC
#define CGROUP2_SUPER_MAGIC 0x63677270
#endif

namespace bpftime
{
namespace
{
// Mount root and mount point of the cgroup2 mount in this process.
std::optional<std::pair<std::string, std::string> > cgroup2_mount()
{
	int fd = ::open("/proc/self/mountinfo", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return {};
	std::string text;
	char chunk[4096];
	ssize_t n;
	while ((n = ::read(fd, chunk, sizeof(chunk))) > 0)
		text.append(chunk, n);
	::close(fd);
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) {
		// id parent major:minor root mount-point options ... - type ...
		auto sep = line.find(" - ");
		if (sep == std::string::npos)
			continue;
		std::istringstream tail(line.substr(sep + 3));
		std::string type;
		tail >> type;
		if (type != "cgroup2")
			continue;
		std::istringstream head(line.substr(0, sep));
		std::string id, parent, dev, root, mount_point;
		head >> id >> parent >> dev >> root >> mount_point;
		return std::make_pair(root, mount_point);
	}
	return {};
}

// cgroup v2 path (relative to the cgroup namespace root) of a directory fd of
// this process, or nullopt with errno = EBADF when it is not a cgroup v2 dir.
std::optional<std::string> cgroup_path_of_fd(int fd)
{
	struct statfs fs;
	struct stat st;
	if (fstatfs(fd, &fs) != 0 || fs.f_type != CGROUP2_SUPER_MAGIC ||
	    fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode)) {
		errno = EBADF;
		return {};
	}
	char link[64];
	snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	char target[PATH_MAX];
	ssize_t len = readlink(link, target, sizeof(target) - 1);
	auto mount = cgroup2_mount();
	if (len < 0 || !mount) {
		errno = EBADF;
		return {};
	}
	target[len] = '\0';
	const std::string absolute(target);
	const auto &[root, mount_point] = *mount;
	if (absolute.compare(0, mount_point.size(), mount_point) != 0 ||
	    (absolute.size() > mount_point.size() &&
	     absolute[mount_point.size()] != '/' && mount_point != "/")) {
		errno = EBADF;
		return {};
	}
	std::string suffix = absolute.substr(mount_point == "/" ? 0 :
								  mount_point.size());
	std::string path = root == "/" ? suffix : root + suffix;
	if (path.empty())
		path = "/";
	return path;
}
} // namespace

namespace
{
// Per-thread fd of /proc/thread-self/cgroup, re-read with pread at offset 0
// (the proc file is regenerated on every read, so the result stays current):
// one system call per check instead of open + read + close. /proc/thread-self
// is resolved at open, so a fork child must reopen; the generation bump in
// the atfork child handler makes every inherited cache stale.
std::atomic<uint64_t> cgroup_fd_generation{ 0 };

struct thread_cgroup_file {
	int fd = -1;
	uint64_t generation = 0;
	~thread_cgroup_file()
	{
		if (fd >= 0)
			::close(fd);
	}
};
thread_local thread_cgroup_file thread_cgroup;

void invalidate_cgroup_files_in_child()
{
	cgroup_fd_generation.fetch_add(1, std::memory_order_relaxed);
}

[[maybe_unused]] const int cgroup_atfork_registered =
	bpftime::register_fork_handlers(nullptr, nullptr,
					invalidate_cgroup_files_in_child);

ssize_t read_thread_cgroup_file(char *buf, size_t size)
{
	auto &file = thread_cgroup;
	const uint64_t generation =
		cgroup_fd_generation.load(std::memory_order_relaxed);
	if (file.fd >= 0 && file.generation != generation) {
		// Inherited from the parent's thread: belongs to it.
		::close(file.fd);
		file.fd = -1;
	}
	if (file.fd < 0) {
		file.fd = ::open("/proc/thread-self/cgroup",
				 O_RDONLY | O_CLOEXEC);
		if (file.fd < 0)
			return -1;
		file.generation = generation;
	}
	ssize_t n;
	do {
		n = ::pread(file.fd, buf, size - 1, 0);
	} while (n < 0 && errno == EINTR);
	if (n < 0)
		return -1;
	buf[n] = '\0';
	return n;
}
} // namespace

bool current_thread_cgroup_path(char *buf, size_t size)
{
	char text[PATH_MAX + 64];
	if (read_thread_cgroup_file(text, sizeof(text)) <= 0)
		return false;
	// cgroup v2 is the "0::<path>" line (hybrid systems list v1 too).
	for (char *line = text; line && *line;) {
		char *next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		if (strncmp(line, "0::", 3) == 0) {
			size_t len = strlen(line + 3);
			if (len + 1 > size)
				return false;
			memcpy(buf, line + 3, len + 1);
			return true;
		}
		line = next;
	}
	return false;
}

bool cgroup_path_is_under(const char *path, const char *ancestor)
{
	size_t len = strlen(ancestor);
	if (len == 1 && ancestor[0] == '/')
		return path[0] == '/';
	return strncmp(path, ancestor, len) == 0 &&
	       (path[len] == '\0' || path[len] == '/');
}

cgroup_array_map_impl::cgroup_array_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries)
	: slots(max_entries, slot{}, memory.get_segment_manager()),
	  _max_entries(max_entries)
{
	if (key_size != 4 || value_size != 4) {
		SPDLOG_ERROR(
			"cgroup array map needs 4-byte keys and values, got {} and {}",
			key_size, value_size);
		throw std::runtime_error("Invalid cgroup array map attributes");
	}
}

void *cgroup_array_map_impl::elem_lookup(const void *key)
{
	errno = EOPNOTSUPP;
	return nullptr;
}

long cgroup_array_map_impl::elem_update(const void *key, const void *value,
					uint64_t flags)
{
	const uint32_t index = *(const uint32_t *)key;
	if (index >= _max_entries) {
		errno = E2BIG;
		return -1;
	}
	if (flags != BPF_ANY) {
		errno = EINVAL;
		return -1;
	}
	auto path = cgroup_path_of_fd(*(const int *)value);
	if (!path)
		return -1;
	if (path->size() + 1 > sizeof(slots[index].path)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	memcpy(slots[index].path, path->c_str(), path->size() + 1);
	slots[index].used = true;
	SPDLOG_DEBUG("cgroup array slot {} -> {}", index, *path);
	return 0;
}

long cgroup_array_map_impl::elem_delete(const void *key)
{
	const uint32_t index = *(const uint32_t *)key;
	if (index >= _max_entries) {
		errno = E2BIG;
		return -1;
	}
	if (!slots[index].used) {
		errno = ENOENT;
		return -1;
	}
	slots[index].used = false;
	return 0;
}

int cgroup_array_map_impl::map_get_next_key(const void *key, void *next_key)
{
	uint32_t next = key == nullptr ? 0 : *(const uint32_t *)key + 1;
	if (key != nullptr && *(const uint32_t *)key >= _max_entries)
		next = 0;
	if (next >= _max_entries) {
		errno = ENOENT;
		return -1;
	}
	*(uint32_t *)next_key = next;
	return 0;
}

long cgroup_array_map_impl::slot_path(uint32_t index, char *buf,
				      size_t size) const
{
	if (index >= _max_entries)
		return -E2BIG;
	if (!slots[index].used)
		return -EAGAIN;
	size_t len = strlen(slots[index].path);
	if (len + 1 > size)
		return -ENAMETOOLONG;
	memcpy(buf, slots[index].path, len + 1);
	return 0;
}

long cgroup_task_under_path(const char *ancestor)
{
	char current[PATH_MAX];
	if (!current_thread_cgroup_path(current, sizeof(current)))
		return 0;
	return cgroup_path_is_under(current, ancestor) ? 1 : 0;
}

uint64_t current_thread_cgroup_id()
{
	// The cgroup2 mount of this process, parsed once (mount namespaces do
	// not change under a running process in practice).
	static const auto mount = cgroup2_mount();
	if (!mount)
		return 0;
	char path[PATH_MAX];
	if (!current_thread_cgroup_path(path, sizeof(path)))
		return 0;
	const auto &[root, mount_point] = *mount;
	// /proc paths and the mount root are both relative to the cgroup
	// namespace root.
	const char *suffix = path;
	if (root != "/") {
		if (strncmp(path, root.c_str(), root.size()) != 0)
			return 0;
		suffix = path + root.size();
	}
	std::string dir = mount_point == "/" ? std::string(suffix) :
					      mount_point + suffix;
	struct stat st;
	if (stat(dir.c_str(), &st) != 0)
		return 0;
	return st.st_ino;
}

long cgroup_array_map_impl::current_task_under_cgroup(uint32_t index) const
{
	char ancestor[PATH_MAX];
	if (long err = slot_path(index, ancestor, sizeof(ancestor)); err < 0)
		return err;
	return cgroup_task_under_path(ancestor);
}

} // namespace bpftime
