/* Versioned connection greeting shared by the native clients and server.
 * GPL-2.0-or-later. This identifies a wire format, not an authenticated user. */
#ifndef DATAMAN_PROTOCOL_H
#define DATAMAN_PROTOCOL_H

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include "errors.h"

#define DM_PROTOCOL_VERSION 1
#define DM_PROTOCOL_HELLO "DMAN0001\n"
#define DM_PROTOCOL_SIZE 9
#define DM_PROTOCOL_TIMEOUT_MS 5000

static inline int64_t dm_protocol_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return -1;

	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* One deadline for the whole handshake, including interrupted/partial I/O. */
static inline int dm_protocol_io(int fd, void *buffer, size_t size,
		int writing, int64_t deadline)
{
	char *bytes = (char *)buffer;

	while (size) {
		int64_t now = dm_protocol_now();
		if (now < 0 || now >= deadline)
			return 0;
		struct pollfd ready = {fd, (short)(writing ? POLLOUT : POLLIN), 0};
		int result = poll(&ready, 1, (int)(deadline - now));
		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0)
			return 0;
		ssize_t count = writing ? send(fd, bytes, size, MSG_NOSIGNAL | MSG_DONTWAIT)
			: recv(fd, bytes, size, MSG_DONTWAIT);
		if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
			continue;
		if (count <= 0)
			return 0;
		bytes += count;
		size -= (size_t)count;
	}
	return 1;
}

static inline int dm_protocol_accept(int fd)
{
	char hello[DM_PROTOCOL_SIZE];
	int64_t deadline = dm_protocol_now() + DM_PROTOCOL_TIMEOUT_MS;

	if (dm_protocol_io(fd, hello, sizeof(hello), 0, deadline) &&
			!memcmp(hello, DM_PROTOCOL_HELLO, sizeof(hello)))
		return 1;
	/* Use an error understood by unversioned clients, whose error tables
	 * cannot safely display newly introduced error numbers. */
	char rejection[] = "-39\n"; /* EINVMSG */
	dm_protocol_io(fd, rejection, sizeof(rejection) - 1, 1,
		dm_protocol_now() + 100);
	return 0;
}

static inline int dm_protocol_connect(int fd)
{
	char hello[] = DM_PROTOCOL_HELLO;
	char response[DM_PROTOCOL_SIZE + 1] = {0};
	int64_t deadline = dm_protocol_now() + DM_PROTOCOL_TIMEOUT_MS;

	if (!dm_protocol_io(fd, hello, DM_PROTOCOL_SIZE, 1, deadline))
		return ENORESP;
	if (!dm_protocol_io(fd, response, 2, 0, deadline))
		return EPROTOCOL;
	if (response[0] == '-') {
		size_t used = 2;
		while (used < sizeof(response) - 1) {
			if (!dm_protocol_io(fd, response + used, 1, 0, deadline) ||
					response[used++] == '\n')
				break;
		}
		int error = atoi(response);
		return error < 0 && error >= EINREC && error != EINVMSG ? error : EPROTOCOL;
	}
	if (memcmp(response, hello, 2) ||
			!dm_protocol_io(fd, response + 2, DM_PROTOCOL_SIZE - 2, 0, deadline) ||
			memcmp(response, hello, DM_PROTOCOL_SIZE))
		return EPROTOCOL;
	return 0;
}
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
