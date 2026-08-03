/* ***************************************************************
 *
 * PROCEDURE:	get.c
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
 * 				Tom Green
 * 				modified to use call interface to server.
 *
 * 				Tue Jul 28 10:48:53 PM MDT 2026
 * 				tomg
 * 				modified to use the V2 index
 *
 ************************************************************* */

/*
 * this routine will retreive from the named index the key passed
 * the calling sequence (using the #define) is:
 *      get(index_name,key)
 * the internal call is:
 *      if g_key(index_name,key) ;
 * where index_name is an index that was previously opened with
 * a call to iopen.  if the key is found the key is read into the
 * index structure, and the master file record is read into memory.
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
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>

#include "index.h"
#include "globs.h"
#include "m_params.h"
#include "../../server/dbfunc.h"
#include "../../server/misc.h"
#include "proto.h"					/* need for definition of type key */
#include "../../server/errors.h"

#define TRUE    1
#define FALSE   0

extern INDEX *findex(char *);
extern char *substr(char *, int, int);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);
extern int in_rec(int, char *);
extern int out_rec(int);
extern int64_t get_ll(char *);

int db_g_key(char *idx, key key_val)
{
    int i;								/* temporary */

	char cmd[128];
	char *buff;
	char *cptr;
	char *tmp_key;

	INDEX *index;

	if (cur_index._wrmode) {
		if (!out_rec(MASTER)) {					/* flush the current record */
			db_err(EOUTREC, "%s: error reading master record", _progname);
			return FALSE;
		}
	}

    if ((index = findex(idx)) == NULL) {					/* find the index number */
		db_err(EIDXNOO, "%s: %s: index named %d is not open", _progname, __func__, idx);
		return FALSE;
	}
/*
 * if this byte isn't a null (we should be getting a proper key)
 * this indicates that they are using the system KEY, and the
 * get_current function is better suited to that.
 */
    if (*(key_val+index->_keylen) != 0) {
		i = sprintf(cmd, "%d|%d|%" PRId64 "|%" PRId64 "|%d|", GET_CURRENT, index->_idxno,
						index->_generation, index->_curnode, index->_offs);
		memcpy(cmd+i, key_val, index->_keylen+KEY_HEADER_LENGTH);
		i += index->_keylen+KEY_HEADER_LENGTH;
	} else {
		i = sprintf(cmd, "%d|%d|%s|", GET, index->_idxno, key_val);
    }
/*
 * send the command and deal with the return.
 */
	buff = db_send(cmd, i, __FILE__);

	if (!buff)
		return FALSE;
/*
 * the first field of the return is an error code if necessary,
 * zero of the key wasn't found, or the length of the data record
 */
	i = atoi(buff);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: Error during GET", _progname);
		free(buff);
		return(FALSE);
	}
/*
 * parse the return and update the globals
   "%d|%d|%"PRIu64"|%"PRIu64"|%u|", len, ret, cursor->generation, cursor->node_offset, (unsigned)cursor->entry_index);
 */
	cptr = strchr(buff, '|') + 1;
	m_fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	index->_generation = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	index->_curnode = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	index->_offs = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;

	tmp_key = substr(cptr, 0, index->_keylen+KEY_HEADER_LENGTH);
	if (!tmp_key) {
		free(buff);
		return FALSE;
	}
	if (index->_curkey)
		free(index->_curkey);
	index->_curkey = tmp_key;

	index->_fno = *(index->_curkey+index->_keylen) - 1;
	cptr += index->_keylen + KEY_HEADER_LENGTH;

	m_chan = *(index->_curkey+index->_keylen) - 1;
	index->_rptr = m_cur = get_ll(index->_curkey+index->_keylen+1);
	m_fdesc = index->_files[m_chan]._filedesc;
	m_head = index->_files[m_chan]._hlen;
	cur_index = *index;
	if (!in_rec(MASTER, cptr)) {
		db_err(EINREC, "%s: Error reading master record", _progname);
		free(buff);
		return FALSE;
	}
	free(buff);

	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
