// Threads keep logging while the process exits (as probes do). With bpftime's
// bpftime_set_logger (which registers bpftime_quiet_logger_at_exit) the
// process must exit cleanly; with a plain spdlog default logger (mode "raw")
// the registry's destructor frees the logger under the logging threads.
// Modes: fix | raw | fix-thread-exit (exit() called from a worker thread) |
// fix-fork (a forked child exits while its threads log).
#include "bpftime_logger.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static void logger_loop(int id)
{
	for (uint64_t i = 0;; i++)
		SPDLOG_ERROR("thread {} message {}", id, i);
}

int main(int argc, char **argv)
{
	const std::string mode = argc > 1 ? argv[1] : "fix";
	// Log to a file in the scratch dir: the point is the lifetime, not output.
	const char *path = argc > 2 ? argv[2] : "/dev/null";
	if (mode == "raw") {
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
		auto logger = std::make_shared<spdlog::logger>("raw", std::move(sink));
		spdlog::set_default_logger(std::move(logger));
	} else {
		bpftime::bpftime_set_logger(path);
	}
	if (mode == "fix-fork") {
		pid_t pid = fork();
		if (pid == 0) {
			for (int t = 0; t < 3; t++)
				std::thread(logger_loop, t).detach();
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
			exit(0);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
	}
	for (int t = 0; t < 3; t++)
		std::thread(logger_loop, t).detach();
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	if (mode == "fix-thread-exit") {
		std::thread([] { exit(0); }).detach();
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 7; // exit() did not finish
	}
	exit(0);
}
