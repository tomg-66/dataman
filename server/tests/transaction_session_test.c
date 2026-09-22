/* Real journal lifecycle with isolated registered-root stand-ins. GPL-2.0-or-later. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "transaction_session.h"
#include "transaction_dispatch.h"
#include "transaction_wire.h"
#include "dbfunc.h"
#include "undo_journal.h"
#include "errors.h"
#include "storage_io.h"
#include "srv_index.h"
#include "misc.h"
#include "index_v2.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s (errno %d)\n", __FILE__, __LINE__, #expr, errno); exit(99); \
} } while (0)

static char root[128], logdir[128];
static int attached[] = {1, 1};
static int fake_shmget(key_t pid, size_t size, int flags)
{
	(void)size; (void)flags;
	if (pid == 110 || pid == 111) return pid - 100;
	errno = ENOENT; return -1;
}
static int fake_shmctl(int id, int op, struct shmid_ds *status)
{
	CHECK(op == IPC_STAT && (id == 10 || id == 11));
	memset(status, 0, sizeof(*status)); status->shm_nattch = attached[id - 10];
	return 0;
}
static int fake_kill(pid_t pid, int sig) { CHECK((pid == 110 || pid == 111) && sig == 0); return 0; }
#define shmget fake_shmget
#define shmctl fake_shmctl
#define kill fake_kill
#include "../session_root.c"
#undef kill
#undef shmctl
#undef shmget
int init_dataman(char *cmd, int offset, char **data) { (void)cmd; (void)offset; (void)data; return EINVMSG; }
int mkidx(char *cmd, int offset, char **data) { (void)cmd; (void)offset; (void)data; return EINVMSG; }

static void register_session(int id)
{
	char cmd[256], *data = NULL;
	int offset = snprintf(cmd, sizeof(cmd), "%d|%d|", id + 100, DEF_ROOT);
	CHECK(snprintf(cmd + offset, sizeof(cmd) - offset, "%s|", root) < (int)sizeof(cmd) - offset);
	CHECK(def_root(cmd, offset, &data) == 4 && !data);
}

static void expect(const char *value)
{
	char bytes[3];
	int dir = open(root, O_RDONLY | O_DIRECTORY);
	int fd = openat(dir, "record", O_RDONLY);
	CHECK(fd >= 0 && read(fd, bytes, 3) == 3 && !memcmp(bytes, value, 3));
	CHECK(close(fd) == 0 && close(dir) == 0);
}

int idx_cnt, dbgsw;
INDEX *_indices;
FILES *_wfiles[MAX_CONNS];
static int locked;

int fl_lock(P_LOCK *lock, int type)
{
	(void)lock;
	if (type == LOCK_EX) { CHECK(!locked); locked = 1; }
	else { CHECK(type == LOCK_UN && locked); locked = 0; }
	return 0;
}

extern int put_blobs(FILES *, int, int64_t, char *);
extern int flush(char *, int, char **);
extern int insert(char *, int, char **);
extern int delete(char *, int, char **);
extern int rm_key(int, int, char *);
extern void put_ll(void *, int64_t);

extern int blob_ctl(char *, char *, int, int64_t, int);

int get_blobs(FILES *file, int fmt, int64_t recno, char **data, int *len)
{
	(void)file; (void)fmt; (void)recno; (void)data; (void)len;
	CHECK(0); return -1;
}

static void routed_flush(int expected)
{
	char cmd[] = "-1|0|32|1|";
	char *data = strdup("new"); CHECK(data);
	CHECK(flush(cmd, 0, &data) == expected);
	CHECK(!data && !locked);
}

static void *unbound_worker(void *context)
{
	int fd = *(int *)context;
	CHECK(dm_storage_mutate_at(fd, "bad", 3, 32 + DATARECORD_HEADER_LENGTH) == -1);
	CHECK(errno == EBUSY);
	return NULL;
}

static pthread_mutex_t admission_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t admission_cond = PTHREAD_COND_INITIALIZER;
static int admitted, release_request;

static void *ordinary_request(void *unused)
{
	(void)unused;
	CHECK(dm_tx_request_enter() == 0);
	pthread_mutex_lock(&admission_mutex);
	admitted = 1;
	pthread_cond_signal(&admission_cond);
	while (!release_request)
		pthread_cond_wait(&admission_cond, &admission_mutex);
	pthread_mutex_unlock(&admission_mutex);
	dm_tx_request_leave();
	return NULL;
}

static void admission_cases(void)
{
	pthread_t worker;
	CHECK(dm_tx_request_enter() == 0);
	CHECK(dm_tx_request_enter() == EINVMSG);
	CHECK(pthread_create(&worker, NULL, ordinary_request, NULL) == 0);
	pthread_mutex_lock(&admission_mutex);
	while (!admitted)
		pthread_cond_wait(&admission_cond, &admission_mutex);
	pthread_mutex_unlock(&admission_mutex);
	CHECK(dm_tx_begin(10) == ENOLOCK);
	dm_tx_request_leave();
	dm_tx_request_leave(); /* A duplicate leave must not release another worker. */
	CHECK(dm_tx_begin(10) == ENOLOCK);
	pthread_mutex_lock(&admission_mutex);
	release_request = 1;
	pthread_cond_signal(&admission_cond);
	pthread_mutex_unlock(&admission_mutex);
	CHECK(pthread_join(worker, NULL) == 0);
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_request_enter() == ENOLOCK);
	CHECK(dm_tx_abort(10) == 0);
	CHECK(dm_tx_request_enter() == 0);
	dm_tx_request_leave();
}

