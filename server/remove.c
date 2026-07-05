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
 ************************************************************* */

/*
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

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>

#include "node.h"
#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define TRUE	1
#define FALSE	0

extern void put_ll(char *,int64_t);
extern int read_node(int64_t, NODE *, int);
extern int collapse_node(int64_t, NODE *, SPLIT *);

extern int idx_cnt;

extern INDEX *_indices;

static int shift_node(int num, int64_t node, NODE *cur_node, INDEX *idx)

{

	int tmp;							/* offset into buff */
	short len;							/* key length */
	char buff[NODESIZE];

	len = idx->_keylen + MISC_LEN;		/* save it here */
	memset(buff,'\0',NODESIZE);			/* blank out the buffer */
	tmp = num * len;
	*buff = --cur_node->_isleaf;
	memcpy(buff+1,cur_node->_keys,tmp);
	memcpy(buff+1+tmp,cur_node->_keys+tmp+len,(N_KEYS-1)*len-tmp);
	tmp = N_KEYS * len + 1;
	llseek(idx->_idxchan, node, SEEK_SET);
	if (write(idx->_idxchan,buff,tmp) != tmp)
		return(ENODWRT);
	return(1);
}

int rm_key(int idxno, int xsw, char *key)

{

	int cnt,offs;					/* loop counter */
	int i;							/* misc usage */
	int len;						/* internal length of key */
	int cmp_len;					/* the length to use in the compare */
	int _rem;
	int diff;
	int size;

	int64_t node;					/* another temporary node reg */
	int64_t temp;					/* temporary node to read */
	int64_t tnode;					/* another temporary node reg */

	uint16_t compare16_value;
	uint32_t compare32_value;

	short kcount;					/* the count of keys in the node */

	char bsw;						/* boolean variable */
	char tkey[64];					/* temporary key pointer */
	char buff[NODESIZE];			/* output buffer */

	NODE cur_node;					/* the node information */
	NODE tmp_node;					/* temporary node information */
	INDEX *idx;
	SPLIT cur_index;

	idx = _indices+idxno;
	if (idx == NULL)
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);
	fl_lock(&idx->_lock, LOCK_EX);
	pthread_mutex_lock(&(idx->_mutex));
	node = idx->_rootpos;				/* search from root node */
	pthread_mutex_unlock(&(idx->_mutex));

	_rem = 0;
	if (*(key+idx->_keylen)) {
		cmp_len = idx->_keylen + MISC_LEN;
		_rem = 1;					/* make it true */
	} else
		cmp_len = idx->_keylen;

	if (idx->_keylen < 4)
		compare16_value = htons(*(uint16_t *)key);
	else
		compare32_value = htonl(*(uint32_t *)key);

	len = idx->_keylen + MISC_LEN;
	bsw = 0;

	while (1) {
		if ((i = read_node(node,&cur_node,idxno)) < 0)
			goto done;
		for (cnt = 0;cnt <= N_KEYS-1;cnt++) {
			i = cnt * len;                      /* beg pos of key */
			if (*(cur_node._keys+i) == '\0')    /* get the key */
				break;                          /* end of this node */
			switch (idx->_keylen) {
				case 1:
					diff = *key - *(cur_node._keys+i);
					size = sizeof(char);
					break;
				case 2:
					diff = compare16_value - htons(*(int16_t *)(cur_node._keys+i));
					size = sizeof(int16_t);
					break;
				case 3:
					diff = compare16_value - htons(*(int16_t *)(cur_node._keys+i));
					if (!diff)
						diff = *(key+sizeof(int16_t))-*(cur_node._keys+i+sizeof(int16_t));
					size = sizeof(int16_t) + sizeof(char);
					break;
				default:
					diff = compare32_value - htonl(*(int32_t *)(cur_node._keys+i));
					size = sizeof(int32_t);
					break;
			}

			if (!diff)
				diff = memcmp(key+size, cur_node._keys+i+size, cmp_len - size);
			if (diff <= 0) {
				if (diff == 0) {
					bsw = 1;                   /* found it */
					tnode = node;
					offs = cnt;
				}
				break;                          /* quit this loop */
			} 
		}
		if (bsw && _rem)
			break;
		if (cur_node._isleaf & LEAF)
			break;                      /* we're at a leaf */
		node = cur_node._kids[cnt];     /* pointer to next node to read */
	}

	if (!bsw) {
		i = FALSE;
		goto done;						/* didn't find anything */
	}

	if (tnode != node) {
		if ((i = read_node(tnode,&cur_node,idxno)) < 0)
			goto done;
		cnt = offs;                             /* the offset */
		node = tnode;                           /* save the node number */
	}
	if (xsw) {
		memcpy(key, cur_node._keys+cnt*len, len);
		i = len;
		goto done;
	}

	memset(tkey, '\0', sizeof(tkey));
	memcpy(tkey, cur_node._keys+cnt*len, len);

	if (cur_node._isleaf & LEAF) {
		if ((i = shift_node(cnt, node, &cur_node, idx)) < 0)
			goto done;
	} else {
		temp = cur_node._kids[cnt+1];           /* the kid node to read */
		while (1) {
			if ((i = read_node(temp,&tmp_node,idxno)) < 0)
				goto done;
			if (tmp_node._isleaf & LEAF)
				break;                          /* found the leaf */
			temp = tmp_node._kids[0];           /* get the lowest level kid */
		}
		memset(buff,'\0',sizeof(buff));
		memset(tkey, '\0', sizeof(tkey));
		memcpy(tkey, tmp_node._keys, len);

		memcpy(cur_node._keys+cnt*len, tkey, len);
		*buff = cur_node._isleaf;
		i = N_KEYS * len;
		memcpy(buff+1,cur_node._keys,i);
		i++;
		memcpy(buff+i, (char *)cur_node._kids, N_KIDS*PTR_LENGTH);
		for(cnt = 0;cnt < N_KIDS;cnt++)
			put_ll(buff+i+cnt*PTR_LENGTH, *(cur_node._kids+cnt));
		i += N_KIDS * PTR_LENGTH;

		llseek(idx->_idxchan, node, SEEK_SET);
		if (write(idx->_idxchan, buff, i) != i) {
			i = (ENODWRT);
			goto done;
		}
		cur_node = tmp_node;
		node = temp;
		if ((i = shift_node(0, node, &cur_node, idx)) < 0)
			goto done;
	}
	kcount = cur_node._isleaf & ~LEAF;
	if (!kcount) {
		cur_index._keylen = idx->_keylen;
		cur_index._idxchan = idx->_idxchan;
		cur_index._curnode = node;
		cur_index._prntnode = cur_node._parent;
		cur_index._rootpos = idx->_rootpos;
		cur_index._curleaf = cur_node._isleaf;
		cur_index._idxno = idxno;
		if ((i = collapse_node(node, &cur_node, &cur_index)) < 0)	/* do the collapse */
			goto done;
	}
	i = TRUE;
done:
	fl_lock(&idx->_lock, LOCK_UN);
	return(i);
}

int dbremove(char *cmd, int c_off, char **ret)
{
	int i;
	int ix;
	int xsw;

	char *cptr;

	*ret = NULL;
	ix = atoi(cmd+c_off);
	if (ix < 0 || ix >= idx_cnt)
		return(ENOINDEX);
	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	xsw = atoi(cptr);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);


	i = rm_key(ix, xsw, cptr);
	if (i > 0) {
		i = sprintf(cmd, "0|%d|", i);
		if (xsw) {
			memcpy(cmd+i, cptr, _indices[ix]._keylen+MISC_LEN);
			i += _indices[ix]._keylen+MISC_LEN;
		}
	}
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
