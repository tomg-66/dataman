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

void blob_ctl(char *root, char *f_name, int fmt, int64_t recno, int op)
{
	struct dirent **namelist;

	int i, j;
	int offs;
	int off2;

	char pathname[1024];
	char destname[1024];
	char filename[64];

	strcpy(filename, f_name);

	strcpy(pathname, root);
	strcat(pathname, "/blobs");

	if (op == UNHIDE) {
		offs = 1;
		*filename = '.';
	} else
		offs = 0;
	sprintf(filename+offs, "%s.%d.%"PRId64".*", f_name, fmt, recno);
	offs = strlen(pathname);

	if ((i = scandir(pathname, &namelist, NULL, alphasort)) > 0) {
		*(pathname+offs) = '/';
		offs++;
		if (op == UNHIDE || op == HIDE) {
			memcpy(destname, pathname, offs);
			off2 = offs;
			if (op == HIDE) {
				*(destname+off2) = '.';
				off2++;
			}
		} else if (op == CLEANUP) {
			*(pathname+offs) = '.';
			offs++;
		}
		for(j = 0; j < i; j++) {
			if (!fnmatch(filename, namelist[j]->d_name, 0)) {
				strcpy(pathname+offs, namelist[j]->d_name);
				if (op == UNLINK || op == CLEANUP)
					unlink(pathname);
				else {
					if (op == UNHIDE)
						strcpy(destname+off2, namelist[j]->d_name+1);
					else
						strcpy(destname+off2, namelist[j]->d_name);
					rename(pathname, destname);
				}
			}
			free(namelist[j]);
		}
		free(namelist);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
