/* Real files and forked crash boundaries. GPL-2.0-or-later. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "storage_io.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s (errno %d)\n", __FILE__, __LINE__, #expr, errno); \
	exit(1); } } while (0)

static const char *crash_point;
static int crash_occurrence, sync_calls, fail_sync;
static int write_calls, fail_write;
static size_t partial_bytes;

static void point(const char *name)
{
	if (crash_point && !strcmp(crash_point, name) && --crash_occurrence == 0)
		_exit(77);
}

static int test_fsync(int fd)
{
	if (++sync_calls == fail_sync) { errno = EIO; return -1; }
	return fsync(fd);
}

static int test_write_at(int fd, const void *data, size_t size, int64_t offset)
{
	if (++write_calls == fail_write) {
		size_t partial = partial_bytes < size ? partial_bytes : size;
		if (partial && dm_storage_write_at(fd, data, partial, offset) < 0) return -1;
		errno = ENOSPC;
		return -1;
	}
	return dm_storage_write_at(fd, data, size, offset);
}

#define UNDO_POINT(name) point(name)
#define fsync test_fsync
#define dm_storage_write_at test_write_at
#include "../undo_journal.c"
#undef fsync
#undef dm_storage_write_at

static char directory[128], journal_directory[128];

static int file(const char *name, int flags)
{
	char path[256];
	CHECK(snprintf(path, sizeof(path), "%s/%s", (strcmp(name, JOURNAL) ? directory : journal_directory), name) < (int)sizeof(path));
	return open(path, flags, 0600);
}

static int64_t journal_start(void)
{
	unsigned char header[HEADER];
	int fd = file(JOURNAL, O_RDONLY);
	CHECK(fd >= 0 && read(fd, header, HEADER) == HEADER && close(fd) == 0);
	return HEADER + get32(header + 40);
}

static void setup(void)
{
	int fd;
	strcpy(directory, "/tmp/dataman-undo-test-XXXXXX");
	CHECK(mkdtemp(directory) != NULL);
	strcpy(journal_directory, "/tmp/dataman-journal-test-XXXXXX");
	CHECK(mkdtemp(journal_directory) != NULL);
	fd = file("a", O_CREAT | O_EXCL | O_RDWR);
	CHECK(fd >= 0 && write(fd, "abcdefgh", 8) == 8 && fsync(fd) == 0);
	CHECK(close(fd) == 0);
	fd = file("b", O_CREAT | O_EXCL | O_RDWR);
	CHECK(fd >= 0 && write(fd, "12345678", 8) == 8 && fsync(fd) == 0);
	CHECK(close(fd) == 0);
	sync_calls = fail_sync = 0;
	write_calls = fail_write = 0; partial_bytes = 0;
}

static void expect(const char *name, const void *data, size_t size)
{
	char buffer[64];
	struct stat st;
	int fd = file(name, O_RDONLY);
	CHECK(fd >= 0 && fstat(fd, &st) == 0 && st.st_size == (off_t)size);
	CHECK(read(fd, buffer, sizeof(buffer)) == (ssize_t)size);
	CHECK(memcmp(buffer, data, size) == 0);
	CHECK(close(fd) == 0);
}

static void original(void)
{
	expect("a", "abcdefgh", 8);
	expect("b", "12345678", 8);
}

static void changed(void)
{
	expect("a", "abXXYYgh\0\0\0\0tail", 16);
	expect("b", "ZZ345678", 8);
}

static void cleanup(void)
{
	int dir = open(directory, O_RDONLY | O_DIRECTORY);
	CHECK(dir >= 0);
	CHECK(unlinkat(dir, "a", 0) == 0);
	CHECK(unlinkat(dir, "b", 0) == 0);
	CHECK(file(JOURNAL, O_RDONLY) == -1 && errno == ENOENT);
	CHECK(close(dir) == 0 && rmdir(directory) == 0);
	CHECK(rmdir(journal_directory) == 0);
}

static void mutations(dm_undo *tx)
{
	CHECK(dm_undo_write(tx, "a", "XXXX", 4, 2) == 0);
	CHECK(dm_undo_write(tx, "a", "YY", 2, 4) == 0);
	CHECK(dm_undo_write(tx, "a", "tail", 4, 12) == 0);
	CHECK(dm_undo_write(tx, "b", "ZZ", 2, 0) == 0);
}

static void wait_crash(pid_t pid)
{
	int status;
	CHECK(waitpid(pid, &status, 0) == pid);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 77);
}

static void crash_transaction(const char *where, int occurrence, int committing)
{
	pid_t pid = fork();
	CHECK(pid >= 0);
	if (!pid) {
		dm_undo *tx;
		crash_point = where; crash_occurrence = occurrence;
		CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
		mutations(tx);
		if (committing) CHECK(dm_undo_commit(tx) == 0);
		else CHECK(dm_undo_abort(tx) == 0);
		_exit(1); /* Requested boundary must have been reached. */
	}
	wait_crash(pid);
}

