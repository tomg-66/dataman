/* ***************************************************************
 *
 * PROCEDURE:	undo_journal.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:17:11 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * dataman transaction processing
 * Existing-file physical undo journal prototype. GPL-2.0-or-later.
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

#include "undo_journal.h"
#include "storage_io.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#define JOURNAL ".dataman-undo"
#define HEADER 56u
#define MAX_PATH 4095u
#define RECORD 72u
#define MAX_WRITE (1024u * 1024u)
#define MAX_JOURNAL (64u * 1024u * 1024u)
#define MAX_RECORDS 128u
#define UNDO 1u
#define RESOLVED 2u
#define BLOB 3u
#define BLOB_META 16u
#define BLOB_STREAM 4u
#define BLOB_CHUNK (64u * 1024u)

/* Test builds supply deterministic crash points, absent from production. */
#ifndef UNDO_POINT
#define UNDO_POINT(point) ((void)0)
#endif

typedef struct blob_name {
	struct blob_name *next;
	char *name;
} blob_name;

struct dm_undo {
	int dir, root, fd, state; /* 0 active, 1 abort-only, 2 recovery-required/finished */
	uint64_t sequence;
	int64_t end;
	blob_name *blobs;
};

typedef struct entry {
	struct entry *previous;
	int fd, parent, borrowed_fd;
	uint32_t type;
	char *name;
	uint64_t offset, size, stream;
	uint32_t length;
	unsigned char *data;
} entry;

static void put64(unsigned char *p, uint64_t value)
{
	unsigned i;
	for (i = 0; i < 8; ++i) {
		p[i] = value & 255;
		value >>= 8;
	}
}

static uint64_t get64(const unsigned char *p)
{
	uint64_t value = 0;
	int i;
	for (i = 7; i >= 0; --i)
		value = (value << 8) | p[i];
	return value;
}

static void put32(unsigned char *p, uint32_t value)
{
	unsigned i;
	for (i = 0; i < 4; ++i) {
		p[i] = value & 255;
		value >>= 8;
	}
}

static uint32_t get32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t crc(const unsigned char *p, size_t size)
{
	uint32_t value = UINT32_MAX;
	unsigned bit;
	while (size--) {
		value ^= *p++;
		for (bit = 0; bit < 8; ++bit)
			value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1)));
	}
	return ~value;
}

static int sync_fd(int fd)
{
	int result;
	do {
		result = fsync(fd);
	} while (result < 0 && errno == EINTR);
	return result;
}

static int invalid(void) { errno = EINVAL; return -1; }
static int corrupt(void) { errno = EIO; return -1; }

static int name_valid(const char *name)
{
	const char *part = name;
	size_t size = strlen(name);
	if (!size || size > MAX_PATH)
		return 0;
	while (*part) {
		const char *slash = strchr(part, '/');
		size_t length = slash ? (size_t)(slash - part) : strlen(part);
		if (!length || length > 255 || (length == 1 && part[0] == '.') ||
			(length == 2 && part[0] == '.' && part[1] == '.')) {
			return 0;
		}
		if (!slash) {
			return 1;
		}
		part = slash + 1;
	}
	return 0;
}

