/* Licensed under GPL-2.0-or-later. */
#include "storage_io.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

static int valid_range(const void *buffer, size_t length, int64_t offset)
{
	uintmax_t end;
	if (offset < 0 || (!buffer && length) ||
			(uintmax_t)length > (uintmax_t)INT64_MAX - (uintmax_t)offset)
		goto invalid;
	end = (uintmax_t)offset + length;
	if ((off_t)end < 0 || (uintmax_t)(off_t)end != end)
		goto invalid;
	return 0;
invalid:
	errno = EINVAL;
	return -1;
}

int dm_storage_read_at(int fd, void *buffer, size_t length, int64_t offset)
{
	unsigned char *next = buffer;
	if (valid_range(buffer, length, offset) < 0)
		return -1;
	while (length) {
		size_t chunk = length > SSIZE_MAX ? SSIZE_MAX : length;
		ssize_t count = pread(fd, next, chunk, (off_t)offset);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (count == 0) {
			errno = EIO;
			return -1;
		}
		next += count;
		offset += count;
		length -= count;
	}
	return 0;
}

int dm_storage_write_at(int fd, const void *buffer, size_t length, int64_t offset)
{
	const unsigned char *next = buffer;
	if (valid_range(buffer, length, offset) < 0)
		return -1;
	while (length) {
		size_t chunk = length > SSIZE_MAX ? SSIZE_MAX : length;
		ssize_t count = pwrite(fd, next, chunk, (off_t)offset);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (count == 0) {
			errno = EIO;
			return -1;
		}
		next += count;
		offset += count;
		length -= count;
	}
	return 0;
}
