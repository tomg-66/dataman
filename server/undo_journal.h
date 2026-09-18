/* ***************************************************************
 *
 * PROCEDURE:	undo_journal.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:42:10 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Fri May  5 15:04:16 MDT 2006
 * 				started specifying bit sizes in variable decls
 * 				that need it.
 * 				tomg
 *
 ************************************************************* */
/* Licensed under GPL-2.0-or-later. */

#ifndef DATAMAN_UNDO_JOURNAL_H
#define DATAMAN_UNDO_JOURNAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct dm_undo dm_undo;

/*
 * Standalone prototype: caller MUST serialize use of the journal directory and
 * exclude other access to the database root during a transaction and recovery.
 * Begin records the canonical database root and its identity in the journal;
 * recovery needs only the persistent journal directory, not a client session.
 * Byte writes address existing regular files below the root (e.g. files/a),
 * may extend them, and exclude blobs/. Blob creation/replacement/removal/rename
 * use dm_undo_blob. No symlink components or traversal. Server startup uses
 * recovery; server owner handlers route writes through this API.
 *
 * Returns 0 on success, -1 with errno on failure. A write failure makes the
 * transaction abort-only. Commit failure requires close + recovery; do not
 * infer that it aborted. Close only releases resources, leaving recovery work.
 * Successful commit/abort consumes the transaction logically, but callers must
 * still close it. Recovery is mandatory before permitting new database access.
 * Byte writes are limited to 1 MiB per call. Blob snapshots stream in 64 KiB
 * chunks, with no fixed blob-size, record-count, or journal-size cap; file-offset
 * and available storage/resource limits apply. Zero-length writes are no-ops.
 */

int dm_undo_begin(const char *journal_directory, const char *database_root, dm_undo **out);
int dm_undo_write(dm_undo *tx, const char *name, const void *data,
		size_t length, int64_t offset);

/*
 * Descriptor-bound variant: require the root-relative target to match fd's
 * device/inode before journaling or writing. fd must be O_RDWR, not O_APPEND;
 * it is borrowed and its position is unchanged. Caller prevents concurrent
 * close/reuse of fd and namespace changes for the entire transaction.
 * Identity mismatch reports ESTALE and makes the transaction abort-only.
 * Validation also applies to zero-length writes.
 */

int dm_undo_write_fd(dm_undo *tx, const char *name, int fd, const void *data,
		size_t length, int64_t offset);

/*
 * Logical blob snapshots: first state per name, streamed capture and replay.
 * Only private regular files directly below blobs/; no hardlinks or symlinks.
 * Restores contents, existence, mode/uid/gid, not inode numbers or extended
 * metadata. Blob files and their parent with xattrs/ACLs are rejected before
 * mutation. Caller excludes all external namespace access through recovery.
 */

#define DM_BLOB_REPLACE 1
#define DM_BLOB_REMOVE 2
#define DM_BLOB_RENAME 3

int dm_undo_blob(dm_undo *tx, int operation, const char *name, const char *dest,
		const void *data, size_t length);
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