static void crash_cases(void)
{
	static const struct { const char *point; int occurrence, committed; } cases[] = {
		{"begin", 1, 0}, {"journal-written", 1, 0}, {"journal-synced", 1, 0},
		{"data-written", 1, 0}, {"data-written", 2, 0}, {"data-written", 3, 0},
		{"data-written", 4, 0}, {"data-synced", 1, 0}, {"data-synced", 4, 0},
		/* The complete marker may survive even before its fsync/acknowledgment. */
		{"journal-written", 5, 1}, {"journal-synced", 5, 1},
		{"resolved", 1, 1}, {"unlinked", 1, 1}
	};
	size_t i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		setup();
		crash_transaction(cases[i].point, cases[i].occurrence, 1);
		CHECK(dm_undo_recover(journal_directory) == 0);
		if (cases[i].committed) changed(); else original();
		CHECK(dm_undo_recover(journal_directory) == 0); /* Repeat is harmless. */
		cleanup();
	}
	for (i = 1; i <= 4; ++i) {
		pid_t pid;
		setup();
		crash_transaction("data-written", 4, 1);
		pid = fork(); CHECK(pid >= 0);
		if (!pid) {
			crash_point = "undo-written"; crash_occurrence = (int)i;
			CHECK(dm_undo_recover(journal_directory) == 0); _exit(1);
		}
		wait_crash(pid);
		CHECK(dm_undo_recover(journal_directory) == 0);
		original(); cleanup();
	}
	setup();
	crash_transaction("resolved", 1, 0);
	CHECK(dm_undo_recover(journal_directory) == 0);
	original(); cleanup();
}

static void normal_cases(void)
{
	dm_undo *tx, *other;
	setup();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_begin(journal_directory, directory, &other) == -1 && errno == EEXIST);
	mutations(tx); changed();
	CHECK(dm_undo_abort(tx) == 0);
	CHECK(dm_undo_commit(tx) == -1);
	dm_undo_close(tx); original();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	mutations(tx);
	CHECK(dm_undo_commit(tx) == 0);
	dm_undo_close(tx);
	CHECK(dm_undo_recover(journal_directory) == 0);
	changed(); cleanup();
	setup();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write(tx, "../a", "X", 1, 0) == -1);
	CHECK(dm_undo_commit(tx) == -1);
	CHECK(dm_undo_abort(tx) == 0);
	dm_undo_close(tx); original(); cleanup();
	setup();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write(tx, "a", "WXYZ", 4, 6) == 0);
	expect("a", "abcdefWXYZ", 10);
	CHECK(dm_undo_write(tx, "a", NULL, 0, 100) == 0);
	expect("a", "abcdefWXYZ", 10);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
	original(); cleanup();
	setup();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write(tx, "a", "X", MAX_WRITE + 1, 0) == -1 && errno == EINVAL);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
	original(); cleanup();
}