static int dispatched_calls;
static int dispatched_flush(void *context)
{
	int result = *(int *)context;
	dispatched_calls++;
	CHECK(dm_tx_commit(10) == ENOLOCK);
	CHECK(dm_tx_disconnect(10) == ENOLOCK);
	routed_flush(4);
	return result;
}

static void *competing_owner(void *unused)
{
	(void)unused;
	CHECK(dm_tx_enter(10, NULL, 0) == ENOLOCK);
	CHECK(dm_tx_commit(10) == ENOLOCK);
	CHECK(dm_tx_request_enter() == ENOLOCK);
	return NULL;
}

static int dispatched_read(void *unused)
{
	(void)unused;
	pthread_t worker;
	CHECK(pthread_create(&worker, NULL, competing_owner, NULL) == 0);
	CHECK(pthread_join(worker, NULL) == 0);
	return 7;
}

static void routed_cases(int dir)
{
	int fd = openat(dir, "data", O_CREAT | O_EXCL | O_RDWR, 0600);
	int ix = openat(dir, "index", O_CREAT | O_EXCL | O_RDWR, 0600);
	CHECK(fd >= 0 && ix >= 0);
	char initial[32 + DATARECORD_HEADER_LENGTH + 3] = {0}, bytes[sizeof(initial)];
	initial[32] = 1;
	put_ll(initial + 18, 32);
	memcpy(initial + 32 + DATARECORD_HEADER_LENGTH, "old", 3);
	CHECK(dm_storage_mutate_at(fd, initial, sizeof(initial), 0) == 0);
	const char *names[] = {"data"};
	CHECK(index_v2_create_empty(ix, 4, 1, names));
	struct stat st; CHECK(fstat(ix, &st) == 0);
	size_t size = st.st_size;
	char *before = malloc(size), *after = malloc(size);
	CHECK(before && after && dm_storage_read_at(ix, before, size, 0) == 0);
	RFDESC record = {0}; FILEDESC desc = {0}; FILES file = {0};
	record.rf_len = 3; desc.n_rformats = 1; desc.record_desc = &record;
	file._filedesc = &desc; file._chan = fd; _wfiles[0] = &file;
	int16_t formats = 1;
	file._desc = &formats; file._hlen = 16;
	FILES *files[] = {&file}; INDEX index = {0};
	index._refcnt = 1; index._f_cnt = 1; index._files = files;
	_indices = &index; idx_cnt = 1;
	dm_tx_target targets[] = {{fd, "data"}, {ix, "index"}};
	CHECK(dm_tx_enter(10, targets, 2) == ENOXACT);
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_enter(11, targets, 2) == ENOXACT);
	/* Neither this worker nor another worker can fall back to a direct write. */
	routed_flush(ERECWRT);
	CHECK(dm_tx_enter(10, targets, 2) == 0);
	CHECK(dm_tx_enter(10, targets, 2) == EINVMSG);
	pthread_t worker;
	CHECK(pthread_create(&worker, NULL, unbound_worker, &fd) == 0);
	CHECK(pthread_join(worker, NULL) == 0);
	routed_flush(4);
	char command[128] = "1|0|0|0|32|", *data = NULL;
	CHECK(insert(command, 0, &data) > 0 && !data && !locked);
	snprintf(command, sizeof(command), "0|0|%zu|%d|", sizeof(initial), NOXACT);
	CHECK(delete(command, 0, &data) > 0 && data && !locked);
	free(data); data = NULL;
	INDEX_V2_CURSOR cursor; uint64_t root_offset;
	for (int i = 0; i < 16; i++) {
		char key[5]; snprintf(key, sizeof(key), "%04d", i);
		CHECK(index_v2_insert(ix, key, 0, 100 + i, &cursor, &root_offset));
	}
	CHECK(index_v2_remove(ix, "0003", 0, 103));
	dm_tx_leave();
	CHECK(dm_tx_abort(10) == 0);
	CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0);
	CHECK(!memcmp(bytes, initial, sizeof(bytes)));
	CHECK(fstat(fd, &st) == 0 && st.st_size == sizeof(initial));
	CHECK(fstat(ix, &st) == 0 && st.st_size == (off_t)size);
	CHECK(dm_storage_read_at(ix, after, size, 0) == 0 && !memcmp(before, after, size));

	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_enter(10, targets, 2) == 0); routed_flush(4); dm_tx_leave();
	CHECK(dm_tx_commit(10) == 0);
	CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0);
	CHECK(!memcmp(bytes + 32 + DATARECORD_HEADER_LENGTH, "new", 3));

	/* An unmapped owner write poisons the transaction rather than bypassing undo. */
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_enter(10, targets + 1, 1) == 0);
	routed_flush(ERECWRT); dm_tx_leave();
	CHECK(dm_tx_commit(10) == ERECWRT && dm_tx_abort(10) == 0);

	/* Unsupported namespace changes are rejected before destructive file calls. */
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_enter(10, targets, 2) == 0);
	CHECK(dm_storage_namespace_check() == -1 && errno == EBUSY);
	dm_tx_leave();
	CHECK(dm_tx_commit(10) == ERECWRT && dm_tx_abort(10) == 0);
	CHECK(dm_storage_namespace_check() == 0);

	/* Completion cannot race a running owner handler. */
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_enter(10, targets, 2) == 0);
	CHECK(dm_tx_abort(10) == ENOLOCK);
	CHECK(dm_tx_commit(10) == ENOLOCK);
	CHECK(dm_tx_disconnect(10) == ENOLOCK);
	dm_tx_complete(ERECWRT);
	CHECK(dm_tx_commit(10) == ERECWRT);
	CHECK(dm_tx_abort(10) == 0);

	/* Derive relative bindings from absolute metadata paths, then run a handler. */
	char pathname[256];
	CHECK(snprintf(pathname, sizeof(pathname), "%s/data", root) < (int)sizeof(pathname));
	dm_tx_target absolute = {fd, pathname};
	int handler_result = ERECWRT;
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_dispatch(11, &absolute, 1, dispatched_flush, &handler_result) == ENOXACT);
	CHECK(dispatched_calls == 0);
	CHECK(dm_tx_dispatch(10, &absolute, 1, dispatched_flush, &handler_result) == ERECWRT);
	CHECK(dispatched_calls == 1);
	CHECK(dm_tx_commit(10) == ERECWRT && dm_tx_abort(10) == 0);
	CHECK(dm_tx_begin(10) == 0);
	const char *bad[] = {"/outside/data", "data", "/../data"};
	for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
		absolute.path = bad[i];
		CHECK(dm_tx_dispatch(10, &absolute, 1, dispatched_flush, &handler_result) == EINVMSG);
	}
	CHECK(snprintf(pathname, sizeof(pathname), "%s-other/data", root) < (int)sizeof(pathname));
	absolute.path = pathname;
	CHECK(dm_tx_dispatch(10, &absolute, 1, dispatched_flush, &handler_result) == EINVMSG);
	CHECK(snprintf(pathname, sizeof(pathname), "%s/../data", root) < (int)sizeof(pathname));
	CHECK(dm_tx_dispatch(10, &absolute, 1, dispatched_flush, &handler_result) == EINVMSG);
	CHECK(dispatched_calls == 1);
	CHECK(dm_tx_dispatch(10, NULL, 0, dispatched_read, NULL) == 7);
	CHECK(snprintf(pathname, sizeof(pathname), "%s/data", root) < (int)sizeof(pathname));
	handler_result = 4;
	CHECK(dm_tx_dispatch(10, &absolute, 1, dispatched_flush, &handler_result) == 4);
	CHECK(dm_tx_commit(10) == 0);

	/* Routed writes survive worker/process loss as recovery work. */
	CHECK(dm_storage_mutate_at(fd, initial, sizeof(initial), 0) == 0);
	pid_t child = fork(); CHECK(child >= 0);
	if (!child) {
		CHECK(dm_tx_begin(10) == 0 && dm_tx_enter(10, targets, 2) == 0);
		routed_flush(4);
		CHECK(index_v2_insert(ix, "dead", 0, 100, &cursor, &root_offset));
		_exit(77);
	}
	int status;
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
	CHECK(dm_undo_recover(logdir) == 0);
	CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0 && !memcmp(bytes, initial, sizeof(bytes)));
	CHECK(fstat(ix, &st) == 0 && st.st_size == (off_t)size);
	CHECK(dm_storage_read_at(ix, after, size, 0) == 0 && !memcmp(before, after, size));
	_wfiles[0] = NULL; _indices = NULL; idx_cnt = 0;
	free(before); free(after);
	CHECK(close(fd) == 0 && close(ix) == 0);
	CHECK(unlinkat(dir, "data", 0) == 0 && unlinkat(dir, "index", 0) == 0);
}

