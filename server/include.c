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

#include "srv_index.h"					/* index description */
#include "node.h"						/* description of last accesed node */
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
extern int split_node(int64_t, NODE *, SPLIT *);
extern int get_node(int64_t, NODE *, SPLIT *);
extern int node_search(char *, int, int64_t, int, SPLIT *, NODE *);
extern int get_datafile_desc(FILES *);

int include(char *cmd, int c_off, char **ret)

{
	int tmp;						/* temporary, misc. usage */
	int idxno;
	int i;
	int fileno;
	int offs;

	int64_t node;					/* the offset to read from */
	int64_t rptr;					/* record pointer */

	char buff[NODESIZE];			/* output buffer */
	char ikey[64];					/* internal rep of key */
	char *cptr;

	INDEX *idx;						/* structure for the dest index */

	NODE cur_node;					/* the last accesed node */

	FILES *fptr;

	SPLIT cur_index;
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
	node = idx->_rootpos;
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
	i = min(strlen(cptr)-1, idx->_keylen);
	memset(ikey, '\0', sizeof(ikey));
	memcpy(ikey, cptr, i);
	*(ikey+idx->_keylen) = fileno + 1;
	put_ll(ikey+idx->_keylen+1, rptr);

	cur_index._keylen = idx->_keylen;
	cur_index._idxchan = idx->_idxchan;
	cur_index._curleaf = cur_node._isleaf;		/* save the leaf info */
	cur_index._curnode = node;					/* current node position */
	cur_index._rootpos = idx->_rootpos;
	cur_index._prntnode = cur_node._parent;		/* save parent node */

	while (1) {
		if ((i = get_node(node,&cur_node,&cur_index)) < 0)
			goto done;
		offs = node_search(ikey,0,0ll,cur_node._isleaf & LEAF,
						&cur_index, &cur_node);
		if (cur_node._isleaf & LEAF)
			break;								/* found the right leaf */
		node = cur_node._kids[offs];			/* go to proper kid */
	}

	if ((cur_node._isleaf & ~LEAF) == N_KEYS) {
		if ((i = split_node(node, &cur_node, &cur_index)) < 0) {
			goto done;
		}
		pthread_mutex_lock(&(idx->_mutex));
		idx->_rootpos = cur_index._rootpos;
		pthread_mutex_unlock(&(idx->_mutex));
	} else {
		*buff = ++cur_node._isleaf;
		tmp = (idx->_keylen + MISC_LEN) * N_KEYS;		/* length of buff */
		memcpy(buff+1,(char *)cur_node._keys,tmp);		/* put keys in */
		llseek(idx->_idxchan,node,SEEK_SET);				/* move to cur node */
		tmp++;
		if (write(cur_index._idxchan,buff,tmp) < tmp) {
			i = ENODWRT;
			goto done;
		}
	}
	tmp = sprintf(cmd, "0|%d|%"PRId64"|", offs, node);
	memcpy(cmd+tmp, ikey, idx->_keylen+MISC_LEN);
	i = tmp + idx->_keylen + MISC_LEN;
	if (dbgsw) {
		fprintf(stderr, "include final packet ->");
		fwrite(cmd, 1, i, stderr);
		fprintf(stderr, "<-, i = %d, keylen = %d, tmp = %d\n", i, idx->_keylen, tmp);
		fflush(stderr);
	}

done:
	fl_lock(&idx->_lock, LOCK_UN);
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
