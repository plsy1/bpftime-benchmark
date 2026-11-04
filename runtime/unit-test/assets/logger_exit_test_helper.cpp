// Logs from an exit handler that runs after spdlog's registry is destroyed
// (registered before the registry exists), as a probe does while its process
// exits. Exit status: 0 the log was dropped by a live, silenced logger; 3 the
// logger and its sinks had been destroyed (what a probe then wrote through);
// 4 the log was written; 5 the logger did not work before exit.
#include "bpftime_logger.hpp"
#include "spdlog/sinks/base_sink.h"
#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <unistd.h>

namespace
{
std::atomic<bool> sink_destroyed{ false };
std::atomic<int> written{ 0 };
bool logged_before_exit = false;

struct watch_sink : spdlog::sinks::base_sink<std::mutex> {
	~watch_sink() override
	{
		sink_destroyed = true;
	}
	void sink_it_(const spdlog::details::log_msg &) override
	{
		written++;
	}
	void flush_() override
	{
	}
};

void log_after_teardown()
{
	if (!logged_before_exit)
		_exit(5);
	if (sink_destroyed)
		_exit(3);
	const int before = written;
	SPDLOG_ERROR("logged after spdlog's registry was destroyed");
	_exit(written != before ? 4 : 0);
}
} // namespace

int main()
{
	// Before the registry exists, so it runs after the registry's
	// destructor.
	std::atexit(log_after_teardown);
	bpftime::bpftime_set_logger("console");
	spdlog::default_logger()->sinks().push_back(
		std::make_shared<watch_sink>());
	SPDLOG_ERROR("logged before exit");
	logged_before_exit = written == 1;
	return 0;
}
