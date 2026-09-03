/* ***************************************************************
 *
 * PROCEDURE:	back.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 * 				Tom Green
 * 				changed to use command and communicate with server.
 ************************************************************* */

/*
 * this routine releases the current record in the database and moves
 * to the previous logical record in the database.
 * the calling sequence (using the #define) is:
 *      back(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to back the work file).  the internal sequence is:
 *      if (bck(idx_name)) ;
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
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "globs.h"
#include "index.h"
#include "m_params.h"
#include "w_params.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"

#define TRUE    1
#define FALSE   0

DATAMAN_HIDDEN extern INDEX cur_index;					/* the current operation index */
DATAMAN_HIDDEN extern int in_xact;						/* transaction switch */

DATAMAN_HIDDEN extern int out_rec(int);
DATAMAN_HIDDEN extern int in_rec(int, char *, size_t, INDEX *, int, int);
DATAMAN_HIDDEN extern INDEX *findex(char *);
DATAMAN_HIDDEN extern char *db_send(char *, int, char *);
DATAMAN_HIDDEN extern char *db_send_len(char *, int, char *, size_t *);

DATAMAN_HIDDEN extern void db_err(int, char *, ...);

DATAMAN_API int db_bck(char *idx_name)
{
	int i;
	int fmt;

	int64_t curr;
    INDEX *idx;							/* the index structure */

	char cmd[128];
	char *buff;
	char *cptr;
	size_t response_len;

	if (idx_name && strlen(idx_name)) {
		if ((idx = findex(idx_name)) == NULL) {			/* get the index */
			return FALSE;
		}
		if (in_xact && idx->_rptr < 0)
			return(FALSE);
		if (cur_index._wrmode) {
			if (!out_rec(MASTER)) {
				db_err(EOUTREC, "In %s: error in back", _progname);
				return FALSE;
			}
		}
		if (idx->_curkey == NULL) {
			db_err(ENOGET, "%s: error in back", _progname);
			return FALSE;
		}
		if (in_xact && idx->_rptr < 0)
			return(FALSE);
		sprintf(cmd, "%d|%d|%d|%"PRId64"|", BACK, idx->_idxno, idx->_fno, idx->_rptr);
	} else {
		sprintf(cmd, "%d|%d|%"PRId64"|", BACK, -w_chan, w_cur);
		if (!out_rec(WORK)) {
			db_err(EOUTREC, "%s: error in back", _progname);
			return FALSE;
		}
	}

	buff = db_send_len(cmd, strlen(cmd), __FILE__, &response_len);

	if (!buff) {
		return FALSE;
	}

	i = atoi(buff);
	if (i < 1) {
		if (i < 0) {
			db_err(i, "%s: error in back", _progname);
		}
		free(buff);
		return(FALSE);					/* no next record in this file */
	}

	cptr = buff;
	if (!dm_next_field(&cptr))
		goto invalid_response;
	curr = strtoll(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	fmt = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;

	if (idx_name && strlen(idx_name)) {
		if ((size_t)(cptr-buff) > response_len ||
				!in_rec(MASTER, cptr, response_len-(size_t)(cptr-buff),
					idx, fmt, idx->_fno)) {
			db_err(EINREC, "Error reading master record", _progname);
			free(buff);
			return FALSE;
		}
		m_fdesc = idx->_files[idx->_fno]._filedesc;
		m_head = idx->_files[idx->_fno]._hlen;
		m_fmt = fmt;
		idx->_rptr = m_cur = curr;
		cur_index = *idx;
	} else {
		if ((size_t)(cptr-buff) > response_len ||
				!in_rec(WORK, cptr, response_len-(size_t)(cptr-buff),
					NULL, fmt, w_chan)) {
			db_err(EINREC, "Error reading work record", _progname);
			free(buff);
			return FALSE;
		}
		w_cur = curr;
		w_fmt = fmt;
	} 
	free(buff);
	return(TRUE);						/* give the ok signal */

invalid_response:
	db_err(EINVMSG, "%s: invalid back response", _progname);
	free(buff);
	return FALSE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
