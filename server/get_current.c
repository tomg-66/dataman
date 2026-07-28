/* ***************************************************************
 *
 * PROCEDURE:	get_current.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Tom Green
 *				Mar 18 2002
 *				modified to include file locking and other stuff
 *				as needed for making it a server routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Mon Jul 27 08:48:40 PM MDT 2026
 *				modified to use the new V2 indexing scheme.
 *				this is for dataman 4.0.0
 *
 ************************************************************* */
/*
 * this restores the index state to the last accessed key
 * the calling sequence is (using the #define):
 *      get_current(index_name)
 * where index_name is the name of the index to update.
 * the internal calling sequence is:
 *      if (g_curr(index_name)) ;
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
#include <stdlib.h>

#include "index_v2.h"
#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define TRUE    1
#define FALSE   0
#define LEAF    0200
#define ROOT	1

extern int idx_cnt;

extern INDEX *_indices;

extern void rm_key(int, int, char *);
extern void put_ll(void *, int64_t);
extern int64_t get_ll(void *);
extern int upd_idx(INDEX *, const unsigned char *, uint16_t, uint64_t,
		const INDEX_V2_CURSOR *, char *, char **);

int get_current(char *cmd, int c_off, char **ret)

{
	int i;								/* misc usage */
	int idxno;

	uint16_t file_id = 0;
	uint64_t record_offset = 0;

	char *cptr;							/* temporary key */
	char *rptr;
	unsigned char matched_key[MAX_KEY_SIZE];
	char system_key[KEY_BUFFER_SIZE];

	INDEX *idx;							/* index to be operated */

	rptr = NULL;
	*ret = NULL;

	INDEX_V2_CURSOR cursor;

	idxno = atoi(cmd+c_off);					/* get the global index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if (_indices == NULL)
		return(ENOINDEX);
	idx = _indices + idxno;
	if (!idx->_refcnt)
		return(EIDXNOO);

	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	cursor.generation = strtoll(cptr, NULL, 0);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	cursor.node_offset = strtoll(cptr, NULL, 0);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	cursor.entry_index = atoi(cptr);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);

	fl_lock(&idx->_lock, LOCK_SH);

	if ((unsigned char)cptr[idx->_keylen] == 0) {
		fl_lock(&idx->_lock, LOCK_UN);
		return(EINVMSG);
	}
	file_id = (uint16_t)(unsigned char)cptr[idx->_keylen] - 1;
	if (file_id >= (uint16_t)idx->_f_cnt) {
		fl_lock(&idx->_lock, LOCK_UN);
		return(EINVMSG);
	}
	record_offset = (uint64_t)get_ll(cptr + idx->_keylen + 1);

	if (!index_v2_find(idx->_idxchan, cptr, true, &file_id, &record_offset, matched_key, &cursor)) {
		fl_lock(&idx->_lock, LOCK_UN);
		return(0);
	}

	i = upd_idx(idx, matched_key, file_id, record_offset, &cursor, cmd, &rptr);
	if (i == 0) {
		memcpy(system_key, matched_key, idx->_keylen);
		system_key[idx->_keylen] = (char)(file_id + 1);
		put_ll(system_key + idx->_keylen + 1, (int64_t)record_offset);
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, system_key);
		fl_lock(&idx->_lock, LOCK_SH);
	}

	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(i);									/* evrything worked */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
