/* ***************************************************************
 *
 * PROCEDURE:	get_next.c
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
 * this routine will retreive from the named index the next sequential
 * key.  the calling sequence (using the #define) is:
 *       get_next(index_name)
 * where index_name is an index that was previously opened with
 * a call to iopen.  the internal sequence is
 *      if (g_next(index_name));
 * if no original key exists (i.e. no get has yet been performed), this
 * procedure will terminate the calling process.  IT IS ILLEGAL TO GET_NEXT,
 * OR GET_PRIOR IF NO GET HAS YET BEEN PERFORMED, OR THE LAST GOTTEN KEY
 * WAS "REMOVED".
 *
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
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>		/* byteorder functions */

#include "node.h"
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
extern int read_node(int64_t, NODE *, int);
extern int upd_idx(INDEX *, NODE *, int, int64_t, int64_t, char *, char **);
extern char *substr(char *,int,int);

int get_next(char *cmd, int c_off, char **ret)

{
	INDEX *idx;							/* index pointer */

	int keylen;							/* internal length of key */
	int cnt;							/* loop counter */
	int offs;							/* saved offset into node */
	int i;								/* misc usage */
	int idxno;
	int diff;
	int size;

	uint16_t compare16;
	uint32_t compare32;
	int64_t node;						/* the node pos */
	int64_t sav_node;					/* save the node info */

	char *tkey;							/* temporary key */
	char *rptr;
	char bsw;							/* boolean value */
	char pass;							/* pass counter */

	NODE cur_node;						/* the last accessed node */

	rptr = NULL;
	*ret = NULL;

	idxno = atoi(cmd+c_off);			/* get the index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+idxno) == NULL)
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);
/*
 * when searching for the current key so we can look for the next
 * we use the simple heuristic that it hasn't moved much.  we look
 * first in the node we last found it in, if it isn't found start
 * at the root.
 */
	if ((tkey = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	node = strtoll(tkey, NULL, 0);		/* search from here */
	if ((tkey = strchr(tkey, '|') + 1) == (char *)1)
		return(EINVMSG);
	offs = atoi(tkey);					/* offset of cur key? */
	if ((tkey = strchr(tkey, '|') + 1) == (char *)1)		/* actual original key to find */
		return(EINVMSG);
	keylen = idx->_keylen + MISC_LEN;	/* internal len of key */
	bsw = FALSE;						/* default value */
	pass = 0;							/* this is the first pass */

	if (idx->_keylen < 4)
		compare16 = htons(*(uint16_t *)tkey);
	else
		compare32 = htonl(*(uint32_t *)tkey);

	fl_lock(&idx->_lock, LOCK_SH);

	while(1) {											/* do until done */
		if ((i = read_node(node,&cur_node,idxno)) < 0)
			goto done;
		for (cnt = offs;cnt < N_KEYS;cnt++) {
			i = cnt * keylen;
			if (*(cur_node._keys+i) == '\0')			/* get key to check */
				break;									/* end of this node */
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
			if (!diff) {
			   diff = memcmp(tkey+size, cur_node._keys+i+size, keylen-size);
				if (diff == 0)
					bsw = TRUE;						/* the key is found */
				break;									/* continue search */
			}
		}
		if (bsw)
			break;									/* the key is found */
		if (pass)
			if (cur_node._isleaf & LEAF)
				break;								/* we're at a leaf */
		if (!pass)
			node = ROOT;							/* search from root node */
		else
			node = cur_node._kids[cnt];				/* next node to read */
		pass++;										/* make it true */
		offs = 0;									/* offset in node */
	}
	if (!bsw) {
		i = ERMKEY;							/* the key was removed! */
		goto done;							/* didn't find current key */
	}
	sav_node = node;
	offs = cnt;								/* save the node offset */

	while (1) {                                         /* do a lot? */
		pass = cur_node._isleaf & LEAF? TRUE : FALSE;
		switch(pass) {
			case TRUE:
				if (*(cur_node._keys+(cnt+1)*keylen) != '\0') {
					i = cnt + 1;
					if (i <= N_KEYS - 1)
						break;					/* found the key */
				}
				bsw = FALSE;					/* haven't found anything yet */
				node = cur_node._parent;
				if (node == 0) {
					i = FALSE;
					goto done;
				}

				while(1) {						/* could back up all the way */
					if ((i = read_node(node,&cur_node,idxno)) < 0)
						goto done;
					for (cnt = 0;cnt < N_KEYS;cnt++) {
						i = cnt * keylen;						/* first offset */
						if (*(cur_node._keys+i) == '\0')	/* key to check */
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
			   				diff = memcmp(tkey+size, cur_node._keys+i+size, keylen-size);
						if (diff <= 0) {
							bsw = TRUE;					/* found it */
							break;
                        }
					}
					if (bsw)
						break;								/* found */
					if (cur_node._parent == 0) {
						i = FALSE;
						goto done;
					}
					node = cur_node._parent;				/* search parent */
				}
				i = cnt;									/* offset to key */
				break;										/* break the case */

			case FALSE:									/* we're at a branch */
				node = cur_node._kids[cnt+1];
				while (1) {								/* go until leaf */
					if ((i = read_node(node,&cur_node,idxno)) < 0)
						goto done;
					if (cur_node._isleaf & LEAF)
						break;
					node = cur_node._kids[0];
				}
				i = 0;
		}												/* END OF SWITCH */

		if ((i = upd_idx(idx,&cur_node,i,node,0,cmd,&rptr)))
			break;
		tkey = substr(cur_node._keys,i*keylen,(i+1)*keylen-1);
/*
 * we need to unlock the index at this point because the call
 * to rm_key necessarily needs to exert a write lock on the
 * index.  after the key is removed, we then re-assert a
 * read lock and go on.
 *
 * this goes for all of the get* routines (and restore as well)
 */
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, tkey);			/* get rid of the key */
		fl_lock(&idx->_lock, LOCK_SH);

		free(tkey);
		read_node(sav_node,&cur_node,idxno);	/* read the updated node */
		cnt = offs;
	}
done:
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
