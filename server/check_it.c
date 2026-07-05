/* ***************************************************************
 *
 * PROCEDURE:	check_it.c
 *
 * PROJECT:		dataman utilities
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Fri Aug 24 10:43:00 MDT 2007
 *				Tom Green
 *				re-wrote for *nix based and uses the newer
 *				functions.
 ************************************************************* */
/*
 * this procedure checks the contents of a data file to find the percentage
 * wasted space and returns that value as an integer.
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
#include <stdio.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "misc.h"
#include "file_desc.h"
#define DBERROR
#include "errors.h"

#define DELETED 0200					/* record deleted mask */

extern void rebuild(char *, char *, FILES *);

int check_it(char *root, char *filename, int max)

{
	int i;						/* just a counter */
	int ret;					/* return value */

	char path[1024];

	float used,unused;			/* used and unused counts */

	loff_t len;					/* record len */

	char chk;					/* check var */

	FILES file_desc;			/* file description */

	sprintf(path, "%s/files/%s", root, filename);
	file_desc._fname = path;
#if !defined __gnu_linux__
	endian_check();
#endif
/*
 * open the datafile and read the description.
 */
	if ((i = get_datafile_desc(&file_desc)) < 0) {
		fprintf(stderr, "Can't read description for file %s: %s\n",
						path, db_err_strings[-i]);
		exit(-1);
	}
	llseek(file_desc._chan, (loff_t)(2*PTR_LENGTH), SEEK_CUR);

	used = unused = 0.0;				/* not checked anything yet */

	while (1) {									/* go until done */
		if (read(file_desc._chan, &chk, 1) != 1)				/* eof?  */
			break;
		if (chk & DELETED) {					/* is the record deleted? */
			len = file_desc._filedesc->record_desc[(chk & ~DELETED)-1].rf_len;
			unused = unused + len + DATARECORD_HEADER_LENGTH;
		} else {
			len = file_desc._filedesc->record_desc[chk-1].rf_len;
			used = used + len + DATARECORD_HEADER_LENGTH;
		}
		len += 2 * PTR_LENGTH;								/* length of rec + pointers */
		llseek(file_desc._chan, len, SEEK_CUR);						/* seek to next record */
	}

	ret = (unused / (unused + used)) * 100.0;   /* calc unused percentage */
	if (ret > max) {
		printf("    Rebuilding file %s...\n", path);
		rebuild(root, filename, &file_desc);
	} else {
		close(file_desc._chan);
		ret = 0;
	}
/*
 * free up the parsed file description.  if you have the parsed file
 * description, then you also have the binary one and it needs to
 * be freed as well, and close the open channel too.
 */
	for (i = 0; i < file_desc._filedesc->n_rformats; i++)
		free(file_desc._filedesc->record_desc[i].field_sizes);
	free(file_desc._filedesc->record_desc);
	free(file_desc._filedesc);
	free(file_desc._desc);
	return(ret);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
