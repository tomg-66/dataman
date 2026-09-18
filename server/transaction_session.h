/* ***************************************************************
 *
 * PROCEDURE:	transaction_session.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:40:06 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * Server-side journal ownership. GPL-2.0-or-later.
 */
#ifndef DATAMAN_TRANSACTION_SESSION_H
#define DATAMAN_TRANSACTION_SESSION_H
#include <stddef.h>
#include <stdint.h>

/*
 * Configure only after startup recovery, with lifetime journal ownership held.
 * One journal owner at a time; contention returns ENOLOCK without blocking a
 * worker. Caller must serialize requests within a session and exclude ALL other
 * database access before using begin/write. Wire dispatch provides this
 * exclusion and routes supported mutations.
 * Return 0 on success, otherwise a negative Dataman error.
 */
/* Admit an ordinary dispatch handler, including reads and maintenance. Calls
 * must be paired on the same worker; nesting is rejected. No mutex is retained
 * across the handler. An active journal blocks ALL ordinary handlers, including
 * its owner, unless they enter transaction-aware owner dispatch.
 */
int dm_tx_request_enter(void);
void dm_tx_request_leave(void);

int dm_tx_configure(const char *journal_directory);
int dm_tx_begin(int shmid);
int dm_tx_write(int shmid, const char *path, const void *data, size_t length, int64_t offset);
/*
 * As above, but also validate the caller's borrowed descriptor against path.
 * Requires O_RDWR without O_APPEND; caller keeps fd and namespace stable.
 * A failed validation makes this owner's transaction abort-only.
 */
int dm_tx_write_fd(int shmid, const char *path, int fd, const void *data,
		size_t length, int64_t offset);
/* Bind root-relative descriptor names for one handler on the current worker.
 * Bindings and strings are borrowed until leave; keep descriptors open and
 * names stable. No nesting. Enter requires the active transaction owner.
 * Zero targets admits a read-only handler; attempted writes fail closed.
 * Only one owner scope can run; finish/disconnect returns ENOLOCK until leave.
 * Use complete with the handler result, including on failure. These scopes route writes only:
 * callers must still provide transaction isolation and session serialization.
 */
typedef struct dm_tx_target {
	int fd;
	const char *path;
} dm_tx_target;

int dm_tx_enter(int shmid, const dm_tx_target *targets, size_t count);
/* Replace borrowed bindings inside an already admitted owner handler. */
int dm_tx_bind(const dm_tx_target *targets, size_t count);
void dm_tx_leave(void);
/* Release this worker scope; a negative handler result makes it abort-only. */
void dm_tx_complete(int result);

/*
 * Call before changing an index in an owner handler. Ordinary calls are no-ops.
 * Keep the INDEX object and descriptor alive until transaction completion.
 * Tracked cached roots/generations are reloaded before admission reopens.
 */
struct _idxbuf_;
int dm_tx_track_index(struct _idxbuf_ *index);

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
