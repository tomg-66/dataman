/* ***************************************************************
 *
 * PROCEDURE:	include.c
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
 *
 *				Mon Jul 17 18:31:52 MDT 2006
 *				added reference to new global in_xact and a new
 *				value on the end of the command to let the server
 *				know whether or not we are in a transaction.
 *				tomg
 ************************************************************* */

/*
 * this routine inserts a key pointing to the current master record
 * of one index file into another index (potentially the same) index
 * file.
 * the calling sequence is:
 *
 *      include(idx1,idx2,key);
 *
 *      where idx1 is the source of the record to insert, idx2 is the
 *      destination of the key.
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
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "globs.h"
#include "index.h"                      /* index description */
#include "../../server/dbfunc.h"
#include "../../server/misc.h"
#include "../../server/errors.h"

DATAMAN_HIDDEN extern INDEX *findex(char *);
DATAMAN_HIDDEN extern char *db_send(char *, int, char *);
DATAMAN_HIDDEN extern void db_err(int, char *, ...);
DATAMAN_HIDDEN extern void put_ll(void *, int64_t);

DATAMAN_HIDDEN extern int in_xact;

DATAMAN_API extern char *substr(const char *, const int, const int);
DATAMAN_API int db_include(char *idx1, char *idx2, char *key)
{

    int tmp;						/* temporary, misc. usage */
    char buff[128];					/* output buffer */
	char *cptr;
	char *ret;
	char *tmp_key;

    INDEX *idx_1;					/* structure for the first index */
	INDEX *idx_2;					/* same for second */

	if ((idx_1 = findex(idx1)) == NULL || (idx_2 = findex(idx2)) == NULL) {
		return FALSE;
	}

	if (!idx_2->_wrmode) {
		db_err(0, "%s: index %s not opened for update\n",
						_progname, idx_2->_idxname);
		return FALSE;
	}
/*
 * make sure the file currently referred to in idx_1 is also
 * found in idx_2
 */
	for (tmp = 0; tmp < idx_2->_nfiles; tmp++) {
		if (!strcmp(idx_1->_files[idx_1->_fno]._fname, idx_2->_files[tmp]._fname))
			break;
	}
	if (tmp >= idx_2->_nfiles) {
		db_err(0, "%s: Include error: file %s not a member of index %s\n",
				_progname, idx_1->_files[idx_1->_fno]._fname, idx_2->_idxname);
		return FALSE;
	}
	idx_2->_fno = tmp;

	if ((tmp = asprintf(&cptr, "%d|%d|%d|%d|%d|%"PRId64"|%s|", INCLUDE, idx_1->_idxno,
				idx_1->_fno, idx_2->_idxno, idx_2->_fno, idx_1->_rptr, key)) < 0) {
		db_err(ENOALLOC, "%s: Can't allocate command buffer", _progname);
		return FALSE;
	}
	ret = db_send(cptr, tmp, __FILE__);
	free(cptr);

	if (!ret)
		return FALSE;

	tmp = atoi(ret);
	if (tmp < 1) {
		if (tmp < 0)
			db_err(tmp, "%s: error in include",_progname);
		free(ret);
		return FALSE;
	}

	uint64_t tmp_generation;
	uint64_t tmp_curnode;
	int tmp_offs;

	cptr = ret;
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_generation = strtoull(cptr, NULL, 10);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_curnode = strtoull(cptr, NULL, 0);
	if (!dm_next_field(&cptr))
		goto invalid_response;
	tmp_offs = atoi(cptr);

	if (in_xact) {
		memset(buff, '\0', sizeof(buff));
		strcpy(buff, key);
		*(buff+idx_2->_keylen) = idx_2->_fno+1;
		put_ll(buff+idx_2->_keylen+1, idx_1->_rptr);
		cptr = buff;
	} else
		if (!dm_next_field(&cptr))
			goto invalid_response;

	tmp_key = substr(cptr, 0, idx_2->_keylen+KEY_HEADER_LENGTH);
	if (!tmp_key) {
		free(ret);
		return FALSE;
	}
	idx_2->_generation = tmp_generation;
	idx_2->_curnode = tmp_curnode;
	idx_2->_offs = tmp_offs;

	if (idx_2->_curkey)
		free(idx_2->_curkey);
	idx_2->_curkey = tmp_key;

	idx_2->_fno = *(idx_2->_curkey+idx_2->_keylen) - 1;
	if (cur_index._idxno == idx_2->_idxno)
		cur_index = *idx_2;
	free(ret);
	return TRUE;

invalid_response:
	db_err(EINVMSG, "%s: invalid INCLUDE response", _progname);
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
