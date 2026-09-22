/* ***************************************************************
 *
 * PROCEDURE:	storage_io.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:14:27 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * dataman transaction journal storage utilities.
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The GNU General Public License is contained in the file COPYING.
 */
/* Licensed under GPL-2.0-or-later. */
#include "storage_io.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

static dm_storage_router mutation_router;
static int (*check_namespace)(void);

void dm_storage_set_router(dm_storage_router router, int (*namespace_check)(void))
{
	mutation_router = router;
	check_namespace = namespace_check;
}

int dm_storage_namespace_check(void)
{
	return check_namespace ? check_namespace() : 0;
}

int dm_storage_mutate_at(int fd, const void *buffer, size_t length, int64_t offset)
{
	if (mutation_router)
		return mutation_router(fd, buffer, length, offset);
	return dm_storage_write_at(fd, buffer, length, offset);
}

static dm_blob_router blob_router;

void dm_storage_set_blob_router(dm_blob_router router)
{
	blob_router = router;
}

int dm_storage_blob_route(int operation, const char *path, const char *dest,
		const void *data, size_t length)
{
	return blob_router ? blob_router(operation, path, dest, data, length) : 0;
}

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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
