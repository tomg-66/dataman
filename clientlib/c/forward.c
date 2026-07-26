/* ***************************************************************
 *
 * PROCEDURE:	forward.c
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
 *				March 2002
 *				Tom Green
 *				Modified to use the command interface to the
 *				server.
 ************************************************************* */

/*
 * this routine releases the current record in the database and moves
 * to the next logical record in the database.
 * the calling sequence (using the #define) is:
 *      forward(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to forward the work file).  the internal sequence is:
 *      if (db_fwd(idx_name)) ;
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

extern int in_xact;

extern INDEX cur_index;					/* the current operation index */

extern void out_rec(int);
extern void in_rec(int, char *);
extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

int db_fwd(char *idx_name)

{
	int i;
	int fmt;

	int64_t curr;
    INDEX *idx;							/* the index structure */

	char cmd[128];
	char *buff;
	char *cptr;

	if (idx_name) {
		idx = findex(idx_name);			/* get the index */
		if (in_xact && idx->_rptr < 0)
			return(FALSE);
		if (cur_index._wrmode)
			out_rec(MASTER);
		if (idx->_curkey == NULL)
			db_err(ENOGET, "%s: error in forward", _progname);
		if (in_xact && idx->_rptr < 0)
			return(FALSE);
		sprintf(cmd, "%d|%d|%d|%"PRId64"|", FORWARD, idx->_idxno, idx->_fno, idx->_rptr);
	} else {
		sprintf(cmd, "%d|%d|%"PRId64"|", FORWARD, -w_chan, w_cur);
		out_rec(WORK);
	}

	buff = db_send(cmd, strlen(cmd), __FILE__);

	i = atoi(buff);
	if (i < 0)
		db_err(i, "%s: error in forward", _progname);
	else if (i == 0) {
		free(buff);
		return(FALSE);					/* no next record in this file */
	}

	cptr = strchr(buff, '|') + 1;
	curr = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;

	if (idx_name) {
		idx->_rptr = m_cur = curr;
		cur_index = *idx;
		m_fdesc = idx->_files[idx->_fno]._filedesc;
		m_head = idx->_files[idx->_fno]._hlen;
		m_fmt = fmt;
		in_rec(MASTER, cptr);
	} else {
		w_cur = curr;
		w_fmt = fmt;
		in_rec(WORK, cptr);
	} 
	free(buff);
	return(TRUE);						/* give the ok signal */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
