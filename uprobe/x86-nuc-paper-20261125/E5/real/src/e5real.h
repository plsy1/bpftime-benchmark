// E5-real record layout, shared by the BPF programs, the loader and (by field order) compare_e5real.py.
#ifndef E5REAL_H
#define E5REAL_H
struct rkey {
	__u64 t;
	__u64 pid_tgid;
};
struct rrec {
	__u64 t, pid_tgid, uid_gid, fd, buf, count;
	char comm[16];
};
#define E5REAL_MAX (1 << 19)
#endif
