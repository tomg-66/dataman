/* ***************************************************************
 *
 * PROCEDURE:	remove.c
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
 * 				March 2002
 * 				Tom Green
 * 				added arguments, and file locking and other
 * 				stuff as needed for the server side.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Mon Jul 27 08:31:04 PM MDT 2026
 *				tomg
 *				modified to use the new V2 indexing system.
 *				this is for dataman 4.0.0
 *
 ************************************************************* */
/* Server-facing key lookup and removal for v2 indexes.
 * this routine removes from the named index the named key
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "index_v2.h"
#include "misc.h"

extern int idx_cnt;
extern INDEX *_indices;
extern int64_t get_ll(void *);
extern void put_ll(void *, int64_t);

/*
 * this removes the key from the index.  it will be called when the record
 * that the key points at has been deleted from the datafile, or explicitly
 * from remove
 */
int rm_key(int idxno, int xsw, char *key)
{
	INDEX *index;
	uint16_t file_id = 0, keylen, file_count;
	uint64_t record_offset = 0, root, generation;
	uint32_t root_crc;
	bool exact;
	int result = 0;

	if (idxno < 0 || idxno >= idx_cnt)
		return(ENOINDEX);
	index = _indices + idxno;
	if (!index->_refcnt)
		return(EIDXNOO);

	exact = key[index->_keylen] != 0;
	if (exact) {
		file_id = (unsigned char)key[index->_keylen] - 1;
		record_offset = (uint64_t)get_ll(key + index->_keylen + 1);
	}

	fl_lock(&index->_lock, LOCK_EX);

	if (!index_v2_find(index->_idxchan, key, exact, &file_id,
			&record_offset, key, NULL))
		goto done;

	memset(key + index->_keylen, 0, KEY_HEADER_LENGTH);
	key[index->_keylen] = (char)(file_id + 1);
	put_ll(key + index->_keylen + 1, (int64_t)record_offset);
	if (xsw) {
		result = index->_keylen + KEY_HEADER_LENGTH;
		goto done;
	}

	if (!index_v2_remove(index->_idxchan, key, file_id, record_offset) ||
			!index_v2_read_header(index->_idxchan, &keylen, &file_count,
				&root, &root_crc, &generation)) {
		result = ENODWRT;
		goto done;
	}

	pthread_mutex_lock(&index->_mutex);
	index->_rootpos = (int64_t)root;
	index->_generation = generation;
	pthread_mutex_unlock(&index->_mutex);
	result = 1;

done:
	fl_lock(&index->_lock, LOCK_UN);
	return(result);
}

int dbremove(char *cmd, int c_off, char **ret)
{
	char *cptr;
	int result, idxno, xsw;

	*ret = NULL;
	idxno = atoi(cmd + c_off);
	if (idxno < 0 || idxno >= idx_cnt)
		return(ENOINDEX);
	if ((cptr = strchr(cmd + c_off, '|')) == NULL)
		return(EINVMSG);
	xsw = atoi(++cptr);
	if ((cptr = strchr(cptr, '|')) == NULL)
		return(EINVMSG);
	cptr++;
	result = rm_key(idxno, xsw, cptr);
	if (result > 0) {
		result = sprintf(cmd, "0|%d|", result);
		if (xsw) {
			memcpy(cmd + result, cptr, _indices[idxno]._keylen + KEY_HEADER_LENGTH);
			result += _indices[idxno]._keylen + KEY_HEADER_LENGTH;
		}
	}
	return(result);
}

/* vim: set noet sw=4 sts=4 ts=4 fdm=marker: */
