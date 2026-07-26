/* ***************************************************************
 *
 * PROCEDURE:	get_last.c
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
 * this routine will retreive from the named index the last key in the
 * index.  the calling sequence (using the #define) is:
 *       get_last(index_name)
 * where index_name is an index that was previously opened with
 * a call to iopen.  the internal sequence is
 *      if (g_last(index_name));
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

#include "globs.h"
#include "index.h"
#include "m_params.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

//extern int in_xact;

#define TRUE    1
#define FALSE   0

extern INDEX *findex(char *);
extern char *substr(char *,int,int);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);
extern void in_rec(int, char *);
extern void out_rec(int);
extern int64_t get_ll(char *);

int db_g_last(char *index_name)
{
    INDEX *idx;                         /* index pointer */

    int i;			/* misc usage */

	char msg[128];
	char *ret;
	char *cptr;

	int64_t recno;

    idx = findex(index_name);      /* get the index */
/*
	recno = get_ll(idx->_curkey+idx->_keylen+1);
	if (in_xact && recno < 0)
		return(FALSE);
*/
	if (cur_index._wrmode)
		out_rec(MASTER);

	sprintf(msg, "%d|%d|", GET_LAST, idx->_idxno);
	i = strlen(msg);
	ret = db_send(msg, i, __FILE__);

	i = atoi(ret);
	if (i < 0)
		db_err(i, "%s: error during get_last", _progname);
	else if (i == 0) {
		free(ret);
		return(FALSE);
	}
/*
 * parse the return and update the globals
 */
	cptr = strchr(ret, '|') + 1;
	m_fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	idx->_curnode = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	idx->_offs = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	if (idx->_curkey)
		free(idx->_curkey);
	idx->_curkey = substr(cptr, 0, idx->_keylen+KEY_HEADER_LENGTH);
	idx->_fno = *(idx->_curkey+idx->_keylen) - 1;
	cptr += idx->_keylen + KEY_HEADER_LENGTH;

	m_chan = *(idx->_curkey+idx->_keylen) - 1;
	idx->_rptr = m_cur = get_ll(idx->_curkey+idx->_keylen+1);
	m_fdesc = idx->_files[m_chan]._filedesc;
	m_head = idx->_files[m_chan]._hlen;
	cur_index = *idx;
	in_rec(MASTER, cptr);
	free(ret);

	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
