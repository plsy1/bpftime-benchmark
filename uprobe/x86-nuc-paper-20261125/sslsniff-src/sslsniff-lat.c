// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// Copyright (c) 2023 Yusheng Zheng
//
// Based on sslsniff from BCC by Adrian Lopez & Mark Drayton.
// 15-Aug-2023   Yusheng Zheng   Created this.
#include <argp.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <ctype.h>
#include <errno.h>
#include <linux/types.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "sslsniff.skel.h"
#include "sslsniff.h"

#define INVALID_UID -1
#define INVALID_PID -1
#define DEFAULT_BUFFER_SIZE 8192

#define __ATTACH_UPROBE(skel, binary_path, sym_name, prog_name, is_retprobe)   \
	do {                                                                       \
	  LIBBPF_OPTS(bpf_uprobe_opts, uprobe_opts, .func_name = #sym_name,        \
				  .retprobe = is_retprobe);                                    \
	  skel->links.prog_name = bpf_program__attach_uprobe_opts(                 \
		  skel->progs.prog_name, env.pid, binary_path, 0, &uprobe_opts);       \
	} while (false)

#define __CHECK_PROGRAM(skel, prog_name)               \
	do {                                               \
	  if (!skel->links.prog_name) {                    \
		perror("no program attached for " #prog_name); \
		return -errno;                                 \
	  }                                                \
	} while (false)

#define __ATTACH_UPROBE_CHECKED(skel, binary_path, sym_name, prog_name,     \
								is_retprobe)                                \
	do {                                                                    \
	  __ATTACH_UPROBE(skel, binary_path, sym_name, prog_name, is_retprobe); \
	  __CHECK_PROGRAM(skel, prog_name);                                     \
	} while (false)

#define ATTACH_UPROBE_CHECKED(skel, binary_path, sym_name, prog_name)     \
	__ATTACH_UPROBE_CHECKED(skel, binary_path, sym_name, prog_name, false)
#define ATTACH_URETPROBE_CHECKED(skel, binary_path, sym_name, prog_name)  \
	__ATTACH_UPROBE_CHECKED(skel, binary_path, sym_name, prog_name, true)

volatile sig_atomic_t exiting = 0;

const char *argp_program_version = "sslsniff 0.1";
const char *argp_program_bug_address = "https://github.com/iovisor/bcc/tree/master/libbpf-tools";
const char argp_program_doc[] =
	"Sniff SSL data.\n"
	"\n"
	"USAGE: sslsniff [OPTIONS]\n"
	"\n"
	"EXAMPLES:\n"
	"    ./sslsniff              # sniff OpenSSL and GnuTLS functions\n"
	"    ./sslsniff -p 181       # sniff PID 181 only\n"
	"    ./sslsniff -u 1000      # sniff only UID 1000\n"
	"    ./sslsniff -c curl      # sniff curl command only\n"
	"    ./sslsniff --no-openssl # don't show OpenSSL calls\n"
	"    ./sslsniff --no-gnutls  # don't show GnuTLS calls\n"
	"    ./sslsniff --no-nss     # don't show NSS calls\n"
	"    ./sslsniff --hexdump    # show data as hex instead of trying to "
	"decode it as UTF-8\n"
	"    ./sslsniff -x           # show process UID and TID\n"
	"    ./sslsniff -l           # show function latency\n"
	"    ./sslsniff -l --handshake  # show SSL handshake latency\n"
	"    ./sslsniff --extra-lib openssl:/path/libssl.so.1.1 # sniff extra "
	"library\n";

struct env {
	pid_t pid;
	int uid;
	bool extra;
	char *comm;
	bool openssl;
	bool gnutls;
	bool nss;
	bool hexdump;
	bool latency;
	bool handshake;
	char *extra_lib;
} env = {
	.uid = INVALID_UID,
	.pid = INVALID_PID,
	.openssl = true,
	.gnutls = true,
	.nss = true,
	.comm = NULL,
};

#define HEXDUMP_KEY 1000
#define HANDSHAKE_KEY 1002
#define EXTRA_LIB_KEY 1003