/* Walk each component with openat/O_NOFOLLOW, not just the final component. */
static int open_beneath(int root, const char *name, int flags)
{
	char *copy, *part, *slash;
	int dir, fd, saved;
	if (!name_valid(name))
		return invalid();
	copy = strdup(name);
	if (!copy)
		return -1;
	dir = fcntl(root, F_DUPFD_CLOEXEC, 0);
	if (dir < 0) {
		free(copy);
		return -1;
	}
	part = copy;
	while ((slash = strchr(part, '/')) != NULL) {
		*slash = 0;
		fd = openat(dir, part, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		saved = errno; close(dir); errno = saved;
		if (fd < 0) {
			free(copy);
			return -1;
		}
		dir = fd; part = slash + 1;
	}
	fd = openat(dir, part, flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
	saved = errno;
	close(dir);
	free(copy);
	errno = saved;
	return fd;
}

static int open_root(const char *path)
{
	int slash, fd, saved;
	if (path[0] != '/')
		return invalid();
	slash = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (slash < 0 || path[1] == 0)
		return slash;
	fd = open_beneath(slash, path + 1, O_RDONLY | O_DIRECTORY);
	saved = errno;
	close(slash);
	errno = saved;
	return fd;
}

static int regular(int fd, struct stat *st)
{
	if (fstat(fd, st) < 0)
		return -1;
	if (!S_ISREG(st->st_mode) || st->st_size < 0)
		return invalid();
	return 0;
}

static void free_entries(entry *e)
{
	int saved = errno;
	while (e) {
		entry *previous = e->previous;
		if (e->fd >= 0 && !e->borrowed_fd)
			close(e->fd);
		if (e->parent >= 0)
			close(e->parent);
		free(e->name);
		free(e->data);
		free(e);
		e = previous;
	}
	errno = saved;
}

void dm_undo_close(dm_undo *tx)
{
	int saved = errno;
	if (tx) {
		while (tx->blobs) {
			blob_name *next = tx->blobs->next;
			free(tx->blobs->name);
			free(tx->blobs);
			tx->blobs = next;
		}
		if (tx->fd >= 0)
			close(tx->fd);
		if (tx->root >= 0)
			close(tx->root);
		close(tx->dir);
		free(tx);
	}
	errno = saved;
}

static dm_undo *open_directory(const char *directory)
{
	dm_undo *tx = calloc(1, sizeof(*tx));
	if (!tx)
		return NULL;
	tx->fd = -1;
	tx->root = -1;
	tx->dir = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (tx->dir < 0) {
		free(tx);
		return NULL;
	}
	tx->end = HEADER;
	return tx;
}

int dm_undo_begin(const char *journal_directory, const char *database_root, dm_undo **out)
{
	unsigned char header[HEADER] = {0};
	struct stat st, root;
	char *path;
	size_t length;
	dm_undo *tx;
	if (!out || !journal_directory || !database_root)
		return invalid();
	*out = NULL;
	path = realpath(database_root, NULL);
	if (!path)
		return -1;
	length = strlen(path);
	if (length > MAX_PATH) {
		free(path);
		errno = ENAMETOOLONG;
		return -1;
	}
	tx = open_directory(journal_directory);
	if (!tx) {
		free(path);
		return -1;
	}
	tx->root = open_root(path);
	if (tx->root < 0 || fstat(tx->dir, &st) < 0 || fstat(tx->root, &root) < 0)
		goto fail;
	tx->fd = openat(tx->dir, JOURNAL, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (tx->fd < 0)
		goto fail;
	memcpy(header, "DMUNDO04", 8);
	put64(header + 8, st.st_dev);
	put64(header + 16, st.st_ino);
	put64(header + 24, root.st_dev);
	put64(header + 32, root.st_ino);
	put32(header + 40, length);
	put32(header + 44, crc((unsigned char *)path, length));
	put32(header + 48, crc(header, 48));

	if (dm_storage_write_at(tx->fd, header, HEADER, 0) < 0 ||
		dm_storage_write_at(tx->fd, path, length, HEADER) < 0 ||
		sync_fd(tx->fd) < 0 || sync_fd(tx->dir) < 0) {
		goto fail;
	}
	tx->end = HEADER + length;
	free(path);
	UNDO_POINT("begin");
	*out = tx;
	return 0;
fail:
	/* Never remove a journal after uncertain I/O. */
	free(path);
	dm_undo_close(tx);
	return -1;
}

static int append(dm_undo *tx, unsigned char *record, size_t size)
{
	put32(record + 60, crc(record + RECORD, size - RECORD));
	put32(record + 64, crc(record, 64));
	if (dm_storage_write_at(tx->fd, record, size, tx->end) < 0)
		return -1;
	UNDO_POINT("journal-written");
	if (sync_fd(tx->fd) < 0)
		return -1;
	tx->end += size;
	++tx->sequence;
	UNDO_POINT("journal-synced");
	return 0;
}

static int write_target(dm_undo *tx, const char *name, int expected_fd, int check_fd,
		const void *data, size_t length, int64_t offset)
{
	struct stat st, journal_st, expected;
	unsigned char *record = NULL;
	size_t before, size, namesize;
	uint64_t end;
	int fd = -1, result = -1, saved;

	if (!tx || tx->state)
		return invalid();
	/* Any rejected write leaves the transaction abort-only. */
	tx->state = 1;
	if (!name || !name_valid(name) || !strncmp(name, "blobs/", 6) || (!data && length) || offset < 0 ||
		length > MAX_WRITE || (uint64_t)offset > INT64_MAX - length) {
		return invalid();
	}
	end = (uint64_t)offset + length;
	if ((off_t)end < 0 || (uint64_t)(off_t)end != end)
		return invalid();
	if (!length && !check_fd) {
		tx->state = 0;
		return 0;
	}
	fd = open_beneath(tx->root, name, O_RDWR);
	if (fd < 0)
		goto done;
	if (regular(fd, &st) < 0 || fstat(tx->fd, &journal_st) < 0)
		goto done;
	if (st.st_dev == journal_st.st_dev && st.st_ino == journal_st.st_ino) {
		invalid();
		goto done;
	}
	if (check_fd) {
		int flags = fcntl(expected_fd, F_GETFL);
		if (flags < 0 || regular(expected_fd, &expected) < 0)
			goto done;
		if ((flags & O_ACCMODE) != O_RDWR || (flags & O_APPEND)) {
			invalid();
			goto done;
		}
		if (st.st_dev != expected.st_dev || st.st_ino != expected.st_ino) {
			errno = ESTALE;
			goto done;
		}
	}
	if (!length) {
		result = 0;
		goto done;
	}
	before = offset >= st.st_size ? 0 : ((uint64_t)(st.st_size - offset) < length ? (size_t)(st.st_size - offset) : length);
	namesize = strlen(name);
	size = RECORD + namesize + before;
	if ((uint64_t)tx->end > INT64_MAX - size - RECORD || tx->sequence >= UINT64_MAX - 1) {
		errno = EFBIG;
		goto done;
	}
	record = calloc(1, size);
	if (!record)
		goto done;

	memcpy(record, "DMUR", 4);
	put32(record + 4, UNDO);
	put64(record + 8, tx->sequence + 1);
	put32(record + 16, size);
	put32(record + 20, namesize);
	put64(record + 24, offset);
	put64(record + 32, st.st_size);
	put64(record + 40, st.st_dev);
	put64(record + 48, st.st_ino);
	put32(record + 56, before);
	memcpy(record + RECORD, name, namesize);

	if (dm_storage_read_at(fd, record + RECORD + namesize, before, offset) < 0 ||
		append(tx, record, size) < 0) {
		goto done;
	}
	if (dm_storage_write_at(fd, data, length, offset) < 0)
		goto done;
	UNDO_POINT("data-written");
	result = 0;
done:
	saved = errno;
	free(record);
	if (fd >= 0 && close(fd) < 0 && result == 0) {
		result = -1;
		saved = errno;
	}
	if (!result)
		tx->state = 0;
	errno = saved;
	return result;
}

int dm_undo_write(dm_undo *tx, const char *name, const void *data,
		size_t length, int64_t offset)
{
	return write_target(tx, name, -1, 0, data, length, offset);
}

int dm_undo_write_fd(dm_undo *tx, const char *name, int fd, const void *data,
		size_t length, int64_t offset)
{
	return write_target(tx, name, fd, 1, data, length, offset);
}

/* Blobs are private regular files directly under root/blobs. Namespace undo
 * restores logical contents/existence, not inode numbers. Hardlinks are rejected.
 * Ordinary byte undo cannot target blobs, so replay never holds a descriptor to
 * a blob that a later namespace entry removes or recreates. */
static int blob_valid(const char *name)
{
	return name && name_valid(name) && !strncmp(name, "blobs/", 6) &&
		!strchr(name + 6, '/');
}

static int blob_current(int parent, const char *name, int *fd, struct stat *st)
{
	*fd = openat(parent, name, O_RDWR | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
	if (*fd < 0)
		return errno == ENOENT ? 0 : -1;
	if (regular(*fd, st) < 0 || st->st_nlink != 1)
		return invalid();
	return 1;
}

/* This prototype restores basic metadata only. Reject extended metadata before
 * mutation rather than losing it, including directory default ACL inheritance. */
static int basic_metadata(int fd)
{
	ssize_t size = flistxattr(fd, NULL, 0);
	if (size > 0) {
		errno = ENOTSUP;
		return -1;
	}
	if (size < 0 && errno != ENOTSUP && errno != EOPNOTSUPP)
		return -1;
	return 0;
}

static int blob_stream_size(uint64_t size, uint64_t *encoded)
{
	uint64_t chunks = size / BLOB_CHUNK + (size % BLOB_CHUNK != 0);
	if (size > INT64_MAX || chunks > ((uint64_t)INT64_MAX - size) / 8)
		return invalid();
	*encoded = size + chunks * 8;
	return 0;
}

/* Validate or copy a chunk stream using bounded memory. All chunks are checked
 * during scan before any replay; copy rechecks them while restoring. */
static int blob_stream(int journal, uint64_t at, uint64_t size, int output)
{
	unsigned char buffer[BLOB_CHUNK], header[8];
	uint64_t offset = 0;

	while (offset < size) {
		uint32_t length = size - offset > BLOB_CHUNK ? BLOB_CHUNK : size - offset;
		if (dm_storage_read_at(journal, header, sizeof(header), at) < 0 ||
			get32(header) != length || dm_storage_read_at(journal, buffer, length, at + 8) < 0 ||
			get32(header + 4) != crc(buffer, length)) {
			return corrupt();
		}
		if (output >= 0) {
			if (dm_storage_write_at(output, buffer, length, offset) < 0)
				return -1;
			UNDO_POINT("blob-undo-chunk");
		}
		at += length + 8; offset += length;
	}
	return 0;
}

static int append_blob(dm_undo *tx, unsigned char *record, size_t size, int fd, uint64_t length)
{
	unsigned char buffer[BLOB_CHUNK], header[8];
	uint64_t encoded, offset = 0, at = tx->end + size;

	if (blob_stream_size(length, &encoded) < 0 ||
		(uint64_t)tx->end > (uint64_t)INT64_MAX - size - RECORD ||
		encoded > (uint64_t)INT64_MAX - tx->end - size - RECORD) {
		return invalid();
	}

	put32(record + 60, crc(record + RECORD, size - RECORD));
	put32(record + 64, crc(record, 64));

	if (dm_storage_write_at(tx->fd, record, size, tx->end) < 0)
		return -1;

	while (offset < length) {
		uint32_t count = length - offset > BLOB_CHUNK ? BLOB_CHUNK : length - offset;
		if (dm_storage_read_at(fd, buffer, count, offset) < 0)
			return -1;
		put32(header, count); put32(header + 4, crc(buffer, count));
		if (dm_storage_write_at(tx->fd, header, sizeof(header), at) < 0 ||
			dm_storage_write_at(tx->fd, buffer, count, at + 8) < 0) {
			return -1;
		}
		offset += count; at += count + 8;
		UNDO_POINT("blob-snapshot-chunk");
	}
	UNDO_POINT("journal-written");
	if (sync_fd(tx->fd) < 0)
		return -1;
	tx->end = at; tx->sequence++;
	UNDO_POINT("journal-synced");
	return 0;
}

static int snapshot_blob(dm_undo *tx, const char *name)
{
	struct stat parent_st, st;
	int parent = -1, fd = -1, present, result = -1, saved;
	unsigned char *record = NULL;
	blob_name *item = NULL;

	if (!blob_valid(name))
		return invalid();
	for (blob_name *b = tx->blobs; b; b = b->next) {
		if (!strcmp(b->name, name))
			return 0;
	}

	parent = open_beneath(tx->root, "blobs", O_RDONLY | O_DIRECTORY);
	if (parent < 0 || fstat(parent, &parent_st) < 0 || basic_metadata(parent) < 0)
		goto done;
	present = blob_current(parent, name + 6, &fd, &st);
	if (present < 0 || (present && basic_metadata(fd) < 0))
		goto done;

	size_t length = present ? BLOB_META : 0;
	size_t namesize = strlen(name), size = RECORD + namesize + length;

	if (tx->sequence >= UINT64_MAX - 1 || (uint64_t)tx->end > INT64_MAX - size - RECORD) {
		errno = EFBIG;
		goto done;
	}
	item = calloc(1, sizeof(*item));
	if (!item || !(item->name = strdup(name)))
		goto done;
	record = calloc(1, size);
	if (!record)
		goto done;

	memcpy(record, "DMUR", 4); put32(record + 4, BLOB_STREAM);
	put64(record + 8, tx->sequence + 1); put32(record + 16, size);
	put32(record + 20, namesize); put64(record + 24, present);
	put64(record + 32, present ? st.st_size : 0);
	put64(record + 40, parent_st.st_dev); put64(record + 48, parent_st.st_ino);
	put32(record + 56, length); memcpy(record + RECORD, name, namesize);

	if (present) {
		unsigned char *meta = record + RECORD + namesize;
		put32(meta, st.st_mode & 07777);
		put32(meta + 4, st.st_uid);
		put32(meta + 8, st.st_gid);
	}
	if (append_blob(tx, record, size, fd, present ? st.st_size : 0) < 0)
		goto done;
	item->next = tx->blobs;
	tx->blobs = item;
	item = NULL;
	result = 0;

done:
	saved = errno;
	if (fd >= 0)
		close(fd);
	if (parent >= 0)
		close(parent);
	if (item) {
		free(item->name);
		free(item);
	}
	free(record);
	errno = saved;
	return result;
}

int dm_undo_blob(dm_undo *tx, int operation, const char *name, const char *dest,
		const void *data, size_t length)
{
	int parent = -1, fd = -1, result = -1, saved;
	struct stat st;

	if (!tx || tx->state)
		return invalid();
	tx->state = 1;
	if (!blob_valid(name) || (operation != DM_BLOB_REPLACE &&
		operation != DM_BLOB_REMOVE && operation != DM_BLOB_RENAME) ||
		(operation == DM_BLOB_REPLACE && ((!data && length) || (uintmax_t)length > INT64_MAX || (off_t)length < 0 || (uintmax_t)(off_t)length != length)) ||
		(operation == DM_BLOB_RENAME && !blob_valid(dest))) {
		return invalid();
	}
	if (snapshot_blob(tx, name) < 0 || (operation == DM_BLOB_RENAME && snapshot_blob(tx, dest) < 0)) {
		goto done;
	}
	parent = open_beneath(tx->root, "blobs", O_RDONLY | O_DIRECTORY);

	if (parent < 0)
		goto done;
	/* Recheck types even on repeated operations, whose snapshots already exist. */
	if (blob_current(parent, name + 6, &fd, &st) < 0)
		goto done;
	if (operation == DM_BLOB_REPLACE) {
		if (fd < 0) {
			fd = openat(parent, name + 6, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666);
			if (fd < 0)
				goto done;
			UNDO_POINT("blob-created");
		}
		if (dm_storage_write_at(fd, data, length, 0) < 0)
			goto done;
		UNDO_POINT("blob-written");
		if (ftruncate(fd, length) < 0)
			goto done;
	} else if (operation == DM_BLOB_REMOVE) {
		if (unlinkat(parent, name + 6, 0) < 0)
			goto done;
	} else {
		int target = -1;
		int status = blob_current(parent, dest + 6, &target, &st);
		if (target >= 0)
			close(target);
		if (status < 0 || renameat(parent, name + 6, parent, dest + 6) < 0)
			goto done;
	}
	UNDO_POINT("blob-mutated");
	result = 0;
done:
	saved = errno;
	if (fd >= 0 && close(fd) < 0 && !result) {
		result = -1;
		saved = errno;
	}
	if (parent >= 0)
		close(parent);
	if (!result)
		tx->state = 0;
	errno = saved;
	return result;
}

/*
 * Validate the ENTIRE log and all target identities before touching any data.
 * Incomplete final framing is ignored; complete invalid records fail closed.
 * A valid RESOLVED record must be the last record, with no trailing bytes.
 */
static int scan(dm_undo *tx, entry **entries, int *resolved)
{
	unsigned char header[HEADER], fixed[RECORD], *record = NULL;
	char path[MAX_PATH + 1];
	uint32_t path_length;
	struct stat st, journal_dir, root, target;
	uint64_t seq = 0;
	int64_t at = HEADER;
	int result = -1, rootfd;

	*entries = NULL;
	*resolved = 0;

	if (regular(tx->fd, &st) < 0 || fstat(tx->dir, &journal_dir) < 0)
		return -1;
	if (st.st_size < HEADER)
		return corrupt();
	if (dm_storage_read_at(tx->fd, header, HEADER, 0) < 0)
		return -1;
	if ((memcmp(header, "DMUNDO02", 8) && memcmp(header, "DMUNDO03", 8) && memcmp(header, "DMUNDO04", 8)) || get64(header + 8) != (uint64_t)journal_dir.st_dev ||
		get64(header + 16) != (uint64_t)journal_dir.st_ino ||
		get32(header + 48) != crc(header, 48) || get32(header + 52)) {
		return corrupt();
	}
	int version4 = !memcmp(header, "DMUNDO04", 8);

	if (!version4 && st.st_size > MAX_JOURNAL)
		return corrupt();
	path_length = get32(header + 40);
	if (!path_length || path_length > MAX_PATH || st.st_size < HEADER + path_length)
		return corrupt();
	if (dm_storage_read_at(tx->fd, path, path_length, HEADER) < 0)
		return -1;
	path[path_length] = 0;
	if (strlen(path) != path_length || path[0] != '/' ||
		get32(header + 44) != crc((unsigned char *)path, path_length)) {
		return corrupt();
	}
	rootfd = open_root(path);
	if (rootfd < 0)
		return -1;
	if (fstat(rootfd, &root) < 0) {
		int saved = errno;
		close(rootfd);
		errno = saved;
		return -1;
	}
	if (get64(header + 24) != (uint64_t)root.st_dev ||
		get64(header + 32) != (uint64_t)root.st_ino) {
		close(rootfd);
		return corrupt();
	}
	if (tx->root >= 0)
		close(tx->root);
	tx->root = rootfd;
	at += path_length;
	while (at < st.st_size) {
		uint32_t size, namesize, length, checksum, type;
		uint64_t extra = 0;
		char name[MAX_PATH + 1];
		entry *e;
		if (st.st_size - at < RECORD)
			break;
		if (dm_storage_read_at(tx->fd, fixed, RECORD, at) < 0)
			goto done;
		size = get32(fixed + 16); namesize = get32(fixed + 20);
		length = get32(fixed + 56); type = get32(fixed + 4);

		if (get32(fixed + 64) != crc(fixed, 64) || get32(fixed + 68) ||
			memcmp(fixed, "DMUR", 4) || get64(fixed + 8) != seq + 1 ||
			namesize > MAX_PATH || length > MAX_WRITE + (type == BLOB ? BLOB_META : 0) ||
			size != RECORD + namesize + length ||
			(type != UNDO && type != RESOLVED && type != BLOB && type != BLOB_STREAM) ||
			(type == BLOB && memcmp(header, "DMUNDO03", 8)) ||
			(type == BLOB_STREAM && !version4) ||
			(type != RESOLVED && !version4 && seq >= MAX_RECORDS) || seq == UINT64_MAX) {
			corrupt();
			goto done;
		}
		if (type == BLOB_STREAM && blob_stream_size(get64(fixed + 32), &extra) < 0) {
			corrupt();
			goto done;
		}
		if ((uint64_t)(st.st_size - at) < size || extra > (uint64_t)(st.st_size - at) - size)
			break;
		record = malloc(size);
		if (!record || dm_storage_read_at(tx->fd, record, size, at) < 0)
			goto done;
		checksum = get32(record + 60);
		if (checksum != crc(record + RECORD, size - RECORD)) {
			corrupt();
			goto done;
		}
		if (type == RESOLVED) {
			unsigned char zeros[36] = {0};
			if (size != RECORD || memcmp(record + 20, zeros, 36) ||
				get32(record + 56) || at + size != st.st_size) {
				corrupt();
				goto done;
			}
			*resolved = 1;
		} else if (type == BLOB || type == BLOB_STREAM) {
			memcpy(name, record + RECORD, namesize); name[namesize] = 0;
			uint64_t present = get64(record + 24), original_size = get64(record + 32);
			unsigned char *meta = record + RECORD + namesize;
			if (!blob_valid(name) || strlen(name) != namesize || present > 1 ||
				(type == BLOB && original_size > MAX_WRITE) || (present ? length != BLOB_META + (type == BLOB ? original_size : 0) :
				(length != 0 || original_size != 0)) ||
				(present && (get32(meta) > 07777 || get32(meta + 12)))) {
				corrupt();
				goto done;
			}
			for (entry *prior = *entries; prior; prior = prior->previous) {
				if ((prior->type == BLOB || prior->type == BLOB_STREAM) && !strcmp(prior->name, name + 6)) {
					corrupt();
					goto done;
				}
			}
			e = calloc(1, sizeof(*e));
			if (!e)
				goto done;
			e->fd = e->parent = -1; e->type = type;
			e->stream = at + size;
			if (type == BLOB_STREAM && blob_stream(tx->fd, e->stream, original_size, -1) < 0) {
				free(e);
				goto done;
			}
			e->previous = *entries;
			*entries = e;
			e->name = strdup(name + 6);
			if (!e->name)
				goto done;
			e->parent = open_beneath(tx->root, "blobs", O_RDONLY | O_DIRECTORY);
			if (e->parent < 0 || fstat(e->parent, &target) < 0)
				goto done;
			if ((uint64_t)target.st_dev != get64(record + 40) ||
				(uint64_t)target.st_ino != get64(record + 48)) {
				corrupt();
				goto done;
			}
			if (blob_current(e->parent, e->name, &e->fd, &target) < 0)
				goto done;
			e->data = record;
			record = NULL;
			e->offset = present;
			e->size = original_size;
			e->length = length;
		} else {
			memcpy(name, record + RECORD, namesize); name[namesize] = 0;
			if (!name_valid(name) || (memcmp(header, "DMUNDO02", 8) && !strncmp(name, "blobs/", 6)) || strlen(name) != namesize ||
				get64(record + 24) > INT64_MAX || get64(record + 32) > INT64_MAX ||
				(off_t)get64(record + 32) < 0 ||
				(uint64_t)(off_t)get64(record + 32) != get64(record + 32) ||
				(length && (get64(record + 24) > get64(record + 32) ||
				length > get64(record + 32) - get64(record + 24)))) {
				corrupt();
				goto done;
			}
			e = calloc(1, sizeof(*e));
			if (!e)
				goto done;
			e->parent = -1;
			e->type = UNDO;
			e->fd = open_beneath(tx->root, name, O_RDWR);
			e->previous = *entries;
			*entries = e;
			if (e->fd < 0 || regular(e->fd, &target) < 0)
				goto done;
			if ((uint64_t)target.st_dev != get64(record + 40) ||
				(uint64_t)target.st_ino != get64(record + 48) ||
				(target.st_dev == st.st_dev && target.st_ino == st.st_ino)) {
				corrupt();
				goto done;
			}
			/* Many page writes to one index must not consume one descriptor
			 * per undo record during commit or recovery. Validate each path,
			 * then share the older entry's descriptor until replay finishes. */
			for (entry *prior = e->previous; prior; prior = prior->previous) {
				if (prior->type == UNDO && get64(prior->data + 40) == (uint64_t)target.st_dev &&
						get64(prior->data + 48) == (uint64_t)target.st_ino) {
					close(e->fd);
					e->fd = prior->fd;
					e->borrowed_fd = 1;
					break;
				}
			}
			e->data = record;
			record = NULL;
			e->offset = get64(e->data + 24); e->size = get64(e->data + 32);
			e->length = length;
		}
		free(record);
		record = NULL;
		at += size + extra;
		++seq;
	}
	tx->end = at;
	tx->sequence = seq;
	result = 0;
done:
	free(record);
	return result;
}

static int retire(dm_undo *tx)
{
	if (unlinkat(tx->dir, JOURNAL, 0) < 0)
		return -1;
	UNDO_POINT("unlinked");
	return sync_fd(tx->dir);
}

static int resolve(dm_undo *tx)
{
	unsigned char record[RECORD] = {0};
	memcpy(record, "DMUR", 4);
	put32(record + 4, RESOLVED);
	put64(record + 8, tx->sequence + 1);
	put32(record + 16, RECORD);
	if (append(tx, record, RECORD) < 0)
		return -1;
	UNDO_POINT("resolved");
	return retire(tx);
}

static int restore_blob(dm_undo *tx, entry *e)
{
	struct stat st;
	if (!e->offset) {
		if (unlinkat(e->parent, e->name, 0) < 0 && errno != ENOENT)
			return -1;
	} else {
		if (e->fd >= 0 && !e->borrowed_fd)
			close(e->fd);
		e->fd = -1;
		int present = blob_current(e->parent, e->name, &e->fd, &st);
		if (present < 0)
			return -1;
		if (!present) {
			e->fd = openat(e->parent, e->name, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
			if (e->fd < 0)
				return -1;
			UNDO_POINT("blob-undo-created");
		}
		unsigned char *meta = e->data + RECORD + get32(e->data + 20);
		if (e->type == BLOB_STREAM) {
			if (blob_stream(tx->fd, e->stream, e->size, e->fd) < 0)
				return -1;
		} else if (dm_storage_write_at(e->fd, meta + BLOB_META, e->size, 0) < 0)
			return -1;
		UNDO_POINT("blob-undo-written");
		if (ftruncate(e->fd, e->size) < 0 || fstat(e->fd, &st) < 0)
			return -1;
		if ((st.st_uid != get32(meta + 4) || st.st_gid != get32(meta + 8)) &&
			fchown(e->fd, get32(meta + 4), get32(meta + 8)) < 0)
			return -1;
		if (fchmod(e->fd, get32(meta)) < 0 || sync_fd(e->fd) < 0)
			return -1;
	}
	UNDO_POINT("blob-undo-restored");
	return sync_fd(e->parent);
}

static int finish(dm_undo *tx, int undo)
{
	entry *entries = NULL, *e;
	int resolved, result = -1;
	if (scan(tx, &entries, &resolved) < 0)
		goto done;
	if (resolved) {
		result = retire(tx);
		goto done;
	}

	for (e = entries; e; e = e->previous) {
		if (e->type == BLOB || e->type == BLOB_STREAM) {
			if (undo) {
				if (restore_blob(tx, e) < 0)
					goto done;
			} else if ((e->fd >= 0 && sync_fd(e->fd) < 0) || sync_fd(e->parent) < 0)
				goto done;
			UNDO_POINT("blob-synced");
			continue;
		}
		if (undo) {
			if (dm_storage_write_at(e->fd, e->data + RECORD + get32(e->data + 20),
				e->length, e->offset) < 0 || ftruncate(e->fd, e->size) < 0)
				goto done;
			UNDO_POINT("undo-written");
		}
		if (sync_fd(e->fd) < 0)
			goto done;
		UNDO_POINT("data-synced");
	}

	/* Discard only a validated incomplete tail, after restoring/syncing data. */
	if (ftruncate(tx->fd, tx->end) < 0 || sync_fd(tx->fd) < 0)
		goto done;
	result = resolve(tx);
done:
	free_entries(entries);
	return result;
}

int dm_undo_commit(dm_undo *tx)
{
	if (!tx || tx->state)
		return invalid();
	tx->state = 2;
	return finish(tx, 0);
}

int dm_undo_abort(dm_undo *tx)
{
	if (!tx || tx->state == 2)
		return invalid();
	tx->state = 2;
	return finish(tx, 1);
}

int dm_undo_recover(const char *directory)
{
	dm_undo *tx;
	int result;

	if (!directory)
		return invalid();
	tx = open_directory(directory);
	if (!tx)
		return -1;
	tx->fd = openat(tx->dir, JOURNAL, O_RDWR | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
	if (tx->fd < 0)
		result = errno == ENOENT ? sync_fd(tx->dir) : -1;
	else
		result = finish(tx, 1);
	dm_undo_close(tx);
	return result;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
