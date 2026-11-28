#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int lk(int fd, long long b, short t)
{
	struct flock fl;
	memset(&fl, 0, sizeof fl);
	fl.l_type = t;
	fl.l_whence = SEEK_SET;
	fl.l_start = b;
	fl.l_len = 1;
	return fcntl(fd, F_OFD_SETLK, &fl);
}
static void show(int fd, const char *what)
{
	char p[64], buf[4096];
	snprintf(p, sizeof p, "/proc/self/fdinfo/%d", fd);
	int f = open(p, O_RDONLY);
	ssize_t n = read(f, buf, sizeof buf - 1);
	close(f);
	buf[n > 0 ? n : 0] = 0;
	printf("== %s\n", what);
	for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n"))
		if (!strncmp(l, "lock:", 5))
			printf("%s\n", l);
}
int main(void)
{
	const char *path = "/dev/shm/bpftime-merge-check";
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	long long base = 1LL << 44;
	printf("slot 255: %d\n", lk(fd, base + 255, F_WRLCK));
	printf("claim 256: %d\n", lk(fd, base + 256, F_WRLCK));
	show(fd, "slot 255 + claim 256 on one description");
	lk(fd, base + 255, F_UNLCK);
	show(fd, "slot 255 released");
	printf("slot 10: %d\n", lk(fd, base + 10, F_WRLCK));
	show(fd, "slot 10 + claim 256");
	close(fd);
	unlink(path);
	return 0;
}
