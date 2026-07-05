/* ***************************************************************
 *
 * PROCEDURE:	protect.c
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
 * this routine 'protects' or checks the current record to the caller
 * it's calling sequence is:
 * 	protect(ixname) else
 * it's internal sequence is:
 * 	if (!prtct(ixname)) ;
 * you can not protect a master record if you have not done an initial get
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
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>

#include "globs.h"
#include "m_params.h"
#include "index.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void in_rec(int, char *);
extern void db_err(int, char *, ...);
extern void add_protect(int, int, int);

#define TRUE	1
#define FALSE	0

int db_prtct(char *ixname)
{
	
	int i, tmp;
	int fmt;

	char cmd[128];
	char *buff;
	char *ptr;

	INDEX *idx;

/*
 * you don't flush the current record when you call protect
 * because part of the idea is to read in the -most current-
 * disk version.  you don't want to write it, then get it!
 */
/*
 * for transaction processing, a new record has a negative number.
 * you can't protect a record that doesn't exist in the database
 * yet, and no one else can have it anyway, so just say ok.
 */
	if (ixname) {
		idx = findex(ixname);
		if (idx->_rptr < 0)
			return(TRUE);
		sprintf(cmd, "%d|%d|%d|%"PRId64"|", PROTECT, idx->_idxno, idx->_fno, idx->_rptr);
	} else
		sprintf(cmd, "%d|%d|%"PRId64"|", PROTECT, -w_chan, w_cur);

	buff = db_send(cmd, strlen(cmd), __FILE__);

	tmp = atoi(buff);
	if (tmp < 0)
		db_err(tmp, "%s: error in protect", _progname);
	else if (tmp == 0) {
		free(buff);
		return(FALSE);
	}

	ptr = buff;
	for(i = 0; i < 5; i++) {
		ptr = strchr(ptr, '|') + 1;
		if (i == 0)
			fmt = atoi(ptr);
	}

	if (ixname) {
		m_fdesc = idx->_files[idx->_fno]._filedesc;
		m_head = idx->_files[idx->_fno]._hlen;
		m_cur = idx->_rptr;
		m_chan = idx->_fno;
		m_fmt = fmt;
		in_rec(MASTER, ptr);
		add_protect(idx->_idxno, idx->_fno, idx->_rptr);
	} else {
		in_rec(WORK, ptr);
		add_protect(-w_chan, 0, w_cur);
	}
	free(buff);
	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
