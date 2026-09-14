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

#endif
