/* Deterministic syscall failures plus real-file positional I/O.
 * Licensed under GPL-2.0-or-later. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s (errno %d)\n", __FILE__, __LINE__, #expr, errno); \
	exit(1); } } while (0)

static int calls, interrupt_next, fail_call, failure, zero_call;
static size_t max_transfer;

static int injected_failure(void)
{
	++calls;
	if (interrupt_next) {
		interrupt_next = 0;
		errno = EINTR;
		return 1;
	}
	if (calls == fail_call) {
		errno = failure;
		return 1;
	}
	return 0;
}

static ssize_t test_pread(int fd, void *buf, size_t size, off_t offset)
{
	if (injected_failure()) return -1;
	if (calls == zero_call) return 0;
	if (max_transfer && size > max_transfer) size = max_transfer;
	return pread(fd, buf, size, offset);
}

static ssize_t test_pwrite(int fd, const void *buf, size_t size, off_t offset)
{
	if (injected_failure()) return -1;
	if (calls == zero_call) return 0;
	if (max_transfer && size > max_transfer) size = max_transfer;
	return pwrite(fd, buf, size, offset);
}

/* Compile the production implementation with syscall substitution only in
 * this test binary; the server has no fault-injection switches. */
#define pread test_pread
#define pwrite test_pwrite
#include "../storage_io.c"
#undef pread
#undef pwrite

static void reset(void)
{
	calls = interrupt_next = fail_call = failure = zero_call = 0;
	max_transfer = 0;
}

int main(void)
{
	char path[] = "/tmp/dataman-storage-XXXXXX";
	char buffer[16] = {0};
	int fd = mkstemp(path);
	CHECK(fd >= 0);
	CHECK(unlink(path) == 0);
	CHECK(write(fd, "abcdefghijklmnop", 16) == 16);
	CHECK(lseek(fd, 7, SEEK_SET) == 7);

	max_transfer = 2;
	interrupt_next = 1;
	CHECK(dm_storage_write_at(fd, "123456", 6, 3) == 0);
	CHECK(calls == 4);
	CHECK(lseek(fd, 0, SEEK_CUR) == 7);
	reset();
	max_transfer = 3;
	interrupt_next = 1;
	CHECK(dm_storage_read_at(fd, buffer, 16, 0) == 0);
	CHECK(memcmp(buffer, "abc123456jklmnop", 16) == 0);
	CHECK(lseek(fd, 0, SEEK_CUR) == 7);

	/* A partial write followed by disk-full must never become success. */
	reset();
	max_transfer = 2;
	fail_call = 2;
	failure = ENOSPC;
	CHECK(dm_storage_write_at(fd, "XXXXXX", 6, 0) == -1);
	CHECK(errno == ENOSPC && calls == 2);
	CHECK(pread(fd, buffer, 6, 0) == 6);
	CHECK(memcmp(buffer, "XXc123", 6) == 0);
	reset();
	zero_call = 1;
	CHECK(dm_storage_write_at(fd, "X", 1, 0) == -1);
	CHECK(errno == EIO && calls == 1);
	reset();
	fail_call = 1;
	failure = EIO;
	CHECK(dm_storage_read_at(fd, buffer, 1, 0) == -1 && errno == EIO);
	reset();
	CHECK(dm_storage_read_at(fd, buffer, 2, 15) == -1 && errno == EIO);
	CHECK(calls == 2);

	reset();
	CHECK(dm_storage_write_at(fd, buffer, 1, -1) == -1 && errno == EINVAL);
	CHECK(dm_storage_read_at(fd, buffer, 2, INT64_MAX) == -1 && errno == EINVAL);
	CHECK(dm_storage_write_at(fd, NULL, 1, 0) == -1 && errno == EINVAL);
	CHECK(dm_storage_write_at(fd, buffer, SIZE_MAX, 1) == -1 && errno == EINVAL);
	CHECK(dm_storage_read_at(fd, NULL, 0, 0) == 0);
	CHECK(dm_storage_write_at(fd, NULL, 0, 0) == 0);
	CHECK(calls == 0);
	CHECK(dm_storage_write_at(-1, buffer, 1, 0) == -1 && errno == EBADF);
	CHECK(close(fd) == 0);
	return 0;
}
