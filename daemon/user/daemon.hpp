/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef BPFTIME_DAEMON_HPP
#define BPFTIME_DAEMON_HPP

#include "daemon_config.hpp"

struct bpf_tracer_bpf;

namespace bpftime
{
int start_daemon(struct daemon_config env);
// Choose which tracer programs load: the open and auto-attach programs per
// ENV, and no tracepoint of a system call this kernel does not have.
void select_tracer_programs(struct bpf_tracer_bpf *obj,
			    const struct daemon_config &env);
} // namespace bpftime

#endif // BPFTIME_DAEMON_HPP