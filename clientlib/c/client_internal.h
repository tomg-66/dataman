#ifndef DATAMAN_CLIENT_INTERNAL_H
#define DATAMAN_CLIENT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "index.h"

#if defined(__GNUC__) || defined(__clang__)
#define DATAMAN_HIDDEN __attribute__((visibility("hidden")))
#else
#define DATAMAN_HIDDEN
#endif

DATAMAN_HIDDEN bool dm_next_field(char **cursor);
DATAMAN_HIDDEN int dm_in_rec_reload(int, char *, size_t, INDEX *, int, int);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 ft=c fdm=marker:
 */
