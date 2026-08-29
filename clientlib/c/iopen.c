/* ***************************************************************
 *
 * PROCEDURE:	iopen.c
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
 * this procedure opens any index file. There may be a maximum of six open
 * at any single time.
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
#include <malloc.h>
#include <string.h>

#include "index.h"
#include "globs.h"
#include "client_internal.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"

extern INDEX _indices[6];				/* the currently opened indices */

extern char *substr(char *, int, int);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

#define FALSE 0
#define TRUE  1

static void send_iclose(int idxno)
{
	char snd_buf[32];
	char *ret_buf;

	// we don't care if the iclose fails on the other side.  we're closed
	// and if the server wastes space, that's it's problem.

	sprintf(snd_buf, "%d|%d|", ICLOSE, idxno);
	ret_buf = db_send(snd_buf, strlen(snd_buf), __FILE__);
	free(ret_buf);
}

int iopen(char *index, int mode)
{
    int idx;                    /* loop counter */
    int x;                      /* use only because open requires it */
	int i;

    char *buff;               /* read the file names from the index */
	char *cptr;

	x = -1;
	for (idx = 0; idx < 6; idx++) {
		if (!strlen(_indices[idx]._idxname) && x == -1) {
			x = idx;
			continue;
		}
		if (!strcmp(index, _indices[idx]._idxname)) {
			db_err(0, "%s: index named %s already open\n",
							_progname, index);
			return FALSE;
		}
	}

	if (x == -1) {
		db_err(0, "%s: too many open indices\n", _progname);
		return FALSE;
	}
	idx = x;
/*
 * send the message and wait for the return
 */
	if ((i = asprintf(&cptr, "%d|%s|%s|", IOPEN, index, _root)) < 0) {
		db_err(ENOALLOC, "%s: can't allocate message buff", _progname);
		return(-1);
	}

	buff = db_send(cptr, i,  __FILE__);
	free(cptr);
	cptr = NULL;

	if (!buff)
		return FALSE;

	i = atoi(buff);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: server error in iopen", _progname);
		free(buff);
		return FALSE;
	}
/*
 * parse the message
 */
	int tmp_idxno = 0;
	int tmp_keylen;
	int tmp_nfiles;

	cptr = buff;
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_idxno = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_keylen = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_nfiles = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;

	_indices[idx]._files = calloc(tmp_nfiles, sizeof(FILES));
	if (!_indices[idx]._files) {
		free(buff);
		send_iclose(tmp_idxno);
		memset(&_indices[idx], 0, sizeof(_indices[idx]));
		return FALSE;
	}

	for (i = 0; i < tmp_nfiles; i++) {
		_indices[idx]._files[i]._fname = strdup(cptr);
		if (!_indices[idx]._files[i]._fname) {
			for (int k = 0; k < i; k++)
				free(_indices[idx]._files[k]._fname);
			free(_indices[idx]._files);
			free(buff);
			send_iclose(tmp_idxno);
			memset(&_indices[idx], 0, sizeof(_indices[idx]));
			return FALSE;
		}
		_indices[idx]._files[i]._fno = i;
		cptr += strlen(cptr) + 1;
	}
	_indices[idx]._idxno = tmp_idxno;
	_indices[idx]._keylen = tmp_keylen;
	_indices[idx]._nfiles = tmp_nfiles;
	strncpy(_indices[idx]._idxname, index, sizeof(_indices[idx]._idxname)-1);
	_indices[idx]._wrmode = mode;
	free(buff);
	return TRUE;

invalid_response:
	db_err(EINVMSG, "%s: invalid IOPEN response", _progname);
	free(buff);
	if (tmp_idxno > 0)
		send_iclose(tmp_idxno);
	memset(&_indices[idx], 0, sizeof(_indices[idx]));
	return FALSE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
