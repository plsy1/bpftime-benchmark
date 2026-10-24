/* SPDX-License-Identifier: MIT */
#ifndef BPFTIME_CREDENTIAL_CACHE_HPP
#define BPFTIME_CREDENTIAL_CACHE_HPP

// bpf_get_current_uid_gid may cache the credentials only while every change of
// them in this process invalidates the cache. The agent enables the cache after
// verifying that its libc set*id interposers are the definitions the process
// binds; a fork child invalidates it. Both calls are async-signal-safe.
extern "C" {
void bpftime_set_credential_cache_enabled(bool enabled);
void bpftime_invalidate_credential_cache();
}

#endif