static int blob_handler(void *context)
{
	FILES *file = context;
	char command[] = "-1|0|32|1|";
	char *data = malloc(10); CHECK(data);
	uint32_t length = htonl(3);
	memcpy(data, "new", 3); memcpy(data + 3, &length, 4); memcpy(data + 7, "XYZ", 3);
	int result = flush(command, 0, &data);
	CHECK(!data);
	if (result < 0) return result;
	CHECK(blob_ctl(root, file->_fname, 1, 32, HIDE) == 0);
	CHECK(blob_ctl(root, file->_fname, 1, 32, UNHIDE) == 0);
	CHECK(blob_ctl(root, file->_fname, 1, 32, HIDE) == 0);
	CHECK(blob_ctl(root, file->_fname, 1, 32, CLEANUP) == 0);
	char replacement[10]; memcpy(replacement, "new", 3);
	memcpy(replacement + 3, &length, 4); memcpy(replacement + 7, "end", 3);
	return put_blobs(file, 1, 32, replacement);
}

static void blob_dispatch_cases(int dir)
{
	CHECK(mkdirat(dir, "files", 0700) == 0 && mkdirat(dir, "blobs", 0700) == 0);
	int fd = openat(dir, "files/blobdata", O_CREAT | O_EXCL | O_RDWR, 0600);
	CHECK(fd >= 0);
	char initial[32 + DATARECORD_HEADER_LENGTH + 3] = {0}, bytes[sizeof(initial)];
	memcpy(initial + 32 + DATARECORD_HEADER_LENGTH, "old", 3);
	CHECK(dm_storage_mutate_at(fd, initial, sizeof(initial), 0) == 0);
	char name[256]; snprintf(name, sizeof(name), "%s/files/blobdata", root);
	int16_t sizes[] = {3, 0};
	RFDESC record = {0}; FILEDESC desc = {0}; FILES file = {0};
	record.rf_len = 3; record.n_fields = 2; record.has_blob = 1; record.field_sizes = sizes;
	desc.n_rformats = 1; desc.record_desc = &record;
	file._filedesc = &desc; file._chan = fd; file._fname = name; _wfiles[0] = &file;
	char original[10]; uint32_t length = htonl(3);
	memcpy(original, "old", 3); memcpy(original + 3, &length, 4); memcpy(original + 7, "abc", 3);
	CHECK(put_blobs(&file, 1, 32, original) == 0);
	dm_tx_target target = {fd, name};
	for (int crash = 0; crash < 2; crash++) {
		pid_t child = crash ? fork() : 0;
		CHECK(child >= 0);
		if (!child) {
			CHECK(dm_tx_begin(10) == 0);
			CHECK(dm_tx_dispatch(10, &target, 1, blob_handler, &file) == 0);
			if (crash) _exit(77);
			CHECK(dm_tx_abort(10) == 0);
		} else {
			int status;
			CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
			CHECK(dm_undo_recover(logdir) == 0);
		}
		CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0 && !memcmp(bytes, initial, sizeof(bytes)));
		int blob = openat(dir, "blobs/blobdata.1.32.1", O_RDONLY); char value[3];
		CHECK(blob >= 0 && read(blob, value, 3) == 3 && !memcmp(value, "abc", 3) && close(blob) == 0);
		CHECK(faccessat(dir, "blobs/.blobdata.1.32.1", F_OK, 0) == -1 && errno == ENOENT);
	}
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_dispatch(10, &target, 1, blob_handler, &file) == 0);
	CHECK(dm_tx_commit(10) == 0);
	CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0);
	CHECK(!memcmp(bytes + 32 + DATARECORD_HEADER_LENGTH, "new", 3));
	int blob = openat(dir, "blobs/blobdata.1.32.1", O_RDONLY); char value[3];
	CHECK(blob >= 0 && read(blob, value, 3) == 3 && !memcmp(value, "end", 3) && close(blob) == 0);
	/* The public wire dispatcher binds production metadata, isolates sessions,
	 * validates payloads before mutation, and supports explicit/implicit finish. */
	char command[128], *payload = NULL;
	strcpy(command, "");
	CHECK(dm_tx_wire(10, START_XACT, command, 0, &payload, 0, NULL) == 4);
	strcpy(command, "-1|0|32|1|10|");
	payload = malloc(10); CHECK(payload); memcpy(payload, original, 10);
	CHECK(dm_tx_wire(11, FLUSH, command, 0, &payload, 10, flush) == ENOLOCK);
	CHECK(dm_tx_wire(10, FLUSH, command, 0, &payload, 10, flush) == 4 && !payload);
	strcpy(command, "");
	CHECK(dm_tx_wire(10, IOPEN, command, 0, &payload, 0, NULL) == ENOLOCK);
	CHECK(dm_tx_wire(10, ROLLBACK, command, 0, &payload, 0, NULL) == 4);
	CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0);
	CHECK(!memcmp(bytes + 32 + DATARECORD_HEADER_LENGTH, "new", 3));
	blob = openat(dir, "blobs/blobdata.1.32.1", O_RDONLY);
	CHECK(blob >= 0 && read(blob, value, 3) == 3 && !memcmp(value, "end", 3) && close(blob) == 0);
	strcpy(command, "-1|0|32|1|10|");
	payload = malloc(10); CHECK(payload); memcpy(payload, original, 10);
	CHECK(dm_tx_wire(10, FLUSH, command, 0, &payload, 10, flush) == 4 && !payload);
	CHECK(dm_storage_read_at(fd, bytes, sizeof(bytes), 0) == 0);
	CHECK(!memcmp(bytes + 32 + DATARECORD_HEADER_LENGTH, "old", 3));
	strcpy(command, "");
	CHECK(dm_tx_wire(10, START_XACT, command, 0, &payload, 0, NULL) == 4);
	strcpy(command, "-1|0|32|1|9|");
	payload = malloc(9); CHECK(payload); memcpy(payload, original, 9);
	CHECK(dm_tx_wire(10, FLUSH, command, 0, &payload, 9, flush) == EINVMSG);
	free(payload); payload = NULL;
	strcpy(command, "");
	CHECK(dm_tx_wire(10, COMMIT, command, 0, &payload, 0, NULL) == ERECWRT);
	CHECK(dm_tx_wire(10, ROLLBACK, command, 0, &payload, 0, NULL) == 4);
	CHECK(close(fd) == 0); _wfiles[0] = NULL;
	CHECK(unlinkat(dir, "files/blobdata", 0) == 0 && unlinkat(dir, "blobs/blobdata.1.32.1", 0) == 0);
	CHECK(unlinkat(dir, "files", AT_REMOVEDIR) == 0 && unlinkat(dir, "blobs", AT_REMOVEDIR) == 0);
}

