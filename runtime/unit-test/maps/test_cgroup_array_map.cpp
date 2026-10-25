#include "bpf_map/userspace/cgroup_array_map.hpp"
#include "bpftime_helper_group.hpp"
#include "catch2/catch_test_macros.hpp"
#include "unit-test/common_def.hpp"
#include <cerrno>
#include <climits>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <linux/bpf.h>
#include <string>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace bpftime;
using namespace boost::interprocess;

static const char *SHM_NAME = "BPFTIME_TEST_CGROUP_ARRAY_MAP";
static const char *CGROUP_ROOT = "/sys/fs/cgroup";

TEST_CASE("Cgroup paths compare on component boundaries", "[cgroup_array]")
{
	REQUIRE(cgroup_path_is_under("/a/b", "/a/b"));
	REQUIRE(cgroup_path_is_under("/a/b/c", "/a/b"));
	REQUIRE_FALSE(cgroup_path_is_under("/a/bc", "/a/b"));
	REQUIRE_FALSE(cgroup_path_is_under("/a", "/a/b"));
	REQUIRE(cgroup_path_is_under("/a", "/"));
	REQUIRE(cgroup_path_is_under("/", "/"));
}

// funclatency -c stores a cgroup directory fd; bpf_current_task_under_cgroup
// then filters on it (helper 37).
TEST_CASE("Cgroup array map stores cgroups and answers under-cgroup checks",
	  "[maps][cgroup_array]")
{
	struct statfs fs;
	if (statfs(CGROUP_ROOT, &fs) != 0 || fs.f_type != 0x63677270) {
		SKIP("no cgroup v2 at /sys/fs/cgroup");
	}
	char own[PATH_MAX];
	REQUIRE(current_thread_cgroup_path(own, sizeof(own)));

	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 1 << 20);
	cgroup_array_map_impl map(mem, 4, 4, 2);

	uint32_t key = 0;
	REQUIRE(map.current_task_under_cgroup(0) == -EAGAIN);
	REQUIRE(map.current_task_under_cgroup(2) == -E2BIG);
	errno = 0;
	REQUIRE(map.elem_lookup(&key) == nullptr);
	REQUIRE(errno == EOPNOTSUPP);

	int root_fd = open(CGROUP_ROOT, O_RDONLY | O_DIRECTORY);
	REQUIRE(root_fd >= 0);
	REQUIRE(map.elem_update(&key, &root_fd, BPF_ANY) == 0);
	REQUIRE(map.current_task_under_cgroup(0) == 1);
	errno = 0;
	REQUIRE(map.elem_update(&key, &root_fd, BPF_NOEXIST) == -1);
	REQUIRE(errno == EINVAL);
	uint32_t out_of_range = 2;
	errno = 0;
	REQUIRE(map.elem_update(&out_of_range, &root_fd, BPF_ANY) == -1);
	REQUIRE(errno == E2BIG);
	close(root_fd);

	// Our own cgroup (stored fd closed afterwards: the slot keeps a path).
	std::string own_dir = std::string(CGROUP_ROOT) + own;
	int own_fd = open(own_dir.c_str(), O_RDONLY | O_DIRECTORY);
	REQUIRE(own_fd >= 0);
	uint32_t key1 = 1;
	REQUIRE(map.elem_update(&key1, &own_fd, BPF_ANY) == 0);
	close(own_fd);
	REQUIRE(map.current_task_under_cgroup(1) == 1);

	// A top-level cgroup that is not an ancestor of ours.
	std::string other;
	if (DIR *dir = opendir(CGROUP_ROOT)) {
		while (auto *ent = readdir(dir)) {
			std::string path = std::string("/") + ent->d_name;
			if (ent->d_type == DT_DIR && ent->d_name[0] != '.' &&
			    !cgroup_path_is_under(own, path.c_str())) {
				other = std::string(CGROUP_ROOT) + path;
				break;
			}
		}
		closedir(dir);
	}
	if (!other.empty()) {
		int other_fd = open(other.c_str(), O_RDONLY | O_DIRECTORY);
		REQUIRE(other_fd >= 0);
		REQUIRE(map.elem_update(&key, &other_fd, BPF_ANY) == 0);
		close(other_fd);
		REQUIRE(map.current_task_under_cgroup(0) == 0);
	}

	// Not a cgroup v2 directory.
	int tmp_fd = open("/tmp", O_RDONLY | O_DIRECTORY);
	REQUIRE(tmp_fd >= 0);
	errno = 0;
	REQUIRE(map.elem_update(&key, &tmp_fd, BPF_ANY) == -1);
	REQUIRE(errno == EBADF);
	close(tmp_fd);

	REQUIRE(map.elem_delete(&key1) == 0);
	errno = 0;
	REQUIRE(map.elem_delete(&key1) == -1);
	REQUIRE(errno == ENOENT);
	REQUIRE(map.current_task_under_cgroup(1) == -EAGAIN);

	uint32_t next = 99;
	REQUIRE(map.map_get_next_key(nullptr, &next) == 0);
	REQUIRE(next == 0);
	REQUIRE(map.map_get_next_key(&next, &next) == 0);
	REQUIRE(next == 1);
	REQUIRE(map.map_get_next_key(&next, &next) == -1);
}