static void damaged_cases(void)
{
	int fd;
	unsigned char byte;
	struct stat st;
	/* A partial final header follows a fully logged/applied transaction. */
	setup(); crash_transaction("data-written", 4, 1);
	fd = file(JOURNAL, O_RDWR | O_APPEND); CHECK(fd >= 0);
	CHECK(write(fd, "DMUR", 4) == 4 && fsync(fd) == 0 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original(); cleanup();
	/* An incomplete final payload: truncate the last before-image, before
	 * its target has been written (the journal-written boundary). */
	setup(); crash_transaction("journal-written", 4, 1);
	fd = file(JOURNAL, O_RDWR); CHECK(fd >= 0);
	CHECK(fstat(fd, &st) == 0 && ftruncate(fd, st.st_size - 1) == 0);
	CHECK(fsync(fd) == 0 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original(); cleanup();
	/* Complete checksum damage fails closed, without undoing earlier records. */
	setup(); crash_transaction("data-written", 4, 1);
	fd = file(JOURNAL, O_RDWR); CHECK(fd >= 0);
	CHECK(pread(fd, &byte, 1, journal_start() + RECORD + 1) == 1);
	byte ^= 1;
	CHECK(pwrite(fd, &byte, 1, journal_start() + RECORD + 1) == 1);
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == EIO); changed();
	byte ^= 1;
	CHECK(pwrite(fd, &byte, 1, journal_start() + RECORD + 1) == 1 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original(); cleanup();
	/* A corrupted length must fail its header checksum, not masquerade as
	 * an incomplete tail. */
	setup(); crash_transaction("data-written", 4, 1);
	fd = file(JOURNAL, O_RDWR); CHECK(fd >= 0);
	CHECK(pread(fd, &byte, 1, journal_start() + 16) == 1); byte ^= 1;
	CHECK(pwrite(fd, &byte, 1, journal_start() + 16) == 1);
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == EIO); changed();
	byte ^= 1;
	CHECK(pwrite(fd, &byte, 1, journal_start() + 16) == 1 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original(); cleanup();
}

static void identity_cases(void)
{
	int dir, fd;
	dm_undo *tx;
	setup(); crash_transaction("data-written", 4, 1);
	dir = open(directory, O_RDONLY | O_DIRECTORY); CHECK(dir >= 0);
	CHECK(renameat(dir, "a", dir, "a-old") == 0);
	fd = file("a", O_CREAT | O_EXCL | O_RDWR);
	CHECK(fd >= 0 && write(fd, "replacement", 11) == 11 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == EIO);
	expect("a", "replacement", 11); expect("b", "ZZ345678", 8);
	CHECK(unlinkat(dir, "a", 0) == 0 && renameat(dir, "a-old", dir, "a") == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original();
	CHECK(symlinkat("a", dir, "link") == 0);
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write(tx, "link", "X", 1, 0) == -1);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
	CHECK(unlinkat(dir, "link", 0) == 0);
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	int journal_dir = open(journal_directory, O_RDONLY | O_DIRECTORY);
	CHECK(journal_dir >= 0);
	CHECK(linkat(journal_dir, JOURNAL, dir, "alias", 0) == 0);
	CHECK(close(journal_dir) == 0);
	CHECK(dm_undo_write(tx, "alias", "X", 1, 0) == -1);
	CHECK(unlinkat(dir, "alias", 0) == 0);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
	CHECK(close(dir) == 0); original(); cleanup();
}

static void failure_cases(void)
{
	dm_undo *tx;
	int next;
	setup();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	fail_sync = sync_calls + 1;
	CHECK(dm_undo_write(tx, "a", "XX", 2, 0) == -1 && errno == EIO);
	original(); CHECK(dm_undo_commit(tx) == -1);
	fail_sync = 0;
	CHECK(dm_undo_abort(tx) == 0);
	dm_undo_close(tx); original(); cleanup();
	/* Fail each data sync, journal-tail sync, marker sync, and retirement sync. */
	for (next = 1; next <= 7; ++next) {
		setup(); CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0); mutations(tx);
		fail_sync = sync_calls + next;
		CHECK(dm_undo_commit(tx) == -1 && errno == EIO);
		CHECK(dm_undo_abort(tx) == -1);
		dm_undo_close(tx); fail_sync = 0;
		CHECK(dm_undo_recover(journal_directory) == 0);
		if (next >= 6) changed(); else original();
		cleanup();
	}
	/* Partial journal writes cannot authorize a data write. Partial target
	 * writes must be undone. Test both paths against the real filesystem. */
	for (next = 1; next <= 2; ++next) {
		setup(); CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
		fail_write = write_calls + next; partial_bytes = 1;
		CHECK(dm_undo_write(tx, "a", "XX", 2, 0) == -1 && errno == ENOSPC);
		CHECK(dm_undo_commit(tx) == -1);
		fail_write = 0;
		CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
		original(); cleanup();
	}
	/* Failure while writing the commit marker has an unknown outcome. */
	for (next = 0; next <= 1; ++next) {
		setup(); CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0); mutations(tx);
		fail_write = write_calls + 1; partial_bytes = next ? RECORD : 1;
		CHECK(dm_undo_commit(tx) == -1 && errno == ENOSPC);
		dm_undo_close(tx); fail_write = 0;
		CHECK(dm_undo_recover(journal_directory) == 0);
		if (next) changed(); else original();
		cleanup();
	}
	/* Failed recovery retains the journal for a later attempt. */
	setup(); crash_transaction("data-written", 4, 1);
	fail_write = write_calls + 1; partial_bytes = 1;
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == ENOSPC);
	fail_write = 0;
	CHECK(dm_undo_recover(journal_directory) == 0); original(); cleanup();
}

