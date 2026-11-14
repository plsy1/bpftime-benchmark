// Compile-only cases for check 5: each case is its own translation unit
// (-DCASE_<name>), so one case failing in the backend cannot hide another.
// Written as tools write them (libbpf-tools: __sync_fetch_and_add on map values;
// bpftime's cuda-counter example: __atomic_add_fetch(..., __ATOMIC_SEQ_CST)).
typedef unsigned long long u64;
typedef unsigned int u32;
struct v64 { u64 x; };
struct v32 { u32 x; };

#ifdef CASE_fadd64_used
u64 f(struct v64 *v) { return __sync_fetch_and_add(&v->x, 1); }
#endif
#ifdef CASE_fadd64_unused
void f(struct v64 *v) { __sync_fetch_and_add(&v->x, 1); }
#endif
#ifdef CASE_fadd32_used
u32 f(struct v32 *v) { return __sync_fetch_and_add(&v->x, 1); }
#endif
#ifdef CASE_fadd32_unused
void f(struct v32 *v) { __sync_fetch_and_add(&v->x, 1); }
#endif
#ifdef CASE_atomic_add_fetch64_used
u64 f(struct v64 *v) { return __atomic_add_fetch(&v->x, 1, __ATOMIC_SEQ_CST); }
#endif
#ifdef CASE_cas64
u64 f(struct v64 *v, u64 o) { return __sync_val_compare_and_swap(&v->x, o, o + 1); }
#endif
#ifdef CASE_cas32
u32 f(struct v32 *v, u32 o) { return __sync_val_compare_and_swap(&v->x, o, o + 1); }
#endif
#ifdef CASE_cas64_loop
u64 f(struct v64 *v)
{
	u64 old;
	do {
		old = v->x;
	} while (__sync_val_compare_and_swap(&v->x, old, old + 1) != old);
	return old;
}
#endif
#ifdef CASE_xchg64
u64 f(struct v64 *v, u64 n) { return __sync_lock_test_and_set(&v->x, n); }
#endif
