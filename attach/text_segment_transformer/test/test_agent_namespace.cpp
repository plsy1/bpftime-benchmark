// The transformer loads the agent into its own link-map namespace, with its
// own libc. Both libcs number pthread keys from their own tables but store
// values in the same per-thread array: nginx under syscount aborted in
// OpenSSL, whose new key read a pointer of the agent. Each case loads a
// stand-in agent in a forked child (loading is irreversible) and reports
// through the exit status.
#include "agent_namespace.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <dlfcn.h>
#include <pthread.h>
#include <set>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
struct fake_agent {
	int (*set)();
	int (*wrong)();
	unsigned (*key)(int);
	int count;
};

bool load(fake_agent &agent)
{
	void *handle = bpftime::load_agent_in_new_namespace(FAKE_AGENT_KEYS_PATH);
	if (handle == nullptr) {
		fprintf(stderr, "load: %s\n", dlerror());
		return false;
	}
	agent.set = (int (*)())dlsym(handle, "fake_agent_set");
	agent.wrong = (int (*)())dlsym(handle, "fake_agent_wrong");
	agent.key = (unsigned (*)(int))dlsym(handle, "fake_agent_key");
	auto count = (int (*)())dlsym(handle, "fake_agent_key_count");
	if (!agent.set || !agent.wrong || !agent.key || !count)
		return false;
	agent.count = count();
	return true;
}

// Keys of this process: each reads NULL when new, and keeps its value.
constexpr int PROCESS_KEYS = 8;
int create_process_keys(pthread_key_t *keys)
{
	int bad = 0;
	for (int i = 0; i < PROCESS_KEYS; i++) {
		if (pthread_key_create(&keys[i], nullptr) != 0)
			return 100;
		if (void *v = pthread_getspecific(keys[i]); v != nullptr) {
			fprintf(stderr, "new key %u reads %p\n", keys[i], v);
			bad++;
		}
		pthread_setspecific(keys[i], (void *)(0xb000L + i));
	}
	return bad;
}

int wrong_process_values(const pthread_key_t *keys)
{
	int bad = 0;
	for (int i = 0; i < PROCESS_KEYS; i++)
		if (pthread_getspecific(keys[i]) != (void *)(0xb000L + i))
			bad++;
	return bad;
}

int in_child(int (*body)())
{
	fflush(nullptr);
	pid_t pid = fork();
	if (pid == 0) {
		alarm(30);
		_exit(body());
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
} // namespace

TEST_CASE("The agent's pthread keys and the process's do not overlap")
{
	REQUIRE(in_child([]() -> int {
			// Keys created before the agent loads (constructors of
			// the process's libraries).
			pthread_key_t early[3];
			for (int i = 0; i < 3; i++) {
				pthread_key_create(&early[i], nullptr);
				pthread_setspecific(early[i], (void *)(0xe000L + i));
			}
			fake_agent agent;
			if (!load(agent))
				return 101;
			std::set<unsigned> agent_keys;
			for (int i = 0; i < agent.count; i++) {
				// Inline keys: no second-level array to free.
				if (agent.key(i) >= 32)
					return 102;
				agent_keys.insert(agent.key(i));
			}
			pthread_key_t keys[PROCESS_KEYS];
			int bad = create_process_keys(keys);
			for (auto k : keys)
				if (agent_keys.count(k))
					bad++;
			for (int i = 0; i < 3; i++)
				if (agent_keys.count(early[i]) ||
				    pthread_getspecific(early[i]) !=
					    (void *)(0xe000L + i))
					bad++;
			if (agent.wrong() != 0)
				bad++;
			agent.set();
			return bad + wrong_process_values(keys);
		}) == 0);
}

TEST_CASE("Threads using both namespaces' keys exit cleanly")
{
	REQUIRE(in_child([]() -> int {
			static fake_agent agent;
			if (!load(agent))
				return 101;
			static pthread_key_t keys[PROCESS_KEYS];
			if (create_process_keys(keys) != 0)
				return 1;
			int bad = 0;
			for (int round = 0; round < 4; round++) {
				pthread_t thread;
				void *ret = nullptr;
				pthread_create(
					&thread, nullptr,
					[](void *) -> void * {
						long bad = 0;
						if (agent.set() != 0)
							bad++;
						for (int i = 0; i < PROCESS_KEYS; i++)
							pthread_setspecific(
								keys[i],
								(void *)(0xc000L +
									 i));
						bad += agent.wrong();
						return (void *)bad;
					},
					nullptr);
				pthread_join(thread, &ret);
				bad += (int)(long)ret;
			}
			return bad + agent.wrong() + wrong_process_values(keys);
		}) == 0);
}

// pthread_atfork in the agent registers with the agent namespace's libc,
// which the application's fork (main libc) never runs: children kept the
// parent's cached pid, perf shard and probe pid. The transformer forwards
// the main libc's fork handlers to the agent's bpftime_agent_fork_hook.
TEST_CASE("Fork handlers of an agent in its own namespace run in fork children")
{
	int code = in_child([] {
		void *handle = bpftime::load_agent_in_new_namespace(
			FAKE_AGENT_FORK_PATH);
		if (handle == nullptr) {
			fprintf(stderr, "load: %s\n", dlerror());
			return 2;
		}
		auto cached_pid =
			(pid_t (*)())dlsym(handle, "fake_agent_cached_pid");
		auto counts = (int (*)())dlsym(handle, "fake_agent_fork_counts");
		if (!cached_pid || !counts || cached_pid() != getpid())
			return 3;
		if (!bpftime::install_agent_fork_hook(handle))
			return 4;
		fflush(nullptr);
		pid_t pid = fork();
		if (pid == 0)
			_exit(cached_pid() == getpid() ? 0 : 5);
		int status = 0;
		if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
			return 6;
		if (WEXITSTATUS(status) != 0)
			return WEXITSTATUS(status);
		// One prepare and one parent handler ran here.
		return counts() == 11 && cached_pid() == getpid() ? 0 : 7;
	});
	REQUIRE(code == 0);
}

// Injected-agent contract: a transformer whose agent cannot be loaded, or is
// not a bpftime agent, must leave the host running (untraced). It called
// exit(1), dereferenced a missing injected_with_frida symbol, and logged its
// entry to the host's stdout.
TEST_CASE("The transformer leaves the host running when the agent is unusable")
{
	for (const char *agent :
	     { "/nonexistent/libbpftime-agent.so", FAKE_AGENT_KEYS_PATH }) {
		INFO(agent);
		std::string command =
			std::string("env LD_PRELOAD=") + TRANSFORMER_SO_PATH +
			" AGENT_SO=" + agent +
			" sh -c 'echo transformer-host-ok' 2>/dev/null";
		FILE *out = popen(command.c_str(), "r");
		REQUIRE(out != nullptr);
		// The host's stdout carries its own output only.
		std::string text;
		char buf[256];
		while (fgets(buf, sizeof(buf), out) != nullptr)
			text += buf;
		const int status = pclose(out);
		CHECK(text == "transformer-host-ok\n");
		CHECK(WIFEXITED(status));
		CHECK(WEXITSTATUS(status) == 0);
	}
}