static void root_cases(void)
{
	dm_undo *tx;
	int dir, fd;
	char moved[160];
	unsigned char byte;
	/* Closing a handle without a decision leaves recovery work, even if the
	 * application closed normally. Recovery receives no database root. */
	setup();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	mutations(tx); dm_undo_close(tx);
	fd = file(JOURNAL, O_RDONLY); CHECK(fd >= 0 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0);
	original(); cleanup();

	/* One persistent journal directory can serve successive applications with
	 * different roots. Resolve the recorded root, regardless of current cwd. */
	setup();
	crash_transaction("data-written", 4, 1);
	CHECK(snprintf(moved, sizeof(moved), "%s-moved", directory) < (int)sizeof(moved));
	CHECK(rename(directory, moved) == 0);
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == ENOENT);
	fd = file(JOURNAL, O_RDONLY); CHECK(fd >= 0 && close(fd) == 0);
	CHECK(mkdir(directory, 0700) == 0);
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == EIO);
	CHECK(rmdir(directory) == 0 && rename(moved, directory) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original();
	/* Reuse the same journal directory with a different root and the same
	 * file names; the original database must remain untouched. */
	CHECK(mkdir(moved, 0700) == 0);
	dir = open(moved, O_RDONLY | O_DIRECTORY); CHECK(dir >= 0);
	fd = openat(dir, "a", O_CREAT | O_EXCL | O_RDWR, 0600);
	CHECK(fd >= 0 && write(fd, "other", 5) == 5 && fsync(fd) == 0 && close(fd) == 0);
	CHECK(dm_undo_begin(journal_directory, moved, &tx) == 0);
	CHECK(dm_undo_write(tx, "a", "XXXXX", 5, 0) == 0);
	dm_undo_close(tx);
	pid_t pid = fork(); CHECK(pid >= 0);
	if (!pid) {
		CHECK(chdir("/") == 0);
		CHECK(dm_undo_recover(journal_directory) == 0);
		_exit(77);
	}
	wait_crash(pid);
	fd = openat(dir, "a", O_RDONLY);
	char buffer[5];
	CHECK(fd >= 0 && read(fd, buffer, 5) == 5 && !memcmp(buffer, "other", 5));
	CHECK(close(fd) == 0 && unlinkat(dir, "a", 0) == 0 && close(dir) == 0);
	CHECK(rmdir(moved) == 0); original(); cleanup();

	setup(); crash_transaction("data-written", 4, 1);
	fd = file(JOURNAL, O_RDWR); CHECK(fd >= 0);
	CHECK(pread(fd, &byte, 1, HEADER) == 1); byte ^= 1;
	CHECK(pwrite(fd, &byte, 1, HEADER) == 1);
	CHECK(dm_undo_recover(journal_directory) == -1 && errno == EIO); changed();
	byte ^= 1;
	CHECK(pwrite(fd, &byte, 1, HEADER) == 1 && close(fd) == 0);
	CHECK(dm_undo_recover(journal_directory) == 0); original(); cleanup();
}

