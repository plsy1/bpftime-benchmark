#include <cstdint>
extern "C" {
bool injected_with_frida = true;
using hook_t = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
void _bpftime__setup_syscall_trace_callback(hook_t *hooker) { (void)hooker; }
}
