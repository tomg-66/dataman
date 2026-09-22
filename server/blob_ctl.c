/* ***************************************************************
 *
 * PROCEDURE:	blob_ctl.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Wed Aug  16 16:16:33 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * either unlink, hide, or unhide all the files that are blobs to
 * this record
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The GNU General Public License is contained in the file COPYING.
 */
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <sys/types.h>

#include "misc.h"
#include "errors.h"
#include "storage_io.h"
#include "undo_journal.h"

int blob_ctl(char *root, char *f_name, int fmt, int64_t recno, int op)
{
	struct dirent **namelist;
	char directory[1024], source[2048], dest[2048], prefix[1024];
	int count, result = 0;
	const char *name = strrchr(f_name, '/');
	name = name ? name + 1 : f_name;
	if (op != UNLINK && op != HIDE && op != UNHIDE && op != CLEANUP)
		return(EINVMSG);
	if (snprintf(directory, sizeof(directory), "%s/blobs", root) >= (int)sizeof(directory) ||
			snprintf(prefix, sizeof(prefix), "%s%s.%d.%"PRId64".",
				(op == UNHIDE || op == CLEANUP) ? "." : "", name, fmt, recno) >= (int)sizeof(prefix))
		return(ENOBLOB);
	count = scandir(directory, &namelist, NULL, alphasort);
	if (count < 0)
		return(ENOBLOB);
	for (int i = 0; i < count; i++) {
		const char *entry = namelist[i]->d_name;
		if (!result && !strncmp(entry, prefix, strlen(prefix))) {
			if (snprintf(source, sizeof(source), "%s/%s", directory, entry) >= (int)sizeof(source))
				result = ENOBLOB;
			else if (op == UNLINK || op == CLEANUP) {
				int routed = dm_storage_blob_route(DM_BLOB_REMOVE, source, NULL, NULL, 0);
				if (routed < 0 || (!routed && (dm_storage_namespace_check() < 0 || unlink(source) < 0)))
					result = EBLOBWRT;
			} else if (snprintf(dest, sizeof(dest), "%s/%s%s", directory,
					op == HIDE ? "." : "", op == UNHIDE ? entry + 1 : entry) >= (int)sizeof(dest))
				result = ENOBLOB;
			else {
				int routed = dm_storage_blob_route(DM_BLOB_RENAME, source, dest, NULL, 0);
				if (routed < 0 || (!routed && (dm_storage_namespace_check() < 0 || rename(source, dest) < 0)))
					result = EBLOBWRT;
			}
		}
		free(namelist[i]);
	}
	free(namelist);
	return(result);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