static void cache_cases(int dir)
{
	int fd = openat(dir, "cache-index", O_CREAT | O_EXCL | O_RDWR, 0600);
	CHECK(fd >= 0);
	const char *names[] = {"record"};
	CHECK(index_v2_create_empty(fd, 4, 1, names));
	INDEX cache = {0}, alias = {0};
	cache._idxchan = alias._idxchan = fd;
	cache._keylen = alias._keylen = 4; cache._f_cnt = alias._f_cnt = 1;
	uint16_t keylen, count; uint32_t crc; uint64_t root_before, generation_before;
	CHECK(index_v2_read_header(fd, &keylen, &count, &root_before, &crc, &generation_before));
	dm_tx_target target = {fd, "cache-index"};
	CHECK(dm_tx_begin(10) == 0 && dm_tx_enter(10, &target, 1) == 0);
	CHECK(dm_tx_track_index(&cache) == 0 && dm_tx_track_index(&alias) == 0);
	INDEX_V2_CURSOR cursor; uint64_t root_after;
	for (int i = 0; i < 16; i++) {
		char key[5]; snprintf(key, sizeof(key), "%04d", i);
		CHECK(dm_tx_track_index(&cache) == 0);
		CHECK(index_v2_insert(fd, key, 0, 100 + i, &cursor, &root_after));
		cache._rootpos = (int64_t)root_after; cache._generation = cursor.generation;
	}
	alias._rootpos = -1; alias._generation = 0;
	CHECK(cache._generation != generation_before);
	dm_tx_leave(); CHECK(dm_tx_abort(10) == 0);
	CHECK(cache._rootpos == (int64_t)root_before && cache._generation == generation_before);
	CHECK(alias._rootpos == cache._rootpos && alias._generation == cache._generation);
	CHECK(dm_tx_request_enter() == 0); dm_tx_request_leave();

	CHECK(dm_tx_begin(10) == 0 && dm_tx_enter(10, &target, 1) == 0);
	CHECK(dm_tx_track_index(&cache) == 0);
	CHECK(index_v2_insert(fd, "keep", 0, 100, &cursor, &root_after));
	cache._rootpos = -1; cache._generation = 0;
	dm_tx_leave(); CHECK(dm_tx_commit(10) == 0);
	CHECK(cache._rootpos == (int64_t)root_after && cache._generation == cursor.generation);

	CHECK(dm_tx_begin(10) == 0 && dm_tx_enter(10, &target, 1) == 0);
	cache._refcnt = 1; _indices = &cache; idx_cnt = 1;
	char key[4 + KEY_HEADER_LENGTH] = {0};
	memcpy(key, "keep", 4); key[4] = 1; put_ll(key + 5, 100);
	CHECK(rm_key(0, NOXACT, key) == 1); /* Production handler registers the cache. */
	cache._rootpos = -1; cache._generation = 0;
	dm_tx_leave(); CHECK(dm_tx_disconnect(10) == 0);
	CHECK(cache._rootpos == (int64_t)root_after && cache._generation == cursor.generation);

	/* A bad descriptor must leave admission closed and publish no cache changes. */
	pid_t child = fork(); CHECK(child >= 0);
	if (!child) {
		CHECK(dm_tx_begin(10) == 0 && dm_tx_enter(10, &target, 1) == 0);
		CHECK(dm_tx_track_index(&cache) == 0);
		cache._rootpos = -1; dm_tx_leave(); CHECK(close(fd) == 0);
		CHECK(dm_tx_abort(10) == EROLLBACK);
		CHECK(dm_tx_blocked() && dm_tx_request_enter() == EROLLBACK);
		CHECK(cache._rootpos == -1); _exit(77);
	}
	int status;
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
	CHECK(dm_undo_recover(logdir) == 0);
	CHECK(close(fd) == 0 && unlinkat(dir, "cache-index", 0) == 0);
	_indices = NULL; idx_cnt = 0;
}

