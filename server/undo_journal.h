/* Licensed under GPL-2.0-or-later. */
#ifndef DATAMAN_UNDO_JOURNAL_H
#define DATAMAN_UNDO_JOURNAL_H
#include <stddef.h>
#include <stdint.h>

typedef struct dm_undo dm_undo;

/* Standalone prototype: caller MUST serialize use of the journal directory and
 * exclude other access to the database root during a transaction and recovery.
 * Begin records the canonical database root and its identity in the journal;
 * recovery needs only the persistent journal directory, not a client session.
 * Existing regular files only, addressed relative to that root (e.g. files/a);
 * no symlink components, traversal, create, rename, unlink, or explicit
 * truncation. Writes may extend files. Server startup uses recovery; live
 * transaction writes are not integrated yet.
 *
 * Returns 0 on success, -1 with errno on failure. A write failure makes the
 * transaction abort-only. Commit failure requires close + recovery; do not
 * infer that it aborted. Close only releases resources, leaving recovery work.
 * Successful commit/abort consumes the transaction logically, but callers must
 * still close it. Recovery is mandatory before permitting new database access.
 * Limits: 1 MiB per write, 128 writes, 64 MiB journal. Zero-length writes are no-ops.
 */
int dm_undo_begin(const char *journal_directory, const char *database_root, dm_undo **out);
int dm_undo_write(dm_undo *tx, const char *name, const void *data,
		size_t length, int64_t offset);
/* Descriptor-bound variant: require the root-relative target to match fd's
 * device/inode before journaling or writing. fd must be O_RDWR, not O_APPEND;
 * it is borrowed and its position is unchanged. Caller prevents concurrent
 * close/reuse of fd and namespace changes for the entire transaction.
 * Identity mismatch reports ESTALE and makes the transaction abort-only.
 * Validation also applies to zero-length writes.
 */
int dm_undo_write_fd(dm_undo *tx, const char *name, int fd, const void *data,
		size_t length, int64_t offset);
int dm_undo_commit(dm_undo *tx);
int dm_undo_abort(dm_undo *tx);
int dm_undo_recover(const char *journal_directory);
void dm_undo_close(dm_undo *tx);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
