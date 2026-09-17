/* Existing-file physical undo journal prototype. GPL-2.0-or-later. */
#include "undo_journal.h"
#include "storage_io.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

/* Test builds supply deterministic crash points, absent from production. */
#ifndef UNDO_POINT
#define UNDO_POINT(point) ((void)0)
#endif

struct dm_undo {
	int dir, root, fd, state; /* 0 active, 1 abort-only, 2 recovery-required/finished */
	uint64_t sequence;
	int64_t end;
};

typedef struct entry {
	struct entry *previous;
	int fd;
	uint64_t offset, size;
	uint32_t length;
	unsigned char *data;
} entry;

static void put64(unsigned char *p, uint64_t value)
{
	unsigned i;
	for (i = 0; i < 8; ++i) { p[i] = value & 255; value >>= 8; }
}

static uint64_t get64(const unsigned char *p)
{
	uint64_t value = 0;
	int i;
	for (i = 7; i >= 0; --i) value = (value << 8) | p[i];
	return value;
}

static void put32(unsigned char *p, uint32_t value)
{
	unsigned i;
	for (i = 0; i < 4; ++i) { p[i] = value & 255; value >>= 8; }
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
	do { result = fsync(fd); } while (result < 0 && errno == EINTR);
	return result;
}

static int invalid(void) { errno = EINVAL; return -1; }
static int corrupt(void) { errno = EIO; return -1; }

static int name_valid(const char *name)
{
	const char *part = name;
	size_t size = strlen(name);
	if (!size || size > MAX_PATH) return 0;
	while (*part) {
		const char *slash = strchr(part, '/');
		size_t length = slash ? (size_t)(slash - part) : strlen(part);
		if (!length || length > 255 || (length == 1 && part[0] == '.') ||
			(length == 2 && part[0] == '.' && part[1] == '.')) return 0;
		if (!slash) return 1;
		part = slash + 1;
	}
	return 0;
}

