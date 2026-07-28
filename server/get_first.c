/* ***************************************************************
 *
 * PROCEDURE:	get_first.c
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
 *				Tom Green
 *				Sun Mar 17 2002
 *				modified to include file locking and other stuff
 *				as needed for making it a server routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Tue Jul 28 08:57:37 AM MDT 2026
 *				tomg
 *				modified to use the V2 index model.
 *
 ************************************************************* */

/* this retrieves the lowest order key from an index */

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
#include <stdlib.h>
#include <malloc.h>

#include "index_v2.h"
#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define FALSE   0

extern int idx_cnt;

extern INDEX *_indices;

extern void rm_key(int, int, char *);
extern void put_ll(void *, int64_t);
extern int upd_idx(INDEX *, const unsigned char *, uint16_t, uint64_t,
		const INDEX_V2_CURSOR *, char *, char **);

int get_first(char *cmd, int c_off, char **ret)
{
	int tmp;					/* misc usage */
	int idxno;

	uint64_t record_ptr;
	uint16_t file_id;

	char *rptr;
	unsigned char first_key[MAX_KEY_SIZE] = {0};
	char system_key[KEY_BUFFER_SIZE];

	INDEX *idx;					/* the current index */
	INDEX_V2_CURSOR cursor;

	*ret = NULL;
	idxno = atoi(cmd+c_off);	/* get the current index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if (!_indices || (idx = _indices+idxno) == NULL)		/* this index */
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	rptr = NULL;
	fl_lock(&idx->_lock, LOCK_SH);
	while(1) {
		if (!index_v2_first(idx->_idxchan, &file_id, &record_ptr, first_key, &cursor)) {
			tmp = FALSE;
			goto done;
		}

		tmp = upd_idx(idx, first_key, file_id, record_ptr, &cursor, cmd, &rptr);
		if (tmp != 0)
			break;

		memcpy(system_key, first_key, idx->_keylen);
		system_key[idx->_keylen] = (char)(file_id+1);
		put_ll(system_key+idx->_keylen+1, (int64_t)record_ptr);
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, system_key);
		fl_lock(&idx->_lock, LOCK_SH);
	}

done:
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(tmp);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