int main(void)
{
	strcpy(root, "/tmp/dataman-tx-root-XXXXXX");
	strcpy(logdir, "/tmp/dataman-tx-log-XXXXXX");
	CHECK(mkdtemp(root) && mkdtemp(logdir));
	int dir = open(root, O_RDONLY | O_DIRECTORY);
	int fd = openat(dir, "record", O_CREAT | O_EXCL | O_RDWR, 0600);
	CHECK(fd >= 0 && write(fd, "old", 3) == 3 && fsync(fd) == 0 && close(fd) == 0);
	CHECK(dm_tx_configure(logdir) == 0);
	register_session(10); register_session(11);
	admission_cases();
	routed_cases(dir);
	blob_dispatch_cases(dir);
	cache_cases(dir);
	fd = openat(dir, "record", O_RDWR); CHECK(fd >= 0);
	CHECK(dm_tx_write_fd(10, "record", fd, "bad", 3, 0) == ENOXACT);
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_write_fd(11, "record", fd, "bad", 3, 0) == ENOXACT);
	CHECK(dm_tx_write_fd(10, "record", fd, "new", 3, 0) == 0); expect("new");
	CHECK(dm_tx_write_fd(10, "record", -1, "bad", 3, 0) == ERECWRT);
	CHECK(dm_tx_commit(10) == ERECWRT);
	CHECK(dm_tx_write_fd(10, "record", fd, "bad", 3, 0) == ERECWRT);
	CHECK(dm_tx_abort(10) == 0); expect("old");
	CHECK(close(fd) == 0);
	CHECK(dm_tx_begin(99) == ENOCONN);
	CHECK(dm_tx_commit(10) == ENOXACT);
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_begin(10) == EINXACT);
	CHECK(dm_tx_begin(11) == ENOLOCK);
	CHECK(dm_tx_write(11, "record", "bad", 3, 0) == ENOXACT);
	CHECK(dm_tx_abort(11) == ENOXACT);
	CHECK(dm_tx_disconnect(11) == 0);
	CHECK(dm_tx_write(10, "record", "new", 3, 0) == 0); expect("new");
	CHECK(dm_tx_disconnect(10) == 0); expect("old");
	CHECK(dm_tx_disconnect(10) == 0);
	CHECK(dm_tx_begin(11) == 0);
	CHECK(dm_tx_write(11, "record", "new", 3, 0) == 0);
	CHECK(dm_tx_commit(11) == 0);
	CHECK(dm_tx_disconnect(11) == 0); expect("new");
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_write(10, "record", "old", 3, 0) == 0);
	CHECK(dm_tx_write(10, "../escape", "bad", 3, 0) == ERECWRT);
	CHECK(dm_tx_commit(10) == ERECWRT);
	CHECK(dm_tx_write(10, "record", "bad", 3, 0) == ERECWRT);
	CHECK(dm_tx_abort(10) == 0); expect("new");
	/* Exercise registry->manager cleanup with real undo, not a mocked abort. */
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_write(10, "record", "old", 3, 0) == 0);
	CHECK(session_root_close(10) == 0); expect("new");
	CHECK(dm_tx_begin(10) == ENOCONN);
	register_session(10);
	CHECK(dm_tx_begin(10) == 0);
	CHECK(dm_tx_write(10, "record", "old", 3, 0) == 0);
	attached[0] = 0;
	CHECK(session_root_reap() == 0); expect("new");
	attached[0] = 1; register_session(10);
	/* Recovery after process death needs no surviving session metadata. */
	pid_t child = fork(); CHECK(child >= 0);
	if (!child) {
		CHECK(dm_tx_begin(10) == 0);
		CHECK(dm_tx_write(10, "record", "old", 3, 0) == 0);
		_exit(77);
	}
	int status;
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
	CHECK(dm_undo_recover(logdir) == 0); expect("new");
	/* Failure to abort retains the journal and never hands ownership onward. */
	child = fork(); CHECK(child >= 0);
	if (!child) {
		CHECK(dm_tx_begin(10) == 0);
		CHECK(dm_tx_write(10, "record", "old", 3, 0) == 0);
		CHECK(renameat(dir, "record", dir, "moved") == 0);
		CHECK(dm_tx_disconnect(10) == EROLLBACK && dm_tx_blocked());
		CHECK(dm_tx_begin(11) == EROLLBACK);
		CHECK(dm_tx_request_enter() == EROLLBACK);
		CHECK(dm_tx_commit(10) == EROLLBACK);
		CHECK(renameat(dir, "moved", dir, "record") == 0);
		_exit(77);
	}
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 77);
	CHECK(dm_undo_recover(logdir) == 0); expect("new");
	CHECK(unlinkat(dir, "record", 0) == 0 && close(dir) == 0);
	CHECK(rmdir(root) == 0 && rmdir(logdir) == 0);
	return 0;
}
