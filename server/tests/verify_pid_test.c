/* Duplicate-start and descriptor inheritance regression. GPL-2.0-or-later. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(99); \
} } while (0)

extern int verify_pid(char *);
void err_sys(char *format, char *arg) { fprintf(stderr, format, arg); exit(98); }

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[1], "contend")) {
		CHECK(verify_pid(argv[2]) == -1);
		return 0;
	}
	char name[40], path[64];
	snprintf(name, sizeof(name), "dataman-pid-test-%ld", (long)getpid());
	snprintf(path, sizeof(path), "/tmp/.%s.pid", name);
	CHECK(verify_pid(name) == 0);
	struct stat original, status;
	CHECK(stat(path, &original) == 0);
	int found = 0;
	for (int fd = 3; fd < 256; ++fd)
		if (fstat(fd, &status) == 0 && status.st_dev == original.st_dev && status.st_ino == original.st_ino) {
			CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
			found = 1;
		}
	CHECK(found);
	pid_t child = fork(); CHECK(child >= 0);
	if (!child) { execl(argv[0], argv[0], "contend", name, (char *)NULL); _exit(99); }
	int result;
	CHECK(waitpid(child, &result, 0) == child && WIFEXITED(result) && WEXITSTATUS(result) == 0);
	CHECK(stat(path, &status) == 0 && status.st_ino == original.st_ino);
	return 0;
}
