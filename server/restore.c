/* ***************************************************************
 *
 * PROCEDURE:	restore.c
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
 *				Mon Apr  1 2002
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
 * and record (not necessarily the same thing)
 * the calling sequence is:
 *      restore(index_name)
 * where index_name is the name of the index to update.
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

#include "srv_index.h"
#include "node.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#include <stdio.h>

#define TRUE    1
#define FALSE   0
#define LEAF    0200
#define ROOT	1

extern int idx_cnt;

extern INDEX *_indices;
extern int dbgsw;

extern void rm_key(int, int, char *);
extern int read_node(int64_t, NODE *,int);
extern int upd_idx(INDEX *, NODE *, int, int64_t, int64_t, char *, char **);
extern char *substr(char *,int,int);

int restore(char *cmd, int c_off, char **ret)

{
	int tmp;							/* misc usage */
	int i,j;							/* misc usage */
	int cnt;							/* loop counter */
	int idxno;

	int64_t node;						/* the node to search for */
	int64_t recptr;					/* record pointer */

	char bsw,sw;						/* boolean values */
	char *tkey;							/* temporary key */
	char *rptr;

	INDEX *idx;							/* index to be operated */
	NODE cur_node;

	*ret = NULL;
	idxno = atoi(cmd+c_off);			/* get the global index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(ENOINDEX);
	if ((idx = _indices + idxno) == NULL)
		return(EINVMSG);
	if (!idx->_refcnt)
		return(EIDXNOO);

	if ((rptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	node = strtoll(rptr, NULL, 0);		/* node key originally was in */

	if ((rptr = strchr(rptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	j = atoi(rptr);						/* offset in node */
	if (j < 0 || j > N_KEYS)
		return(EINVMSG);
	if ((rptr = strchr(rptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	recptr = strtoll(rptr, NULL, 0);	/* saved record pointer */
	if ((tkey = strchr(rptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	tmp = idx->_keylen + MISC_LEN;		/* internal length of the key */
	bsw = 0;							/* default to not found */
	sw = 0;								/* on prelim pass */
	rptr = NULL;

	fl_lock(&idx->_lock, LOCK_SH);

	while (1) {
		if ((i = read_node(node, &cur_node, idxno)) < 0)
			goto done;
		for(cnt = j;cnt <= N_KEYS;cnt++) {
			i = cnt * tmp;						/* offset to first of key */
			if (*(cur_node._keys+i) == '\0')	/* get the key */
				break;
			if ((i = memcmp(tkey,cur_node._keys+i,tmp)) <= 0) {
				if (i == 0)
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

	if (!(i = upd_idx(idx,&cur_node,cnt, node, recptr, cmd, &rptr))) {
        tkey = substr(cur_node._keys,cnt*tmp,(cnt+1)*tmp-1); /* get key to remove */
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, tkey);			/* remove it */
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
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
