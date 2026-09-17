/* Server-side journal ownership. GPL-2.0-or-later. */
#ifndef DATAMAN_TRANSACTION_SESSION_H
#define DATAMAN_TRANSACTION_SESSION_H
#include <stddef.h>
#include <stdint.h>

/*
 * Configure only after startup recovery, with lifetime journal ownership held.
 * One journal owner at a time; contention returns ENOLOCK without blocking a
 * worker. Caller must serialize requests within a session and exclude ALL other
 * database access before using begin/write. The legacy wire transaction path
 * does not call these functions until that isolation and write routing exist.
 * Return 0 on success, otherwise a negative Dataman error.
 */
/* Admit an ordinary dispatch handler, including reads and maintenance. Calls
 * must be paired on the same worker; nesting is rejected. No mutex is retained
 * across the handler. An active journal blocks ALL ordinary handlers, including
 * its owner, until transaction-aware dispatch is implemented separately.
 */
int dm_tx_request_enter(void);
void dm_tx_request_leave(void);

int dm_tx_configure(const char *journal_directory);
int dm_tx_begin(int shmid);
int dm_tx_write(int shmid, const char *path, const void *data, size_t length, int64_t offset);
/* As above, but also validate the caller's borrowed descriptor against path.
 * Requires O_RDWR without O_APPEND; caller keeps fd and namespace stable.
 * A failed validation makes this owner's transaction abort-only.
 */
int dm_tx_write_fd(int shmid, const char *path, int fd, const void *data,
		size_t length, int64_t offset);
/* Bind root-relative descriptor names for one handler on the current worker.
 * Bindings and strings are borrowed until leave; keep descriptors open and
 * names stable. No nesting. Enter requires the active transaction owner.
 * Always leave, including handler failures. These scopes route writes only:
 * callers must still provide transaction isolation and session serialization.
 */
typedef struct dm_tx_target {
	int fd;
	const char *path;
} dm_tx_target;
int dm_tx_enter(int shmid, const dm_tx_target *targets, size_t count);
void dm_tx_leave(void);

int dm_tx_commit(int shmid);
int dm_tx_abort(int shmid);
int dm_tx_disconnect(int shmid);
int dm_tx_blocked(void);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