// The per-thread /proc/thread-self/cgroup fd names the thread that opened it.
// A fork child inherits it; once that parent thread is gone the child must
// still answer for itself.
TEST_CASE("Cgroup checks in a fork child do not use the parent thread's file",
	  "[maps][cgroup_array]")
{
	struct statfs fs;
	if (statfs(CGROUP_ROOT, &fs) != 0 || fs.f_type != 0x63677270) {
		SKIP("no cgroup v2 at /sys/fs/cgroup");
	}
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 1 << 20);
	cgroup_array_map_impl map(mem, 4, 4, 1);
	uint32_t key = 0;
	int root_fd = open(CGROUP_ROOT, O_RDONLY | O_DIRECTORY);
	REQUIRE(root_fd >= 0);
	REQUIRE(map.elem_update(&key, &root_fd, BPF_ANY) == 0);
	close(root_fd);

	int go[2];
	REQUIRE(pipe(go) == 0);
	pid_t child = -1;
	long in_thread = -1;
	std::thread forker([&]() {
		in_thread = map.current_task_under_cgroup(0); // opens the fd
		child = fork();
		if (child == 0) {
			char c;
			if (read(go[0], &c, 1) != 1)
				_exit(2);
			_exit(map.current_task_under_cgroup(0) == 1 ? 0 : 1);
		}
	});
	forker.join(); // the thread that opened the file is gone
	REQUIRE(in_thread == 1);
	REQUIRE(child > 0);
	REQUIRE(write(go[1], "x", 1) == 1);
	int status = 0;
	REQUIRE(waitpid(child, &status, 0) == child);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	close(go[0]);
	close(go[1]);
}

TEST_CASE("bpf_get_current_cgroup_id is the inode of the thread's cgroup",
	  "[cgroup_array]")
{
	struct statfs fs;
	if (statfs(CGROUP_ROOT, &fs) != 0 || fs.f_type != 0x63677270) {
		SKIP("no cgroup v2 at /sys/fs/cgroup");
	}
	char own[PATH_MAX];
	REQUIRE(current_thread_cgroup_path(own, sizeof(own)));
	struct stat st;
	REQUIRE(stat((std::string(CGROUP_ROOT) + own).c_str(), &st) == 0);
	REQUIRE(current_thread_cgroup_id() == st.st_ino);
	REQUIRE(current_thread_cgroup_id() != 0);
}

TEST_CASE("Cgroup helpers are registered", "[cgroup_array]")
{
	auto has = [](const bpftime::bpftime_helper_group &group, int id) {
		for (auto x : group.get_helper_ids())
			if ((int)x == id)
				return true;
		return false;
	};
	using bpftime::bpftime_helper_group;
	// bpf_get_current_cgroup_id and bpf_current_task_under_cgroup.
	REQUIRE(has(bpftime_helper_group::get_kernel_utils_helper_group(), 80));
	REQUIRE(has(bpftime_helper_group::get_shm_maps_helper_group(), 37));
}
