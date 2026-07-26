/* ***************************************************************
 *
 * PROCEDURE:	get_prior.c
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
 *
 *				Wed Aug  3 18:46:26 MDT 2005
 *				changed to 64 bit offsets in misc.h
 *				tomg
 ************************************************************* */
/*
 * this routine will retreive from the named index the prior sequential
 * key.  the calling sequence (using the #define) is:
 *       get_prior(index_name)
 * where index_name is an index that was previously opened with
 * a call to iopen.  the internal sequence is
 *      if (g_pror(index_name));
 * if no original key exists (i.e. no get has yet been performed), this
 * procedure will terminate the calling process.  IT IS ILLEGAL TO GET_NEXT,
 * OR GET_PRIOR IF NO GET HAS YET BEEN PERFORMED, OR THE LAST GOTTEN KEY
 * WAS "REMOVED".
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
extern int upd_idx(INDEX *,NODE *,int,int64_t, int64_t, char *, char **);
extern char *substr(char *,int, int);

int get_prior(char *cmd, int c_off, char **ret)

{
	INDEX *idx;						/* index pointer */

	int keylen;						/* internal length of key */
	int cnt;						/* loop counter */
	int i;							/* misc usage */
	int acc;						/* misc usage */
	int idxno;
	int offs;
	int size;
	int diff;

	uint16_t compare16;
	uint32_t compare32;
	int64_t node;					/* the node pos */
	int64_t sav_node;				/* save the node info */

	char *tkey;						/* temporary key */
	char *rptr;

	char bsw;						/* boolean value */
	char pass;						/* pass counter */

	NODE cur_node;					/* the last accessed node */

	*ret = NULL;
	idxno = atoi(cmd+c_off);		/* get the index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+idxno) == NULL)
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	if ((tkey = strchr(cmd+c_off, '|') + 1) == (char *)1)	/* we get the hint of where to look for */
		return(EINVMSG);
	node = strtoll(tkey, NULL, 0);							/* the current key from the last search */
	if ((tkey = strchr(tkey, '|') + 1) == (char *)1)		/* we are passed the node number and the */
		return(EINVMSG);
	offs = atoi(tkey);										/* offset to the key from where we found */
	if ((tkey = strchr(tkey, '|') + 1) == (char *)1)		/* -this- key last time */
		return(EINVMSG);
															/* then finally, point to the system key */

	keylen = idx->_keylen + KEY_HEADER_LENGTH;			/* internal len of key */
	bsw = FALSE;					/* default value */
	pass = 0;						/* this is the first pass */
	rptr = NULL;

	if (idx->_keylen < 4)
		compare16 = htons(*(uint16_t *)tkey);
	else
		compare32 = htonl(*(uint32_t *)tkey);

	fl_lock(&idx->_lock, LOCK_SH);

	while(1) {									/* do until done */
		if ((i = read_node(node,&cur_node,idxno)) < 0) {
			goto done;
		}
		for (cnt = offs;cnt < N_KEYS;cnt++) {
			i = cnt * keylen;
			if (*(cur_node._keys+i) == '\0')
				break;							/* end of this node */
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
			   diff = memcmp(tkey+size,cur_node._keys+i+size,keylen-size);
				if (diff == 0)
					bsw = TRUE;					/* the key is found */
				break;							/* continue search */
			}
		}
		if (bsw)
			break;								/* the key is found */
		if (pass)
			if (cur_node._isleaf & LEAF)
				break;							/* we're at a leaf */
		if (!pass)
			node = idx->_rootpos;					/* search root next */
		else
			node = cur_node._kids[cnt];			/* next node to search */
		pass++;									/* make it true */
		offs = 0;								/* for next pass */
	}
	if (!bsw) {
		i = ERMKEY;								/* the key was removed! */
		goto done;
	}
	sav_node = node;
	acc = cnt;

	while (1) {											/* do a lot? */
		pass = cur_node._isleaf & LEAF? TRUE : FALSE;
		switch(pass) {
			case TRUE:
				if (cnt > 0) {
					i = cnt - 1;
					break;								/* found the key */
				}
				bsw = FALSE;				/* haven't found anything yet */
				node = cur_node._parent;
				if (node == 0) {
					i = FALSE;
					goto done;
				}

				while(1) {					/* could back up all the way */
					if ((i = read_node(node,&cur_node,idxno)) < 0) {
						goto done;
					}
					for (cnt = (cur_node._isleaf & ~LEAF)-1;cnt >= 0;cnt--) {
						i = cnt * keylen;
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
			   				diff = memcmp(tkey+size,cur_node._keys+i+size,keylen-size);
						if (diff >= 0) {
							bsw = TRUE;        /* found it */
							break;
						}
					}
					if (bsw)
						break;							/* found a key */
					if (cur_node._parent == 0) {
						i = FALSE;
						goto done;
					}
					node = cur_node._parent;			/* search next prnt */
				}
				i = cnt;								/* offset to key */
				break;									/* break the case */

			case FALSE:
				node = cur_node._kids[cnt];
				while (1) {								/* go until leaf */
					if ((i = read_node(node,&cur_node,idxno)) < 0) {
						goto done;
					}
					if (cur_node._isleaf & LEAF)
						break;
					node = cur_node._kids[cur_node._isleaf & ~LEAF];
				}
				i = ((cur_node._isleaf & ~LEAF) - 1);
        }												/* END OF SWITCH */

		if ((i = upd_idx(idx,&cur_node,i,node, 0, cmd, &rptr)))
            break;

		tkey = substr(cur_node._keys,i*keylen,(i+1)*keylen-1);
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, tkey);	/* get rid of the key */
		fl_lock(&idx->_lock, LOCK_SH);
		free(tkey);                     /* free the space */
		read_node(sav_node,&cur_node,idxno);
        cnt = acc;
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
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
