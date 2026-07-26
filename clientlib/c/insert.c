/* ***************************************************************
 *
 * PROCEDURE:	insert.c
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
 * this procedure inserts a new record into a data file logically 
 * before or after the current master record in that index.  The calling
 * sequence is:
 *      insert(fmt,mode,idx);
 * where fmt is the format number to insert
 *       mode is BEFORE or AFTER
 *       idx is the index to do the insert on
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

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "index.h"              /* index description */
#include "m_params.h"           /* master file description */
#include "globs.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

extern INDEX cur_index;         /* the current operating index */

extern void in_rec(int, char *);
extern void out_rec(int);
extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

void insert(int fmt, int mode, char *ixname)
{

	INDEX *idx;							/* the insert index structure */

	int tmp;						/* misc usage */

	char cmd[128];
	char *buff;
	char *ptr;

	if (cur_index._wrmode)
    	out_rec(MASTER);			/* write out the current record */
	idx = findex(ixname);
	if (!idx->_wrmode)
		db_err(0, "%s: index %s not opened for update\n",
						_progname, idx->_idxname);
	if (fmt < 1 || fmt > idx->_files[idx->_fno]._filedesc->n_rformats)
		db_err(EBADFMT, "%s: insert error: fmt=%d, file=%s\n",
						_progname, fmt, idx->_files[idx->_fno]._fname);

	if (!idx->_curkey)					/* can't insert around nothing */
		db_err(ENOGET, "%s: insert error", _progname);

	sprintf(cmd, "%d|%d|%d|%d|%d|%"PRId64"|", INSERT, fmt, mode,
					idx->_idxno, idx->_fno, idx->_rptr);
	buff = db_send(cmd, strlen(cmd), __FILE__);

	tmp = atoi(buff);
	if (tmp < 0)
		db_err(tmp, "%s: insert error", _progname);

	m_fmt = fmt;
	m_chan = idx->_files[idx->_fno]._fno;
	m_fdesc = idx->_files[idx->_fno]._filedesc;
	tmp = idx->_files[idx->_fno]._filedesc->record_desc[fmt-1].rf_len;

	ptr = strchr(buff, '|') + 1;
	m_cur = idx->_rptr = strtoll(ptr, NULL, 0);

	ptr = malloc(tmp);
	memset(ptr, ' ', tmp);
    cur_index = *idx;					/* save the new current index */
    in_rec(MASTER, ptr);				/* read in the empty record */
	free(ptr);
	free(buff);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
