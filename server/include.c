/* ***************************************************************
 *
 * PROCEDURE:	include.c
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
 * 				tomg
 * 				changes to accomidate the command, file locking,
 * 				and other stuff for server side function.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Mon Jul 27 08:17:24 PM MDT 2026
 *				tomg
 *				updated to use the V2 index copy on write model.
 *
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <inttypes.h>

#include "index_v2.h"
#include "srv_index.h"					/* index description */
#include "lock.h"
#include "errors.h"
#include "misc.h"

#if !defined min
#define min(a, b)		((a) > (b) ? (b) : (a))
#endif

extern int idx_cnt;
extern int dbgsw;

extern INDEX *_indices;					/* the opened indices */

extern void put_ll(char *, int64_t);
extern int get_datafile_desc(FILES *);

int include(char *cmd, int c_off, char **ret)

{
	int tmp;						/* temporary, misc. usage */
	int idxno;
	int i;
	int fileno;
	int64_t rptr;					/* record pointer */

	char ikey[64];					/* internal rep of key */
	char *cptr;

	INDEX *idx;						/* structure for the dest index */

	FILES *fptr;
/*
 * the incoming command has:
 *	source index number
 *	source file number
 *	destination index number
 *	destination file number
 *	record pointer to include
 *	key
 */
	*ret = NULL;
	i = atoi(cmd+c_off);
	if (i < 0 || i > idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+i) == NULL)					/* this is the source index */
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	fileno = atoi(cptr);
	if (fileno < 0 || fileno >= idx->_f_cnt)
		return(EINVMSG);
	if (!idx->_files[fileno]->_hlen)
		return(ENOTOPEN);
	if ((fptr = idx->_files[fileno]) == NULL)		/* this is the file of the source */
		return(EINVMSG);

	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	idxno = atoi(cptr);
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+idxno) == NULL)				/* this is now the dest index */
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	fileno = atoi(cptr);
	if (fileno < 0 || fileno >= idx->_f_cnt)
		return(EINVMSG);

	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);

	fl_lock(&idx->_lock, LOCK_EX);
	pthread_mutex_lock(&(idx->_mutex));

/*
 * if the header length for this file is zero, it hasn't been opened
 * before, and needs to be now.
 */
/*
 * what to do here.... if we call get_datafile_desc that is easy, it
 * is already written, but it has to read the file a couple of times
 * to do it's thing.  on the other hand, if we copy the description
 * from the already open file, we have to duplicate the code to parse
 * the header...   faster but duplicated code?  slower but more compact
 * code.... that is always the tradeoff no?
 *
 * also, with using the pre-written routine, the first two fields of
 * the command string are no longer important....
 *
 * here is the start to the duplicated code....

	if (!idx->_files[fileno]._hlen) {
		if ((idx->_files[fileno]._chan = my_open(idx->_files[fileno]._fname, O_RDWR)) < 0) {
			pthread_mutex_unlock(&(idx->_mutex));
			fl_lock(idx->_idxchan, LOCK_UN, 1, 1);
			return(ENOTOPEN);
		}
		idx->_files[fileno]._longest = fptr->_longest;
		idx->_files[fileno]._hlen = fptr->_hlen;
		idx->_files[fileno]._desc = malloc(fptr->_hlen);
		memcpy(idx->_files[fileno]._desc, fptr->_desc, fptr->_hlen);
	}
	fptr = idx->_files+fileno;
 *
 *
 * and here is the easy way....
 */
	fptr = idx->_files[fileno];
	if (!idx->_files[fileno]->_hlen) {
		if ((i = get_datafile_desc(fptr)) < 0) {
			pthread_mutex_unlock(&(idx->_mutex));
			goto done;
		}
	}

	pthread_mutex_unlock(&(idx->_mutex));

	rptr = strtoll(cptr, NULL, 0);						/* record pointer in the file */
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1) {	/* this is now pointing to the key */
		i = EINVMSG;
		goto done;
	}
/*
 * put together the key.  the format is
 * 0-keylen bytes is the key
 * 1 byte as the file offset.  it is saved in the key as 1 thru n
 * instead of 0 thru n-1
 * PTR_SIZE bytes for the record pointer.
 */
	i = (int)min(strlen(cptr)-1, (size_t)idx->_keylen);
	memset(ikey, '\0', sizeof(ikey));
	memcpy(ikey, cptr, i);
	*(ikey+idx->_keylen) = fileno + 1;
	put_ll(ikey+idx->_keylen+1, rptr);

	uint16_t disk_keylen, disk_file_count;
	uint32_t root_crc;
	uint64_t root, generation;

	if (!index_v2_insert(idx->_idxchan, ikey, fileno, rptr) ||
			!index_v2_read_header(idx->_idxchan, &disk_keylen,
				&disk_file_count, &root, &root_crc, &generation)) {
		i = ENODWRT;
		goto done;
	}
	pthread_mutex_lock(&(idx->_mutex));
	idx->_rootpos = (int64_t)root;
	idx->_generation = generation;
	pthread_mutex_unlock(&(idx->_mutex));
	tmp = sprintf(cmd, "0|0|");
	memcpy(cmd+tmp, ikey, idx->_keylen + KEY_HEADER_LENGTH);
	i = tmp + idx->_keylen + KEY_HEADER_LENGTH;

done:
	fl_lock(&idx->_lock, LOCK_UN);
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
