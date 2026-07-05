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

#define FALSE   0
#define TRUE    1

extern char **_fnames;					/* the file names */
extern int _fileno;						/* the current file number */
extern short _maxfil;						/* maximum number of files */

extern void in_rec(int, char *);			/* format the work record */
extern void out_rec(int);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

int db_rel()

{
	int i;

    char cmd[256];                     /* path to file */
	char *ptr;
	char *cptr;

    _file = FALSE;
    out_rec(WORK);
    if (!w_next) {
		if (++_fileno == _maxfil) {
			_fileno--;
			return (FALSE);
		}
		for (i = 0; i < w_fdesc->n_rformats; i++)
			free (w_fdesc->record_desc[i].field_sizes);
		free(w_fdesc->record_desc);
		free(w_fdesc);
		w_fdesc = NULL;
		sprintf(cmd, "%d|%d|0|%s/files/%s|", RELEASE, w_chan, _root,
						_fnames[_fileno]);
		_file = TRUE;
	} else
		sprintf(cmd, "%d|%d|%"PRId64"|", RELEASE, w_chan, w_next);
/*
 * send the command and wait for the response.
 */
	ptr = db_send(cmd, strlen(cmd), __FILE__);
	if (dbgsw) {
		fprintf(stderr, "release returns ->%s<-\n", ptr);
		fflush(stderr);
	}

	cptr = ptr;
	i = atoi(cptr);
	if (i < 0)
		db_err(i, "%s: error in release", _progname);
/*
 * parse the response and save the appropriate stuff
 */
	cptr = strchr(cptr, '|') + 1;
	cptr = strchr(cptr, '|') + 1;
	w_fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	w_cur = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	w_prev = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	w_next = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') +1 ;
/*
 * ok, done!
 */
	in_rec(WORK, cptr);					/* read record into memory */
	free(ptr);
    return (TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
