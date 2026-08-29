/* Internal helpers for parsing responses from the database server. */

#include <string.h>

#include "client_internal.h"

bool dm_next_field(char **cursor)
{
	char *separator;

	if (!cursor || !*cursor)
		return false;

	separator = strchr(*cursor, '|');
	if (!separator)
		return false;

	*cursor = separator + 1;
	return true;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 ft=c fdm=marker:
 */
