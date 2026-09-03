/* ***************************************************************
 *
 * PROCEDURE:	restore.c
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
 * this routine restores an index to it's last SAVEd state
 * the calling sequence is:
 *      restore (idx_name);
 * where idx_name is the name of the index whose state you want to restore
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
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "globs.h"
#include "index.h"
#include "m_params.h"
#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

DATAMAN_HIDDEN extern int in_rec(int, char *, size_t, INDEX *, int, int);
DATAMAN_HIDDEN extern int out_rec(int);
DATAMAN_HIDDEN extern INDEX *findex(char *);
DATAMAN_HIDDEN extern char *db_send(char *, int, char *);
DATAMAN_HIDDEN extern char *db_send_len(char *, int, char *, size_t *);
DATAMAN_HIDDEN extern void db_err(int, char *, ...);

DATAMAN_API extern char *substr(const char *, const int, const int);

#define TRUE	1
#define FALSE	0

DATAMAN_API int db_restore(char *idx_name)
{
	INDEX *idx;                         /* pointer to index */

	int i;							/* misc usage */

	char cmd[128];
	char *buff;
	char *ptr;
	char *tmp_key;
	size_t response_len;

	if (cur_index._wrmode) {
		if (!out_rec(MASTER)) {				/* write out cur record */
			db_err(EOUTREC, "%s: %s: error writing record", _progname, __func__);
			return FALSE;
		}
	}

	if ((idx = findex(idx_name)) == NULL) {				/* get the index */
		db_err(EIDXNOO, "%s: %s: index named %s is not open", _progname, __func__, idx_name);
		return FALSE;
	}
    if (idx->_savptr == NULL) {
		db_err(0, "%s: in restore, index %s has not been saved\n",
					_progname, idx->_idxname);
		return FALSE;
	}

	sprintf(cmd, "%d|%d|%"PRIu64"|%d|%"PRId64"|", RESTORE, idx->_idxno,
					idx->_savptr->_savnode, idx->_savptr->_savoffs,
					idx->_savptr->_savrec);
	i = strlen(cmd);
	memcpy(cmd+i, idx->_savptr->_savkey, idx->_keylen+KEY_HEADER_LENGTH);
	i += idx->_keylen+KEY_HEADER_LENGTH;

	buff = db_send_len(cmd, i, __FILE__, &response_len);

	if (!buff)
		return FALSE;

	i = atoi(buff);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: restore error", _progname);
		free(idx->_savptr->_savkey);
		free(idx->_savptr);
		idx->_savptr = NULL;
		free(buff);
		return(FALSE);
	}
/*
 * parse the return and update the globals
 */
	int tmp_fmt;
	int tmp_offs;
	int tmp_chan;
	int64_t tmp_rptr;
	uint64_t tmp_generation;
	uint64_t tmp_curnode;

	ptr = buff;
	if (!dm_next_field(&ptr))
		goto invalid_response;
	tmp_fmt = atoi(ptr);
	if (!dm_next_field(&ptr))
		goto invalid_response;
	tmp_generation = strtoull(ptr, NULL, 0);
	if (!dm_next_field(&ptr))
		goto invalid_response;
	tmp_curnode = strtoull(ptr, NULL, 0);
	if (!dm_next_field(&ptr))
		goto invalid_response;
	tmp_offs = atoi(ptr);
	if (!dm_next_field(&ptr))
		goto invalid_response;

	tmp_key = substr(ptr, 0, idx->_keylen+KEY_HEADER_LENGTH);
	if (!tmp_key) {
		free(buff);
		return FALSE;
	}

	tmp_chan = *(tmp_key+idx->_keylen) - 1;
	tmp_rptr = idx->_savptr->_savrec;
	ptr += idx->_keylen + KEY_HEADER_LENGTH;
	if ((size_t)(ptr-buff) > response_len ||
			!in_rec(MASTER, ptr, response_len-(size_t)(ptr-buff),
				idx, tmp_fmt, tmp_chan)) {
		db_err(EINREC, "%s: %s: cant read record", _progname, __func__);
		free(buff);
		free(tmp_key);
		return FALSE;
	}

	m_fmt = tmp_fmt;
	idx->_generation = tmp_generation;
	idx->_curnode = tmp_curnode;
	idx->_offs = tmp_offs;
	free(idx->_savptr->_savkey);
	free(idx->_savptr);
	idx->_savptr = NULL;

	if (idx->_curkey)
		free(idx->_curkey);

	idx->_curkey = tmp_key;

	idx->_fno = tmp_chan;

	m_chan = tmp_chan;
	idx->_rptr = m_cur = tmp_rptr;
	m_fdesc = idx->_files[m_chan]._filedesc;
	m_head = idx->_files[m_chan]._hlen;

	cur_index = *idx;

	free(buff);
	return(TRUE);

invalid_response:
	db_err(EINVMSG, "%s: invalid RESTORE response", _progname);
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
