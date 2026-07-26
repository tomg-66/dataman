/* ***************************************************************
 *
 * PROCEDURE:	clear.c
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
 * 				changed to send command to server side
 ************************************************************* */

/*
 * this routine clears the protection on a record that was 'checked out'
 * with a call to protect.  it's calling sequence is:
 *      clear(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to back the work file).
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

extern INDEX cur_index;					/* the current operation index */

extern void out_rec(int);
extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);

extern void db_err(int, char *, ...);
extern void del_protect(int, int, int);

void clear(char *idx_name)

{
	int i;

    INDEX *idx;							/* the index structure */

	char cmd[128];
	char *buff;

	if (idx_name) {
		if (cur_index._wrmode)
			out_rec(MASTER);
		idx = findex(idx_name);			/* get the index */
		if (idx->_rptr < 0)
			return;
		sprintf(cmd, "%d|%d|%d|%"PRId64"|", CLEAR, idx->_idxno, idx->_fno, idx->_rptr);
	} else {
		sprintf(cmd, "%d|%d|%"PRId64"|", CLEAR, -w_chan, w_cur);
		out_rec(WORK);
	}

	buff = db_send(cmd, strlen(cmd), __FILE__);

	i = atoi(buff);
	if (i < 0)
		db_err(i, "%s: error in clear", _progname);
	free(buff);

	if (idx_name)
		del_protect(idx->_idxno, idx->_fno, idx->_rptr);
	else
		del_protect(-w_chan, 0, w_cur);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