/* Walk each component with openat/O_NOFOLLOW, not just the final component. */
static int open_beneath(int root, const char *name, int flags)
{
	char *copy, *part, *slash;
	int dir, fd, saved;
	if (!name_valid(name)) return invalid();
	copy = strdup(name);
	if (!copy) return -1;
	dir = fcntl(root, F_DUPFD_CLOEXEC, 0);
	if (dir < 0) { free(copy); return -1; }
	part = copy;
	while ((slash = strchr(part, '/')) != NULL) {
		*slash = 0;
		fd = openat(dir, part, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		saved = errno; close(dir); errno = saved;
		if (fd < 0) { free(copy); return -1; }
		dir = fd; part = slash + 1;
	}
	fd = openat(dir, part, flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
	saved = errno; close(dir); free(copy); errno = saved;
	return fd;
}

static int open_root(const char *path)
{
	int slash, fd, saved;
	if (path[0] != '/') return invalid();
	slash = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (slash < 0 || path[1] == 0) return slash;
	fd = open_beneath(slash, path + 1, O_RDONLY | O_DIRECTORY);
	saved = errno; close(slash); errno = saved;
	return fd;
}

static int regular(int fd, struct stat *st)
{
	if (fstat(fd, st) < 0) return -1;
	if (!S_ISREG(st->st_mode) || st->st_size < 0) return invalid();
	return 0;
}

static void free_entries(entry *e)
{
	int saved = errno;
	while (e) {
		entry *previous = e->previous;
		close(e->fd);
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
		if (tx->fd >= 0) close(tx->fd);
		if (tx->root >= 0) close(tx->root);
		close(tx->dir); free(tx);
	}
	errno = saved;
}

static dm_undo *open_directory(const char *directory)
{
	dm_undo *tx = calloc(1, sizeof(*tx));
	if (!tx) return NULL;
	tx->fd = -1;
	tx->root = -1;
	tx->dir = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (tx->dir < 0) { free(tx); return NULL; }
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
	if (!out || !journal_directory || !database_root) return invalid();
	*out = NULL;
	path = realpath(database_root, NULL);
	if (!path) return -1;
	length = strlen(path);
	if (length > MAX_PATH) { free(path); errno = ENAMETOOLONG; return -1; }
	tx = open_directory(journal_directory);
	if (!tx) { free(path); return -1; }
	tx->root = open_root(path);
	if (tx->root < 0 || fstat(tx->dir, &st) < 0 || fstat(tx->root, &root) < 0) goto fail;
	tx->fd = openat(tx->dir, JOURNAL,
		O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (tx->fd < 0) goto fail;
	memcpy(header, "DMUNDO02", 8);
	put64(header + 8, st.st_dev);
	put64(header + 16, st.st_ino);
	put64(header + 24, root.st_dev);
	put64(header + 32, root.st_ino);
	put32(header + 40, length);
	put32(header + 44, crc((unsigned char *)path, length));
	put32(header + 48, crc(header, 48));
	if (dm_storage_write_at(tx->fd, header, HEADER, 0) < 0 ||
		dm_storage_write_at(tx->fd, path, length, HEADER) < 0 ||
		sync_fd(tx->fd) < 0 || sync_fd(tx->dir) < 0) goto fail;
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
	if (dm_storage_write_at(tx->fd, record, size, tx->end) < 0) return -1;
	UNDO_POINT("journal-written");
	if (sync_fd(tx->fd) < 0) return -1;
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
	if (!tx || tx->state) return invalid();
	/* Any rejected write leaves the transaction abort-only. */
	tx->state = 1;
	if (!name || !name_valid(name) || (!data && length) || offset < 0 ||
		length > MAX_WRITE || (uint64_t)offset > INT64_MAX - length)
		return invalid();
	end = (uint64_t)offset + length;
	if ((off_t)end < 0 || (uint64_t)(off_t)end != end) return invalid();
	if (!length && !check_fd) { tx->state = 0; return 0; }
	fd = open_beneath(tx->root, name, O_RDWR);
	if (fd < 0) goto done;
	if (regular(fd, &st) < 0 || fstat(tx->fd, &journal_st) < 0) goto done;
	if (st.st_dev == journal_st.st_dev && st.st_ino == journal_st.st_ino) {
		invalid(); goto done;
	}
	if (check_fd) {
		int flags = fcntl(expected_fd, F_GETFL);
		if (flags < 0 || regular(expected_fd, &expected) < 0) goto done;
		if ((flags & O_ACCMODE) != O_RDWR || (flags & O_APPEND)) {
			invalid(); goto done;
		}
		if (st.st_dev != expected.st_dev || st.st_ino != expected.st_ino) {
			errno = ESTALE; goto done;
		}
	}
	if (!length) { result = 0; goto done; }
	before = offset >= st.st_size ? 0 :
		((uint64_t)(st.st_size - offset) < length ? (size_t)(st.st_size - offset) : length);
	namesize = strlen(name);
	size = RECORD + namesize + before;
	if (tx->end + size + RECORD > MAX_JOURNAL || tx->sequence >= MAX_RECORDS) {
		errno = EFBIG; goto done;
	}
	record = calloc(1, size);
	if (!record) goto done;
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
		append(tx, record, size) < 0) goto done;
	if (dm_storage_write_at(fd, data, length, offset) < 0) goto done;
	UNDO_POINT("data-written");
	result = 0;
done:
	saved = errno;
	free(record);
	if (fd >= 0 && close(fd) < 0 && result == 0) { result = -1; saved = errno; }
	if (!result) tx->state = 0;
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

/* Validate the ENTIRE log and all target identities before touching any data.
 * Incomplete final framing is ignored; complete invalid records fail closed.
 * A valid RESOLVED record must be the last record, with no trailing bytes. */
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
	if (regular(tx->fd, &st) < 0 || fstat(tx->dir, &journal_dir) < 0) return -1;
	if (st.st_size < HEADER || st.st_size > MAX_JOURNAL) return corrupt();
	if (dm_storage_read_at(tx->fd, header, HEADER, 0) < 0) return -1;
	if (memcmp(header, "DMUNDO02", 8) || get64(header + 8) != (uint64_t)journal_dir.st_dev ||
		get64(header + 16) != (uint64_t)journal_dir.st_ino ||
		get32(header + 48) != crc(header, 48) || get32(header + 52)) return corrupt();
	path_length = get32(header + 40);
	if (!path_length || path_length > MAX_PATH || st.st_size < HEADER + path_length)
		return corrupt();
	if (dm_storage_read_at(tx->fd, path, path_length, HEADER) < 0) return -1;
	path[path_length] = 0;
	if (strlen(path) != path_length || path[0] != '/' ||
		get32(header + 44) != crc((unsigned char *)path, path_length)) return corrupt();
	rootfd = open_root(path);
	if (rootfd < 0) return -1;
	if (fstat(rootfd, &root) < 0) { int saved = errno; close(rootfd); errno = saved; return -1; }
	if (get64(header + 24) != (uint64_t)root.st_dev ||
		get64(header + 32) != (uint64_t)root.st_ino) { close(rootfd); return corrupt(); }
	if (tx->root >= 0) close(tx->root);
	tx->root = rootfd;
	at += path_length;
	while (at < st.st_size) {
		uint32_t size, namesize, length, checksum, type;
		char name[MAX_PATH + 1];
		entry *e;
		if (st.st_size - at < RECORD) break;
		if (dm_storage_read_at(tx->fd, fixed, RECORD, at) < 0) goto done;
		size = get32(fixed + 16); namesize = get32(fixed + 20);
		length = get32(fixed + 56); type = get32(fixed + 4);
		if (get32(fixed + 64) != crc(fixed, 64) || get32(fixed + 68) ||
			memcmp(fixed, "DMUR", 4) || get64(fixed + 8) != seq + 1 ||
			namesize > MAX_PATH || length > MAX_WRITE || size != RECORD + namesize + length ||
			(type != UNDO && type != RESOLVED) ||
			(type == UNDO && seq >= MAX_RECORDS)) { corrupt(); goto done; }
		if (st.st_size - at < size) break;
		record = malloc(size);
		if (!record || dm_storage_read_at(tx->fd, record, size, at) < 0) goto done;
		checksum = get32(record + 60);
		if (checksum != crc(record + RECORD, size - RECORD)) { corrupt(); goto done; }
		if (type == RESOLVED) {
			unsigned char zeros[36] = {0};
			if (size != RECORD || memcmp(record + 20, zeros, 36) ||
				get32(record + 56) || at + size != st.st_size) { corrupt(); goto done; }
			*resolved = 1;
		} else {
			memcpy(name, record + RECORD, namesize); name[namesize] = 0;
			if (!name_valid(name) || strlen(name) != namesize ||
				get64(record + 24) > INT64_MAX || get64(record + 32) > INT64_MAX ||
				(off_t)get64(record + 32) < 0 ||
				(uint64_t)(off_t)get64(record + 32) != get64(record + 32) ||
				(length && (get64(record + 24) > get64(record + 32) ||
				length > get64(record + 32) - get64(record + 24)))) { corrupt(); goto done; }
			e = calloc(1, sizeof(*e));
			if (!e) goto done;
			e->fd = open_beneath(tx->root, name, O_RDWR);
			e->previous = *entries; *entries = e;
			if (e->fd < 0 || regular(e->fd, &target) < 0) goto done;
			if ((uint64_t)target.st_dev != get64(record + 40) ||
				(uint64_t)target.st_ino != get64(record + 48) ||
				(target.st_dev == st.st_dev && target.st_ino == st.st_ino)) { corrupt(); goto done; }
			e->data = record; record = NULL;
			e->offset = get64(e->data + 24); e->size = get64(e->data + 32);
			e->length = length;
		}
		free(record); record = NULL;
		at += size; ++seq;
	}
	tx->end = at; tx->sequence = seq;
	result = 0;
done:
	free(record);
	return result;
}

static int retire(dm_undo *tx)
{
	if (unlinkat(tx->dir, JOURNAL, 0) < 0) return -1;
	UNDO_POINT("unlinked");
	return sync_fd(tx->dir);
}

static int resolve(dm_undo *tx)
{
	unsigned char record[RECORD] = {0};
	memcpy(record, "DMUR", 4); put32(record + 4, RESOLVED);
	put64(record + 8, tx->sequence + 1); put32(record + 16, RECORD);
	if (append(tx, record, RECORD) < 0) return -1;
	UNDO_POINT("resolved");
	return retire(tx);
}

static int finish(dm_undo *tx, int undo)
{
	entry *entries = NULL, *e;
	int resolved, result = -1;
	if (scan(tx, &entries, &resolved) < 0) goto done;
	if (resolved) { result = retire(tx); goto done; }
	for (e = entries; e; e = e->previous) {
		if (undo) {
			if (dm_storage_write_at(e->fd, e->data + RECORD + get32(e->data + 20),
				e->length, e->offset) < 0 || ftruncate(e->fd, e->size) < 0) goto done;
			UNDO_POINT("undo-written");
		}
		if (sync_fd(e->fd) < 0) goto done;
		UNDO_POINT("data-synced");
	}
	/* Discard only a validated incomplete tail, after restoring/syncing data. */
	if (ftruncate(tx->fd, tx->end) < 0 || sync_fd(tx->fd) < 0) goto done;
	result = resolve(tx);
done:
	free_entries(entries);
	return result;
}

int dm_undo_commit(dm_undo *tx)
{
	if (!tx || tx->state) return invalid();
	tx->state = 2;
	return finish(tx, 0);
}

int dm_undo_abort(dm_undo *tx)
{
	if (!tx || tx->state == 2) return invalid();
	tx->state = 2;
	return finish(tx, 1);
}

int dm_undo_recover(const char *directory)
{
	dm_undo *tx;
	int result;
	if (!directory) return invalid();
	tx = open_directory(directory);
	if (!tx) return -1;
	tx->fd = openat(tx->dir, JOURNAL, O_RDWR | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
	if (tx->fd < 0) result = errno == ENOENT ? sync_fd(tx->dir) : -1;
	else result = finish(tx, 1);
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
