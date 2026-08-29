/* ***************************************************************
 *
 * PROCEDURE:	get_next.c
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
 * this routine will retreive from the named index the next sequential
 * key.  the calling sequence (using the #define) is:
 *       get_next(index_name)
 * where index_name is an index that was previously opened with
 * a call to iopen.  the internal sequence is
 *      if (g_next(index_name));
 * if no original key exists (i.e. no get has yet been performed), this
 * procedure will terminate the calling process.  IT IS ILLEGAL TO GET_NEXT,
 * OR GET_PRIOR IF NO GET HAS YET BEEN PERFORMED, OR THE LAST GOTTEN KEY
 * WAS "REMOVED".
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
#include <inttypes.h>

#include "globs.h"
#include "index.h"
#include "m_params.h"
#include "client_internal.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

#define TRUE    1
#define FALSE   0

extern int dbgsw;
extern int in_xact;

extern INDEX *findex(char *);
extern char *substr(char *,int,int);
extern char *db_send(char *, int, char *);
extern char *db_send_len(char *, int, char *, size_t *);
extern void db_err(int, char *, ...);
extern int in_rec(int, char *, size_t, INDEX *, int, int);
extern int out_rec(int);
extern int64_t get_ll(char *);

int db_g_next(char *index_name)
{
    INDEX *idx;                         /* index pointer */

    int i;			/* misc usage */

	char msg[128];
	char *ret;
	char *cptr;
	char *tmp_key;
	size_t response_len;

	int64_t recno;

	if (cur_index._wrmode) {
		if (!out_rec(MASTER)) {
			db_err(EOUTREC, "%s: error writing record", _progname);
			return FALSE;
		}
	}

    if ((idx = findex(index_name)) == NULL) {      /* get the index */
		db_err(EIDXNOO, "%s: Index named %s is not open", _progname, index_name);
		return FALSE;
	}
/*
 * if this key was inserted in this transaction then
 * it doesn't make sense to get the next one because
 * this key isn't yet in the index.
 */
	if (!idx->_curkey) {
		db_err(ENOGET, "%s: %s", _progname, __func__);
		return FALSE;
	}

	recno = get_ll(idx->_curkey+idx->_keylen+1);
	if (in_xact && recno < 0)
		return(FALSE);

    if (idx->_curkey == 0) {
        db_err(ENOGET, "%s: Can't get_next", _progname);
		return FALSE;
	}

	sprintf(msg, "%d|%d|%"PRIu64"|%"PRIu64"|%d|", GET_NEXT, idx->_idxno,
					idx->_generation, idx->_curnode, idx->_offs);
	i = strlen(msg);
	memcpy(msg+i, idx->_curkey, idx->_keylen+KEY_HEADER_LENGTH);
	i += idx->_keylen+KEY_HEADER_LENGTH;
	ret = db_send_len(msg, i, __FILE__, &response_len);

	if (!ret)
		return FALSE;

	i = atoi(ret);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: error during get_next", _progname);
		free(ret);
		return(FALSE);
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

	tmp_chan = *(tmp_key+idx->_keylen) - 1;
	cptr += idx->_keylen + KEY_HEADER_LENGTH;
	if ((size_t)(cptr-ret) > response_len ||
			!in_rec(MASTER, cptr, response_len-(size_t)(cptr-ret),
				idx, tmp_fmt, tmp_chan)) {
		db_err(EINREC, "%s: error reading record", _progname);
		free(ret);
		free(tmp_key);
		return FALSE;
	}

	free(ret);

	m_fmt = tmp_fmt;
	idx->_generation = tmp_generation;
	idx->_curnode = tmp_curnode;
	idx->_offs = tmp_offs;

	if (idx->_curkey)
		free(idx->_curkey);
	idx->_curkey = tmp_key;

	idx->_fno = tmp_chan;

	m_chan = tmp_chan;
	idx->_rptr = m_cur = get_ll(idx->_curkey+idx->_keylen+1);
	m_fdesc = idx->_files[m_chan]._filedesc;
	m_head = idx->_files[m_chan]._hlen;
	cur_index = *idx;

	return(TRUE);

invalid_response:
	db_err(EINVMSG, "%s: invalid GET_NEXT response", _progname);
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
