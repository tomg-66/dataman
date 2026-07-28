/* ***************************************************************
 *
 * PROCEDURE:	get
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY
 * 				Tom Green
 *				Feb 26 2002
 *				modified to include file locking and other stuff
 *				as needed for making it a server routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Mon Jul 27 07:55:27 PM MDT 2026
 *				tomg
 *				implemented get using the new V2 index routines
 *				this is now version 4.0.0
 *
 ************************************************************* */
/*
 * this routine will retreive from the named index the key passed
 * the calling sequence (using the #define) is:
 *		get(index_name,key)
 * the internal call is:
 *		if g_key(index_name,key) ;
 * where index_name is an index that was previously opened with
 * a call to iopen.  if the key is found the key is read into the
 * index structure, and the master file record is read into memory.
 *
 * this is always a -non- exact match, ie the file number and record
 * pointer aren't required for this.  the client side works around this
 * by checking if they are requrired and it re-routes the call through
 * get_current.
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
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#include "index_v2.h"
#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

extern void rm_key(int, int, char *);
extern void put_ll(void *, int64_t);
extern int upd_idx(INDEX *, const unsigned char *, uint16_t, uint64_t,
		const INDEX_V2_CURSOR *, char *, char **);

extern int dbgsw;
extern int idx_cnt;

extern INDEX *_indices;

int get(char *cmd, int c_off, char **ret)
{

	INDEX *index;	/* the current index description */

	int i;
	int idxno;				/* the index number */
	uint64_t record_ptr;
	uint16_t file_offset;
	size_t key_length;
	INDEX_V2_CURSOR cursor;

	char *key;
	char *cptr;				/* pointer to test key */
	char *rptr;				/* return pointer from upd_idx */
	unsigned char lookup_key[MAX_KEY_SIZE];
	unsigned char matched_key[MAX_KEY_SIZE];
	char system_key[KEY_BUFFER_SIZE];

	if (dbgsw) {
		fprintf(stderr, "enter GET, cmd = %s\n", cmd);
		fflush(stderr);
	}
	rptr = NULL;
	*ret = NULL;

	idxno = atoi(cmd+c_off);
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((key = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	if ((cptr = strchr(key, '|')) == NULL)
		return(EINVMSG);
	key_length = (size_t)(cptr - key);
	*cptr = '\0';

	if (dbgsw) {
		fprintf(stderr, "in GET, idxno = %d\n", idxno);
		fflush(stderr);
	}

	if (!_indices)
		return(ENOINDEX);

	if ((index = _indices+idxno) == NULL)					/* find the index number */
		return(ENOINDEX);
	if (dbgsw) {
		fprintf(stderr, "idx->_keylen = %d\n"
						"idx->_idxchan = %d\n"
						"idx->_f_cnt = %d\n"
						"idx->_refcnt = %d\n"
						"idx->_rootpos = %"PRId64"\n"
						"idx->_idxname = %s\n"
						"idx->_rootdir = %s\n",
						index->_keylen, index->_idxchan, index->_f_cnt,
						index->_refcnt, index->_rootpos, index->_idxname,
						index->_rootdir);
		fflush(stderr);
	}

	if (!index->_refcnt)
		return(EIDXNOO);
	if (key_length > (size_t)index->_keylen)
		return(EINVMSG);
	memset(lookup_key, 0, sizeof(lookup_key));
	memcpy(lookup_key, key, key_length);

	fl_lock(&index->_lock, LOCK_SH);

	while (1) {								/* do until we get a key */
		if (!index_v2_find(index->_idxchan, lookup_key, false, &file_offset,
				&record_ptr, matched_key, &cursor)) {
			fl_lock(&index->_lock, LOCK_UN);
			return(0);
		}

		i = upd_idx(index, matched_key, file_offset, record_ptr, &cursor, cmd, &rptr);
		if (i == 0) {
			memcpy(system_key, matched_key, index->_keylen);
			system_key[index->_keylen] = (char)(file_offset + 1);
			put_ll(system_key + index->_keylen + 1, (int64_t)record_ptr);
			fl_lock(&index->_lock, LOCK_UN);
			rm_key(idxno, NOXACT, system_key);
			fl_lock(&index->_lock, LOCK_SH);
			continue;
		}

		*ret = rptr;
		fl_lock(&index->_lock, LOCK_UN);
		return(i);								/* return value */
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
