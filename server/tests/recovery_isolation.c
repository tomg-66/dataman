/* Test-only resource isolation for the real server executables.
 * Linked using --wrap; production binaries do not include this file.
 * GPL-2.0-or-later. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>

static long setting(const char *name)
{
	char *end;
	const char *text = getenv(name);
	long value = text ? strtol(text, &end, 10) : 0;
	if (!text || !*text || *end || value <= 0) {
		fprintf(stderr, "Missing/invalid recovery-test setting: %s\n", name);
		exit(99);
	}
	return value;
}

int __real_msgget(key_t, int);
int __wrap_msgget(key_t key, int flags)
{
	(void)key;
	/* The runner exclusively reserves this queue before starting us. */
	return __real_msgget((key_t)setting("DM_TEST_MSGKEY"), flags);
}

int __real_verify_pid(char *);
int __wrap_verify_pid(char *name)
{
	char isolated[40];
	(void)name;
	snprintf(isolated, sizeof(isolated), "dm-recovery-%ld", (long)getpid());
	return __real_verify_pid(isolated);
}

int __real_bind(int, const struct sockaddr *, socklen_t);
int __wrap_bind(int fd, const struct sockaddr *address, socklen_t length)
{
	struct sockaddr_in local;
	if (length != sizeof(local) || address->sa_family != AF_INET) {
		errno = EINVAL;
		return -1;
	}
	local = *(const struct sockaddr_in *)address;
	local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	local.sin_port = htons((unsigned short)setting("DM_TEST_PORT"));
	return __real_bind(fd, (const struct sockaddr *)&local, sizeof(local));
}

/* Never allow test connection-server shutdown to signal the installed server. */
void __wrap_term_dbserve(void) { }
