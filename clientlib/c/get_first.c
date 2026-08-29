/* ***************************************************************
 *
 * PROCEDURE:	get_first.c
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
 * this routine will retreive from the named index the first key in the
 * index.  the calling sequence (using the #define) is:
 *       get_first(index_name)
 * where index_name is an index that was previously opened with
 * a call to iopen.  the internal sequence is
 *      if (g_frst(index_name));
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
#include <stdint.h>

#include "globs.h"
#include "index.h"
#include "m_params.h"
#include "client_internal.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

//extern int in_xact;

#define TRUE    1
#define FALSE   0

extern INDEX *findex(char *);
extern char *substr(char *,int,int);
extern char *db_send(char *, int, char *);
extern char *db_send_len(char *, int, char *, size_t *);
extern void db_err(int, char *, ...);
extern int in_rec(int, char *, size_t, INDEX *, int, int);
extern int out_rec(int);
extern int64_t get_ll(char *);

int db_g_frst(char *index_name)
{
    INDEX *idx;                         /* index pointer */

    int i;			/* misc usage */

	char msg[128];
	char *ret;
	char *cptr;
	char *tmp_key;
	size_t response_len;

	int64_t recno;

    if ((idx = findex(index_name)) == NULL) {     /* get the index */
		return FALSE;
	}

	if (cur_index._wrmode) {
		if (!out_rec(MASTER)) {
			db_err(EOUTREC, "%s: Error in out_rec", _progname);
			return FALSE;
		}
	}

	sprintf(msg, "%d|%d|", GET_FIRST, idx->_idxno);
	i = strlen(msg);
	ret = db_send_len(msg, i, __FILE__, &response_len);

	if (!ret)
		return FALSE;

	i = atoi(ret);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: error during get_first", _progname);
		free(ret);
		return FALSE;
	}
/*
 * parse the return and update the globals
 */
	int tmp_fmt;
	int tmp_offs;
	int tmp_chan;
	uint64_t tmp_generation;
	uint64_t tmp_curnode;

	cptr = ret;
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_fmt = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_generation = strtoull(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_curnode = strtoull(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_offs = atoi(cptr);
	if (!dm_next_field(&cptr))
		goto invalid_response;

	tmp_key = substr(cptr, 0, idx->_keylen+KEY_HEADER_LENGTH);
	if (!tmp_key) {
		free(ret);
		return FALSE;
	}

	cptr += idx->_keylen + KEY_HEADER_LENGTH;
	tmp_chan = *(tmp_key+idx->_keylen) - 1;

	if ((size_t)(cptr-ret) > response_len ||
			!in_rec(MASTER, cptr, response_len-(size_t)(cptr-ret),
				idx, tmp_fmt, tmp_chan)) {
		db_err(EINREC, "%s: Error in in_rec", _progname);
		free(ret);
		free(tmp_key);
		return FALSE;
	}

	m_fmt = tmp_fmt;
	idx->_generation = tmp_generation;
	idx->_curnode = tmp_curnode;
	idx->_offs = tmp_offs;

	if (idx->_curkey)
		free(idx->_curkey);
	idx->_curkey = tmp_key;
	idx->_fno = tmp_chan;
	idx->_rptr = m_cur = get_ll(idx->_curkey+idx->_keylen+1);
	m_chan = tmp_chan;
	m_fdesc = idx->_files[m_chan]._filedesc;
	m_head = idx->_files[m_chan]._hlen;
	cur_index = *idx;
	
	free(ret);
	return(TRUE);

invalid_response:
	db_err(EINVMSG, "%s: invalid GET_FIRST response", _progname);
	free(ret);
	return FALSE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
