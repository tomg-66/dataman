/* Exercise the real dbserve startup path with isolated IPC/worker substitutes.
 * Journal and filesystem operations are real. GPL-2.0-or-later. */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "journal_startup.h"
#include "undo_journal.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s (errno %d)\n", __FILE__, __LINE__, #expr, errno); \
	exit(99); } } while (0)

static char base[128], journal_path[160], data_path[160];
static int verified, queued, reject_pid, worker_pipe = -1;
static int stop_after_resolve;

static void undo_point(const char *name)
{
	if (stop_after_resolve && !strcmp(name, "resolved")) _exit(77);
}
#define UNDO_POINT(name) undo_point(name)
#include "../undo_journal.c"
#undef UNDO_POINT

static int test_verify_pid(char *name)
{
	CHECK(!strcmp(name, "dataman_srv"));
	verified = 1;
	return reject_pid ? -1 : 0;
}

static int test_msgget(key_t key, int flags)
{
	(void)key; (void)flags;
	CHECK(verified);
	queued = 1;
	return 7;
}

static ssize_t test_msgrcv(int id, void *message, size_t size, long type, int flags)
{
	(void)message; (void)size; (void)type; (void)flags;
	CHECK(id == 7 && queued);
	errno = ENOMSG;
	return -1;
}

static int test_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		void *(*start)(void *), void *arg)
{
	(void)thread; (void)attr; (void)start; (void)arg;
	CHECK(verified && queued);
	int dir = open(journal_path, O_RDONLY | O_DIRECTORY);
	CHECK(dir >= 0);
	CHECK(faccessat(dir, ".dataman-undo", F_OK, 0) < 0 && errno == ENOENT);
	CHECK(close(dir) == 0);
	CHECK(write(worker_pipe, "W", 1) == 1);
	exit(0);
}

#define verify_pid test_verify_pid
#define msgget test_msgget
#define msgrcv test_msgrcv
#define pthread_create test_pthread_create
#define main dbserve_main
#include "../dbserve.c"
#undef main
#undef pthread_create
#undef msgrcv
#undef msgget
#undef verify_pid

void *dispatch(void *arg) { return arg; }
int dm_tx_configure(const char *path) { CHECK(!strcmp(path, journal_path)); return 0; }
int dm_tx_blocked(void) { return 0; }
int session_root_reap(void) { return 0; }
void print_version(char *name) { (void)name; exit(0); }
void err_sys(char *format, char *arg) { fprintf(stderr, format, arg); exit(98); }

static void setup(void)
{
	strcpy(base, "/tmp/dataman-startup-test-XXXXXX");
	CHECK(mkdtemp(base));
	snprintf(journal_path, sizeof(journal_path), "%s/journal", base);
	snprintf(data_path, sizeof(data_path), "%s/database", base);
	CHECK(mkdir(data_path, 0700) == 0);
	int dir = open(data_path, O_RDONLY | O_DIRECTORY);
	CHECK(dir >= 0);
	int fd = openat(dir, "record", O_RDWR | O_CREAT | O_EXCL, 0600);
	CHECK(fd >= 0 && write(fd, "old", 3) == 3 && fsync(fd) == 0);
	CHECK(close(fd) == 0 && close(dir) == 0);
}

static void expect_data(const char *expected)
{
	char data[3];
	int dir = open(data_path, O_RDONLY | O_DIRECTORY);
	CHECK(dir >= 0);
	int fd = openat(dir, "record", O_RDONLY);
	CHECK(fd >= 0 && read(fd, data, 3) == 3 && !memcmp(data, expected, 3));
	CHECK(close(fd) == 0 && close(dir) == 0);
}

static void cleanup(void)
{
	int dir = open(journal_path, O_RDONLY | O_DIRECTORY);
	CHECK(dir >= 0);
	CHECK(unlinkat(dir, ".dataman-undo", 0) < 0 && errno == ENOENT);
	CHECK(unlinkat(dir, ".server.lock", 0) == 0);
	CHECK(close(dir) == 0 && rmdir(journal_path) == 0);
	dir = open(data_path, O_RDONLY | O_DIRECTORY);
	CHECK(dir >= 0 && unlinkat(dir, "record", 0) == 0 && close(dir) == 0);
	CHECK(rmdir(data_path) == 0 && rmdir(base) == 0);
}

static void run_server(int expected_status, int expect_workers)
{
	int pipes[2], status;
	char byte;
	CHECK(pipe(pipes) == 0);
	pid_t child = fork(); CHECK(child >= 0);
	if (!child) {
		close(pipes[0]); worker_pipe = pipes[1];
		CHECK(setenv("DATAMAN_JOURNAL_DIR", journal_path, 1) == 0);
		char name[] = "dataman_srv", *argv[] = {name, NULL};
		exit(dbserve_main(1, argv));
	}
	close(pipes[1]);
	CHECK(waitpid(child, &status, 0) == child);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == expected_status);
	CHECK(read(pipes[0], &byte, 1) == expect_workers);
	CHECK(close(pipes[0]) == 0);
}

static void leave_transaction(void)
{
	dm_undo *tx;
	CHECK(mkdir(journal_path, 0700) == 0);
	CHECK(dm_undo_begin(journal_path, data_path, &tx) == 0);
	CHECK(dm_undo_write(tx, "record", "new", 3, 0) == 0);
	dm_undo_close(tx);
}

int main(void)
{
	dm_journal_guard *guard;
	/* Fresh directory; no journal. Startup must create it before workers. */
	setup(); run_server(0, 1); expect_data("old"); cleanup();
	setup(); leave_transaction(); run_server(0, 1); expect_data("old"); cleanup();
	/* A committed marker surviving a crash must preserve the new data. */
	setup(); CHECK(mkdir(journal_path, 0700) == 0);
	pid_t child = fork(); CHECK(child >= 0);
	if (!child) {
		dm_undo *tx;
		CHECK(dm_undo_begin(journal_path, data_path, &tx) == 0);
		CHECK(dm_undo_write(tx, "record", "new", 3, 0) == 0);
		stop_after_resolve = 1;
		(void)dm_undo_commit(tx); _exit(99);
	}
	int status;
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
	run_server(0, 1); expect_data("new"); cleanup();
	/* Recovery failure must not reach any worker. Restore availability and retry. */
	setup(); leave_transaction();
	char moved[180]; snprintf(moved, sizeof(moved), "%s-moved", data_path);
	CHECK(rename(data_path, moved) == 0);
	run_server(EXIT_FAILURE, 0);
	CHECK(rename(moved, data_path) == 0);
	run_server(0, 1); expect_data("old"); cleanup();
	/* Persistent ownership survives even when the PID check is substituted. */
	setup(); CHECK(dm_journal_open(journal_path, &guard) == 0);
	dm_journal_guard *duplicate;
	CHECK(dm_journal_open(journal_path, &duplicate) == -1 && errno == EALREADY);
	run_server(EXIT_FAILURE, 0);
	dm_journal_close(guard);
	run_server(0, 1); cleanup();
	/* Unsafe permissions and failed PID checks must also prevent workers. */
	setup(); CHECK(mkdir(journal_path, 0755) == 0 && chmod(journal_path, 0755) == 0);
	run_server(EXIT_FAILURE, 0);
	CHECK(chmod(journal_path, 0700) == 0);
	reject_pid = 1; run_server(EXIT_FAILURE, 0); reject_pid = 0;
	run_server(0, 1); cleanup();
	return 0;
}
