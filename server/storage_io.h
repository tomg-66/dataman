/* Checked storage I/O. Licensed under GPL-2.0-or-later. */
#ifndef DATAMAN_STORAGE_IO_H
#define DATAMAN_STORAGE_IO_H

#include <stddef.h>
#include <stdint.h>

/* Return 0 on completion, -1 with errno on failure. Retry EINTR and short
 * transfers. A failure can leave a partially written range: these helpers
 * provide neither atomicity nor durability. Callers retain storage locks.
 * Positional operations leave the descriptor's current offset unchanged.
 * Unexpected EOF and zero-progress writes report EIO; invalid ranges EINVAL.
 */
int dm_storage_read_at(int fd, void *buffer, size_t length, int64_t offset);
int dm_storage_write_at(int fd, const void *buffer, size_t length, int64_t offset);

/* Database mutation entry point. The server installs its router before workers
 * start; standalone tools default to direct checked I/O. Journal recording and
 * replay MUST use dm_storage_write_at to avoid routing recursively.
 * Router installation is startup-only; do not change it while I/O is running.
 */
typedef int (*dm_storage_router)(int, const void *, size_t, int64_t);
void dm_storage_set_router(dm_storage_router router);
int dm_storage_mutate_at(int fd, const void *buffer, size_t length, int64_t offset);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
