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
#include <arpa/inet.h>		/* byteorder functions */

#include "srv_index.h"
#include "node.h"
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
extern int read_node(int64_t,NODE *,int);
extern int upd_idx(INDEX *,NODE*,int,int64_t, int64_t, char *, char **);
extern char *substr(char *,int,int);

int get_current(char *cmd, int c_off, char **ret)

{
	int i,j;							/* misc usage */
	int cnt;							/* loop counter */
	int keylength;						/* misc usage */
	int idxno;
	int diff;
	int size;

	uint16_t compare16;
	uint32_t compare32;
	int64_t node;						/* the node to search for */

	char bsw,sw;						/* boolean values */
	char *tkey;							/* temporary key */
	char *rptr;

	INDEX *idx;							/* index to be operated */
	NODE cur_node;

	rptr = NULL;
	*ret = NULL;

	idxno = atoi(cmd+c_off);					/* get the global index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((idx = _indices + idxno) == NULL)
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	if ((tkey = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EIDXNOO);
	node = strtoll(tkey, NULL, 0);
	if ((tkey = strchr(tkey, '|') + 1) == (char *)1)
		return(EIDXNOO);
	j = atoi(tkey);
	if ((tkey = strchr(tkey, '|') + 1) == (char *)1)
		return(EIDXNOO);
	keylength = idx->_keylen + MISC_LEN;		/* internal length of the key */
	bsw = 0;							/* default to not found */
	sw = 0;								/* on prelim pass */

	if (idx->_keylen < 4)
		compare16 = htons(*(uint16_t *)tkey);
	else
		compare32 = htonl(*(uint32_t *)tkey);

	fl_lock(&idx->_lock, LOCK_SH);

	while (1) {
		if ((i = read_node(node,&cur_node,idxno)) < 0)
			goto done;
		for(cnt = j;cnt <= N_KEYS;cnt++) {
			i = cnt * keylength;						/* offset to first of key */
			if (*(cur_node._keys+i) == '\0')	/* get the key */
				break;
			switch (idx->_keylen) {
				case 1:
					diff = *tkey - *(cur_node._keys+i);
					size = sizeof(char);
					break;
				case 2:
					diff = compare16 - htons(*(uint16_t *)(cur_node._keys+i));
					size = sizeof(uint16_t);
					break;
				case 3:
					diff = compare16 - htons(*(uint16_t *)(cur_node._keys+i));
					if (!diff)
						diff = *(tkey+sizeof(uint16_t)) - *(cur_node._keys+i+sizeof(uint16_t));
					size = sizeof(uint16_t)+sizeof(char);
					break;
				default:
					diff = compare32 - htonl(*(uint32_t *)(cur_node._keys+i));
					size = sizeof(uint32_t);
					break;
			}

			if (!diff)
				diff = memcmp(tkey+size, cur_node._keys+i+size, keylength-size);
			if (diff <= 0) {
				if (diff == 0)
					bsw = TRUE;				/* matched key */
				break;							/* quit the loop */
			}
		}
        if (bsw)								/* found? */
			break;								/* break outer loop */
		if (!sw) {
			sw = 1;								/* have made prelim pass */
			node = idx->_rootpos;				/* search from the root */
			j = 0;								/* start from beg of node */
			continue;							/* start the loop */
		}
		if (cur_node._isleaf & LEAF) {			/* at a leaf? */
			i = FALSE;
			goto done;
		}
		node = cur_node._kids[cnt];             /* next kid to search */
    }
	if (!(i = upd_idx(idx,&cur_node,cnt,node, 0, cmd, &rptr))) {
        tkey = substr(cur_node._keys,cnt*keylength,(cnt+1)*keylength-1); /* get key to remove */
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, tkey);					/* remove it */
		fl_lock(&idx->_lock, LOCK_SH);
        free(tkey);								/* free temp key */
	}
done:
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(i);									/* evrything worked */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
