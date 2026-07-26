/* ***************************************************************
 *
 * PROCEDURE:	delete.c
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
 *				modified for command servicing by the server.
 *
 *				Mon Jul 17 20:09:22 MDT 2006
 *				added a new value at the end of the command to
 *				let the server know if we are in a transaction
 *				or not.  also a new global in_xact.
 *				tomg
 ************************************************************* */

/*
 * this routine deletes the current record pointed to by the named index.
 * the calling sequence is:
 *      delete(idx_name);
 * where idx_name is any index that is open and has a record.  the current
 * record usually then becomes the one logically prior to the deleted one.
 * if the deleted record is the first of the file, the current record
 * becomes the record that logically followed the deleted record.
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

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "index.h"
#include "m_params.h"
#include "globs.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"


extern void in_rec(int, char *);
extern void out_rec(int);
extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

extern int in_xact;

void delete(char *idx_name)
{

	INDEX *idx;					/* the insert index structure */

	int tmp;					/* misc usage */

	char cmd[128];				/* command to send */
    char *buff;					/* output buffer */
	char *ptr;

	if (dbgsw) {
		fprintf(stderr, "entered delete\n");
		fflush(stderr);
	}

	if (cur_index._wrmode)
    	out_rec(MASTER);			/* write out the current record */

    idx = findex(idx_name);			/* get the index structure */
    if (idx->_rptr == 0)			/* can't del if we don't have it */
		db_err(ENOGET, "%s: delete error", _progname);

	sprintf(cmd, "%d|%d|%d|%"PRId64"|%d|", DELETE, idx->_idxno,
					idx->_fno, idx->_rptr,in_xact);
	buff = db_send(cmd, strlen(cmd), __FILE__);

	tmp = atoi(buff);
	if (tmp < 0)
		db_err(tmp, "%s: delete error", _progname);

	if (dbgsw) {
		fprintf(stderr, "returnd buffer is %s\n", buff);
		fflush(stderr);
	}

	ptr = strchr(buff, '|') + 1;
	idx->_rptr = m_cur = strtoll(ptr, NULL, 0);
	ptr = strchr(ptr, '|') + 1;
	m_fmt = atoi(ptr);
	ptr = strchr(ptr, '|') + 1;
	m_chan = idx->_fno;
	m_fdesc = idx->_files[m_chan]._filedesc;
	m_head = idx->_files[m_chan]._hlen;	
	cur_index = *idx;
	if (dbgsw) {
		fprintf(stderr, "m_fmt = %d, m_cur = %"PRId64"\n", m_fmt, m_cur);
		fflush(stderr);
	}
	in_rec(MASTER, ptr);
	free(buff);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
