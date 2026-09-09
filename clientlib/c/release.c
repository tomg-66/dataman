/* ***************************************************************
 *
 * PROCEDURE:	release.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 *				Tom Green
 *				modified to use call interface to server
 ************************************************************* */

/*
 * this routine is very similar to forward.  The first defference is that
 * when this routine comes to the end of a data file it tries to get the
 * first record in the next named data file, if it is at the en of the data
 * file list it returns false.  The second difference is that it works only
 * on a work file.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>

#include "globs.h"
#include "w_params.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"

#define FALSE   0
#define TRUE    1

DATAMAN_HIDDEN extern char **_fnames;					/* the file names */
DATAMAN_HIDDEN extern int _fileno;						/* the current file number */
DATAMAN_HIDDEN extern short _maxfil;						/* maximum number of files */

DATAMAN_HIDDEN extern int in_rec(int, char *, size_t, INDEX *, int, int);
DATAMAN_HIDDEN extern int out_rec(int);
DATAMAN_HIDDEN extern char *db_send(char *, int, char *);
DATAMAN_HIDDEN extern char *db_send_len(char *, int, char *, size_t *);
DATAMAN_HIDDEN extern void db_err(int, char *, ...);

DATAMAN_HIDDEN int dm_in_rec_reload(int, char *, size_t, INDEX *, int, int);

DATAMAN_API int db_rel()
{
	int i;

    char cmd[256];                     /* path to file */
	char *ptr;
	char *cptr;
	size_t response_len;

	int tmp_fmt;
	int tmp_fileno;
	int new_file;
	uint64_t tmp_cur;
	uint64_t tmp_prev;
	uint64_t tmp_next;

	new_file = !w_next;
	tmp_fileno = _fileno;
    if (!out_rec(WORK)) {
		db_err(EOUTREC, "%s: RELEASE", _progname);
		return FALSE;
	}

    if (new_file) {
		if (++tmp_fileno == _maxfil) {
			return (FALSE);
		}
		sprintf(cmd, "%d|%d|0|%s/files/%s|", RELEASE, w_chan, _root,
						_fnames[tmp_fileno]);
	} else
		sprintf(cmd, "%d|%d|%"PRId64"|", RELEASE, w_chan, w_next);
/*
 * send the command and wait for the response.
 */
	ptr = db_send_len(cmd, strlen(cmd), __FILE__, &response_len);

	if (!ptr)
		return FALSE;

	if (dbgsw) {
		fprintf(stderr, "release returns ->%s<-\n", ptr);
		fflush(stderr);
	}

	cptr = ptr;
	i = atoi(cptr);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: error in release", _progname);
		free(ptr);
		return FALSE;
	}
/*
 * parse the response and save the appropriate stuff
 */
	if (!dm_next_field(&cptr) || !dm_next_field(&cptr))
		goto invalid_response;
	tmp_fmt = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_cur = strtoll(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_prev = strtoll(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_next = strtoll(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
/*
 * ok, done!
 */
	if ((size_t)(cptr-ptr) > response_len ||
			!(new_file ?
				dm_in_rec_reload(WORK, cptr, response_len-(size_t)(cptr-ptr),
					NULL, tmp_fmt, w_chan) :
				in_rec(WORK, cptr, response_len-(size_t)(cptr-ptr),
					NULL, tmp_fmt, w_chan))) {
		db_err(EINREC, "%s: RELEASE", _progname);
		free(ptr);
		return FALSE;
	}

	w_fmt = tmp_fmt;
	w_cur = tmp_cur;
	w_prev = tmp_prev;
	w_next = tmp_next;
	_fileno = tmp_fileno;
	_file = new_file;

	free(ptr);
    return (TRUE);

invalid_response:
	db_err(EINVMSG, "%s: invalid release response", _progname);
	free(ptr);
	return FALSE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