static void nested_cases(void)
{
	dm_undo *tx;
	int dir;
	setup();
	dir = open(directory, O_RDONLY | O_DIRECTORY); CHECK(dir >= 0);
	CHECK(mkdirat(dir, "files", 0700) == 0 && mkdirat(dir, "index", 0700) == 0);
	CHECK(renameat(dir, "a", dir, "files/a") == 0);
	CHECK(renameat(dir, "b", dir, "index/b") == 0);
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write(tx, "files/a", "XX", 2, 0) == 0);
	CHECK(dm_undo_write(tx, "index/b", "YY", 2, 0) == 0);
	dm_undo_close(tx);
	CHECK(dm_undo_recover(journal_directory) == 0);
	expect("files/a", "abcdefgh", 8); expect("index/b", "12345678", 8);
	CHECK(symlinkat("files", dir, "alias") == 0);
	const char *bad[] = {"alias/a", "files/../index/b", "/etc/passwd",
		"files//a", "./files/a", "files/a/"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
		CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
		CHECK(dm_undo_write(tx, bad[i], "X", 1, 0) == -1);
		CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
	}
	CHECK(unlinkat(dir, "alias", 0) == 0);
	CHECK(renameat(dir, "files/a", dir, "a") == 0);
	CHECK(renameat(dir, "index/b", dir, "b") == 0);
	CHECK(unlinkat(dir, "files", AT_REMOVEDIR) == 0);
	CHECK(unlinkat(dir, "index", AT_REMOVEDIR) == 0);
	CHECK(close(dir) == 0); original(); cleanup();
}

static void descriptor_cases(void)
{
	dm_undo *tx;
	int fd;
	setup();
	fd = file("a", O_RDWR); CHECK(fd >= 0);
	CHECK(lseek(fd, 3, SEEK_SET) == 3);
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write_fd(tx, "a", fd, "XX", 2, 0) == 0);
	CHECK(lseek(fd, 0, SEEK_CUR) == 3);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx); original();

	/* A replaced pathname must not redirect a write from an already-open file. */
	int dir = open(directory, O_RDONLY | O_DIRECTORY); CHECK(dir >= 0);
	CHECK(renameat(dir, "a", dir, "saved-a") == 0);
	int replacement = openat(dir, "a", O_CREAT | O_EXCL | O_RDWR, 0600);
	CHECK(replacement >= 0 && write(replacement, "replacement", 11) == 11);
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write_fd(tx, "a", fd, "XX", 2, 0) == -1 && errno == ESTALE);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
	expect("a", "replacement", 11); expect("saved-a", "abcdefgh", 8);
	CHECK(close(replacement) == 0 && unlinkat(dir, "a", 0) == 0);
	CHECK(renameat(dir, "saved-a", dir, "a") == 0 && close(dir) == 0);

	/* A mismatched descriptor must neither append undo nor change either file. */
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	int64_t end = tx->end;
	CHECK(dm_undo_write_fd(tx, "b", fd, "XX", 2, 0) == -1 && errno == ESTALE);
	CHECK(tx->end == end);
	CHECK(dm_undo_commit(tx) == -1);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx); original();
	CHECK(close(fd) == 0);

	int modes[] = {O_RDONLY, O_WRONLY, O_RDWR | O_APPEND};
	for (size_t i = 0; i < sizeof(modes)/sizeof(modes[0]); i++) {
		fd = file("a", modes[i]); CHECK(fd >= 0);
		CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
		CHECK(dm_undo_write_fd(tx, "a", fd, "X", 1, 0) == -1);
		CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx);
		CHECK(close(fd) == 0); original();
	}
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write_fd(tx, "a", -1, NULL, 0, 0) == -1);
	CHECK(dm_undo_abort(tx) == 0); dm_undo_close(tx); original();

	fd = file("a", O_RDWR); CHECK(fd >= 0);
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write_fd(tx, "a", fd, "XX", 2, 8) == 0);
	dm_undo_close(tx);
	CHECK(dm_undo_recover(journal_directory) == 0); original();
	CHECK(dm_undo_begin(journal_directory, directory, &tx) == 0);
	CHECK(dm_undo_write_fd(tx, "a", fd, "XX", 2, 0) == 0);
	CHECK(dm_undo_commit(tx) == 0); dm_undo_close(tx);
	expect("a", "XXcdefgh", 8);
	CHECK(close(fd) == 0); cleanup();
}

int main(void)
{
	normal_cases(); crash_cases(); damaged_cases(); identity_cases(); failure_cases();
	root_cases(); nested_cases(); descriptor_cases();
	return 0;
}