static const struct argp_option opts[] = {
	{"pid", 'p', "PID", 0, "Sniff this PID only."},
	{"uid", 'u', "UID", 0, "Sniff this UID only."},
	{"extra", 'x', NULL, 0, "Show extra fields (UID, TID)"},
	{"comm", 'c', "COMMAND", 0, "Sniff only commands matching string."},
	{"no-openssl", 'o', NULL, 0, "Do not show OpenSSL calls."},
	{"no-gnutls", 'g', NULL, 0, "Do not show GnuTLS calls."},
	{"no-nss", 'n', NULL, 0, "Do not show NSS calls."},
	{"hexdump", HEXDUMP_KEY, NULL, 0,
	 "Show data as hexdump instead of trying to decode it as UTF-8"},
	{"latency", 'l', NULL, 0, "Show function latency"},
	{"handshake", HANDSHAKE_KEY, NULL, 0,
	 "Show SSL handshake latency, enabled only if latency option is on."},
	{"extra-lib", EXTRA_LIB_KEY, "LIBTYPE:LIBPATH", 0,
	 "Intercept calls from extra library"},
	{"verbose", 'v', NULL, 0, "Verbose debug output"},
	{ NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" },
	{},
};

static bool verbose = false;

static error_t parse_arg(int key, char *arg, struct argp_state *state) {
	switch (key) {
	case 'p':
		env.pid = atoi(arg);
		break;
	case 'u':
		env.uid = atoi(arg);
		break;
	case 'x':
		env.extra = true;
		break;
	case 'c':
		env.comm = strdup(arg);
		break;
	case 'o':
		env.openssl = false;
		break;
	case 'g':
		env.gnutls = false;
		break;
	case 'n':
		env.nss = false;
		break;
	case 'l':
		env.latency = true;
		break;
	case 'v':
		verbose = true;
		break;
	case HEXDUMP_KEY:
		env.hexdump = true;
		break;
	case HANDSHAKE_KEY:
		env.handshake = true;
		break;
	case EXTRA_LIB_KEY:
		env.extra_lib = strdup(arg);
		break;
	case 'h':
		argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#define PERF_BUFFER_PAGES 16
#define PERF_POLL_TIMEOUT_MS 100
#define warn(...) fprintf(stderr, __VA_ARGS__)

static struct argp argp = {
	opts,
	parse_arg,
	NULL,
	argp_program_doc
};

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
						   va_list args) {
	if (level == LIBBPF_DEBUG && !verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

/* Paper counting loader: events and full return-value bytes per direction,
 * losses reported by the perf buffer, and the events drained after SIGINT. */
static unsigned long long cnt_ev[3], cnt_bytes[3], cnt_lost;

/* ---- H2 latency loader (sslsniff-s4lat) ----
 * Per delivered event: lat = clock_gettime(CLOCK_MONOTONIC) at delivery - event->timestamp_ns
 * (bpf_ktime_get_ns at SSL_read/SSL_write return; kernel: CLOCK_MONOTONIC; bpftime:
 * std::chrono::steady_clock = CLOCK_MONOTONIC on Linux/libstdc++).
 * Window (by the event's own timestamp): [anchor + warmup, t_stop). warmup defaults to 1 s
 * (SSLSNIFF_LAT_WARMUP_MS overrides); t_stop is the CLOCK_MONOTONIC time at which SIGUSR1
 * arrived (unset = open until exit). The anchor is chosen at start-up by SSLSNIFF_LAT_ANCHOR:
 *  - "consume" (default, the H2 spec): t_consume, taken just before the first perf_buffer__poll.
 *    SIGUSR2 is only recorded (t_usr2), it does not move the window.
 *  - "usr2": the first SIGUSR2 (the harness sends it just before the load generator starts).
 *    Until it arrives the window is closed: every event delivered (and loss reported) before it
 *    is warm-up, which is exact because such an event was stamped before the signal. If no
 *    SIGUSR2 ever arrives, nothing is counted and "usr2_missing" is true. run_paper.py uses this
 *    mode for stage S4lat, so the nginx readiness probe and the first second of load are
 *    excluded on every leg (kernel and bpftime alike).
 * Events produced inside the window are counted whenever they are delivered (also in the
 * final drain after SIGINT: "late_ev"), so the queueing they saw is included; events produced
 * outside it are not ("pre_ev", "post_ev").
 * Histogram: HDR-style log-linear, 32 sub-buckets per power of two (values < 64 ns exact,
 * relative bucket width <= 1/32 above), covering 0 .. 2^(LAT_MAX_EXP+1) ns (~2199 s); larger
 * values go to the top bucket and are counted in "overflow". Printed quantiles are the upper
 * bound of the bucket holding the rank (biased up by < 1/32), clamped to the exact max; the
 * buckets are printed so the analysis can interpolate. */
#define LAT_SUB_BITS 5
#define LAT_SUB (1u << LAT_SUB_BITS)
#define LAT_MAX_EXP 40
#define LAT_NBUCKETS ((LAT_MAX_EXP - LAT_SUB_BITS + 1) * LAT_SUB + LAT_SUB)
static unsigned long long lat_hist[LAT_NBUCKETS];
static unsigned long long lat_count, lat_sum, lat_min = UINT64_MAX, lat_max, lat_overflow,
	lat_negative, lat_pre_ev, lat_post_ev, lat_late_ev;
static unsigned long long lat_lost_pre, lat_lost_win, lat_lost_post;
static unsigned long long lat_t_consume, lat_t_warm_end, lat_warmup_ns = 1000000000ULL;
static volatile unsigned long long lat_t_stop; /* 0 = not stopped (written by the SIGUSR1 handler) */
static volatile unsigned long long lat_t_usr2; /* 0 = no SIGUSR2 (load start marker) */
static int lat_anchor_usr2; /* SSLSNIFF_LAT_ANCHOR=usr2: window anchored at the first SIGUSR2 */

static inline unsigned long long lat_now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
}

static inline unsigned lat_index(unsigned long long v) {
	if (v < 2 * LAT_SUB)
		return (unsigned)v;
	unsigned e = 63 - __builtin_clzll(v); /* >= LAT_SUB_BITS + 1 */
	if (e > LAT_MAX_EXP)
		return LAT_NBUCKETS - 1;
	return (e - LAT_SUB_BITS + 1) * LAT_SUB + (unsigned)((v >> (e - LAT_SUB_BITS)) & (LAT_SUB - 1));
}

/* Lowest and highest value of bucket i (inclusive). */
static inline unsigned long long lat_lo(unsigned i) {
	if (i < 2 * LAT_SUB)
		return i;
	unsigned e = i / LAT_SUB + LAT_SUB_BITS - 1, sub = i % LAT_SUB;
	return (1ULL << e) + ((unsigned long long)sub << (e - LAT_SUB_BITS));
}
static inline unsigned long long lat_hi(unsigned i) {
	if (i < 2 * LAT_SUB)
		return i;
	unsigned e = i / LAT_SUB + LAT_SUB_BITS - 1;
	return lat_lo(i) + (1ULL << (e - LAT_SUB_BITS)) - 1;
}

static void lat_record(const struct probe_SSL_data_t *e) {
	unsigned long long now = lat_now(), ts = e->timestamp_ns;
	unsigned long long stop = lat_t_stop;
	if (lat_anchor_usr2) {
		unsigned long long u2 = lat_t_usr2;
		if (!u2) { /* load not started yet: stamped before the signal, so warm-up */
			lat_pre_ev++;
			return;
		}
		lat_t_warm_end = u2 + lat_warmup_ns;
	}
	if (ts < lat_t_warm_end) {
		lat_pre_ev++;
		return;
	}
	if (stop && ts >= stop) {
		lat_post_ev++;
		return;
	}
	if (stop && now >= stop)
		lat_late_ev++; /* produced in the window, delivered after SIGUSR1 */
	unsigned long long v;
	if (now >= ts) {
		v = now - ts;
	} else {
		v = 0;
		lat_negative++;
	}
	unsigned idx = lat_index(v);
	if (v >> (LAT_MAX_EXP + 1))
		lat_overflow++;
	lat_hist[idx]++;
	lat_count++;
	lat_sum += v;
	if (v < lat_min)
		lat_min = v;
	if (v > lat_max)
		lat_max = v;
}

/* Value at quantile qm/1000: highest value of the first bucket whose cumulative count
 * reaches ceil(qm * total / 1000) (integer arithmetic), clamped to the observed max. total may include lost events counted as
 * +inf (extra > 0); returns -1 when the rank falls among them or there is nothing. The
 * "_lost_inf" quantiles count lost_win + lost_post as +inf (lost_post: losses reported after
 * SIGUSR1 may have happened inside the window). */
static long long lat_quantile(unsigned qm, unsigned long long extra) {
	unsigned long long total = lat_count + extra;
	if (!total)
		return -1;
	unsigned long long rank = (qm * total + 999) / 1000;
	if (rank < 1)
		rank = 1;
	if (rank > lat_count)
		return -1;
	unsigned long long c = 0;
	for (unsigned i = 0; i < LAT_NBUCKETS; i++) {
		c += lat_hist[i];
		if (c >= rank) {
			unsigned long long h = lat_hi(i);
			return (long long)(h > lat_max ? lat_max : h);
		}
	}
	return (long long)lat_max;
}

static void sig_usr1(int signo) {
	(void)signo;
	if (!lat_t_stop)
		lat_t_stop = lat_now();
}

static void sig_usr2(int signo) {
	(void)signo;
	if (!lat_t_usr2)
		lat_t_usr2 = lat_now();
}

static void lat_print(void) {
	unsigned long long lost_inf = lat_lost_win + lat_lost_post;
	int usr2_missing = lat_anchor_usr2 && !lat_t_usr2;
	if (lat_anchor_usr2)
		lat_t_warm_end = lat_t_usr2 ? lat_t_usr2 + lat_warmup_ns : 0;
	static const unsigned qs[] = {500, 900, 990, 999};
	static const char *qn[] = {"p50", "p90", "p99", "p999"};
	fprintf(stderr, "SSLSNIFF_LAT {\"unit\":\"ns\",\"clock\":\"CLOCK_MONOTONIC\",\"sub_bits\":%d,"
		"\"anchor\":\"%s\",\"usr2_missing\":%s,\"warmup_ns\":%llu,\"t_consume\":%llu,\"t_usr2\":%llu,\"t_warm_end\":%llu,\"t_stop\":%llu,"
		"\"count\":%llu,\"sum\":%llu,\"min\":%llu,\"max\":%llu,\"mean\":%.1f,",
		LAT_SUB_BITS, lat_anchor_usr2 ? "usr2" : "consume", usr2_missing ? "true" : "false",
		lat_warmup_ns, lat_t_consume, (unsigned long long)lat_t_usr2, lat_t_warm_end, (unsigned long long)lat_t_stop,
		lat_count, lat_sum, lat_count ? lat_min : 0ULL, lat_max,
		lat_count ? (double)lat_sum / (double)lat_count : 0.0);
	/* null: no events (plain quantiles) or the rank falls among the lost events (+inf). */
	for (int k = 0; k < 8; k++) {
		long long v = lat_quantile(qs[k % 4], k < 4 ? 0 : lost_inf);
		if (v < 0)
			fprintf(stderr, "\"%s%s\":null,", qn[k % 4], k < 4 ? "" : "_lost_inf");
		else
			fprintf(stderr, "\"%s%s\":%lld,", qn[k % 4], k < 4 ? "" : "_lost_inf", v);
	}
	fprintf(stderr, "\"lost\":%llu,\"lost_pre\":%llu,\"lost_win\":%llu,\"lost_post\":%llu,"
		"\"pre_ev\":%llu,\"post_ev\":%llu,\"late_ev\":%llu,\"negative\":%llu,\"overflow\":%llu,"
		"\"buckets\":[",
		cnt_lost, lat_lost_pre, lat_lost_win, lat_lost_post, lat_pre_ev, lat_post_ev,
		lat_late_ev, lat_negative, lat_overflow);
	int first = 1;
	for (unsigned i = 0; i < LAT_NBUCKETS; i++) {
		if (!lat_hist[i])
			continue;
		fprintf(stderr, "%s[%llu,%llu]", first ? "" : ",", lat_lo(i), lat_hist[i]);
		first = 0;
	}
	fprintf(stderr, "]}\n");
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt) {
	cnt_lost += lost_cnt;
	{
		/* A loss carries no timestamp: classified by the time it is reported. */
		unsigned long long now = lat_now(), stop = lat_t_stop, u2 = lat_t_usr2;
		if (lat_anchor_usr2)
			lat_t_warm_end = u2 ? u2 + lat_warmup_ns : ~0ULL;
		if (!lat_t_consume || now < lat_t_warm_end)
			lat_lost_pre += lost_cnt;
		else if (stop && now >= stop)
			lat_lost_post += lost_cnt;
		else
			lat_lost_win += lost_cnt;
	}
	warn("lost %llu events on CPU #%d\n", lost_cnt, cpu);
}

static void sig_int(int signo) { 
	exiting = 1;
}

bool ssllib_type(const char *input_str, char *lib_type, size_t lib_type_size, char *lib_path, size_t lib_path_size) {
	const char *valid_types[] = {"openssl", "gnutls", "nss"};
	size_t valid_types_count = sizeof(valid_types) / sizeof(valid_types[0]);
	bool valid_type_found = false;

	// Temporary buffers
	char lib_type_temp[10];
	char lib_path_temp[256];

	sscanf(input_str, "%9[^:]:%255s", lib_type_temp, lib_path_temp);

	if (lib_type_temp[0] == '\0' || lib_path_temp[0] == '\0') {
		fprintf(stderr, "Invalid SSL library param: %s\n", input_str);
		return false;
	}

	// Check if lib_type is in valid_types
	for (size_t i = 0; i < valid_types_count; i++) {
		if (strcmp(lib_type_temp, valid_types[i]) == 0) {
			valid_type_found = true;
			break;
		}
	}

	if (!valid_type_found) {
		fprintf(stderr, "Invalid SSL library type: %s\n", lib_type_temp);
		return false;
	}

	// Check if lib_path is a valid file
	FILE *f = fopen(lib_path_temp, "r");
	if (f == NULL) {
		fprintf(stderr, "Invalid library path: %s\n", lib_path_temp);
		return false;
	}
	fclose(f);

	strncpy(lib_type, lib_type_temp, lib_type_size);
	strncpy(lib_path, lib_path_temp, lib_path_size);

	return true;
}

int attach_openssl(struct sslsniff_bpf *skel, const char *lib) {
	ATTACH_UPROBE_CHECKED(skel, lib, SSL_write, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, SSL_write, probe_SSL_write_exit);
	ATTACH_UPROBE_CHECKED(skel, lib, SSL_read, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, SSL_read, probe_SSL_read_exit);

	if (env.latency && env.handshake) {
		ATTACH_UPROBE_CHECKED(skel, lib, SSL_do_handshake,
							probe_SSL_do_handshake_enter);
		ATTACH_URETPROBE_CHECKED(skel, lib, SSL_do_handshake,
								probe_SSL_do_handshake_exit);
	}

	return 0;
}

int attach_gnutls(struct sslsniff_bpf *skel, const char *lib) {
	ATTACH_UPROBE_CHECKED(skel, lib, gnutls_record_send, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, gnutls_record_send, probe_SSL_write_exit);
	ATTACH_UPROBE_CHECKED(skel, lib, gnutls_record_recv, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, gnutls_record_recv, probe_SSL_read_exit);

	return 0;
}

int attach_nss(struct sslsniff_bpf *skel, const char *lib) {
	ATTACH_UPROBE_CHECKED(skel, lib, PR_Write, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, PR_Write, probe_SSL_write_exit);
	ATTACH_UPROBE_CHECKED(skel, lib, PR_Send, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, PR_Send, probe_SSL_write_exit);
	ATTACH_UPROBE_CHECKED(skel, lib, PR_Read, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, PR_Read, probe_SSL_read_exit);
	ATTACH_UPROBE_CHECKED(skel, lib, PR_Recv, probe_SSL_rw_enter);
	ATTACH_URETPROBE_CHECKED(skel, lib, PR_Recv, probe_SSL_read_exit);

	return 0;
}

/*
 * Find the path of a library using ldconfig.
 */
char *find_library_path(const char *libname) {
	char cmd[128];
	static char path[512];
	FILE *fp;

	// Construct the ldconfig command with grep
	snprintf(cmd, sizeof(cmd), "ldconfig -p | grep %s", libname);

	// Execute the command and read the output
	fp = popen(cmd, "r");
	if (fp == NULL) {
		perror("Failed to run ldconfig");
		return NULL;
	}

	// Read the first line of output which should have the library path
	if (fgets(path, sizeof(path) - 1, fp) != NULL) {
		// Extract the path from the ldconfig output
		char *start = strrchr(path, '>');
		if (start && *(start + 1) == ' ') {
			memmove(path, start + 2, strlen(start + 2) + 1);
			char *end = strchr(path, '\n');
			if (end) {
				*end = '\0';  // Null-terminate the path
			}
			pclose(fp);
			return path;
		}
	}

	pclose(fp);
	return NULL;
}

void buf_to_hex(const uint8_t *buf, size_t len, char *hex_str) {
	for (size_t i = 0; i < len; i++) {
		sprintf(hex_str + 2 * i, "%02x", buf[i]);
	}
}

// Function to print the event from the perf buffer
void print_event(struct probe_SSL_data_t *event, const char *evt) {
	static unsigned long long start =
		0;  // Use static to retain value across function calls
	char buf[MAX_BUF_SIZE + 1] = {0};  // +1 for null terminator
	unsigned int buf_size;

	if (event->len <= MAX_BUF_SIZE) {
		buf_size = event->len;
	} else {
		buf_size = MAX_BUF_SIZE;
	}

	if (event->buf_filled == 1) {
		memcpy(buf, event->buf, buf_size);
	} else {
		buf_size = 0;
	}

	if (env.comm && strcmp(env.comm, event->comm) != 0) {
		return;
	}

	if (start == 0) {
		start = event->timestamp_ns;
	}
	double time_s = (double)(event->timestamp_ns - start) / 1000000000;

	char lat_str[10];
	if (event->delta_ns) {
		snprintf(lat_str, sizeof(lat_str), "%.3f",
				(double)event->delta_ns / 1000000);
	} else {
		strncpy(lat_str, "N/A", sizeof(lat_str));
	}

	char s_mark[] = "----- DATA -----";
	char e_mark[64] = "----- END DATA -----";
	if (buf_size < event->len) {
		snprintf(e_mark, sizeof(e_mark),
				"----- END DATA (TRUNCATED, %d bytes lost) -----",
				event->len - buf_size);
	}

	char *rw_event[] = {
		"READ/RECV",
		"WRITE/SEND",
		"HANDSHAKE"
	};

#define BASE_FMT "%-12s %-18.9f %-16s %-7d %-6d"
#define EXTRA_FMT " %-7d %-7d"
#define LATENCY_FMT " %-7s"

	if (env.extra && env.latency) {
		printf(BASE_FMT EXTRA_FMT LATENCY_FMT, rw_event[event->rw], 
			time_s, event->comm, event->pid,
			event->len, event->uid, event->tid, lat_str);
	} else if (env.extra) {
		printf(BASE_FMT EXTRA_FMT, rw_event[event->rw], time_s, event->comm, event->pid,
			event->len, event->uid, event->tid);
	} else if (env.latency) {
		printf(BASE_FMT LATENCY_FMT, rw_event[event->rw], time_s, event->comm, event->pid,
			event->len, lat_str);
	} else {
		printf(BASE_FMT, rw_event[event->rw], time_s, event->comm, event->pid,
			event->len);
	}

	if (buf_size != 0) {
		if (env.hexdump) {
			// 2 characters for each byte + null terminator
			char hex_data[MAX_BUF_SIZE * 2 + 1] = {0};  
			buf_to_hex((uint8_t *)buf, buf_size, hex_data);
			
			printf("\n%s\n", s_mark);
			for (size_t i = 0; i < strlen(hex_data); i += 32) {
				printf("%.32s\n", hex_data + i);
			}
			printf("%s\n\n", e_mark);
		} else {
			printf("\n%s\n%s\n%s\n\n", s_mark, buf, e_mark);
		}
	}
	fflush(stdout);
}

static void handle_event(void *ctx, int cpu, void *data, __u32 data_size) {
	struct probe_SSL_data_t *e = data;
	if (e->rw <= 2) {
		cnt_ev[e->rw]++;
		cnt_bytes[e->rw] += e->len;
	}
	lat_record(e);
	if (e->is_handshake) {
		print_event(e, "perf_SSL_do_handshake");
	} else {
		print_event(e, "perf_SSL_rw");
	}
	fflush(stdout);
}

int main(int argc, char **argv) {
	LIBBPF_OPTS(bpf_object_open_opts, open_opts);
	struct sslsniff_bpf *obj = NULL;
	struct perf_buffer *pb = NULL;
	int err;

	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;

	libbpf_set_print(libbpf_print_fn);

	/* H2: install SIGUSR1/SIGUSR2 first (their default action terminates the process), so a
	 * slow BPF load/attach cannot be killed by the harness's load-start / stop signals. */
	if (signal(SIGUSR1, sig_usr1) == SIG_ERR || signal(SIGUSR2, sig_usr2) == SIG_ERR) {
		warn("can't set SIGUSR1/SIGUSR2 handler: %s\n", strerror(errno));
		return 1;
	}
	{
		const char *wu = getenv("SSLSNIFF_LAT_WARMUP_MS");
		const char *an = getenv("SSLSNIFF_LAT_ANCHOR");
		if (wu)
			lat_warmup_ns = strtoull(wu, NULL, 10) * 1000000ULL;
		if (an && strcmp(an, "usr2") == 0)
			lat_anchor_usr2 = 1;
		else if (an && strcmp(an, "consume") != 0) {
			warn("SSLSNIFF_LAT_ANCHOR must be consume or usr2, got %s\n", an);
			return 1;
		}
	}

	obj = sslsniff_bpf__open_opts(&open_opts);
	if (!obj) {
		warn("failed to open BPF object\n");
		goto cleanup;
	}

	// obj->rodata->targ_uid = env.uid;
	// obj->rodata->targ_pid = env.pid == INVALID_PID ? 0 : env.pid;

	int stats_fd = -1;
	if (getenv("SSLSNIFF_BPF_STATS")) {
		stats_fd = bpf_enable_stats(BPF_STATS_RUN_TIME);
		if (stats_fd < 0)
			warn("bpf_enable_stats failed: %d\n", stats_fd);
	}

	err = sslsniff_bpf__load(obj);
	if (err) {
		warn("failed to load BPF object: %d\n", err);
		goto cleanup;
	}

	if (env.openssl) {
		char *openssl_path = find_library_path("libssl.so");
		printf("OpenSSL path: %s\n", openssl_path);
		attach_openssl(obj, openssl_path);
	}
	if (env.gnutls) {
		char *gnutls_path = find_library_path("libgnutls.so");
		printf("GnuTLS path: %s\n", gnutls_path);
		attach_gnutls(obj, gnutls_path);
	}
	if (env.nss) {
		char *nss_path = find_library_path("libnspr4.so");
		printf("NSS path: %s\n", nss_path);
		attach_nss(obj, nss_path);
	}
	if (env.extra_lib) {
		char lib_type[10];
		char lib_path[256];
		if (ssllib_type(env.extra_lib, lib_type, sizeof(lib_type), lib_path, sizeof(lib_path))) {
			if (strcmp(lib_type, "openssl") == 0) {
				attach_openssl(obj, lib_path);
			} else if (strcmp(lib_type, "gnutls") == 0) {
				attach_gnutls(obj, lib_path);
			} else if (strcmp(lib_type, "nss") == 0) {
				attach_nss(obj, lib_path);
			} else {
				fprintf(stderr, "Unknown library type: %s\n", lib_type);
				goto cleanup;
			}
		} else {
			fprintf(stderr, "Failed to parse extra library option: %s\n", env.extra_lib);
			goto cleanup;
		}
	}

	{
		/* Paper E15: SSLSNIFF_PB_WAKEUP=N asks the kernel for a consumer wakeup every N samples
		 * (libbpf sets wakeup_events = sample_period); unset = libbpf default (every sample). */
		LIBBPF_OPTS(perf_buffer_opts, pb_opts);
		const char *wb = getenv("SSLSNIFF_PB_WAKEUP");
		if (wb && atoi(wb) > 1)
			pb_opts.sample_period = (__u32)atoi(wb);
		pb = perf_buffer__new(bpf_map__fd(obj->maps.perf_SSL_events),
							PERF_BUFFER_PAGES, handle_event, handle_lost_events,
							NULL, &pb_opts);
	}
	if (!pb) {
		err = -errno;
		warn("failed to open perf buffer: %d\n", err);
		goto cleanup;
	}

	if (signal(SIGINT, sig_int) == SIG_ERR) {
		warn("can't set signal handler: %s\n", strerror(errno));
		err = 1;
		goto cleanup;
	}

	// Print header
	printf("%-12s %-18s %-16s %-7s %-7s", "FUNC", "TIME(s)", "COMM", "PID",
			"LEN");
	if (env.extra) {
		printf(" %-7s %-7s", "UID", "TID");
	}
	if (env.latency) {
		printf(" %-7s", "LAT(ms)");
	}
	printf("\n");
	fflush(stdout);
	lat_t_consume = lat_now();
	lat_t_warm_end = lat_t_consume + lat_warmup_ns;
	while (!exiting) {
		err = perf_buffer__poll(pb, PERF_POLL_TIMEOUT_MS);
		if (err < 0 && err != -EINTR) {
			warn("error polling perf buffer: %s\n", strerror(-err));
			goto cleanup;
		}
		err = 0;
	}
	{
		unsigned long long before = cnt_ev[0] + cnt_ev[1] + cnt_ev[2];
		int cerr = perf_buffer__consume(pb);
		unsigned long long tail = cnt_ev[0] + cnt_ev[1] + cnt_ev[2] - before;
		fprintf(stderr,
			"SSLSNIFF_CNT {\"read_ev\":%llu,\"write_ev\":%llu,\"hs_ev\":%llu,"
			"\"read_bytes\":%llu,\"write_bytes\":%llu,\"lost\":%llu,"
			"\"tail_ev\":%llu,\"consume_err\":%d}\n",
			cnt_ev[0], cnt_ev[1], cnt_ev[2], cnt_bytes[0], cnt_bytes[1],
			cnt_lost, tail, cerr);
		lat_print();
		if (stats_fd >= 0) {
			struct bpf_program *prog;
			bpf_object__for_each_program(prog, obj->obj) {
				struct bpf_prog_info info = {};
				__u32 len = sizeof(info);
				if (bpf_prog_get_info_by_fd(bpf_program__fd(prog), &info, &len) == 0)
					fprintf(stderr,
						"SSLSNIFF_STATS {\"prog\":\"%s\",\"run_cnt\":%llu,\"run_time_ns\":%llu}\n",
						bpf_program__name(prog),
						(unsigned long long)info.run_cnt,
						(unsigned long long)info.run_time_ns);
			}
		}
		fflush(stderr);
	}

cleanup:
	perf_buffer__free(pb);
	sslsniff_bpf__destroy(obj);
	return err != 0;
}
